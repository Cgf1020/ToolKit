#include "network/udp/udp_client_interface.h"
#include "base/eventloop/event_loop_thread.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

#include <boost/asio/ip/udp.hpp>
#include <boost/asio/post.hpp>

namespace itflee {

namespace {
using udp = boost::asio::ip::udp;
}

class UdpClientBoost final
    : public UdpClientInterface
    , public std::enable_shared_from_this<UdpClientBoost> {
public:
    UdpClientBoost(std::shared_ptr<UdpCallback> callback,
                   std::shared_ptr<EventLoopInterface> loop)
        : callback_(std::move(callback)) {
        if (loop) {
            loop_ = std::move(loop);
        } else {
            loop_ = EventLoopInterface::Create();
            loop_thread_ = std::make_unique<EventLoopThread>(loop_);
        }
    }

    ~UdpClientBoost() override {
        Stop();
    }

    int Open(const std::string& remote_host, int remote_port) override {
        if (remote_host.empty() || remote_port <= 0 || remote_port > 65535) {
            return -1;
        }

        auto executor = std::any_cast<boost::asio::io_context::executor_type>(
            loop_->GetNativeExecutor());

        boost::system::error_code ec;
        auto addr = boost::asio::ip::make_address(remote_host, ec);
        if (ec) return -2;

        remote_endpoint_ = udp::endpoint(addr, static_cast<uint16_t>(remote_port));

        socket_ = std::make_unique<udp::socket>(executor);

        auto protocol = remote_endpoint_.protocol();
        socket_->open(protocol, ec);
        if (ec) return -3;

        remote_host_ = remote_host;
        remote_port_ = remote_port;
        return 0;
    }

    void Start() override {
        if (loop_thread_) {
            loop_thread_->start();
        }
        auto self = shared_from_this();
        loop_->Post([self]() { self->DoReceive(); });
    }

    void Stop() override {
        if (socket_) {
            auto self = shared_from_this();
            loop_->Post([self]() {
                if (self->socket_ && self->socket_->is_open()) {
                    boost::system::error_code ec;
                    self->socket_->close(ec);
                }
            });
        }

        if (loop_thread_) {
            loop_thread_->stop(true);
        }
    }

    std::shared_ptr<EventLoopInterface> GetLoop() const noexcept override {
        return loop_;
    }

    int Send(const void* data, std::size_t len) override {
        if (!socket_ || !socket_->is_open()) return -1;

        auto buf = std::make_shared<std::vector<char>>(
            static_cast<const char*>(data),
            static_cast<const char*>(data) + len);
        auto target = remote_endpoint_;

        auto self = shared_from_this();
        boost::asio::post(socket_->get_executor(),
            [self, buf, target]() {
                if (self->socket_ && self->socket_->is_open()) {
                    boost::system::error_code ec;
                    self->socket_->send_to(
                        boost::asio::buffer(*buf), target, 0, ec);
                }
            });
        return 0;
    }

    int SendTo(const void* data, std::size_t len,
               const std::string& host, uint16_t port) override {
        if (!socket_ || !socket_->is_open()) return -1;

        boost::system::error_code ec;
        auto addr = boost::asio::ip::make_address(host, ec);
        if (ec) return -2;

        udp::endpoint target(addr, port);

        auto buf = std::make_shared<std::vector<char>>(
            static_cast<const char*>(data),
            static_cast<const char*>(data) + len);

        auto self = shared_from_this();
        boost::asio::post(socket_->get_executor(),
            [self, buf, target]() {
                if (self->socket_ && self->socket_->is_open()) {
                    boost::system::error_code send_ec;
                    self->socket_->send_to(
                        boost::asio::buffer(*buf), target, 0, send_ec);
                }
            });
        return 0;
    }

private:
    void DoReceive() {
        if (!socket_ || !socket_->is_open()) return;

        auto self = shared_from_this();
        socket_->async_receive_from(
            boost::asio::buffer(recv_buffer_), sender_endpoint_,
            [self](boost::system::error_code ec, std::size_t bytes_transferred) {
                if (!ec && bytes_transferred > 0) {
                    if (self->callback_) {
                        self->callback_->OnMessage(
                            self->recv_buffer_.data(), bytes_transferred,
                            self->sender_endpoint_.address().to_string(),
                            self->sender_endpoint_.port());
                    }
                }
                if (self->socket_ && self->socket_->is_open()) {
                    self->DoReceive();
                }
            });
    }

    std::shared_ptr<EventLoopInterface> loop_;
    std::unique_ptr<EventLoopThread> loop_thread_;
    std::shared_ptr<UdpCallback> callback_;

    std::unique_ptr<udp::socket> socket_;
    udp::endpoint remote_endpoint_;
    std::string remote_host_;
    int remote_port_{0};

    static constexpr std::size_t kRecvBufferSize = 65536;
    std::array<char, kRecvBufferSize> recv_buffer_{};
    udp::endpoint sender_endpoint_;
};

std::shared_ptr<UdpClientInterface> UdpClientInterface::Create(
    std::shared_ptr<UdpCallback> callback,
    std::shared_ptr<EventLoopInterface> loop) {
    return std::make_shared<UdpClientBoost>(std::move(callback), std::move(loop));
}

} // namespace itflee
