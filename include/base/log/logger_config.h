/**
 * @file logger_config.h
 * @brief Logger 初始化配置：全局选项与模块-文件路由。
 */
#ifndef ITFLEE_LOGGER_CONFIG_H_
#define ITFLEE_LOGGER_CONFIG_H_

#include "base/log/log_level.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace itflee {

/**
 * @brief 日志实现后端。进程内由 Logger::init 选择其一。
 *
 * @note 线程安全：不适用（无共享可变状态）。
 */
enum class LogBackendKind {
    Spdlog = 0,   ///< spdlog 异步/同步文件与控制台
    BoostLog = 1  ///< Boost.Log；异步时每个文件 sink 自带投递线程
};

/**
 * @brief 单个具名模块的文件路由与输出配置。未填的 optional 项回退到 LoggerConfig 全局值。
 *
 * @note 线程安全：不适用（无共享可变状态）。
 */
struct ModuleConfig {
    std::string name;  ///< 模块名，作为 Logger::get 的查找键与文件路由键
    std::string file;  ///< 逻辑文件名，空则 `{name}.log`；落盘为 `{stem}_{YYYY-MM-DD}_{HH-MM-SS}{ext}`
    std::optional<LogLevel> level;          ///< 该模块文件的最低级别；空则使用全局 level
    std::optional<bool> console;            ///< 该模块是否输出到控制台；空则使用全局 console
    std::optional<LogLevel> console_level;  ///< 该模块控制台最低级别；空则使用全局 console_level

    ModuleConfig() = default;
    ModuleConfig(std::string name_,
                 std::string file_,
                 std::optional<LogLevel> level_ = {},
                 std::optional<bool> console_ = {},
                 std::optional<LogLevel> console_level_ = {})
        : name(std::move(name_)),
          file(std::move(file_)),
          level(std::move(level_)),
          console(console_),
          console_level(console_level_)
    {
    }
};

/**
 * @brief 日志系统进程级配置，由 Application 在 Logger::init 时传入。
 *
 * Logger 不解析 INI/YAML；由调用方填充本结构。
 *
 * @note 线程安全：不适用（无共享可变状态）。
 */
struct LoggerConfig {
    LogLevel level{LogLevel::Info};  ///< 全局最低级别；更低级别的日志不格式化、不落盘
    bool console{true};              ///< 默认是否输出到控制台；可被 ModuleConfig::console 覆盖
    LogLevel console_level{LogLevel::Info};  ///< 默认控制台最低级别；可被 ModuleConfig::console_level 覆盖
    std::string directory{"logs"};   ///< 日志目录，不存在则创建
    std::size_t max_size{100 * 1024 * 1024};  ///< 单个文件滚动阈值（字节），默认 100MB
    std::size_t max_files{10};  ///< 每个模块额外保留的历史文件数（不含当前正在写入的 `{stem}_{时间}.log`）
    bool async{true};                ///< 是否异步写盘
    std::size_t async_queue_size{8192};  ///< 异步队列容量（条）；Boost.Log 为编译期常量，实现固定 8192
    std::size_t async_thread_count{1};  ///< spdlog 共享线程池大小；Boost.Log 每个 sink 一条投递线程，本字段无效
    SourceLocationMode source_location{SourceLocationMode::Auto};  ///< 是否在源文件名后附加行号，如 [file.cpp:123]
    LogLevel flush_level{LogLevel::Warn};  ///< 达到该级别立即 flush（含更高级别）
    std::string default_module;      ///< LOG_INFO 等默认宏写入的模块；空则用 modules 第一项或自动创建的 default
    std::vector<ModuleConfig> modules;  ///< 模块列表；空则自动创建 name=default, file=default.log
    LogBackendKind backend{LogBackendKind::Spdlog};  ///< 底层实现；换后端须先 shutdown 再 init
};

}  // namespace itflee

#endif  // ITFLEE_LOGGER_CONFIG_H_
