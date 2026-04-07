#include "network/udp/udp_server_interface.h"
#include "base/eventloop/event_loop_thread.h"

#include <array>
#include <memory>
#include <sstream>
#include <string>

#include <boost/asio/ip/udp.hpp>
#include <boost/asio/post.hpp>

namespace itflee {

namespace {
using udp = boost::asio::ip::udp;
}

class UdpServerBoost final
    : public UdpServerInterface
    , public std::enable_shared_from_this<UdpServerBoost> {
public:
    UdpServerBoost(std::shared_ptr<UdpCallback> callback,
                   std::shared_ptr<EventLoopInterface> loop)
        : callback_(std::move(callback)) {
        if (loop) {
            loop_ = std::move(loop);
        } else {
            loop_ = EventLoopInterface::Create();
            loop_thread_ = std::make_unique<EventLoopThread>(loop_);
        }
    }

    ~UdpServerBoost() override {
        Stop();
    }

    int Bind(int port, const std::string& host) override {
        auto executor = std::any_cast<boost::asio::io_context::executor_type>(
            loop_->GetNativeExecutor());

        boost::system::error_code ec;
        auto addr = boost::asio::ip::make_address(host, ec);
        if (ec) return -1;

        udp::endpoint endpoint(addr, static_cast<uint16_t>(port));
        socket_ = std::make_unique<udp::socket>(executor);

        socket_->open(endpoint.protocol(), ec);
        if (ec) return -2;

        socket_->set_option(boost::asio::socket_base::reuse_address(true), ec);

        socket_->bind(endpoint, ec);
        if (ec) return -3;

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
    static constexpr std::size_t kRecvBufferSize = 65536;
    std::array<char, kRecvBufferSize> recv_buffer_{};
    udp::endpoint sender_endpoint_;
};

std::shared_ptr<UdpServerInterface> UdpServerInterface::Create(
    std::shared_ptr<UdpCallback> callback,
    std::shared_ptr<EventLoopInterface> loop) {
    return std::make_shared<UdpServerBoost>(std::move(callback), std::move(loop));
}

} // namespace itflee
