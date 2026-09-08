/**
 * @file backend.h
 * @brief 日志后端抽象：门面只依赖本接口，不包含 spdlog 或 Boost.Log。
 */
#pragma once

#include "base/log/logger_config.h"

#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace itflee {
namespace log_backend {

/**
 * @brief 从源码路径取出文件名（不含目录）。
 * @param path `__FILE__` 或空。
 * @return 文件名；path 为空时返回空字符串。
 */
inline std::string sourceFileName(const char* path)
{
    if (path == nullptr || path[0] == '\0') {
        return {};
    }
    const std::string full(path);
    const auto pos = full.find_last_of("/\\");
    if (pos == std::string::npos) {
        return full;
    }
    return full.substr(pos + 1);
}

/**
 * @brief 生成格式中的源码标签：`file.cpp` 或 `file.cpp:123`。
 * @param loc 源码位置。
 * @param with_line 是否追加行号。
 * @return 标签文本；无文件名时为空。
 */
inline std::string sourceFileTag(const SourceLocation& loc, bool with_line)
{
    std::string tag = sourceFileName(loc.file);
    if (with_line && loc.line > 0 && !tag.empty()) {
        tag.append(":").append(std::to_string(loc.line));
    }
    return tag;
}

/**
 * @brief 级别的大写名称，用于 stderr 回退输出。
 * @param level 日志级别。
 * @return 静态字符串，如 `INFO`。
 */
inline const char* levelName(LogLevel level)
{
    switch (level) {
        case LogLevel::Trace:
            return "TRACE";
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warn:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
        case LogLevel::Critical:
            return "CRITICAL";
        default:
            return "INFO";
    }
}

/**
 * @brief 未初始化或后端不可用时同步写 stderr。
 * @param name 模块名。
 * @param level 级别。
 * @param message 正文。
 */
inline void logToStderr(std::string_view name, LogLevel level, std::string_view message)
{
    std::cerr << '[' << levelName(level) << "] [" << name << "] " << message << '\n';
    if (static_cast<int>(level) >= static_cast<int>(LogLevel::Warn)) {
        std::cerr.flush();
    }
}

/**
 * @brief 取两者中更细（数值更小）的级别。
 * @param a 级别 A。
 * @param b 级别 B。
 * @return 更细的级别。
 */
inline LogLevel mostVerbose(LogLevel a, LogLevel b)
{
    return static_cast<int>(a) < static_cast<int>(b) ? a : b;
}

/**
 * @brief 解析模块是否输出到控制台。
 * @param config 全局配置。
 * @param module 模块配置。
 * @return 模块未指定时返回全局 console。
 */
inline bool resolveModuleConsole(const LoggerConfig& config, const ModuleConfig& module)
{
    return module.console.value_or(config.console);
}

/**
 * @brief 解析模块控制台最低级别。
 * @param config 全局配置。
 * @param module 模块配置。
 * @return 模块未指定时返回全局 console_level。
 */
inline LogLevel resolveModuleConsoleLevel(const LoggerConfig& config, const ModuleConfig& module)
{
    return module.console_level.value_or(config.console_level);
}

/**
 * @brief 解析 Auto/On/Off 是否在格式中输出行号。
 * @param mode 配置项。
 * @return 需要行号返回 true。
 */
inline bool resolveSourceEnabled(SourceLocationMode mode)
{
    switch (mode) {
        case SourceLocationMode::On:
            return true;
        case SourceLocationMode::Off:
            return false;
        case SourceLocationMode::Auto:
        default:
#ifdef NDEBUG
            return false;
#else
            return true;
#endif
    }
}

/**
 * @brief 进程级日志后端。
 *
 * @note 线程安全：部分安全。不安全 API：init、shutdown（不可与其它公开 API 并发）。
 * @note 安全 API：isInitialized、flush、setLevel、resolveName、shouldLog、log。
 * @note 可重入：否。不可重入 API：log 在 sink/formatter 执行期间再次调用。
 */
class ILogBackend {
public:
    /**
     * @brief 销毁后端。
     */
    virtual ~ILogBackend() = default;

    /**
     * @brief 按配置创建 sink 与模块路由。
     * @param config 进程级配置。
     * @return 首次成功返回 true；已初始化或失败返回 false。
     */
    virtual bool init(const LoggerConfig& config) = 0;

    /**
     * @brief 刷空队列并释放底层资源。
     */
    virtual void shutdown() = 0;

    /**
     * @brief 主动刷盘。
     */
    virtual void flush() = 0;

    /**
     * @brief 是否已 init 且未 shutdown。
     * @return 已初始化返回 true。
     */
    virtual bool isInitialized() const = 0;

    /**
     * @brief 设置全局最低级别。
     * @param level 新的全局级别。
     */
    virtual void setLevel(LogLevel level) = 0;

    /**
     * @brief 设置单个模块的最低级别。
     * @param name 模块名。
     * @param level 该模块文件级别。
     */
    virtual void setLevel(std::string_view name, LogLevel level) = 0;

    /**
     * @brief 将请求的模块名解析为已注册名。
     * @param name 请求的模块名。
     * @return 实际使用的模块名。
     */
    virtual std::string resolveName(std::string_view name) = 0;

    /**
     * @brief 判断该模块在当前级别下是否会输出。
     * @param name 已解析的模块名。
     * @param level 待判断级别。
     * @return 会输出返回 true；未初始化时返回 true。
     */
    virtual bool shouldLog(std::string_view name, LogLevel level) const = 0;

    /**
     * @brief 写入一条已格式化日志。
     * @param name 已解析的模块名。
     * @param level 级别。
     * @param loc 源码位置。
     * @param message 正文。
     */
    virtual void log(std::string_view name,
                     LogLevel level,
                     const SourceLocation& loc,
                     std::string_view message) = 0;
};

}  // namespace log_backend
}  // namespace itflee