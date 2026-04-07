#include "network/tcp/tcp_server_interface.h"
#include "base/eventloop/event_loop_thread.h"
#include "tcp_connection_impl.h"

#include <atomic>
#include <chrono>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>

#if defined(SO_REUSEPORT) || defined(__linux__) || defined(__APPLE__)
#include <sys/socket.h>
#endif

namespace itflee {

namespace {
using tcp = boost::asio::ip::tcp;
}

class TcpServerBoost final
    : public TcpServerInterface
    , public std::enable_shared_from_this<TcpServerBoost> {

    struct SubReactor {
        std::shared_ptr<EventLoopInterface> loop;
        std::unique_ptr<EventLoopThread> thread;
        std::map<uint32_t, std::shared_ptr<TcpConnectionImpl>> connections;
        mutable std::mutex mutex;
    };

public:
    TcpServerBoost(std::shared_ptr<TcpConnectionCallback> callback,
                   std::shared_ptr<EventLoopInterface> loop,
                   const TcpServerOptions& options)
        : callback_(std::move(callback))
        , options_(options) {
        if (loop) {
            main_loop_ = std::move(loop);
        } else {
            main_loop_ = EventLoopInterface::Create();
            main_thread_ = std::make_unique<EventLoopThread>(main_loop_);
        }

        const size_t n = options_.GetEffectiveWorkerCount();
        sub_reactors_.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            auto sub = std::make_unique<SubReactor>();
            sub->loop = EventLoopInterface::Create();
            sub->thread = std::make_unique<EventLoopThread>(sub->loop);
            sub_reactors_.push_back(std::move(sub));
        }
    }

    ~TcpServerBoost() override {
        (void)Shutdown(TcpShutdownMode::kForce, std::chrono::milliseconds(0));
    }

    int Listen(int port, const std::string& host) override {
        RuntimeState expected_init = RuntimeState::kInitialized;
        if (!state_.compare_exchange_strong(expected_init, RuntimeState::kRunning)) {
            return -10;
        }
        accepting_.store(true, std::memory_order_release);

        auto executor = std::any_cast<boost::asio::io_context::executor_type>(
            main_loop_->GetNativeExecutor());

        boost::system::error_code ec;
        auto addr = boost::asio::ip::make_address(host, ec);
        if (ec) return -1;

        tcp::endpoint endpoint(addr, static_cast<uint16_t>(port));
        acceptor_ = std::make_unique<tcp::acceptor>(executor);

        acceptor_->open(endpoint.protocol(), ec);
        if (ec) {
            state_.store(RuntimeState::kInitialized, std::memory_order_release);
            return -2;
        }

        if (options_.reuse_addr) {
            acceptor_->set_option(boost::asio::socket_base::reuse_address(true), ec);
        }
#if defined(SO_REUSEPORT)
        if (options_.reuse_port) {
            const int one = 1;
            ::setsockopt(
                acceptor_->native_handle(),
                SOL_SOCKET,
                SO_REUSEPORT,
                reinterpret_cast<const char*>(&one),
                static_cast<socklen_t>(sizeof(one)));
        }
#endif

        acceptor_->bind(endpoint, ec);
        if (ec) {
            state_.store(RuntimeState::kInitialized, std::memory_order_release);
            return -3;
        }

        const int backlog = options_.backlog > 0
            ? options_.backlog
            : boost::asio::socket_base::max_listen_connections;
        acceptor_->listen(backlog, ec);
        if (ec) {
            state_.store(RuntimeState::kInitialized, std::memory_order_release);
            return -4;
        }

        for (auto& sub : sub_reactors_) {
            sub->thread->start();
        }

        if (main_thread_) {
            main_thread_->start();
        }

        auto self = shared_from_this();
        main_loop_->Post([self]() { self->DoAccept(); });
        return 0;
    }

    std::shared_ptr<EventLoopInterface> GetLoop() const noexcept override {
        return main_loop_;
    }

    std::size_t ConnectionCount() const override { return GetStats().total_connections; }

    int Broadcast(const void* data, std::size_t len) override {
        if (data == nullptr || len == 0) {
            return 0;
        }
        auto payload = std::make_shared<std::vector<char>>(
            static_cast<const char*>(data),
            static_cast<const char*>(data) + len);
        int total = 0;
        for (auto& sub : sub_reactors_) {
            std::vector<std::shared_ptr<TcpConnectionImpl>> conns;
            {
                std::lock_guard<std::mutex> lock(sub->mutex);
                conns.reserve(sub->connections.size());
                for (auto& kv : sub->connections) {
                    conns.push_back(kv.second);
                }
            }
            total += static_cast<int>(conns.size());
            if (conns.empty()) {
                continue;
            }
            sub->loop->Post([conns = std::move(conns), payload]() {
                for (auto& conn : conns) {
                    (void)conn->Write(payload->data(), payload->size());
                }
            });
        }
        return total;
    }

    int Shutdown(TcpShutdownMode mode, std::chrono::milliseconds timeout) override {
        for (;;) {
            auto st = state_.load(std::memory_order_acquire);
            if (st == RuntimeState::kStopped) {
                return 0;
            }
            if (st == RuntimeState::kStopping) {
                return -1;
            }
            if (st == RuntimeState::kInitialized) {
                state_.store(RuntimeState::kStopped, std::memory_order_release);
                return 0;
            }
            if (state_.compare_exchange_strong(st, RuntimeState::kStopping)) {
                break;
            }
        }

        accepting_.store(false, std::memory_order_release);

        auto close_acceptor = [this]() {
            if (!acceptor_) return;
            boost::system::error_code ec;
            if (acceptor_->is_open()) {
                acceptor_->close(ec);
            }
        };
        if (main_loop_ && main_loop_->IsLoopThread()) {
            close_acceptor();
        } else if (main_loop_) {
            main_loop_->Post(close_acceptor);
        } else {
            close_acceptor();
        }

        if (mode == TcpShutdownMode::kGraceful) {
            const auto wait_timeout = timeout.count() > 0 ? timeout : options_.graceful_shutdown_timeout;
            const auto deadline = std::chrono::steady_clock::now() + wait_timeout;
            while (ConnectionCount() > 0 && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }

        if (acceptor_) {
            boost::system::error_code ec;
            if (acceptor_->is_open()) {
                acceptor_->close(ec);
            }
        }

        {
            auto barrier = std::make_shared<std::atomic<size_t>>(sub_reactors_.size());
            auto done_promise = std::make_shared<std::promise<void>>();
            auto done_future = done_promise->get_future();

            for (auto& sub : sub_reactors_) {
                std::vector<std::shared_ptr<TcpConnectionImpl>> conns;
                {
                    std::lock_guard<std::mutex> lock(sub->mutex);
                    conns.reserve(sub->connections.size());
                    for (auto& kv : sub->connections) {
                        conns.push_back(kv.second);
                    }
                    sub->connections.clear();
                }
                sub->loop->Post([conns = std::move(conns), barrier, done_promise]() {
                    for (auto& conn : conns) {
                        conn->Close();
                    }
                    if (barrier->fetch_sub(1, std::memory_order_acq_rel) == 1) {
                        done_promise->set_value();
                    }
                });
            }

            done_future.wait();
        }

        for (auto& sub : sub_reactors_) {
            sub->thread->stop(true);
        }

        if (main_thread_) {
            main_thread_->stop(true);
        }
        state_.store(RuntimeState::kStopped, std::memory_order_release);
        return 0;
    }

    TcpServerStats GetStats() const override {
        TcpServerStats out;
        out.per_sub.reserve(sub_reactors_.size());
        for (size_t i = 0; i < sub_reactors_.size(); ++i) {
            const auto& sub = sub_reactors_[i];
            TcpServerSubStats item;
            item.sub_index = i;
            {
                std::lock_guard<std::mutex> lock(sub->mutex);
                item.connection_count = sub->connections.size();
            }
            out.total_connections += item.connection_count;
            out.per_sub.push_back(item);
        }
        return out;
    }

private:
    enum class RuntimeState {
        kInitialized,
        kRunning,
        kStopping,
        kStopped
    };

    size_t PickSubReactorIndex() {
        if (options_.load_balance_policy == TcpLoadBalancePolicy::kLeastConn) {
            size_t best_idx = 0;
            size_t best_conn = static_cast<size_t>(-1);
            for (size_t i = 0; i < sub_reactors_.size(); ++i) {
                std::lock_guard<std::mutex> lock(sub_reactors_[i]->mutex);
                const size_t n = sub_reactors_[i]->connections.size();
                if (n < best_conn) {
                    best_conn = n;
                    best_idx = i;
                }
            }
            return best_idx;
        }
        return next_sub_.fetch_add(1, std::memory_order_relaxed) % sub_reactors_.size();
    }

    void DispatchStateChangedCallback(size_t sub_idx,
                                      const TcpConnectionPtr& c,
                                      ConnectionState state) {
        auto cb = callback_.lock();
        if (!cb) {
            return;
        }
        if (options_.cb_dispatch_mode == TcpCallbackDispatchMode::kSubThread) {
            cb->OnConnectionStateChanged(c, state);
            return;
        }
        if (main_loop_) {
            main_loop_->Post([cb, c, state]() { cb->OnConnectionStateChanged(c, state); });
            return;
        }
        (void)sub_idx;
        cb->OnConnectionStateChanged(c, state);
    }

    void DispatchMessageCallback(size_t sub_idx,
                                 const TcpConnectionPtr& c,
                                 const void* data,
                                 std::size_t len) {
        auto cb = callback_.lock();
        if (!cb) {
            return;
        }
        if (options_.cb_dispatch_mode == TcpCallbackDispatchMode::kSubThread) {
            cb->OnMessage(c, data, len);
            return;
        }
        auto payload = std::make_shared<std::vector<char>>(
            static_cast<const char*>(data),
            static_cast<const char*>(data) + len);
        if (main_loop_) {
            main_loop_->Post([cb, c, payload]() { cb->OnMessage(c, payload->data(), payload->size()); });
            return;
        }
        (void)sub_idx;
        cb->OnMessage(c, payload->data(), payload->size());
    }

    void DispatchWriteCompleteCallback(size_t sub_idx, const TcpConnectionPtr& c) {
        auto cb = callback_.lock();
        if (!cb) {
            return;
        }
        if (options_.cb_dispatch_mode == TcpCallbackDispatchMode::kSubThread) {
            cb->OnWriteComplete(c);
            return;
        }
        if (main_loop_) {
            main_loop_->Post([cb, c]() { cb->OnWriteComplete(c); });
            return;
        }
        (void)sub_idx;
        cb->OnWriteComplete(c);
    }

    void DoAccept() {
        if (!acceptor_ || !acceptor_->is_open()) return;
        if (!accepting_.load(std::memory_order_acquire)) return;

        const size_t sub_idx = PickSubReactorIndex();
        auto& sub = *sub_reactors_[sub_idx];

        auto sub_executor = std::any_cast<boost::asio::io_context::executor_type>(
            sub.loop->GetNativeExecutor());
        auto peer = std::make_shared<tcp::socket>(sub_executor);

        auto self = shared_from_this();
        acceptor_->async_accept(
            *peer,
            [self, peer, sub_idx](boost::system::error_code ec) {
                if (!ec) {
                    self->OnNewConnection(std::move(*peer), sub_idx);
                }
                if (self->acceptor_ && self->acceptor_->is_open() &&
                    self->accepting_.load(std::memory_order_acquire)) {
                    self->DoAccept();
                }
            });
    }

    void OnNewConnection(tcp::socket socket, size_t sub_idx) {
        auto& sub = *sub_reactors_[sub_idx];
        uint32_t id = next_conn_id_.fetch_add(1, std::memory_order_relaxed);
        auto conn = std::make_shared<TcpConnectionImpl>(std::move(socket), id);

        auto weak_self = std::weak_ptr<TcpServerBoost>(shared_from_this());
        sub.loop->Post([this, weak_self, conn, id, sub_idx]() {
            if (auto self = weak_self.lock()) {
                auto& s = *self->sub_reactors_[sub_idx];
                {
                    std::lock_guard<std::mutex> lock(s.mutex);
                    s.connections[id] = conn;
                }

                conn->on_state_changed =
                    [weak_self, id, sub_idx](const TcpConnectionPtr& c, ConnectionState state) {
                        if (auto self2 = weak_self.lock()) {
                            self2->DispatchStateChangedCallback(sub_idx, c, state);
                            if (state == ConnectionState::kDisconnected ||
                                state == ConnectionState::kDisconnecting) {
                                auto& s2 = *self2->sub_reactors_[sub_idx];
                                std::lock_guard<std::mutex> lock2(s2.mutex);
                                s2.connections.erase(id);
                            }
                        }
                    };

                conn->on_message =
                    [weak_self, sub_idx](const TcpConnectionPtr& c, const void* data, std::size_t len) {
                        if (auto self2 = weak_self.lock()) {
                            self2->DispatchMessageCallback(sub_idx, c, data, len);
                        }
                    };

                conn->on_write_complete =
                    [weak_self, sub_idx](const TcpConnectionPtr& c) {
                        if (auto self2 = weak_self.lock()) {
                            self2->DispatchWriteCompleteCallback(sub_idx, c);
                        }
                    };

                conn->SetState(TcpConnection::kConnected);
                conn->StartRead();
                self->DispatchStateChangedCallback(sub_idx, conn, ConnectionState::kConnected);
            }
        });
    }

    std::shared_ptr<EventLoopInterface> main_loop_;
    std::unique_ptr<EventLoopThread> main_thread_;
    std::weak_ptr<TcpConnectionCallback> callback_;
    TcpServerOptions options_;

    std::unique_ptr<tcp::acceptor> acceptor_;
    std::vector<std::unique_ptr<SubReactor>> sub_reactors_;
    std::atomic<size_t> next_sub_{0};
    std::atomic<uint32_t> next_conn_id_{1};
    std::atomic<RuntimeState> state_{RuntimeState::kInitialized};
    std::atomic<bool> accepting_{true};
};

std::shared_ptr<TcpServerInterface> TcpServerInterface::Create(
    std::shared_ptr<TcpConnectionCallback> callback,
    std::shared_ptr<EventLoopInterface> loop,
    const TcpServerOptions& options) {
    return std::make_shared<TcpServerBoost>(
        std::move(callback), std::move(loop), options);
}

} // namespace itflee
