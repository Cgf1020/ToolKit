/**
 * @file boost_log_backend.h
 * @brief Boost.Log 日志后端。仅 src/base/log 使用，本头文件不包含 Boost.Log。
 *
 * 异步时每个文件/控制台 sink 使用独立投递线程，无法把
 * LoggerConfig::async_thread_count 映射为共享线程池。
 * 异步队列容量为编译期常量 8192，满时阻塞（block），忽略运行时
 * async_queue_size。
 */
#pragma once

#include "backend.h"

#include <memory>

namespace itflee {
namespace log_backend {

/**
 * @brief 基于 Boost.Log 的 ILogBackend 实现。
 *
 * @note 线程安全：部分安全。不安全 API：init、shutdown。
 * @note 安全 API：isInitialized、flush、setLevel、resolveName、shouldLog、log。
 * @note 可重入：否。不可重入 API：log 在 sink/formatter 执行期间再次调用。
 */
class BoostLogBackend final : public ILogBackend {
public:
    /**
     * @brief 构造未初始化的后端。
     */
    BoostLogBackend();

    /**
     * @brief 若仍处于 initialized，先 shutdown。
     */
    ~BoostLogBackend() override;

    /**
     * @brief 禁止拷贝。
     */
    BoostLogBackend(const BoostLogBackend&) = delete;

    /**
     * @brief 禁止赋值。
     */
    BoostLogBackend& operator=(const BoostLogBackend&) = delete;

    /**
     * @brief 按配置创建 Boost.Log sink 与模块路由。
     * @param config 进程级配置。
     * @return 首次成功返回 true；已初始化或失败返回 false。
     */
    bool init(const LoggerConfig& config) override;

    /**
     * @brief 刷空队列并移除全部 sink。
     */
    void shutdown() override;

    /**
     * @brief 刷新全部 sink。
     */
    void flush() override;

    /**
     * @brief 是否处于 init 成功且未 shutdown 的状态。
     * @return 已初始化返回 true。
     */
    bool isInitialized() const override;

    /**
     * @brief 设置全局最低级别并更新各模块文件 sink 过滤器。
     * @param level 新的全局级别。
     */
    void setLevel(LogLevel level) override;

    /**
     * @brief 设置单个模块的最低级别。
     * @param name 模块名。
     * @param level 该模块文件级别。
     */
    void setLevel(std::string_view name, LogLevel level) override;

    /**
     * @brief 将请求的模块名解析为已注册名。
     * @param name 请求的模块名。
     * @return 实际使用的模块名。
     */
    std::string resolveName(std::string_view name) override;

    /**
     * @brief 判断该模块在当前级别下是否会输出。
     * @param name 已解析的模块名。
     * @param level 待判断级别。
     * @return 会输出返回 true；未初始化时返回 true。
     */
    bool shouldLog(std::string_view name, LogLevel level) const override;

    /**
     * @brief 写入一条已格式化日志。
     * @param name 已解析的模块名。
     * @param level 级别。
     * @param loc 源码位置。
     * @param message 正文。
     */
    void log(std::string_view name,
             LogLevel level,
             const SourceLocation& loc,
             std::string_view message) override;

private:
    struct Impl;                 ///< 持有 Boost.Log 类型，定义在 .cpp
    std::unique_ptr<Impl> impl_; ///< 实现细节
};

/**
 * @brief 创建 Boost.Log 后端实例。
 * @return 未 init 的后端。
 */
std::unique_ptr<ILogBackend> createBoostLogBackend();

}  // namespace log_backend
}  // namespace itflee
