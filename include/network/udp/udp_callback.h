#ifndef ITFLEE_UDP_CALLBACK_H_
#define ITFLEE_UDP_CALLBACK_H_

#include <cstddef>
#include <cstdint>
#include <string>

namespace itflee {

/**
 * @file udp_callback.h
 * @brief UDP 模块回调定义。
 */

/**
 * @brief UDP 消息回调接口，UDP Server 与 UDP Client 共用。
 * @note 所有回调均在关联的 EventLoop 线程上调用。
 */
class UdpCallback {
public:
    /** @brief 虚析构，允许通过基类指针安全析构派生回调对象。 */
    virtual ~UdpCallback() = default;

    /**
     * @brief 收到 UDP 数据报时回调。
     * @param data      数据指针（仅在回调期间有效，需要保留请自行拷贝）。
     * @param len       数据长度（字节）。
     * @param peer_ip   发送方 IP 地址。
     * @param peer_port 发送方端口。
     */
    virtual void OnMessage(const void* data, std::size_t len,
                           const std::string& peer_ip, uint16_t peer_port) = 0;
};

} // namespace itflee

#endif // ITFLEE_UDP_CALLBACK_H_
