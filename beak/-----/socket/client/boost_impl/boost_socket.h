#ifndef BOOST_CLIENT_NO_TLS_H_
#define BOOST_CLIENT_NO_TLS_H_

#include "ToolKit/define.h"
#include "../socket_client.h"


#ifdef _WINDOWS
#include <SDKDDKVer.h>
#endif // _WINDOWS


#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/ssl.hpp>


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

    public: // SocketClient interface
        int connect(std::string const& uri, std::shared_ptr<SocketClientListener> listener, const std::string& subprotocol = "")  override;
        int close() override;
        int sendText(const std::string& data) override;

    private:    // boost socket callback functions
        void on_resolve(beast::error_code ec, tcp::resolver::results_type results);
        void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep);
        void on_handshake(beast::error_code ec);
        void on_write(beast::error_code ec, std::size_t bytes_transferred);
        void on_read(beast::error_code ec, std::size_t bytes_transferred);
        void on_close(beast::error_code ec);
        void on_control(boost::beast::websocket::frame_type type, boost::string_view payload);

    private:
        void uninit();
        void destory();

    private:    //boost     
        net::io_context ioc_;// The io_context is required for all I/O

        //ssl::context ctx_;

        tcp::resolver resolver_;

        //websocket::stream<beast::tcp_stream> ws_;

        //boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>
        std::variant<std::shared_ptr<websocket::stream<beast::tcp_stream>>, 
                    std::shared_ptr<websocket::stream<beast::ssl_stream<beast::tcp_stream>>>> ws_;
        //std::shared_ptr<>
        

        boost::beast::flat_buffer buffer_;  // message buffer

        std::string host_;

        std::string port_;

        std::string subprotocol_;

        std::string type_;
    private:
        std::weak_ptr<SocketClientListener> listener_;

        std::shared_ptr<std::thread> thread_;

        int status_; //IDEL, CONNECT, DISCONNECT

        //std::atomic<int> status;

    private:
        //socket status
        static const int STATUS_IDEL = 0;     //C++17 support
        static const int STATUS_CONNECT = 1;
        static const int STATUS_DISTCONNECT = 2;
    };

}



#endif // BOOST_CLIENT_NO_TLS_H_
