// Simple TCP client using libhv C++ API.
// Connects to 127.0.0.1:9000 and sends a few messages.

#include <iostream>
#include <string>
#include <cstdint>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>

#include "hv/TcpClient.h"
#include "hv/Buffer.h"
#include "hv/hloop.h"

// 与服务端一致的心跳协议与参数
static const char HEARTBEAT_PING[] = "PING";
static const char HEARTBEAT_PONG[] = "PONG";
static constexpr int HEARTBEAT_INTERVAL_MS = 3000;
static constexpr int KEEPALIVE_TIMEOUT_MS = 10000;

static bool is_heartbeat_msg(const char* data, size_t size, const char* tag) {
    return size == 4 && std::memcmp(data, tag, 4) == 0;
}

int main(int argc, char** argv) {
    const char* host = "127.0.0.1";
    int port = 9000;
    if (argc > 1) {
        host = argv[1];
    }
    if (argc > 2) {
        port = std::atoi(argv[2]);
    }

    hv::TcpClient client;

    // 统计每 10 秒的发送/接收消息条数（仅统计业务消息，不含心跳）
    std::atomic<std::uint64_t> msgs_sent_10s{0};
    std::atomic<std::uint64_t> msgs_recv_10s{0};

    client.onConnection = [](const hv::TcpClient::TSocketChannelPtr& channel) {
        if (channel && channel->isConnected()) {
            std::cout << "connected to server: fd=" << channel->fd() << std::endl;
            // 客户端定时发 PING；若在 KEEPALIVE_TIMEOUT_MS 内未收到 PONG 则触发断连，由重连逻辑处理
            channel->setHeartbeat(HEARTBEAT_INTERVAL_MS, [channel]() {
                if (channel->isConnected()) {
                    channel->write(HEARTBEAT_PING, 4);
                }
            });
            channel->setKeepaliveTimeout(KEEPALIVE_TIMEOUT_MS);
        } else {
            std::cout << "disconnected from server" << std::endl;
        }
    };

    client.onMessage = [&msgs_recv_10s](const hv::TcpClient::TSocketChannelPtr& channel, hv::Buffer* buf) {
        const char* data = static_cast<const char*>(buf->data());
        size_t size = buf->size();
        // 心跳：服务端发来的 PING 需回复 PONG；收到 PONG 仅刷新 keepalive
        if (is_heartbeat_msg(data, size, HEARTBEAT_PING)) {
            channel->write(HEARTBEAT_PONG, 4);
            return;
        }
        if (is_heartbeat_msg(data, size, HEARTBEAT_PONG)) {
            return;
        }
        std::string msg(data, size);
        // std::cout << "recv from server fd=" << channel->fd()
        //           << ", size=" << size
        //           << ", msg=" << msg
        //           << std::endl;
        // 只按条数统计业务消息
        msgs_recv_10s += 1;
    };

    int connfd = client.createsocket(port, host);
    if (connfd < 0) {
        std::cerr << "create socket failed to " << host << ":" << port << ", ret=" << connfd << std::endl;
        return -1;
    }

    reconn_setting_t reconn;
    reconn.min_delay = 1000;
    reconn.max_delay = 5000;
    reconn.delay_policy = 1; // linear
    reconn.max_retry_cnt = INFINITE;
    client.setReconnect(&reconn);

    client.start(true);

    std::cout << "tcp_client_test running, target " << host << ":" << port << std::endl;

    // 使用 libhv 的事件循环定时器实现精确 10Hz 发送和 10s 统计
    auto loop = client.loop();
    if (loop) {
        // 客户端业务发送（10Hz），并统计发送条数
        std::uint64_t seq = 0;
        const std::string base_msg = "client ping ";
        loop->setInterval(100, [&client, &msgs_sent_10s, &seq, base_msg](hv::TimerID /*timerID*/) {
            if (client.isConnected()) {
                std::string msg = base_msg + std::to_string(++seq);
                int ret = client.send(msg);
                std::cout << "send ret=" << ret << ", msg=" << msg << std::endl;
                if (ret > 0) {
                    // 发送成功按条计数（不按字节）
                    msgs_sent_10s += 1;
                }
            } else {
                std::cout << "not connected, waiting for reconnect..." << std::endl;
            }
        });

        // 每 10 秒输出一次客户端侧的发送/接收统计（以消息条数计）
        loop->setInterval(10000, [&msgs_sent_10s, &msgs_recv_10s](hv::TimerID /*timerID*/) {
            const int interval_sec = 10;
            std::uint64_t sent = msgs_sent_10s.exchange(0);
            std::uint64_t recv = msgs_recv_10s.exchange(0);
            std::cout << "[client stats] last " << interval_sec << "s: sent="
                      << sent << " messages, recv=" << recv << " messages" << std::endl;
        });
    }

    // 主线程保持阻塞
    for (;;) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}

