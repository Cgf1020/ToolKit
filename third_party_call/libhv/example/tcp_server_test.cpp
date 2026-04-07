// Simple TCP echo server using libhv C++ API.
// Listens on 127.0.0.1:9000 and echoes received data.

#include <iostream>
#include <string>
#include <cstdint>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "hv/TcpServer.h"
#include "hv/Buffer.h"

// 心跳协议：PING / PONG（各 4 字节），用于保活与检测死连接
static const char HEARTBEAT_PING[] = "PING";
static const char HEARTBEAT_PONG[] = "PONG";
static constexpr int HEARTBEAT_INTERVAL_MS = 3000;  // 心跳间隔
static constexpr int KEEPALIVE_TIMEOUT_MS = 10000;  // 未收到 PONG 则断连

static void parse_ip_port(const std::string& addr, std::string& ip, int& port) {
    ip.clear();
    port = 0;
    if (addr.empty()) return;
    auto pos = addr.rfind(':');
    if (pos == std::string::npos || pos + 1 >= addr.size()) {
        ip = addr;
        return;
    }
    ip = addr.substr(0, pos);
    std::string port_str = addr.substr(pos + 1);
    try {
        port = std::stoi(port_str);
    } catch (...) {
        port = 0;
    }
    // 处理 IPv6 形如 [::1]:9000 的情况，去掉前后的中括号
    if (ip.size() >= 2 && ip.front() == '[' && ip.back() == ']') {
        ip = ip.substr(1, ip.size() - 2);
    }
}

static bool is_heartbeat_msg(const char* data, size_t size, const char* tag) {
    return size == 4 && std::memcmp(data, tag, 4) == 0;
}

int main(int argc, char** argv) {
    int port = 9000;
    if (argc > 1) {
        port = std::atoi(argv[1]);
    }

    hv::TcpServer server;

    std::mutex channels_mutex;
    std::unordered_map<int, hv::TcpServer::TSocketChannelPtr> channels;

    // 统计每 10 秒的发送/接收消息条数（仅统计业务消息，不含心跳）
    std::atomic<std::uint64_t> msgs_sent_10s{0};
    std::atomic<std::uint64_t> msgs_recv_10s{0};

    server.onConnection = [&](const hv::TcpServer::TSocketChannelPtr& channel) {
        if (channel->isConnected()) {
            std::string peer = channel->peeraddr();
            std::string ip;
            int port = 0;
            parse_ip_port(peer, ip, port);
            std::cout << "client connected: fd=" << channel->fd()
                      << ", ip=" << ip
                      << ", port=" << port
                      << std::endl;
            // 服务端定时发 PING，若在 KEEPALIVE_TIMEOUT_MS 内未收到 PONG 则关闭连接
            channel->setHeartbeat(HEARTBEAT_INTERVAL_MS, [channel]() {
                if (channel->isConnected()) {
                    channel->write(HEARTBEAT_PING, 4);
                }
            });
            channel->setKeepaliveTimeout(KEEPALIVE_TIMEOUT_MS);
            std::lock_guard<std::mutex> lock(channels_mutex);
            channels[channel->fd()] = channel;
        } else {
            std::cout << "client disconnected: fd=" << channel->fd() << std::endl;
            std::lock_guard<std::mutex> lock(channels_mutex);
            channels.erase(channel->fd());
        }
    };

    server.onMessage = [&msgs_recv_10s](const hv::TcpServer::TSocketChannelPtr& channel, hv::Buffer* buf) {
        const char* data = static_cast<const char*>(buf->data());
        size_t size = buf->size();
        // 心跳：客户端回复 PONG 或主动发 PING
        if (is_heartbeat_msg(data, size, HEARTBEAT_PONG)) {
            return;  // 收到 PONG，仅刷新 keepalive，不打印
        }
        if (is_heartbeat_msg(data, size, HEARTBEAT_PING)) {
            channel->write(HEARTBEAT_PONG, 4);
            return;
        }
        std::string msg(data, size);
        // std::cout << "recv from fd=" << channel->fd()
        //           << ", size=" << size
        //           << ", msg=" << msg
        //           << std::endl;
        // 只按条数统计业务消息
        msgs_recv_10s += 1;
    };

    int listenfd = server.createsocket(port, "0.0.0.0");
    if (listenfd < 0) {
        std::cerr << "create socket failed on port " << port << ", ret=" << listenfd << std::endl;
        return -1;
    }

    server.setThreadNum(2); // 1 acceptor + 1 worker
    server.start(true);

    // 使用 libhv 的事件循环定时器实现精确 10Hz 发送和 10s 统计
    auto loop = server.loop();
    if (loop) {
        // 10Hz 定时向所有客户端发送业务消息，并统计发送条数
        loop->setInterval(100, [&channels_mutex, &channels, &msgs_sent_10s](hv::TimerID /*timerID*/) {
            static std::uint64_t seq = 0;

            std::vector<hv::TcpServer::TSocketChannelPtr> snapshot;
            {
                std::lock_guard<std::mutex> lock(channels_mutex);
                snapshot.reserve(channels.size());
                for (const auto& kv : channels) {
                    snapshot.push_back(kv.second);
                }
            }

            if (snapshot.empty()) {
                return;
            }

            std::string msg = "server push " + std::to_string(++seq);
            for (const auto& ch : snapshot) {
                if (!ch || !ch->isConnected()) {
                    continue;
                }
                int ret = ch->write(msg.c_str(), (int)msg.size());
                if (ret > 0) {
                    // 发送成功按条计数（不按字节）
                    msgs_sent_10s += 1;
                }
            }
        });

        // 每 10 秒输出一次服务器侧的发送/接收统计（以消息条数计）
        loop->setInterval(10000, [&msgs_sent_10s, &msgs_recv_10s](hv::TimerID /*timerID*/) {
            const int interval_sec = 10;
            std::uint64_t sent = msgs_sent_10s.exchange(0);
            std::uint64_t recv = msgs_recv_10s.exchange(0);
            std::cout << "[server stats] last " << interval_sec << "s: sent="
                      << sent << " messages, recv=" << recv << " messages" << std::endl;
        });
    }

    std::cout << "tcp_server_test listening on 0.0.0.0:" << port << std::endl;
    std::cout << "press Ctrl+C to exit." << std::endl;

    // Run forever; TcpServer manages its own EventLoopThread.
    // Just block main thread.
    for (;;) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}

