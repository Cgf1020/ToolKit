/**
 * @file logger.h
 * @brief 进程级日志门面：按模块名路由到不同文件，不向业务暴露底层日志库类型。
 */
#ifndef ITFLEE_LOGGER_H_
#define ITFLEE_LOGGER_H_

#include "base/log/logger_config.h"

#include <exception>
#include <string>
#include <string_view>

#ifndef FMT_HEADER_ONLY
#define FMT_HEADER_ONLY
#endif
#ifndef FMT_USE_WINDOWS_H
#define FMT_USE_WINDOWS_H 0
#endif
#include <bundled/format.h>

namespace itflee {

/**
 * @brief 将 fmt 风格格式串渲染为字符串；格式错误时返回错误说明而不是抛出。
 * @tparam Args 格式参数类型。
 * @param fmt 格式串，使用 `{}` 占位符。
 * @param args 格式参数。
 * @return 渲染后的文本。
 */
template <typename... Args>
inline std::string formatLog(const char* fmt, const Args&... args)
{
    try {
        return fmt::format(fmt::runtime(fmt), args...);
    } catch (const std::exception& e) {
        return std::string("log format error: ") + e.what();
    }
}

/**
 * @brief 具名模块日志句柄，以及进程级 init/shutdown。
 *
 * Application 在启动时调用 init，在停业务后调用 shutdown。
 * 业务通过 get(name) 或 LOG_INFO / LOG_M_INFO 打日志。
 *
 * @note 线程安全：部分安全。不安全 API：init、shutdown（不可与任何公开 API 并发）。
 * @note 安全 API：get、getDefault、isInitialized、shouldLog、log、logMessage、
 *       trace/debug/info/warn/error/critical、setLevel、flush。
 * @note 可重入：否。不可重入 API：log 及其级别封装在 sink/formatter 执行期间再次调用。
 */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif
class ITFLEEEXPORT Logger {
public:
    /**
     * @brief 按已注册模块名构造句柄。
     * @param name 模块名；未注册时 get() 会回退到 default 模块。
     */
    explicit Logger(std::string name);

    /**
     * @brief 初始化日志系统。进程内首次成功后再次调用将被忽略。
     * @param config 全局与模块配置。
     * @return 首次成功返回 true；已初始化或失败返回 false。
     */
    static bool init(const LoggerConfig& config);

    /**
     * @brief 刷空异步队列并关闭日志系统。之后打日志走 stderr，直到再次 init。
     */
    static void shutdown();

    /**
     * @brief 主动将已入队日志刷到文件和控制台。
     */
    static void flush();

    /**
     * @brief 查询是否已成功 init 且尚未 shutdown。
     * @return 已初始化返回 true。
     * @note 线程安全：是。
     */
    static bool isInitialized();

    /**
     * @brief 设置全局最低日志级别，同时作用于所有模块。
     * @param level 新的全局最低级别。
     */
    static void setLevel(LogLevel level);

    /**
     * @brief 设置单个模块文件 sink 的最低日志级别（不影响该模块的控制台开关）。
     * @param name 模块名。
     * @param level 该模块文件的最低级别。
     */
    static void setLevel(std::string_view name, LogLevel level);

    /**
     * @brief 获取具名模块句柄。
     * @param name 模块名。未注册时回退到 default 模块，并仅 Warn 一次。
     * @return 模块日志句柄。
     */
    static Logger get(std::string_view name);

    /**
     * @brief 获取默认模块句柄（供 LOG_INFO 等宏使用）。
     * @return 默认模块日志句柄。
     */
    static Logger getDefault();

    /**
     * @brief 本句柄对应的模块名。
     * @return 模块名视图，生命周期与句柄内存储的字符串相同。
     */
    std::string_view name() const;

    /**
     * @brief 当前配置下该级别是否会输出。
     * @param level 待判断级别。
     * @return 会输出返回 true。未 init 时返回 true（将写 stderr）。
     * @note 线程安全：是。
     */
    bool shouldLog(LogLevel level) const;

    /**
     * @brief 写入已格式化的一条日志。
     * @param level 级别。
     * @param loc 源码位置；用于格式中的文件名。source_location 关闭时仍输出文件名，但不输出行号。
     * @param message 已经格式化的正文。
     */
    void logMessage(LogLevel level, const SourceLocation& loc, std::string_view message) const;

    /**
     * @brief 按级别格式化并写入日志；级别不够时不格式化。
     * @tparam Args 格式参数类型。
     * @param level 级别。
     * @param loc 源码位置。
     * @param fmt 格式串。
     * @param args 格式参数。
     */
    template <typename... Args>
    void log(LogLevel level, const SourceLocation& loc, const char* fmt, const Args&... args) const
    {
        if (!shouldLog(level)) {
            return;
        }
        logMessage(level, loc, formatLog(fmt, args...));
    }

    /**
     * @brief 输出 TRACE 日志。
     * @tparam Args 格式参数类型。
     * @param fmt 格式串。
     * @param args 格式参数。
     */
    template <typename... Args>
    void trace(const char* fmt, const Args&... args) const
    {
        log(LogLevel::Trace, SourceLocation{}, fmt, args...);
    }

    /**
     * @brief 输出 DEBUG 日志。
     * @tparam Args 格式参数类型。
     * @param fmt 格式串。
     * @param args 格式参数。
     */
    template <typename... Args>
    void debug(const char* fmt, const Args&... args) const
    {
        log(LogLevel::Debug, SourceLocation{}, fmt, args...);
    }

    /**
     * @brief 输出 INFO 日志。
     * @tparam Args 格式参数类型。
     * @param fmt 格式串。
     * @param args 格式参数。
     */
    template <typename... Args>
    void info(const char* fmt, const Args&... args) const
    {
        log(LogLevel::Info, SourceLocation{}, fmt, args...);
    }

    /**
     * @brief 输出 WARN 日志。
     * @tparam Args 格式参数类型。
     * @param fmt 格式串。
     * @param args 格式参数。
     */
    template <typename... Args>
    void warn(const char* fmt, const Args&... args) const
    {
        log(LogLevel::Warn, SourceLocation{}, fmt, args...);
    }

    /**
     * @brief 输出 ERROR 日志。
     * @tparam Args 格式参数类型。
     * @param fmt 格式串。
     * @param args 格式参数。
     */
    template <typename... Args>
    void error(const char* fmt, const Args&... args) const
    {
        log(LogLevel::Error, SourceLocation{}, fmt, args...);
    }

    /**
     * @brief 输出 CRITICAL 日志并立即 flush。
     * @tparam Args 格式参数类型。
     * @param fmt 格式串。
     * @param args 格式参数。
     */
    template <typename... Args>
    void critical(const char* fmt, const Args&... args) const
    {
        log(LogLevel::Critical, SourceLocation{}, fmt, args...);
    }

private:
    std::string name_;  ///< 模块名
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

}  // namespace itflee

/**
 * @brief 当前源码位置，供日志宏使用。
 */
#define ITFLEE_LOG_LOC ::itflee::SourceLocation{__FILE__, __LINE__, __FUNCTION__}

/**
 * @brief 向默认模块输出 TRACE。
 */
#define LOG_TRACE(...) ::itflee::Logger::getDefault().log(::itflee::LogLevel::Trace, ITFLEE_LOG_LOC, __VA_ARGS__)

/**
 * @brief 向默认模块输出 DEBUG。
 */
#define LOG_DEBUG(...) ::itflee::Logger::getDefault().log(::itflee::LogLevel::Debug, ITFLEE_LOG_LOC, __VA_ARGS__)

/**
 * @brief 向默认模块输出 INFO。
 */
#define LOG_INFO(...) ::itflee::Logger::getDefault().log(::itflee::LogLevel::Info, ITFLEE_LOG_LOC, __VA_ARGS__)

/**
 * @brief 向默认模块输出 WARN。
 */
#define LOG_WARN(...) ::itflee::Logger::getDefault().log(::itflee::LogLevel::Warn, ITFLEE_LOG_LOC, __VA_ARGS__)

/**
 * @brief 向默认模块输出 ERROR。
 */
#define LOG_ERROR(...) ::itflee::Logger::getDefault().log(::itflee::LogLevel::Error, ITFLEE_LOG_LOC, __VA_ARGS__)

/**
 * @brief 向默认模块输出 CRITICAL。
 */
#define LOG_CRITICAL(...) ::itflee::Logger::getDefault().log(::itflee::LogLevel::Critical, ITFLEE_LOG_LOC, __VA_ARGS__)

/**
 * @brief 向指定模块输出 TRACE。
 * @param module 模块名字符串。
 */
#define LOG_M_TRACE(module, ...) ::itflee::Logger::get(module).log(::itflee::LogLevel::Trace, ITFLEE_LOG_LOC, __VA_ARGS__)

/**
 * @brief 向指定模块输出 DEBUG。
 * @param module 模块名字符串。
 */
#define LOG_M_DEBUG(module, ...) ::itflee::Logger::get(module).log(::itflee::LogLevel::Debug, ITFLEE_LOG_LOC, __VA_ARGS__)

/**
 * @brief 向指定模块输出 INFO。
 * @param module 模块名字符串。
 */
#define LOG_M_INFO(module, ...) ::itflee::Logger::get(module).log(::itflee::LogLevel::Info, ITFLEE_LOG_LOC, __VA_ARGS__)

/**
 * @brief 向指定模块输出 WARN。
 * @param module 模块名字符串。
 */
#define LOG_M_WARN(module, ...) ::itflee::Logger::get(module).log(::itflee::LogLevel::Warn, ITFLEE_LOG_LOC, __VA_ARGS__)

/**
 * @brief 向指定模块输出 ERROR。
 * @param module 模块名字符串。
 */
#define LOG_M_ERROR(module, ...) ::itflee::Logger::get(module).log(::itflee::LogLevel::Error, ITFLEE_LOG_LOC, __VA_ARGS__)

/**
 * @brief 向指定模块输出 CRITICAL。
 * @param module 模块名字符串。
 */
#define LOG_M_CRITICAL(module, ...) ::itflee::Logger::get(module).log(::itflee::LogLevel::Critical, ITFLEE_LOG_LOC, __VA_ARGS__)

/**
 * @brief 记录标准异常，等价于 ERROR + e.what()。
 * @param e 异常对象，需提供 what()。
 */
#define LOG_EXCEPTION(e) LOG_ERROR("Exception: {}", (e).what())

#endif  // ITFLEE_LOGGER_H_
