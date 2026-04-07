#include "WebSocketHelper.h"

#include "ToolKit/log_helper.h"

namespace itflee {
    WebSocketHelper::WebSocketHelper(ssl::context& ssl_context)
        : timer_(boost::asio::make_strand(ioc_)), resolver_(boost::asio::make_strand(ioc_)),
          ws_(std::make_shared<websocket::stream<beast::ssl_stream<beast::tcp_stream>>>(boost::asio::make_strand(ioc_), ssl_context)) {}

    WebSocketHelper::WebSocketHelper()
        : timer_(boost::asio::make_strand(ioc_)), resolver_(boost::asio::make_strand(ioc_)),
          ws_(std::make_shared<websocket::stream<beast::tcp_stream>>(boost::asio::make_strand(ioc_))) {}

    WebSocketHelper::~WebSocketHelper() {
        try {
            Disconnect();
        } catch (const std::exception& e) {
            //utils::LogHelper::Log("[WebSocket] ~WebSocketHelper Error:{}", e.what());
        }
    }

    void WebSocketHelper::Connect(const std::string& host, const std::string& port, const std::string& subprotocol, const std::string& path, bool useSSL,
        std::function<void(const bool isConnect)> handle) {
        try {
            host_ = host;
            port_ = port;
            path_ = path;
            subprotocol_ = subprotocol;
            onConnected_ = handle;
            useSSL_ = useSSL;

            sendMessageThread_ = std::thread(&WebSocketHelper::SendQueuedMessage, this);
            sendMessageThread_.detach();

            timer_.expires_from_now(boost::posix_time::seconds(0) /*std::chrono::seconds(1)*/); // 启动定时器，进行网络连接状态检查
            timer_.async_wait([this](const boost::system::error_code& error) { CheckNetWorkStatus(timer_, error); });

            //Utils::LogHelper::Log("[Websocket] Connect host:{} port:{} path:{}", host, port, path);
            resolver_.async_resolve(host, port, boost::beast::bind_front_handler(&WebSocketHelper::OnResolve, shared_from_this()));

            ioThread_ = std::thread([this]() { ioc_.run(); });
            ioThread_.detach();

        } catch (const std::exception& e) {
            //Utils::LogHelper::Log("[WebSocket] Connect Error:{}", e.what());
        }
    }

    void WebSocketHelper::Send(const Message&& message) {
        std::lock_guard<std::mutex> lock(queueMutex_);
        messageQueue_.push_back(message);
    }

    void WebSocketHelper::Send(const std::string& message) {
        try {
            //Utils::LogHelper::Log("[WebSocket] SendMsg:{} size:{}", message, message.size());
            if (!useSSL_) {
                auto ws = std::get<0>(ws_);
                if (ws == nullptr) {
                    return;
                }
                if (ws->is_open() && !writting_) {
                    ws->text(true);
                    writting_ = true;
                    boost::beast::get_lowest_layer(*ws).expires_after(std::chrono::seconds(15));
                    ws->async_write(boost::asio::buffer(message), boost::beast::bind_front_handler(&WebSocketHelper::OnWrite, shared_from_this()));
                }
            } else {
                auto ws = std::get<1>(ws_);
                if (ws == nullptr) {
                    return;
                }
                if (ws->is_open() && !writting_) {
                    ws->text(true);
                    writting_ = true;
                    boost::beast::get_lowest_layer(*ws).expires_after(std::chrono::seconds(15));
                    ws->async_write(boost::asio::buffer(message), boost::beast::bind_front_handler(&WebSocketHelper::OnWrite, shared_from_this()));
                }
            }
        } catch (const std::exception& e) {
            //Utils::LogHelper::Log("[WebSocket] Send Error:{}", e.what());
        }
    }

    void WebSocketHelper::ReceiveMessage(std::function<void(const Message&)> onReceiveMsg) {
        try {
            onReceiveMsg_ = onReceiveMsg;
        } catch (const std::exception& e) {
            //Utils::LogHelper::Log("[WebSocket] ReceiveMessage Error:{}", e.what());
        }
    }

    void WebSocketHelper::Disconnect() {
        try {
            isQuit_ = true;

            if (sendMessageThread_.joinable()) {
                sendMessageThread_.join();
            }
            if (!useSSL_) {
                auto ws = std::get<0>(ws_);
                boost::beast::get_lowest_layer(*ws).expires_after(std::chrono::seconds(30));
                if (ws->is_open()) {
                    ws->async_close(boost::beast::websocket::close_code::normal, boost::beast::bind_front_handler(&WebSocketHelper::OnClose, shared_from_this()));
                }
            } else {
                auto ws = std::get<1>(ws_);
                boost::beast::get_lowest_layer(*ws).expires_after(std::chrono::seconds(30));
                if (ws->is_open()) {
                    ws->async_close(boost::beast::websocket::close_code::normal, boost::beast::bind_front_handler(&WebSocketHelper::OnClose, shared_from_this()));
                }
            }

            // 停止ioc的运行
            ioc_.stop();

            if (ioThread_.joinable()) {
                ioThread_.join();
            }
            //Utils::LogHelper::Log("[WebSocket] Disconnect.");
        } catch (const std::exception& e) {
            //Utils::LogHelper::Log("[WebSocket] Disconnect Error:{}", e.what());
        }
    }

    void WebSocketHelper::NetStatus(std::function<void(bool isConnected)> handle) {
        onNetConnected_ = handle;
    }

    /****************************************************************************************/

    void WebSocketHelper::OnResolve(const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::results_type results) {
        if (ec) {
            //Utils::LogHelper::Log("[WebSocket] OnResolve Error:{}", ec.message());
            return;
        }
        //Utils::LogHelper::Log("[WebSocket] OnResolve.");

        if (!useSSL_) {
            auto ws = std::get<0>(ws_);
            boost::beast::get_lowest_layer(*ws).expires_after(std::chrono::seconds(15));
            boost::beast::get_lowest_layer(*ws).async_connect(results, boost::beast::bind_front_handler(&WebSocketHelper::OnConnect, shared_from_this()));
        } else {
            auto ws = std::get<1>(ws_);
            boost::beast::get_lowest_layer(*ws).expires_after(std::chrono::seconds(15));
            boost::beast::get_lowest_layer(*ws).async_connect(results, boost::beast::bind_front_handler(&WebSocketHelper::OnConnect, shared_from_this()));
        }
    }

    void WebSocketHelper::OnConnect(const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::results_type::endpoint_type ep) {
        if (ec) {
            //Utils::LogHelper::Log("[WebSocket] OnConnect Error:{}", ec.message());
            return;
        }

        //Utils::LogHelper::Log("[WebSocket] OnConnect.");
        if (!useSSL_) {
            auto ws = std::get<0>(ws_);
            ws->set_option(boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::client));
            ws->set_option(boost::beast::websocket::stream_base::decorator([this](websocket::request_type& req) {
                req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-async");
                req.set(http::field::sec_websocket_protocol, subprotocol_.c_str());
            }));
        } else {
            auto ws = std::get<1>(ws_);
            boost::beast::get_lowest_layer(*ws).expires_after(std::chrono::seconds(30));
            if (!SSL_set_tlsext_host_name(ws->next_layer().native_handle(), host_.c_str())) {
                auto ec_ = boost::beast::error_code(static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category());
                return;
            }
        }

        host_ += ':' + std::to_string(ep.port());

        if (!useSSL_) {
            auto ws = std::get<0>(ws_);
            ws->async_handshake(host_, path_, boost::beast::bind_front_handler(&WebSocketHelper::OnHandshake, shared_from_this()));
        } else {
            auto ws = std::get<1>(ws_);
            ws->next_layer().async_handshake(boost::asio::ssl::stream_base::client, boost::beast::bind_front_handler(&WebSocketHelper::OnSSLHandshake, shared_from_this()));
        }
    }

    void WebSocketHelper::OnSSLHandshake(const boost::system::error_code& ec) {
        if (ec) {
            //Utils::LogHelper::Log("[WebSocket] OnSSLHandshake Error:{}", ec.message());
            return;
        }

        //Utils::LogHelper::Log("[WebSocket] OnSSLHandshake.");

        auto ws = std::get<1>(ws_);
        boost::beast::get_lowest_layer(*ws).expires_never();

        ws->set_option(boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::client));
        ws->set_option(boost::beast::websocket::stream_base::decorator([this](websocket::request_type& req) {
            req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-async-ssl");
            req.set(http::field::sec_websocket_protocol, subprotocol_.c_str());
        }));

        // Perform the WebSocket handshake
        ws->async_handshake(host_, path_, boost::beast::bind_front_handler(&WebSocketHelper::OnHandshake, shared_from_this()));
    }

    void WebSocketHelper::OnHandshake(const boost::system::error_code& ec) {
        if (ec) {
            //Utils::LogHelper::Log("[WebSocket] OnHandshake Error:{}", ec.message());
            onConnected_(false);
            return;
        }

        onConnected_(true);

        //Utils::LogHelper::Log("[WebSocket] OnHandshake.");
        if (!useSSL_) {
            auto ws = std::get<0>(ws_);
            if (ws->is_open()) {
                ws->control_callback(boost::beast::bind_front_handler(&WebSocketHelper::OnCtrol, shared_from_this()));
                boost::beast::get_lowest_layer(*ws).expires_after(std::chrono::seconds(30));
                ws->async_read(buffer_, boost::beast::bind_front_handler(&WebSocketHelper::OnRead, shared_from_this()));
            }
        } else {
            auto ws = std::get<1>(ws_);
            if (ws->is_open()) {
                ws->control_callback(boost::beast::bind_front_handler(&WebSocketHelper::OnCtrol, shared_from_this()));
                boost::beast::get_lowest_layer(*ws).expires_after(std::chrono::seconds(30));
                ws->async_read(buffer_, boost::beast::bind_front_handler(&WebSocketHelper::OnRead, shared_from_this()));
            }
        }
    }

    void WebSocketHelper::OnCtrol(boost::beast::websocket::frame_type type, boost::string_view payload) {
        switch (type) {
        case boost::beast::websocket::frame_type::close:
            //Utils::LogHelper::Log("[WebSocket] OnCtrol close");
            break;
        case boost::beast::websocket::frame_type::ping:
            //Utils::LogHelper::Log("[WebSocket] OnCtrol Ping");
            break;
        case boost::beast::websocket::frame_type::pong:
            //Utils::LogHelper::Log("[WebSocket] OnCtrol Pong");
            break;
        }
    }

    void WebSocketHelper::OnRead(const boost::system::error_code& ec, std::size_t bytes_transferred) {
        try {
            if (ec) {
                //Utils::LogHelper::Log("[WebSocket] OnRead Error:{}", ec.message());
                return;
            }

            /*notify app*/
            auto data = buffer_.data();
            // Utils::LogHelper::Log("[WebSocket] Received ____ message:{}.", boost::beast::buffers_to_string(data));
            MessageType type;

            if (!useSSL_) {
                auto ws = std::get<0>(ws_);
                type = ws->got_binary() ? MessageType::BINARY : MessageType::TEXT;
            } else {
                auto ws = std::get<1>(ws_);
                type = ws->got_binary() ? MessageType::BINARY : MessageType::TEXT;
            }

            Message message;
            message.type = type;
            message.content = boost::beast::buffers_to_string(data);
            if (onReceiveMsg_) {
                onReceiveMsg_(message);
            }

            buffer_.clear();
            if (!useSSL_) {
                auto ws = std::get<0>(ws_);
                boost::beast::get_lowest_layer(*ws).expires_after(std::chrono::seconds(15));
                if (ws->is_open()) {
                    ws->async_read(buffer_, boost::beast::bind_front_handler(&WebSocketHelper::OnRead, shared_from_this()));
                }
            } else {
                auto ws = std::get<1>(ws_);
                boost::beast::get_lowest_layer(*ws).expires_after(std::chrono::seconds(15));
                if (ws->is_open()) {
                    ws->async_read(buffer_, boost::beast::bind_front_handler(&WebSocketHelper::OnRead, shared_from_this()));
                }
            }
        } catch (const std::exception& e) {
            //Utils::LogHelper::Log("[WebSocket] OnRead_ Error:{}", e.what());
        }
    }

    void WebSocketHelper::OnWrite(const boost::system::error_code& ec, std::size_t bytes_transferred) {
        if (!ec) {
            // Utils::LogHelper::Log("OnWrite Success.");
        } else {
            //Utils::LogHelper::Log("[WebSocket] OnWrite Error:{}", ec.message());
        }
        writting_ = false;
    }

    void WebSocketHelper::OnClose(const boost::system::error_code& ec) {
        if (ec) {
            //Utils::LogHelper::Log("[WebSocket] OnClose Error:{}", ec.message());
            return;
        }
        //Utils::LogHelper::Log("[WebSocket] OnClose.");
    }

    /****************************************************************************************/

    void WebSocketHelper::SendQueuedMessage() {
        while (!isQuit_) {
            std::lock_guard<std::mutex> lock(queueMutex_);
            if (messageQueue_.empty()) {
                continue;
            }
            Message message = messageQueue_.front();
            messageQueue_.pop_front();

            //Utils::LogHelper::Log("[WebSocket] SendQueuedMessage:{} ", message.content);
            try {
                switch (message.type) {
                case MessageType::TEXT:
                    if (!useSSL_) {
                        auto ws = std::get<0>(ws_);
                        if (ws->is_open() && !writting_) {
                            ws->text(true);
                            writting_ = true;
                            boost::beast::get_lowest_layer(*ws).expires_after(std::chrono::seconds(15));
                            ws->async_write(boost::asio::buffer(message.content), boost::beast::bind_front_handler(&WebSocketHelper::OnWrite, shared_from_this()));
                        }
                    } else {
                        auto ws = std::get<1>(ws_);
                        if (ws->is_open() && !writting_) {
                            ws->text(true);
                            writting_ = true;
                            boost::beast::get_lowest_layer(*ws).expires_after(std::chrono::seconds(15));
                            ws->async_write(boost::asio::buffer(message.content), boost::beast::bind_front_handler(&WebSocketHelper::OnWrite, shared_from_this()));
                        }
                    }
                    break;
                case MessageType::BINARY:
                    if (!useSSL_) {
                        auto ws = std::get<0>(ws_);
                        if (ws->is_open() && !writting_) {
                            ws->binary(true);
                            writting_ = true;
                            boost::beast::get_lowest_layer(*ws).expires_after(std::chrono::seconds(15));
                            ws->async_write(boost::asio::buffer(message.content), boost::beast::bind_front_handler(&WebSocketHelper::OnWrite, shared_from_this()));
                        }
                    } else {
                        auto ws = std::get<1>(ws_);
                        if (ws->is_open() && !writting_) {
                            ws->binary(true);
                            writting_ = true;
                            boost::beast::get_lowest_layer(*ws).expires_after(std::chrono::seconds(15));
                            ws->async_write(boost::asio::buffer(message.content), boost::beast::bind_front_handler(&WebSocketHelper::OnWrite, shared_from_this()));
                        }
                    }
                    break;
                }

            } catch (const std::exception& e) {
                //Utils::LogHelper::Log("[WebSocket] SendQueuedMessage Error:{}", e.what());
            }

            std::this_thread::sleep_for(std::chrono::microseconds(2));
        }
    }

    void WebSocketHelper::CheckNetWorkStatus(boost::asio::deadline_timer& timer, const boost::system::error_code& error) {
        if (error == boost::asio::error::operation_aborted) {
            //Utils::LogHelper::Log("[NetWorkStatus] NetWork Thread Abord."); // 定时器被取消，忽略此次检查
            return;
        }

        boost::asio::io_context ioContext;
        boost::asio::ip::tcp::socket socket(ioContext);
        boost::system::error_code error_;
        std::string host = this->host_;
        int port = std::stoi(this->port_);

        // 尝试连接一个一直的IP地址和端口
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address(host), port);
        socket.connect(endpoint, error_);

        // 检测网络连接状态
        if (!error_) {
            if (!netConnected) {
                // onNetConnected_(true);
                //Utils::LogHelper::Log("[NetWorkStatus] Network connection is established.");
            }
            netConnected = true;

        } else {
            if (netConnected) {
                // onNetConnected_(false);
                //Utils::LogHelper::Log("[NetWorkStatus] Network connection is lost.");
            }
            netConnected = false;
        }

        // 重新设置定时器，进行下一次检查
        timer.expires_at(timer.expires_at() + boost::posix_time::seconds(1) /*std::chrono::seconds(1)*/);
        timer.async_wait([this, &timer](const boost::system::error_code& error) { CheckNetWorkStatus(timer, error); });
    }

} // namespace Utils
