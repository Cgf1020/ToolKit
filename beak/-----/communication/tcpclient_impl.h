#ifndef TCPCLIENT_IMPL_H
#define TCPCLIENT_IMPL_H

#include <string>
#include <cstring>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <condition_variable>
#include "communication/communication.h"

// 前向声明，避免在头文件中引入平台相关的网络头
struct sockaddr_in;

namespace itflee {  

    class TcpClientImpl: public TcpClient
    {
    public:
        TcpClientImpl();
        TcpClientImpl(const TcpClientImpl&) = delete;
        TcpClientImpl& operator=(const TcpClientImpl&) = delete;
        ~TcpClientImpl();
    public:
        bool Connect(const std::string& host,
                     int port,
                     std::shared_ptr<TcpClientObserver> observer,
                     bool auto_reconnect = true) override;

        bool DisConnect() override;
        
        // 停止自动重连
        void StopAutoReconnect() override;

        // 发送数据
        bool Send(const std::string& data) override;
        bool Send(const char* data, size_t length) override;

        bool IsConnected() const noexcept override;
    private:
        void receiveDataFunc();
        void connectThreadFunc();  // 连接线程函数（包含重连逻辑）
        std::optional<std::string> doConnect();  // 执行实际的连接操作
        void uninit();  // 统一资源清理函数

    private:
        // 状态机：使用原子状态枚举替代多个 bool 标志
        enum class State { Idle, Connecting, Connected, Disconnecting };
        std::atomic<State> state_{ State::Idle };
        std::atomic<bool> should_stop_connect_{ false };  // 控制线程退出
        std::atomic<bool> should_stop_receive_{ false };  // 控制接收线程退出

        // 跨平台统一使用 int 保存 socket 句柄（Windows 下为 SOCKET 的整数值）
        int m_socket;
        struct sockaddr_in* m_serverAddr;  // 改为栈对象，使用 RAII

        std::unique_ptr<std::thread> receiveThread_;
        std::unique_ptr<std::thread> connectThread_;  // 连接线程（包含重连逻辑）

        std::weak_ptr<TcpClientObserver> observer_;
        
        // Reconnection related
        std::atomic<bool> auto_reconnect_{false};
        std::string connect_host_;
        std::string connect_ip_;  // Connected IP address
        int connect_port_{0};
        std::atomic<int> reconnect_attempts_{0};
        int max_reconnect_attempts_{-1};
        int reconnect_delay_{5};
        
        // Condition variable for interruptible reconnect delay
        std::mutex reconnect_mutex_;
        std::condition_variable reconnect_cv_;
    };

} // namespace itflee

#endif // TCPCLIENT_IMPL_H

