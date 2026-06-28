#include "boost_socket.h"

#include "root_certificates.hpp"
#include <boost/version.hpp>
#include <chrono>



namespace itflee {

    boost_client::boost_client()
        : work_guard_(net::make_work_guard(ioc_))
        , resolver_(net::make_strand(ioc_))
        , host_("")
        , port_("")
        , subprotocol_("")
        , type_("ws")
        , status_(0)
    {
        reconnect_timer_ = std::make_unique<net::steady_timer>(ioc_);
        thread_ = std::make_shared<std::thread>([this]() {
            ioc_.run();
        });
    }

    boost_client::~boost_client()
    {
        destory();
        if (reconnect_timer_) {
#if BOOST_VERSION >= 108900
            reconnect_timer_->cancel();
#else
            boost::system::error_code ec;
            reconnect_timer_->cancel(ec);
#endif
        }
        work_guard_.reset();
        ioc_.stop();
        if (thread_ && thread_->joinable()) {
            thread_->join();
        }
    }

    int boost_client::connect(std::string const& uri, 
        std::shared_ptr<SocketClientListener> listener, 
        const std::string& subprotocol)
    {
        auto st = state_.load();
        if (st == State::Connecting || st == State::Connected) {
            return -1;
        }

        listener_ = listener;
        subprotocol_ = subprotocol;
        reconnect_pending_ = false;
        reconnect_attempts_ = 0;
        last_uri_ = uri;

        net::post(ioc_, [self = shared_from_this(), uri]() {
            self->uninit();

			if (!self->parse_uri(uri)) {
				return;
			}
            self->start_connect();
        });

        return 0;
    }

    int boost_client::close()
    {
        auto expected = State::Connected;
        if (!state_.compare_exchange_strong(expected, State::Closing)) {
            return -1;
        }

        net::post(ioc_, [self = shared_from_this()]() {
            if (self->type_ == "ws")
            {
                auto* p = std::get_if<std::shared_ptr<websocket::stream<beast::tcp_stream>>>(&self->ws_);
                if (p && *p) {
                    (*p)->async_close(websocket::close_code::normal, beast::bind_front_handler(&boost_client::on_close, self));
                }
            }
            else
            {
                auto* p = std::get_if<std::shared_ptr<websocket::stream<beast::ssl_stream<beast::tcp_stream>>>>(&self->ws_);
                if (p && *p) {
                    (*p)->async_close(websocket::close_code::normal, beast::bind_front_handler(&boost_client::on_close, self));
                }
            }
        });

        return 0;
    }

    void boost_client::setAutoReconnect(bool enable)
    {
        enable_reconnect_ = enable;
    }

    int boost_client::sendText(const std::string& data)
    {
        if (state_.load() != State::Connected) {
            return -1;
        }

        auto payload = std::make_shared<std::string>(data);
        net::post(ioc_, [self = shared_from_this(), payload]() {
            if (self->state_.load() != State::Connected) {
                return;
            }
            if (self->type_ == "ws")
            {
                auto* p = std::get_if<std::shared_ptr<websocket::stream<beast::tcp_stream>>>(&self->ws_);
                if (p && *p) {
                    (*p)->async_write(net::buffer(*payload), beast::bind_front_handler(&boost_client::on_write, self));
                }
            }
            else
            {
                auto* p = std::get_if<std::shared_ptr<websocket::stream<beast::ssl_stream<beast::tcp_stream>>>>(&self->ws_);
                if (p && *p) {
                    (*p)->async_write(net::buffer(*payload), beast::bind_front_handler(&boost_client::on_write, self));
                }
            }
        });
 
        return 0;
    }

    void boost_client::on_resolve(beast::error_code ec, tcp::resolver::results_type results)
    {
        do
        {
            if (ec)
            {
                break;
            }

            // make the connection on the IP address we get from a lookup
            if (type_ == "ws")
            {
                auto ws = std::get<0>(ws_);

                // Set the timeout for the operation
                boost::beast::get_lowest_layer(*ws).expires_after(std::chrono::seconds(30));

                // Make the connection on the IP address we get from a lookup
                beast::get_lowest_layer(*ws).async_connect(results, beast::bind_front_handler(&boost_client::on_connect, shared_from_this()));
            }
            else
            {
                auto ws = std::get<1>(ws_);
                // Set the timeout for the operation
                boost::beast::get_lowest_layer(*ws).expires_after(std::chrono::seconds(30));

                // Make the connection on the IP address we get from a lookup
                beast::get_lowest_layer(*ws).async_connect(results, beast::bind_front_handler(&boost_client::on_connect, shared_from_this()));
            }

            return;
        } while (0);

        if (auto listener = listener_.lock()) 
        {
            listener->onFail(ec.value(), "on_resolve: " + ec.message());
        }
        if (ec)
        {
			schedule_reconnect();
        }
    }

    void boost_client::on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep)
    {
        do
        {
            if (ec)
            {
                break;
            }

            if (type_ == "ws")
            {
                auto ws = std::get<0>(ws_);

                // Turn off the timeout on the tcp_stream, because the websocket stream has its own timeout system.
                beast::get_lowest_layer(*ws).expires_never();

                // Set suggested timeout settings for the websocket
                ws->set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

                // Set a decorator to change the User-Agent of the handshake
                ws->set_option(websocket::stream_base::decorator([this](websocket::request_type& req) {
                    req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-async");
                    if (!subprotocol_.empty()) {
                        req.set(boost::beast::http::field::sec_websocket_protocol, subprotocol_);
                    }
                }));


                // Update the host_ string. This will provide the value of the
                // Host HTTP header during the WebSocket handshake.
                // See https://tools.ietf.org/html/rfc7230#section-5.4
               // Build the Host header value for the WebSocket handshake (RFC 7230 §5.4).
                // Use a local variable to avoid mutating host_ on every reconnect.
                std::string host_header = host_ + ':' + std::to_string(ep.port());

                // Perform the websocket handshake on text message
                ws->async_handshake(host_header, "/", boost::beast::bind_front_handler(&boost_client::on_handshake, shared_from_this()));
            }
            else
            {
                auto ws = std::get<1>(ws_);

                // Turn off the timeout on the tcp_stream, because the websocket stream has its own timeout system.
                beast::get_lowest_layer(*ws).expires_never();

                // Set suggested timeout settings for the websocket
                ws->set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

                // Set a decorator to change the User-Agent of the handshake
                ws->set_option(websocket::stream_base::decorator([this](websocket::request_type& req) {
                    req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-async");
                    if (!subprotocol_.empty()) {
                        req.set(boost::beast::http::field::sec_websocket_protocol, subprotocol_);
                    }
                }));

                // Build the Host header value (local var – do NOT mutate host_)
                std::string host_header = host_ + ':' + std::to_string(ep.port());

                // Perform the websocket handshake
                ws->async_handshake(host_header, "/", boost::beast::bind_front_handler(&boost_client::on_handshake, shared_from_this()));

            }

            return;
        } while (0);

        if (auto listener = listener_.lock()) {
            listener->onFail(ec.value(), "on_connect: " + ec.message());
        }
		if (ec)
		{
			schedule_reconnect();
		}
    }

    void boost_client::on_handshake(beast::error_code ec)
    {
        if (auto l = listener_.lock())
        {
            if (ec)
            {
                l->onFail(ec.value(), "on_handshake: " + ec.message());
            }
            else
            {
				state_ = State::Connected;
				reconnect_pending_ = false;
                l->onOpen();

                if (type_ == "ws")
                {
                    auto ws = std::get<0>(ws_);

                    // on control message
                    ws->control_callback(boost::beast::bind_front_handler(&boost_client::on_control, shared_from_this()));

                    // on text message
                    ws->async_read(buffer_, beast::bind_front_handler(&boost_client::on_read, shared_from_this()));

                }
                else
                {
                    auto ws = std::get<1>(ws_);

                    // on control message
                    ws->control_callback(boost::beast::bind_front_handler(&boost_client::on_control, shared_from_this()));

                    // on text message
                    ws->async_read(buffer_, beast::bind_front_handler(&boost_client::on_read, shared_from_this()));
                } 
            }
        }
		if (ec)
		{
			state_ = State::Idle;
			schedule_reconnect();
		}
    }

    void boost_client::on_write(beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
        {
            // on write message failed
        }
    }

    void boost_client::on_read(beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
        {
            if (auto l = listener_.lock()) {
                l->onFail(ec.value(), "on_read: " + ec.message());
            }
			schedule_reconnect();
            return;
        }

        // extract text and dispatch to listener
        auto data = buffer_.data();
        std::string msg(static_cast<const char*>(data.data()), data.size());
        buffer_.consume(data.size());

        if (auto l = listener_.lock()) {
            l->onTextMessage(msg);
        }

        // continue reading
        if (type_ == "ws")
        {
            auto ws = std::get<0>(ws_);
            // short timeout if needed; currently commented out
            // beast::get_lowest_layer(*ws).expires_after(std::chrono::seconds(10));
            ws->async_read(buffer_, boost::beast::bind_front_handler(&boost_client::on_read, shared_from_this()));
        }
        else
        {
            auto ws = std::get<1>(ws_);
            // short timeout if needed; currently commented out
            // beast::get_lowest_layer(*ws).expires_after(std::chrono::seconds(10));
            ws->async_read(buffer_, boost::beast::bind_front_handler(&boost_client::on_read, shared_from_this()));
        }
    }

    void boost_client::on_close(beast::error_code ec)
    {
        if (auto l = listener_.lock())
        {
            if (ec) {
                l->onFail(ec.value(), "on_close: " + ec.message());
            } else {
                l->onClose(ec.value(), "by user");
            }
        }
		state_ = State::Idle;
		uninit();
		if (ec || reconnect_pending_) {
			schedule_reconnect();
		}
    }

    void boost_client::on_control(boost::beast::websocket::frame_type type, boost::string_view payload)
    {
        std::string data(payload.begin(), payload.end());

        switch (type) {
        case boost::beast::websocket::frame_type::close:
        {
            std::cout << "on_control close, payload: " << data << std::endl;
            // peer sent close frame, notify upper layer
            if (auto l = listener_.lock()) {
                // parse close code (first 2 bytes)
                int close_code = 0;
                if (payload.size() >= 2) {
                    close_code = (static_cast<unsigned char>(payload[0]) << 8) | static_cast<unsigned char>(payload[1]);
                }
                std::string reason = payload.size() > 2 ? std::string(payload.begin() + 2, payload.end()) : "peer closed";
                l->onClose(close_code, reason);
            }
            break;
        }
        case boost::beast::websocket::frame_type::ping:
        {
            std::cout << "on_control ping, payload: " << data << std::endl;
            // notify upper layer ping received
            bool should_reply = true;
            if (auto l = listener_.lock()) {
                should_reply = l->onPing(data);
            }
            // reply pong depending on upper layer decision
            if (should_reply) {
                websocket::ping_data pd(payload);
                if (type_ == "ws") {
                    auto ws = std::get<0>(ws_);
                    ws->pong(pd); // sync pong
                } else {
                    auto ws = std::get<1>(ws_);
                    ws->pong(pd); // sync pong
                }
            }
            break;
        }
        case boost::beast::websocket::frame_type::pong:
        {
            std::cout << "on_control pong, payload: " << data << std::endl;
            // notify upper layer pong received (heartbeat)
            if (auto l = listener_.lock()) {
                l->onPong(data);
            }
            break;
        }
        default:
            std::cout << "on_control unknown frame type" << std::endl;
            break;
        }
    }

	bool boost_client::parse_uri(const std::string& uri)
	{
		auto scheme_pos = uri.find("://");
		if (scheme_pos == std::string::npos) {
			if (auto l = listener_.lock()) {
				l->onFail(-1, "invalid uri: missing scheme");
			}
			return false;
		}

		type_ = uri.substr(0, scheme_pos); // ws or wss
		std::string rest = uri.substr(scheme_pos + 3);

		auto slash_pos = rest.find('/');
		std::string hostport = (slash_pos == std::string::npos) ? rest : rest.substr(0, slash_pos);

		auto colon_pos = hostport.find(':');
		if (colon_pos == std::string::npos) {
			if (auto l = listener_.lock()) {
				l->onFail(-1, "invalid uri: missing port");
			}
			return false;
		}

		host_ = hostport.substr(0, colon_pos);
		port_ = hostport.substr(colon_pos + 1);
		return true;
	}

	void boost_client::start_connect()
	{
		auto st = state_.load();
		if (st == State::Connecting || st == State::Connected) {
			return;
		}
		state_ = State::Connecting;

		bool use_ssl = (type_ == "wss");

		if (!use_ssl)
		{
			ws_ = std::make_shared<websocket::stream<beast::tcp_stream>>(net::make_strand(ioc_));
		}
		else
		{
			ssl_ctx_ = std::make_shared<ssl::context>(ssl::context::tlsv12_client);
			load_root_certificates(*ssl_ctx_);
			ws_ = std::make_shared<websocket::stream<beast::ssl_stream<beast::tcp_stream>>>(net::make_strand(ioc_), *ssl_ctx_);
		}

		resolver_.async_resolve(host_, port_, beast::bind_front_handler(&boost_client::on_resolve, shared_from_this()));
	}

	void boost_client::schedule_reconnect()
	{
		if (!enable_reconnect_) {
			return;
		}
		state_ = State::Idle;
		if (reconnect_attempts_.load() >= max_reconnect_attempts_) {
			if (auto l = listener_.lock()) {
				l->onFail(-1, "max reconnect attempts reached");
			}
			return;
		}
		reconnect_pending_ = true;
		reconnect_attempts_++;
		if (reconnect_timer_) {
			reconnect_timer_->expires_after(reconnect_delay_);
			reconnect_timer_->async_wait([self = shared_from_this()](const boost::system::error_code& ec) {
				self->handle_reconnect_timer(ec);
			});
		}
	}

	void boost_client::handle_reconnect_timer(const boost::system::error_code& ec)
	{
		if (ec) return;
		if (!reconnect_pending_) return;
		if (!parse_uri(last_uri_)) return;
		start_connect();
	}

    void boost_client::uninit()
    {
        resolver_.cancel();
        buffer_.consume(buffer_.size());

        ws_ = {};
        ssl_ctx_.reset();
        host_.clear();
        port_.clear();
        subprotocol_.clear();
        type_ = "ws";
		state_ = State::Idle;
		reconnect_pending_ = false;
    }

    void boost_client::destory()
    {
        uninit();
    }


}

