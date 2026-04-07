#ifndef BOOST_CLIENT_NO_TLS_H_
#define BOOST_CLIENT_NO_TLS_H_

#include "define.h"
#include "socket_client.h"


#ifdef _WINDOWS
#include <SDKDDKVer.h>
#endif // _WINDOWS


#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <variant>
#include <atomic>
#include <chrono>



namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>



namespace itflee {

    class boost_client : public SocketClient
                       , public std::enable_shared_from_this<boost_client>
    {
    public:
        boost_client();
        ~boost_client();

    public:    // override SocketClient interface
        int connect(std::string const& uri, std::shared_ptr<SocketClientListener> listener, const std::string& subprotocol = "")  override;
        int close() override;
        int sendText(const std::string& data) override;
        void setAutoReconnect(bool enable) override;

    private:    // boost socket callback functions override
        void on_resolve(beast::error_code ec, tcp::resolver::results_type results);
        void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep);
        void on_handshake(beast::error_code ec);
        void on_write(beast::error_code ec, std::size_t bytes_transferred);
        void on_read(beast::error_code ec, std::size_t bytes_transferred);
        void on_close(beast::error_code ec);
        void on_control(boost::beast::websocket::frame_type type, boost::string_view payload);

    private:    // boost socket callback functions
        void uninit();
        void destory();
        void start_connect();
        void schedule_reconnect();
        void handle_reconnect_timer(const boost::system::error_code& ec);
        bool parse_uri(const std::string& uri);

    private:    //boost socket member variables
        net::io_context ioc_;// The io_context is required for all I/O
        net::executor_work_guard<net::io_context::executor_type> work_guard_;

        tcp::resolver resolver_;

        std::variant<std::shared_ptr<websocket::stream<beast::tcp_stream>>, 
                    std::shared_ptr<websocket::stream<beast::ssl_stream<beast::tcp_stream>>>> ws_;

        boost::beast::flat_buffer buffer_;  // message buffer
        std::unique_ptr<net::steady_timer> reconnect_timer_;
        std::shared_ptr<ssl::context> ssl_ctx_;

        std::string host_;
        std::string port_;
        std::string subprotocol_;
        std::string type_;
    private:
        std::weak_ptr<SocketClientListener> listener_;
        std::shared_ptr<std::thread> thread_;
        int status_; // status IDLE, CONNECT, DISCONNECT    
        enum class State { Idle, Connecting, Connected, Closing };
        std::atomic<State> state_{ State::Idle };
        std::atomic<bool> reconnect_pending_{ false };  // true if reconnect is pending
        std::atomic<int> reconnect_attempts_{ 0 };      // number of reconnect attempts
        int max_reconnect_attempts_{ 5 };
        std::chrono::milliseconds reconnect_delay_{ std::chrono::milliseconds(2000) };  // 2 seconds delay between reconnect attempts
        bool enable_reconnect_{ true };                 // controlled by upper layer
        std::string last_uri_;
    };

}



#endif // BOOST_CLIENT_NO_TLS_H_
