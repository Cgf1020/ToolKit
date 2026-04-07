#ifndef ITFLEE_TCP_CONNECTION_H_
#define ITFLEE_TCP_CONNECTION_H_

#include "define.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

namespace itflee {

/**
 * @file tcp_connection.h
 * @brief TCP 连接抽象接口定义。
 */

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#pragma warning(disable : 4275)
#endif

/**
 * @brief TCP 连接抽象（类似 libhv SocketChannel）。
 * @note 实例由 TcpServer / TcpClient 内部创建，用户通过回调获得 shared_ptr。
 * @note Write() 线程安全（内部 post 到 loop）；其余查询方法也线程安全。
 */
class ITFLEEEXPORT TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    /** @brief 连接生命周期状态。 */
    enum State {
        kConnecting,    ///< 正在连接
        kConnected,     ///< 已连接
        kDisconnecting, ///< 主动关闭中
        kDisconnected   ///< 已断开
    };

    /** @brief 虚析构，允许通过基类指针安全释放实现对象。 */
    virtual ~TcpConnection() = default;

    /**
     * @brief 获取当前连接状态。
     * @return 当前 State。
     */
    virtual State GetState() const noexcept = 0;

    /**
     * @brief 判断连接是否处于已连接状态。
     * @return true 表示 kConnected。
     */
    bool IsConnected() const noexcept { return GetState() == kConnected; }

    /**
     * @brief 异步写入数据（线程安全）。
     * @return 0 成功排队；-1 未连接或已关闭。
     */
    virtual int Write(const void* data, std::size_t len) = 0;
    int Write(const std::string& str) { return Write(str.data(), str.size()); }

    /**
     * @brief 主动关闭连接（线程安全）。
     * @note 请求关闭后、loop 执行实际关断前，GetState() 可能短暂为 kDisconnecting；
     *       关断完成后在 loop 上触发 OnConnectionStateChanged(kDisconnected)，与最终状态一致。
     */
    virtual void Close() = 0;

    /**
     * @brief 获取本地端地址字符串（ip:port）。
     * @return 本地地址；不可用时返回空字符串。
     */
    virtual std::string LocalAddr() const = 0;

    /**
     * @brief 获取对端地址字符串（ip:port）。
     * @return 对端地址；不可用时返回空字符串。
     */
    virtual std::string PeerAddr() const = 0;

    /**
     * @brief 获取连接唯一标识。
     * @return 连接 ID。
     */
    virtual uint32_t Id() const noexcept = 0;

    /**
     * @brief 启用连接心跳（线程安全）。
     * @param interval_ms 心跳发送间隔（毫秒，需 > 0）。
     * @param timeout_ms  读超时阈值（毫秒，需 > 0，通常 >= interval_ms）。
     * @param heartbeat_payload 心跳包内容（为空则仅做超时检测，不主动发送心跳）。
     * @return 0 成功；-1 参数非法或连接不可用。
     * @note 任意收到数据都会刷新活跃时间；超过 timeout_ms 未收到数据将主动 Close()。
     */
    virtual int EnableHeartbeat(int interval_ms,
                                int timeout_ms,
                                std::string heartbeat_payload = "PING\n") = 0;

    /**
     * @brief 关闭心跳（线程安全）。
     */
    virtual void DisableHeartbeat() = 0;

    /**
     * @brief 绑定用户上下文对象到连接。
     * @param ctx 任意共享对象，可为 nullptr。
     */
    void SetContext(std::shared_ptr<void> ctx) { context_ = std::move(ctx); }

    /**
     * @brief 获取已绑定的用户上下文对象。
     * @return 上下文 shared_ptr，未设置时为空。
     */
    std::shared_ptr<void> GetContext() const { return context_; }

    template <typename T>
    /**
     * @brief 按类型获取上下文对象。
     * @tparam T 目标类型。
     * @return 转换后的上下文对象；类型不匹配时行为与 static_pointer_cast 一致。
     */
    std::shared_ptr<T> GetContextAs() const {
        return std::static_pointer_cast<T>(context_);
    }

protected:
    TcpConnection() = default;

private:
    std::shared_ptr<void> context_;
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

} // namespace itflee

#endif // ITFLEE_TCP_CONNECTION_H_
