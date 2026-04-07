#ifndef ITFLEE_TCP_SERVER_INTERFACE_H_
#define ITFLEE_TCP_SERVER_INTERFACE_H_

#include "define.h"
#include "network/tcp/tcp_callback.h"
#include "base/eventloop/event_loop_interface.h"

#include <cstddef>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace itflee {

/**
 * @file tcp_server_interface.h
 * @brief TCP 服务端抽象接口定义（MainReactor/SubReactor 架构）。
 */

/**
 * @brief TCP Server 配置选项。
 */
enum class TcpLoadBalancePolicy {
    kRoundRobin,   ///< 轮询
    kLeastConn     ///< 最少连接优先
};

enum class TcpCallbackDispatchMode {
    kSubThread,    ///< 在 SubReactor 线程回调
    kMainThread    ///< 统一投递到 MainReactor 线程回调
};

enum class TcpShutdownMode {
    kGraceful,     ///< 尝试 drain 后再强制关闭
    kForce         ///< 立即关闭连接并停止线程
};

struct TcpServerSubStats {
    size_t sub_index = 0;
    size_t connection_count = 0;
};

struct TcpServerStats {
    size_t total_connections = 0;
    std::vector<TcpServerSubStats> per_sub;
};

struct TcpServerOptions {
    /** @brief SubReactor 工作线程数，0 表示使用 hardware_concurrency。 */
    size_t worker_count = 0;

    /** @brief listen backlog，0 表示使用系统默认。 */
    int backlog = 0;

    /** @brief 是否设置 SO_REUSEADDR。 */
    bool reuse_addr = true;

    /** @brief 是否尝试设置 SO_REUSEPORT（Linux 可选，平台不支持时忽略）。 */
    bool reuse_port = false;

    /** @brief 回调派发模式。 */
    TcpCallbackDispatchMode cb_dispatch_mode = TcpCallbackDispatchMode::kSubThread;

    /** @brief 优雅停机等待连接自然 drain 的超时时间。 */
    std::chrono::milliseconds graceful_shutdown_timeout{1500};

    /** @brief 新连接负载均衡策略。 */
    TcpLoadBalancePolicy load_balance_policy = TcpLoadBalancePolicy::kRoundRobin;

    size_t GetEffectiveWorkerCount() const noexcept {
        if (worker_count > 0) return worker_count;
        const auto n = std::thread::hardware_concurrency();
        return n > 1 ? n : 1;
    }
};

/**
 * @brief TCP 服务器抽象接口（MainReactor/SubReactor 架构）。
 *
 * MainReactor 仅负责 accept 新连接；SubReactor（N 个工作线程）负责连接 I/O。
 * 新连接通过轮询策略分发到某个 SubReactor。
 *
 * @note Create(callback) 内部创建 MainReactor 和所有 SubReactor 线程。
 * @note Create(callback, loop) 使用外部 loop 作为 MainReactor，SubReactor 仍由内部创建。
 */
class ITFLEEEXPORT TcpServerInterface {
public:
    virtual ~TcpServerInterface() = default;

    /**
     * @brief 工厂方法。
     * @param callback 连接/消息回调（生命周期需覆盖 TcpServer）。
     * @param loop     MainReactor 事件循环；为空则内部创建。
     * @param options  服务器配置选项。
     */
    static std::shared_ptr<TcpServerInterface> Create(
        std::shared_ptr<TcpConnectionCallback> callback,
        std::shared_ptr<EventLoopInterface> loop = nullptr,
        const TcpServerOptions& options = {});

    /**
     * @brief 绑定并监听端口，启动 MainReactor 和所有 SubReactor 线程，开始 accept。
     * @return 0 成功；负值失败。
     */
    virtual int Listen(int port, const std::string& host = "0.0.0.0") = 0;

    /**
     * @brief 获取 MainReactor 事件循环实例。
     */
    virtual std::shared_ptr<EventLoopInterface> GetLoop() const noexcept = 0;

    /**
     * @brief 当前所有 SubReactor 上的活动连接总数。
     */
    virtual std::size_t ConnectionCount() const = 0;

    /**
     * @brief 向所有已连接客户端广播数据（分桶并行投递到各 SubReactor）。
     * @return 广播的连接数。
     */
    virtual int Broadcast(const void* data, std::size_t len) = 0;

    int Broadcast(const std::string& str) { return Broadcast(str.data(), str.size()); }

    virtual int Shutdown(TcpShutdownMode mode = TcpShutdownMode::kGraceful,
                         std::chrono::milliseconds timeout = std::chrono::milliseconds(0)) = 0;

    /**
     * @brief 获取当前统计信息（总连接数、各 SubReactor 连接分布）。
     */
    virtual TcpServerStats GetStats() const = 0;

protected:
    TcpServerInterface() = default;
};

} // namespace itflee

#endif // ITFLEE_TCP_SERVER_INTERFACE_H_
