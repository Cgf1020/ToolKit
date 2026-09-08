/**
 * @file timestamped_file_sink.h
 * @brief 按大小滚动的文件 sink：文件名为模块 stem 加上打开/滚动时的本地时间。
 */
#pragma once

#include <spdlog/details/file_helper.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/details/os.h>
#include <spdlog/sinks/base_sink.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

namespace itflee {
namespace log_backend {

/**
 * @brief 把本地时间格式化为 `YYYY-MM-DD_HH-MM-SS`。
 * @param tm 本地分解时间。
 * @return 时间戳字符串。
 */
inline std::string formatLogTimestamp(const std::tm& tm)
{
    char buf[32];
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", &tm) == 0) {
        return "1970-01-01_00-00-00";
    }
    return buf;
}

/**
 * @brief 将系统时钟转为本地 std::tm。
 * @param tp 时间点。
 * @return 本地分解时间。
 */
inline std::tm toLocalTm(std::chrono::system_clock::time_point tp)
{
    const std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return tm;
}

/**
 * @brief 按文件大小滚动；每次打开新文件时使用 `{stem}_{YYYY-MM-DD}_{HH-MM-SS}{ext}`。
 *
 * 例如配置 `communication.log` 时，实际文件为 `communication_2026-09-08_11-16-53.log`。
 * 超过 max_size 后关闭当前文件并新建带新时间戳的文件；目录中同类文件超过
 * max_files+1 时删除最旧的。
 *
 * @tparam Mutex 锁类型，多线程用 std::mutex。
 *
 * @note 线程安全：部分安全。不安全 API：构造。
 * @note 安全 API：log、flush、set_level、set_formatter（base_sink 持锁后调用 sink_it_/flush_）。
 * @note 可重入：否。不可重入 API：sink_it_、flush_。
 */
template <typename Mutex>
class TimestampedRotatingFileSink final : public spdlog::sinks::base_sink<Mutex> {
public:
    /**
     * @brief 创建 sink 并立刻打开一个带当前时间戳的文件。
     * @param base_filename 逻辑路径，如 `logs/communication.log`；时间戳插在扩展名前。
     * @param max_size 单个文件最大字节数，必须大于 0。
     * @param max_files 额外保留的历史文件数（不含当前正在写入的文件）。
     */
    TimestampedRotatingFileSink(spdlog::filename_t base_filename,
                                std::size_t max_size,
                                std::size_t max_files)
        : base_filename_(std::move(base_filename)),
          max_size_(max_size == 0 ? 1 : max_size),
          max_files_(max_files)
    {
        file_helper_.open(makeUniqueFilename());
        current_size_ = file_helper_.size();
        pruneOldFiles_();
    }

protected:
    /**
     * @brief 写入一条已格式化日志，必要时滚动到新的时间戳文件。
     * @param msg spdlog 日志记录。
     */
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
        auto new_size = current_size_ + formatted.size();
        if (new_size > max_size_) {
            file_helper_.flush();
            if (file_helper_.size() > 0) {
                rotate_();
                new_size = formatted.size();
            }
        }
        file_helper_.write(formatted);
        current_size_ = new_size;
    }

    /**
     * @brief 将当前文件刷到磁盘。
     */
    void flush_() override
    {
        file_helper_.flush();
    }

private:
    /**
     * @brief 生成尚未占用的时间戳文件名。
     * @return 完整路径。
     * @note 前提：已持有 base_sink::mutex_ 或仍在构造中。
     */
    spdlog::filename_t makeUniqueFilename()
    {
        spdlog::filename_t basename;
        spdlog::filename_t ext;
        std::tie(basename, ext) = spdlog::details::file_helper::split_by_extension(base_filename_);

        const auto now = std::chrono::system_clock::now();
        const std::string stamp = formatLogTimestamp(toLocalTm(now));
        spdlog::filename_t candidate = basename + "_" + stamp + ext;
        if (!spdlog::details::os::path_exists(candidate)) {
            return candidate;
        }

        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch())
                            .count() %
                        1000;
        char ms_buf[8];
        std::snprintf(ms_buf, sizeof(ms_buf), "%03lld", static_cast<long long>(ms));
        candidate = basename + "_" + stamp + "_" + ms_buf + ext;
        if (!spdlog::details::os::path_exists(candidate)) {
            return candidate;
        }

        for (int i = 1; i < 1000; ++i) {
            candidate = basename + "_" + stamp + "_" + ms_buf + "_" + std::to_string(i) + ext;
            if (!spdlog::details::os::path_exists(candidate)) {
                return candidate;
            }
        }
        return candidate;
    }

    /**
     * @brief 关闭当前文件并打开一个新的时间戳文件。
     * @note 前提：已持有 base_sink::mutex_。
     */
    void rotate_()
    {
        file_helper_.close();
        file_helper_.open(makeUniqueFilename());
        current_size_ = 0;
        pruneOldFiles_();
    }

    /**
     * @brief 删除超出保留数量的最旧时间戳文件。
     * @note 前提：已持有 base_sink::mutex_ 或仍在构造中。
     */
    void pruneOldFiles_()
    {
        namespace fs = std::filesystem;
        const fs::path base(base_filename_);
        fs::path dir = base.parent_path();
        if (dir.empty()) {
            dir = ".";
        }
        if (!fs::exists(dir) || !fs::is_directory(dir)) {
            return;
        }

        spdlog::filename_t basename;
        spdlog::filename_t ext;
        std::tie(basename, ext) = spdlog::details::file_helper::split_by_extension(base_filename_);
        const std::string prefix = fs::path(basename).filename().string() + "_";
        const std::string current = fs::path(file_helper_.filename()).filename().string();

        std::vector<fs::path> files;
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const std::string name = entry.path().filename().string();
            if (name.size() <= prefix.size()) {
                continue;
            }
            if (name.compare(0, prefix.size(), prefix) != 0) {
                continue;
            }
            if (entry.path().extension().string() != ext) {
                continue;
            }
            files.push_back(entry.path());
        }

        std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) {
            std::error_code ec1;
            std::error_code ec2;
            const auto ta = fs::last_write_time(a, ec1);
            const auto tb = fs::last_write_time(b, ec2);
            if (ec1 || ec2) {
                return a.filename().string() < b.filename().string();
            }
            return ta < tb;
        });

        const std::size_t keep = max_files_ + 1;
        while (files.size() > keep) {
            auto it = files.begin();
            while (it != files.end() && it->filename().string() == current) {
                ++it;
            }
            if (it == files.end()) {
                break;
            }
            std::error_code ec;
            fs::remove(*it, ec);
            files.erase(it);
        }
    }

    spdlog::filename_t base_filename_;            ///< 逻辑文件路径，时间戳插在扩展名前
    std::size_t max_size_{1};                     ///< 滚动阈值（字节）
    std::size_t max_files_{0};                    ///< 额外保留的历史文件数
    std::size_t current_size_{0};                 ///< 当前已打开文件的字节数
    spdlog::details::file_helper file_helper_;    ///< 当前打开的文件
};

using TimestampedRotatingFileSinkMt = TimestampedRotatingFileSink<std::mutex>;
using TimestampedRotatingFileSinkSt = TimestampedRotatingFileSink<spdlog::details::null_mutex>;

}  // namespace log_backend
}  // namespace itflee
