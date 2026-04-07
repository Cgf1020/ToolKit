#include "boost_socket.h"

#include "../root_certificates.hpp"



//#define CHECK_READY_RETURN_INT \
//CHECK_INIT_RETURN_INT; \
//if (_status != STATUS_ALLREADY) { \
//	TKLOG_E("[rtc-sdk] Not joined the room yet. function: " << __func__); \
//	return ERR_INVALID_STATUS; \
//}




namespace itflee {

    std::shared_ptr<SocketClient> SocketClient::getSocketClient()
   {
       std::shared_ptr<SocketClient> socket(new boost_client());
       return socket;
   }

   

    boost_client::boost_client()
        : resolver_(net::make_strand(ioc_))
        , host_("")
        , port_("")
        , subprotocol_("")
        , type_("ws")
        , status_(STATUS_IDEL)
    {
    }

    boost_client::~boost_client()
    {
        destory();
    }

    int boost_client::connect(std::string const& uri, std::shared_ptr<SocketClientListener> listener, const std::string& subprotocol)
    {
        //if (status_ == STATUS_CONNECT)
        //    return 0;

        listener_ = listener;

        //--. get host port path from url
        host_ = "47.104.168.9";
        port_ = "51188";
        
        //--. create socket
        if (type_ == "ws")
        {
            ws_ = std::make_shared<websocket::stream<beast::tcp_stream>>(net::make_strand(ioc_));
        }
        else    //wss
        {
            // The SSL context is required, and holds certificates
            ssl::context ctx{ssl::context::tlsv12_client};

            // This holds the root certificate used for verification
            load_root_certificates(ctx);

            // Launch the asynchronous operation
            ws_ = std::make_shared<websocket::stream<beast::ssl_stream<beast::tcp_stream>>>(net::make_strand(ioc_), ctx);
        }


        //--. Look up the domain name
        resolver_.async_resolve(host_, port_, beast::bind_front_handler(&boost_client::on_resolve, shared_from_this()));


        //--. Run the I/O service. The call will return when the socket is closed.
        thread_ = std::make_shared<std::thread>([this]() {

            boost::beast::error_code ec;

            ioc_.run(ec);

            if (ec)
            {
                if (auto listener = listener_.lock())
                {
                    listener->onClose(ec.value(), "connect: " + ec.message());
                }
            }
            });
        thread_->detach();
    
        return 0;
    }

    int boost_client::close()
    {
        // Close the WebSocket connection
        if (type_ == "ws")
        {
            auto ws = std::get<0>(ws_);
            ws->async_close(websocket::close_code::normal, beast::bind_front_handler(&boost_client::on_close, shared_from_this()));
        }
        else
        {
            auto ws = std::get<1>(ws_);
            ws->async_close(websocket::close_code::normal, beast::bind_front_handler(&boost_client::on_close, shared_from_this()));
        }

        return 0;
    }

    int boost_client::sendText(const std::string& data)
    {
        //if (status_ == STATUS_CONNECT)
        //{
        //    
        //}
        if (type_ == "ws")
        {
            auto ws = std::get<0>(ws_);
            ws->async_write(net::buffer(data), beast::bind_front_handler(&boost_client::on_write, shared_from_this()));
        }
        else
        {
            auto ws = std::get<1>(ws_);
            ws->async_write(net::buffer(data), beast::bind_front_handler(&boost_client::on_write, shared_from_this()));
        }

        // Send the message
        
       
        return 0;
    }

    void boost_client::on_resolve(beast::error_code ec, tcp::resolver::results_type results)
    {
        //std::cout << "boost_client_no_tls::on_resolve" << std::endl;
        do
        {
            if (ec)
            {
                //std::cout << "on_resolve err = " << ec.message() << std::endl;
                break;
            }

            //make the connection on the IP address we get from a lookup
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
                ws->set_option(websocket::stream_base::decorator([](websocket::request_type& req) {
                    req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-async");
                    req.set(boost::beast::http::field::sec_websocket_protocol, "janus-protocol");
                    }));


                // Update the host_ string. This will provide the value of the
                // Host HTTP header during the WebSocket handshake.
                // See https://tools.ietf.org/html/rfc7230#section-5.4
               // Update the host_ string. This will provide the value of the
                        // Host HTTP header during the WebSocket handshake.
                        // See https://tools.ietf.org/html/rfc7230#section-5.4
                host_ += ':' + std::to_string(ep.port());

                // Perform the websocket handshake on text message
                ws->async_handshake(host_, "/", boost::beast::bind_front_handler(&boost_client::on_handshake, shared_from_this()));

            }
            else
            {
                auto ws = std::get<1>(ws_);

                // Turn off the timeout on the tcp_stream, because the websocket stream has its own timeout system.
                beast::get_lowest_layer(*ws).expires_never();

                // Set suggested timeout settings for the websocket
                ws->set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

                // Set a decorator to change the User-Agent of the handshake
                ws->set_option(websocket::stream_base::decorator([](websocket::request_type& req) {
                    req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-async");
                    req.set(boost::beast::http::field::sec_websocket_protocol, "janus-protocol");
                    }));


                // Update the host_ string. This will provide the value of the
                // Host HTTP header during the WebSocket handshake.
                // See https://tools.ietf.org/html/rfc7230#section-5.4
               // Update the host_ string. This will provide the value of the
                        // Host HTTP header during the WebSocket handshake.
                        // See https://tools.ietf.org/html/rfc7230#section-5.4
                host_ += ':' + std::to_string(ep.port());

                // Perform the websocket handshake on text message
                ws->async_handshake(host_, "/", boost::beast::bind_front_handler(&boost_client::on_handshake, shared_from_this()));

            }

            return;
        } while (0);

        if (auto listener = listener_.lock()) {
            listener->onFail(ec.value(), "on_connect: " + ec.message());
        }
    }

    void boost_client::on_handshake(beast::error_code ec)
    {
        if (listener_.lock())
        {
            if (ec)
            {
                listener_.lock()->onFail(ec.value(), "on_handshake: " + ec.message());
            }
            else
            {
                //status_ = STATUS_CONNECT;

                listener_.lock()->onOpen();

                if (type_ == "ws")
                {
                    auto ws = std::get<0>(ws_);

                    //on control message
                    ws->control_callback(boost::beast::bind_front_handler(&boost_client::on_control, shared_from_this()));

                    //on text message
                    ws->async_read(buffer_, beast::bind_front_handler(&boost_client::on_read, shared_from_this()));

                }
                else
                {
                    auto ws = std::get<1>(ws_);

                    //on control message
                    ws->control_callback(boost::beast::bind_front_handler(&boost_client::on_control, shared_from_this()));

                    //on text message
                    ws->async_read(buffer_, beast::bind_front_handler(&boost_client::on_read, shared_from_this()));
                } 
            }
        }
    }

    void boost_client::on_write(beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        std::cout << "on_write = " << ec.value() << std::endl;

        if (ec)
        {
            //on write message failed
        }
    }

    void boost_client::on_read(beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (listener_.lock())
        {
            if (ec)
            {
                //listener_.lock()->onFail(ec.value(), "on_handshake: " + ec.message());
            }
            else
            {
                auto data = buffer_.data();

                //std::cout << "on_read data = " << std::string(reinterpret_cast<char*>(data.data()), data.size()) << std::endl;

                buffer_.clear();

                if (type_ == "ws")
                {
                    auto ws = std::get<0>(ws_);

                    // Set the timeout for the operation
                    beast::get_lowest_layer(*ws).expires_after(std::chrono::seconds(30));

                    //on text message
                    ws->async_read(buffer_, boost::beast::bind_front_handler(&boost_client::on_read, shared_from_this()));
                }
                else
                {
                    auto ws = std::get<1>(ws_);

                    // Set the timeout for the operation
                    beast::get_lowest_layer(*ws).expires_after(std::chrono::seconds(30));

                    //on text message
                    ws->async_read(buffer_, boost::beast::bind_front_handler(&boost_client::on_read, shared_from_this()));
                }    
            }
        }
    }

    void boost_client::on_close(beast::error_code ec)
    {
        if (ec && listener_.lock())
        {
            listener_.lock()->onFail(ec.value(), "on_close: " + ec.message());
        }
        else if(listener_.lock())
        {
            listener_.lock()->onClose(ec.value(), "by user");
        }
    }

    void boost_client::on_control(boost::beast::websocket::frame_type type, boost::string_view payload)
    {
        switch (type) {
        case boost::beast::websocket::frame_type::close:

            std::cout << "on_control close" << std::endl;

            break;
        case boost::beast::websocket::frame_type::ping:

            std::cout << "on_control ping" << std::endl;

            break;
        case boost::beast::websocket::frame_type::pong:

            std::cout << "on_control pong" << std::endl;

            break;
        }
    }

    void boost_client::uninit()
    {
        //status_
        status_ = STATUS_IDEL;


        //listener_
        //listener_.lock().reset();

        //thread_
        /*if (thread_->joinable())
        {
            thread_->join();
            thread_.reset();
        }*/
    }

    void boost_client::destory()
    {
        uninit();
    }


}

