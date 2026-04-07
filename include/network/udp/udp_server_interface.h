#ifndef ITFLEE_UDP_SERVER_INTERFACE_H_
#define ITFLEE_UDP_SERVER_INTERFACE_H_

#include "define.h"
#include "network/udp/udp_callback.h"
#include "base/eventloop/event_loop_interface.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace itflee {

/**
 * @file udp_server_interface.h
 * @brief UDP 服务端抽象接口定义。
 */

/**
 * @brief UDP 服务器抽象接口。
 * @note 设计参考 hv::UdpServer，基于 EventLoopInterface 驱动异步 I/O。
 * @note Bind() → Start() → 收到数据通过 UdpCallback::OnMessage 回调。
 */
class ITFLEEEXPORT UdpServerInterface {
public:
    /** @brief 虚析构，允许通过基类指针安全释放实现对象。 */
    virtual ~UdpServerInterface() = default;

    /**
     * @brief 工厂方法。
     * @param callback UDP 消息回调。
     * @param loop     事件循环；为空则内部创建 EventLoopThread。
     */
    static std::shared_ptr<UdpServerInterface> Create(
        std::shared_ptr<UdpCallback> callback,
        std::shared_ptr<EventLoopInterface> loop = nullptr);

    /**
     * @brief 绑定本地端口（同步操作）。
     * @return 0 成功；负值失败。
     */
    virtual int Bind(int port, const std::string& host = "0.0.0.0") = 0;

    /** @brief 启动收包流程。 */
    virtual void Start() = 0;
    /** @brief 停止收发并释放底层资源。 */
    virtual void Stop() = 0;

    /**
     * @brief 获取绑定的事件循环实例。
     * @return Create 时传入的 loop，或内部创建并持有的 loop。
     */
    virtual std::shared_ptr<EventLoopInterface> GetLoop() const noexcept = 0;

    /**
     * @brief 向指定地址发送 UDP 数据报。
     * @return 0 已提交；负值失败。
     */
    virtual int SendTo(const void* data, std::size_t len,
                       const std::string& host, uint16_t port) = 0;

    /**
     * @brief SendTo 的字符串便捷重载。
     * @param str  待发送字符串。
     * @param host 目标主机地址。
     * @param port 目标端口。
     * @return 0 已提交；负值失败。
     */
    int SendTo(const std::string& str, const std::string& host, uint16_t port) {
        return SendTo(str.data(), str.size(), host, port);
    }

protected:
    UdpServerInterface() = default;
};

} // namespace itflee

#endif // ITFLEE_UDP_SERVER_INTERFACE_H_
