#include "network/tcp/tcp_client_interface.h"
#include "base/eventloop/event_loop_thread.h"
#include "tcp_connection_impl.h"

#include <future>
#include <memory>
#include <string>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/version.hpp>

namespace itflee {

namespace {
using tcp = boost::asio::ip::tcp;

// 服务器不可达时，系统 TCP 连接可能长时间挂起；限制单次 connect 等待时间以便重连循环能继续。
constexpr int kConnectTimeoutMs = 5000;
} // namespace

class TcpClientBoost final
    : public TcpClientInterface
    , public std::enable_shared_from_this<TcpClientBoost> {
public:
    TcpClientBoost(std::shared_ptr<TcpConnectionCallback> callback,
                   std::shared_ptr<EventLoopInterface> loop)
        : callback_(std::move(callback)) {
        if (loop) {
            loop_ = std::move(loop);
        } else {
            loop_ = EventLoopInterface::Create();
            loop_thread_ = std::make_unique<EventLoopThread>(loop_);
        }

        auto executor = std::any_cast<boost::asio::io_context::executor_type>(
            loop_->GetNativeExecutor());
        resolver_ = std::make_unique<tcp::resolver>(executor);
        reconnect_timer_ = std::make_unique<boost::asio::steady_timer>(executor);
    }

    ~TcpClientBoost() override {
        Close();
    }

    std::shared_ptr<EventLoopInterface> GetLoop() const noexcept override {
        return loop_;
    }

    int Connect(const std::string& host, int port) override {
        if (host.empty() || port <= 0 || port > 65535) return -1;
        host_ = host;
        port_ = port;
        reconnect_retry_count_ = 0;

        if (loop_thread_) {
            loop_thread_->start();
        }

        auto self = shared_from_this();
        loop_->Post([self]() { self->DoResolve(); });
        return 0;
    }

    void Close() override {
        auto_reconnect_ = false;
        reconnect_retry_count_ = 0;

        if (reconnect_timer_) {
#if BOOST_VERSION >= 108900
            reconnect_timer_->cancel();
#else
            boost::system::error_code ec;
            reconnect_timer_->cancel(ec);
#endif
        }

        if (conn_) {
            if (loop_ && loop_->IsLoopThread()) {
                conn_->CloseWhenDone(nullptr);
                conn_.reset();
            } else {
                std::promise<void> closed;
                auto fut = closed.get_future();
                conn_->CloseWhenDone([&closed]() { closed.set_value(); });
                fut.wait();
                conn_.reset();
            }
        }

        if (loop_thread_) {
            if (loop_ && loop_->IsLoopThread()) {
                loop_thread_->stop(false);
            } else {
                loop_thread_->stop(true);
            }
        }
    }

    bool IsConnected() const override {
        return conn_ && conn_->IsConnected();
    }

    int Send(const void* data, std::size_t len) override {
        if (!conn_ || !conn_->IsConnected()) return -1;
        return conn_->Write(data, len);
    }

    void SetReconnect(int interval_ms, int max_retries) override {
        if (interval_ms <= 0) {
            auto_reconnect_ = false;
            reconnect_interval_ms_ = 0;
            reconnect_max_retries_ = 0;
            return;
        }
        auto_reconnect_ = true;
        reconnect_interval_ms_ = interval_ms;
        reconnect_max_retries_ = max_retries;
    }

private:
    void DoResolve() {
        if (auto cb = callback_.lock()) {
            cb->OnConnectionStateChanged(nullptr, ConnectionState::kConnecting);
        }

        auto self = shared_from_this();
        resolver_->async_resolve(
            host_, std::to_string(port_),
            [self](boost::system::error_code ec,
                   tcp::resolver::results_type results) {
                if (!ec) {
                    self->DoConnect(results);
                } else {
                    self->OnConnectFailed();
                }
            });
    }

    void DoConnect(const tcp::resolver::results_type& endpoints) {
        auto executor = std::any_cast<boost::asio::io_context::executor_type>(
            loop_->GetNativeExecutor());
        auto socket = std::make_shared<tcp::socket>(executor);
        auto deadline = std::make_shared<boost::asio::steady_timer>(executor);

        auto self = shared_from_this();
        deadline->expires_after(std::chrono::milliseconds(kConnectTimeoutMs));
        deadline->async_wait([socket](boost::system::error_code ec) {
            if (ec) {
                return;
            }
            boost::system::error_code close_ec;
            socket->close(close_ec);
        });

        boost::asio::async_connect(
            *socket, endpoints,
            [self, socket, deadline](boost::system::error_code ec,
                                     const tcp::endpoint& /*ep*/) {
#if BOOST_VERSION >= 108900
                deadline->cancel();
#else
                boost::system::error_code cancel_ec;
                deadline->cancel(cancel_ec);
#endif
                if (!ec) {
                    self->OnConnected(std::move(*socket));
                } else {
                    self->OnConnectFailed();
                }
            });
    }

    void OnConnected(tcp::socket socket) {
        static std::atomic<uint32_t> s_conn_id{1};
        uint32_t id = s_conn_id.fetch_add(1, std::memory_order_relaxed);

        conn_ = std::make_shared<TcpConnectionImpl>(std::move(socket), id);
        reconnect_retry_count_ = 0;

        auto weak_self = std::weak_ptr<TcpClientBoost>(shared_from_this());

        conn_->on_state_changed = [weak_self](const TcpConnectionPtr& c, ConnectionState state) {
            if (auto self = weak_self.lock()) {
                if (auto cb = self->callback_.lock()) {
                    cb->OnConnectionStateChanged(c, state);
                }
                if (state == ConnectionState::kDisconnected) {
                    self->conn_.reset();
                    self->TryReconnect();
                }
            }
        };

        conn_->on_message = [weak_self](const TcpConnectionPtr& c, const void* data, std::size_t len) {
            if (auto self = weak_self.lock()) {
                if (auto cb = self->callback_.lock()) {
                    cb->OnMessage(c, data, len);
                }
            }
        };

        conn_->on_write_complete = [weak_self](const TcpConnectionPtr& c) {
            if (auto self = weak_self.lock()) {
                if (auto cb = self->callback_.lock()) {
                    cb->OnWriteComplete(c);
                }
            }
        };

        conn_->SetState(TcpConnection::kConnected);
        conn_->StartRead();

        if (auto cb = callback_.lock()) {
            cb->OnConnectionStateChanged(conn_, ConnectionState::kConnected);
        }
    }

    void OnConnectFailed() {
        if (auto cb = callback_.lock()) {
            cb->OnConnectionStateChanged(nullptr, ConnectionState::kConnectFailed);
        }
        TryReconnect();
    }

    void TryReconnect() {
        if (!auto_reconnect_) return;
        if (reconnect_max_retries_ >= 0 &&
            reconnect_retry_count_ >= reconnect_max_retries_) {
            return;
        }
        ++reconnect_retry_count_;

        auto self = shared_from_this();
        reconnect_timer_->expires_after(
            std::chrono::milliseconds(reconnect_interval_ms_));
        reconnect_timer_->async_wait(
            [self](boost::system::error_code ec) {
                if (!ec) {
                    self->DoResolve();
                }
            });
    }

    std::shared_ptr<EventLoopInterface> loop_;
    std::unique_ptr<EventLoopThread> loop_thread_;
    std::weak_ptr<TcpConnectionCallback> callback_;

    std::unique_ptr<tcp::resolver> resolver_;
    std::shared_ptr<TcpConnectionImpl> conn_;
    std::unique_ptr<boost::asio::steady_timer> reconnect_timer_;

    std::string host_;
    int port_{0};

    bool auto_reconnect_{false};
    int reconnect_interval_ms_{0};
    int reconnect_max_retries_{-1};
    int reconnect_retry_count_{0};
};

std::shared_ptr<TcpClientInterface> TcpClientInterface::Create(
    std::shared_ptr<TcpConnectionCallback> callback,
    std::shared_ptr<EventLoopInterface> loop) {
    return std::make_shared<TcpClientBoost>(std::move(callback), std::move(loop));
}

} // namespace itflee
