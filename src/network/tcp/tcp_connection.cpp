#include "tcp_connection_impl.h"

#include <sstream>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>

namespace itflee {

TcpConnectionImpl::TcpConnectionImpl(boost::asio::ip::tcp::socket socket, uint32_t id)
    : socket_(std::move(socket))
    , heartbeat_timer_(socket_.get_executor())
    , id_(id) {}

TcpConnectionImpl::~TcpConnectionImpl() {
    boost::system::error_code ec;
    if (socket_.is_open()) {
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        socket_.close(ec);
    }
}

TcpConnection::State TcpConnectionImpl::GetState() const noexcept {
    return state_.load(std::memory_order_acquire);
}

uint32_t TcpConnectionImpl::Id() const noexcept {
    return id_;
}

std::string TcpConnectionImpl::LocalAddr() const {
    boost::system::error_code ec;
    auto ep = socket_.local_endpoint(ec);
    if (ec) return {};
    std::ostringstream oss;
    oss << ep.address().to_string() << ":" << ep.port();
    return oss.str();
}

std::string TcpConnectionImpl::PeerAddr() const {
    boost::system::error_code ec;
    auto ep = socket_.remote_endpoint(ec);
    if (ec) return {};
    std::ostringstream oss;
    oss << ep.address().to_string() << ":" << ep.port();
    return oss.str();
}

void TcpConnectionImpl::StartRead() {
    last_recv_tp_ = std::chrono::steady_clock::now();
    DoRead();
}

void TcpConnectionImpl::DoRead() {
    auto self = shared_from_this();
    socket_.async_read_some(
        boost::asio::buffer(read_buffer_),
        [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
            if (!ec && bytes_transferred > 0) {
                last_recv_tp_ = std::chrono::steady_clock::now();
                if (on_message) {
                    on_message(self, read_buffer_.data(), bytes_transferred);
                }
                DoRead();
            } else {
                HandleError(ConnectionState::kDisconnected);
            }
        });
}

int TcpConnectionImpl::Write(const void* data, std::size_t len) {
    if (state_.load(std::memory_order_acquire) != kConnected || !data || len == 0) {
        return -1;
    }

    auto buf = std::make_shared<std::vector<char>>(
        static_cast<const char*>(data),
        static_cast<const char*>(data) + len);

    auto self = shared_from_this();
    boost::asio::post(socket_.get_executor(), [this, self, buf]() {
        bool was_empty = write_queue_.empty();
        write_queue_.push_back(std::move(*buf));
        if (!writing_ && was_empty) {
            DoWrite();
        }
    });
    return 0;
}

void TcpConnectionImpl::DoWrite() {
    if (write_queue_.empty()) {
        writing_ = false;
        if (on_write_complete) {
            on_write_complete(shared_from_this());
        }
        return;
    }
    writing_ = true;
    auto self = shared_from_this();
    boost::asio::async_write(
        socket_,
        boost::asio::buffer(write_queue_.front()),
        [this, self](boost::system::error_code ec, std::size_t /*bytes*/) {
            if (!ec) {
                write_queue_.pop_front();
                DoWrite();
            } else {
                HandleError(ConnectionState::kDisconnected);
            }
        });
}

void TcpConnectionImpl::DoCloseInLoop() {
    DisableHeartbeatInLoop();
    boost::system::error_code ec;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    socket_.close(ec);

    state_.store(kDisconnected, std::memory_order_release);
    if (on_state_changed) {
        on_state_changed(shared_from_this(), ConnectionState::kDisconnected);
    }
}

void TcpConnectionImpl::CloseWhenDone(std::function<void()> on_done) {
    auto cur = state_.load(std::memory_order_acquire);
    if (cur == kDisconnected || cur == kDisconnecting) {
        if (on_done) {
            boost::asio::dispatch(socket_.get_executor(),
                                  [cb = std::move(on_done)]() mutable { cb(); });
        }
        return;
    }
    state_.store(kDisconnecting, std::memory_order_release);

    auto self = shared_from_this();
    boost::asio::dispatch(socket_.get_executor(), [this, self, cb = std::move(on_done)]() mutable {
        DoCloseInLoop();
        if (cb) cb();
    });
}

void TcpConnectionImpl::Close() {
    CloseWhenDone(nullptr);
}

int TcpConnectionImpl::EnableHeartbeat(int interval_ms, int timeout_ms, std::string heartbeat_payload) {
    if (interval_ms <= 0 || timeout_ms <= 0) {
        return -1;
    }
    if (state_.load(std::memory_order_acquire) == kDisconnected) {
        return -1;
    }
    auto self = shared_from_this();
    boost::asio::post(socket_.get_executor(), [this, self,
                                               interval = std::chrono::milliseconds(interval_ms),
                                               timeout = std::chrono::milliseconds(timeout_ms),
                                               payload = std::move(heartbeat_payload)]() mutable {
        heartbeat_interval_ = interval;
        heartbeat_timeout_ = timeout;
        heartbeat_payload_ = std::move(payload);
        heartbeat_enabled_ = true;
        if (last_recv_tp_ == std::chrono::steady_clock::time_point{}) {
            last_recv_tp_ = std::chrono::steady_clock::now();
        }
        ScheduleHeartbeatTick();
    });
    return 0;
}

void TcpConnectionImpl::DisableHeartbeat() {
    auto self = shared_from_this();
    boost::asio::post(socket_.get_executor(), [this, self]() {
        DisableHeartbeatInLoop();
    });
}

void TcpConnectionImpl::ScheduleHeartbeatTick() {
    if (!heartbeat_enabled_) {
        return;
    }
    heartbeat_timer_.expires_after(heartbeat_interval_);
    auto self = shared_from_this();
    heartbeat_timer_.async_wait([this, self](const boost::system::error_code& ec) {
        OnHeartbeatTick(ec);
    });
}

void TcpConnectionImpl::OnHeartbeatTick(const boost::system::error_code& ec) {
    if (ec || !heartbeat_enabled_) {
        return;
    }
    if (state_.load(std::memory_order_acquire) != kConnected) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now - last_recv_tp_ >= heartbeat_timeout_) {
        HandleError(ConnectionState::kDisconnected);
        return;
    }
    if (!heartbeat_payload_.empty()) {
        Write(heartbeat_payload_.data(), heartbeat_payload_.size());
    }
    ScheduleHeartbeatTick();
}

void TcpConnectionImpl::DisableHeartbeatInLoop() noexcept {
    heartbeat_enabled_ = false;
    boost::system::error_code ec;
    heartbeat_timer_.cancel(ec);
}

void TcpConnectionImpl::HandleError(ConnectionState reason) {
    auto expected = kConnected;
    if (!state_.compare_exchange_strong(expected, kDisconnected, std::memory_order_acq_rel)) {
        return;
    }

    boost::system::error_code ec;
    DisableHeartbeatInLoop();
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    socket_.close(ec);

    if (on_state_changed) {
        on_state_changed(shared_from_this(), reason);
    }
}

} // namespace itflee
