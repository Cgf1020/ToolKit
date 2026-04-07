#ifndef ITFLEE_TCP_CONNECTION_IMPL_H_
#define ITFLEE_TCP_CONNECTION_IMPL_H_

#include "network/tcp/tcp_connection.h"
#include "network/tcp/tcp_callback.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/post.hpp>

namespace itflee {

class TcpConnectionImpl : public TcpConnection {
public:
    using StateChangedHandler = std::function<void(const TcpConnectionPtr&, ConnectionState)>;
    using MessageHandler      = std::function<void(const TcpConnectionPtr&, const void*, std::size_t)>;
    using WriteCompleteHandler = std::function<void(const TcpConnectionPtr&)>;

    TcpConnectionImpl(boost::asio::ip::tcp::socket socket, uint32_t id);
    ~TcpConnectionImpl() override;

    State GetState() const noexcept override;
    int Write(const void* data, std::size_t len) override;
    void Close() override;
    // 与 Close 等价，但会在关闭逻辑于 io 线程执行完毕后调用 on_done（用于上层 Close 前同步）
    void CloseWhenDone(std::function<void()> on_done);
    int EnableHeartbeat(int interval_ms, int timeout_ms, std::string heartbeat_payload) override;
    void DisableHeartbeat() override;
    std::string LocalAddr() const override;
    std::string PeerAddr() const override;
    uint32_t Id() const noexcept override;

    void StartRead();
    void SetState(State s) noexcept { state_.store(s, std::memory_order_release); }

    StateChangedHandler   on_state_changed;
    MessageHandler        on_message;
    WriteCompleteHandler  on_write_complete;

private:
    void DoCloseInLoop();
    void DoRead();
    void DoWrite();
    void HandleError(ConnectionState reason);
    void ScheduleHeartbeatTick();
    void OnHeartbeatTick(const boost::system::error_code& ec);
    void DisableHeartbeatInLoop() noexcept;

    boost::asio::ip::tcp::socket socket_;
    boost::asio::steady_timer heartbeat_timer_;
    uint32_t id_;
    std::atomic<State> state_{kConnecting};
    bool heartbeat_enabled_{false};
    std::chrono::milliseconds heartbeat_interval_{0};
    std::chrono::milliseconds heartbeat_timeout_{0};
    std::string heartbeat_payload_;
    std::chrono::steady_clock::time_point last_recv_tp_{};

    static constexpr std::size_t kReadBufferSize = 8192;
    std::array<char, kReadBufferSize> read_buffer_{};

    std::deque<std::vector<char>> write_queue_;
    bool writing_{false};
};

} // namespace itflee

#endif // ITFLEE_TCP_CONNECTION_IMPL_H_
