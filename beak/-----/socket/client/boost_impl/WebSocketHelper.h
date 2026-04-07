#pragma once

#include <iostream>
#include <string>
#include <functional>
#include <deque>
#include <mutex>
#include <variant>

#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>

namespace itflee {
    namespace {
        using tcp = boost::asio::ip::tcp;
        namespace beast = boost::beast;
        namespace websocket = boost::beast::websocket;
        namespace ssl = boost::asio::ssl;
        namespace http = boost::beast::http;
    } // namespace

    class WebSocketHelper : public std::enable_shared_from_this<WebSocketHelper> {
    public:
        enum class MessageType { BINARY, TEXT };
        struct Message {
            MessageType type;
            std::string content;
        };

    public:
        WebSocketHelper(ssl::context& ssl_context);
        WebSocketHelper();
        ~WebSocketHelper();

        void Connect(const std::string& host, const std::string& port, const std::string& subprotocal, const std::string& path, bool useSSL,
            std::function<void(bool isConnect)> handle);
        void Send(const Message&& message);
        void Send(const std::string& message);
        void ReceiveMessage(std::function<void(const Message&)> onMessage);
        void Disconnect();
        void NetStatus(std::function<void(bool isConnected)> handle);

    private:
        void OnResolve(const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::results_type results);
        void OnConnect(const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::results_type::endpoint_type ep);
        void OnSSLHandshake(const boost::system::error_code& ec);
        void OnHandshake(const boost::system::error_code& ec);
        void OnCtrol(boost::beast::websocket::frame_type type, boost::string_view payload);
        void OnRead(const boost::system::error_code& ec, std::size_t bytes_transferred);
        void OnWrite(const boost::system::error_code& ec, std::size_t bytes_transferred);
        void OnClose(const boost::system::error_code& ec);

        void SendQueuedMessage();
        void CheckNetWorkStatus(boost::asio::deadline_timer& timer, const boost::system::error_code& error);

    private:
        std::function<void(const bool isConnect)> onConnected_;
        std::function<void(const Message&)> onReceiveMsg_;
        std::function<void(const bool isConnect)> onNetConnected_;

    private:
        std::string host_;
        std::string port_;
        std::string path_;
        std::string subprotocol_;

        bool isConnected{false};
        bool netConnected{false};
        bool useSSL_{false};
        boost::asio::io_context ioc_;
        boost::asio::ip::tcp::resolver resolver_;
        bool isQuit_{false};
        boost::asio::deadline_timer timer_;

        std::variant<std::shared_ptr<websocket::stream<beast::tcp_stream>>, std::shared_ptr<websocket::stream<boost::beast::ssl_stream<beast::tcp_stream>>>> ws_;
        boost::beast::multi_buffer buffer_;
        std::mutex queueMutex_;
        std::deque<Message> messageQueue_;
        std::thread heartbeatThread_;
        std::thread sendMessageThread_;
        std::thread ioThread_;
        std::unordered_map<std::string, std::shared_ptr<std::function<void(std::string& jsonData)>>> callbackMap_;
        std::atomic<bool> writting_{false};
    };
} // namespace itflee
