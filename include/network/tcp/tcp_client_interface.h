#ifndef ITFLEE_TCP_CLIENT_INTERFACE_H_
#define ITFLEE_TCP_CLIENT_INTERFACE_H_

#include "define.h"
#include "network/tcp/tcp_callback.h"
#include "base/eventloop/event_loop_interface.h"

#include <cstddef>
#include <memory>
#include <string>

namespace itflee {

/**
 * @file tcp_client_interface.h
 * @brief TCP 客户端抽象接口定义。
 */

/**
 * @brief TCP 客户端抽象接口。
 * @note 设计参考 hv::TcpClient，基于 EventLoopInterface 驱动异步 I/O。
 * @note Connect() 启动内部线程（若有）并发起异步连接；Close() 关闭连接并停止内部线程（若有）。
 */
class ITFLEEEXPORT TcpClientInterface {
public:
    /** @brief 虚析构，允许通过基类指针安全释放实现对象。 */
    virtual ~TcpClientInterface() = default;

    /**
     * @brief 工厂方法。
     * @param callback 连接 / 消息回调。
     * @param loop     事件循环；为空则内部创建 EventLoopThread。
     */
    static std::shared_ptr<TcpClientInterface> Create(
        std::shared_ptr<TcpConnectionCallback> callback,
        std::shared_ptr<EventLoopInterface> loop = nullptr);

    /**
     * @brief 获取绑定的事件循环实例。
     * @return Create 时传入的 loop，或内部创建并持有的 loop。
     */
    virtual std::shared_ptr<EventLoopInterface> GetLoop() const noexcept = 0;

    /**
     * @brief 发起异步连接到远端（DNS 解析 + TCP 握手均为异步）。
     * @note 内部 loop 模式下，本函数会自动启动事件循环线程。
     * @return 0 已提交；-1 参数错误。
     */
    virtual int Connect(const std::string& host, int port) = 0;

    /**
     * @brief 主动关闭当前连接（若已连接）。
     */
    virtual void Close() = 0;

    /**
     * @brief 查询当前是否处于已连接状态。
     * @return true 表示连接可发送数据。
     */
    virtual bool IsConnected() const = 0;

    /**
     * @brief 发送数据（线程安全）。
     * @return 0 已排队；-1 未连接。
     */
    virtual int Send(const void* data, std::size_t len) = 0;

    /**
     * @brief Send 的字符串便捷重载。
     * @param str 待发送字符串。
     * @return 0 已排队；-1 未连接。
     */
    int Send(const std::string& str) { return Send(str.data(), str.size()); }

    /**
     * @brief 配置自动重连。
     * @param interval_ms 重连间隔毫秒；<= 0 禁用自动重连。
     * @param max_retries 最大重试次数；-1 表示无限重试。
     */
    virtual void SetReconnect(int interval_ms, int max_retries = -1) = 0;

protected:
    TcpClientInterface() = default;
};

} // namespace itflee

#endif // ITFLEE_TCP_CLIENT_INTERFACE_H_
