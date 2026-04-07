#ifndef ITFLEE_TCP_CALLBACK_H_
#define ITFLEE_TCP_CALLBACK_H_

#include <cstddef>
#include <memory>
#include <string>

namespace itflee {

/**
 * @file tcp_callback.h
 * @brief TCP 模块回调与连接状态定义。
 */

class TcpConnection;
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

/**
 * @brief TCP 连接状态枚举。
 */
enum class ConnectionState {
    kConnecting,    ///< 正在连接
    kConnected,     ///< 连接成功
    kDisconnecting, ///< 本端主动关闭中
    kDisconnected,  ///< 连接已关闭（对端关闭或网络错误）
    kConnectFailed  ///< 连接失败（解析失败/拒绝连接等）
};

/**
 * @brief 将 ConnectionState 转换为可读字符串。
 * @param state 连接状态枚举值。
 * @return 对应的状态名称字符串。
 */
inline std::string ConnectionStateToString(ConnectionState state) {
    switch (state) {
    case ConnectionState::kConnecting:
        return "kConnecting";
    case ConnectionState::kConnected:
        return "kConnected";
    case ConnectionState::kDisconnecting:
        return "kDisconnecting";
    case ConnectionState::kDisconnected:
        return "kDisconnected";
    case ConnectionState::kConnectFailed:
        return "kConnectFailed";
    default:
        return "UnknownConnectionState";
    }
}

/**
 * @brief TCP 连接回调接口，TCP Server 与 TCP Client 共用。
 * @note 所有回调均在关联的 EventLoop 线程上调用。
 */
class TcpConnectionCallback {
public:
    /** @brief 虚析构，允许通过基类指针安全析构派生回调对象。 */
    virtual ~TcpConnectionCallback() = default;

    /**
     * @brief 连接状态变化回调。
     * @param conn  关联连接对象；当 state 为 kConnectFailed 时可能为空。
     * @param state 新的连接状态。
     */
    virtual void OnConnectionStateChanged(const TcpConnectionPtr& conn, ConnectionState state) = 0;

    /**
     * @brief 收到 TCP 数据时回调。
     * @param conn  关联连接对象。
     * @param data  数据指针（仅在回调期间有效，需要保留请自行拷贝）。
     * @param len   数据长度（字节）。
     */
    virtual void OnMessage(const TcpConnectionPtr& conn, const void* data, std::size_t len) = 0;

    /**
     * @brief 发送队列清空时回调（可选覆写）。
     * @param conn 关联连接对象。
     */
    virtual void OnWriteComplete(const TcpConnectionPtr& conn) { (void)conn; }
};

} // namespace itflee

#endif // ITFLEE_TCP_CALLBACK_H_
