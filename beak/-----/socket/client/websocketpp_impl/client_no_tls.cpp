#include "client_no_tls.h"


using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

namespace itflee {

//#ifdef USEWEBSOCKETPP
//    std::shared_ptr<SocketClient> SocketClient::getSocketClient()
//    {
//        std::shared_ptr<SocketClient> socket(new client_no_tls());
//        return socket;
//    }
//
//#endif // USEWEBSOCKETPP

    
    client_no_tls::client_no_tls()
    {

        //1. clear access channels and error channels

        client_.clear_access_channels(websocketpp::log::alevel::all);
        client_.clear_error_channels(websocketpp::log::elevel::all);

        //2. initialize the client's request
        client_.init_asio();
        client_.start_perpetual();

        //3. create a thread to handle the client's request
        thread_.reset(new websocketpp::lib::thread(&client::run, &client_));
    }

    client_no_tls::~client_no_tls()
    {
        if (thread_->joinable())
        {
            thread_->join();
            thread_.reset();
        }
    }

    int client_no_tls::connect(std::string const& uri, std::shared_ptr<ISocketClientListener> listener, const std::string& subprotocol)
    {
        listener_ = listener;

        //1. 
        websocketpp::lib::error_code ec;
        con_ = client_.get_connection(uri, ec);

        if (ec) {
            //ELOG("> Connect initialization error: {}", ec.message());
            std::cout << "Connect initialization error: " << ec.message() << std::endl;
            return -1;
        }


        //2. 
        if (!subprotocol.empty()) {
            con_->add_subprotocol(subprotocol, ec);
            if (ec) {
                //ELOG("> add subprotocol error: {}", ec.message());
                return -1;
            }
        }


        //int newId = connectionid_++;
        //std::shared_ptr<ConnectionMetadata> metadataPtr = websocketpp::lib::make_shared<ConnectionMetadata>(newId, con->get_handle(), uri, listener);
        //connectionMap_[newId] = metadataPtr;


        //3. Register our message handler
        con_->set_open_handler(websocketpp::lib::bind(&client_no_tls::onOpen, this, &client_, websocketpp::lib::placeholders::_1));

        con_->set_fail_handler(websocketpp::lib::bind(&client_no_tls::onFail, this, &client_, websocketpp::lib::placeholders::_1));

        con_->set_close_handler(websocketpp::lib::bind(&client_no_tls::onClose, this, &client_, websocketpp::lib::placeholders::_1));

        con_->set_message_handler(websocketpp::lib::bind(&client_no_tls::onMessage, this, &client_, websocketpp::lib::placeholders::_1, websocketpp::lib::placeholders::_2));


        //con->set_ping_handler(websocketpp::lib::bind(
        //    &ConnectionMetadata::onPing,
        //    metadataPtr,
        //    &_endpoint,
        //    websocketpp::lib::placeholders::_1,
        //    websocketpp::lib::placeholders::_2
        //));

        //con->set_pong_handler(websocketpp::lib::bind(
        //    &ConnectionMetadata::onPong,
        //    metadataPtr,
        //    &_endpoint,
        //    websocketpp::lib::placeholders::_1,
        //    websocketpp::lib::placeholders::_2
        //));

        //con->set_pong_timeout_handler(websocketpp::lib::bind(
        //    &ConnectionMetadata::onPongTimeout,
        //    metadataPtr,
        //    &_endpoint,
        //    websocketpp::lib::placeholders::_1,
        //    websocketpp::lib::placeholders::_2
        //));


        //4. connect and notify the client's request
        client_.connect(con_);

        return 0;
    }

    int client_no_tls::close()
    {
        websocketpp::lib::error_code ec;

        client_.close(con_->get_handle(), websocketpp::close::status::normal, "by user", ec);
 
        if (ec) 
        {
            //std::cout << "Error close " << ec.message() << std::endl;
            return -1;
        }
        return 0;
    }

    int client_no_tls::sendText(const std::string& data)
    {
        websocketpp::lib::error_code ec;

        client_.send(con_->get_handle(), data, websocketpp::frame::opcode::text, ec);

        if (ec) {
            std::cout << "Error sending text message " << ec.message() << std::endl;
            return -1;
        }

        return 0;
    }

    void client_no_tls::onOpen(client* c, websocketpp::connection_hdl hdl)
    {
        //client::connection_ptr con = c->get_con_from_hdl(hdl);
        //listener_ = con->get_response_header("Server");
        if (auto listener = listener_.lock()) {
            listener->onOpen();
        }

    }

    void client_no_tls::onFail(client* c, websocketpp::connection_hdl hdl)
    {
        std::string _errorReason = con_->get_ec().message();

        int errorCode = con_->get_ec().value();

        if (auto listener = listener_.lock()) {
            listener->onFail(errorCode, _errorReason);
        }
    }

    void client_no_tls::onClose(client* c, websocketpp::connection_hdl hdl)
    {
        std::string _errorReason = con_->get_remote_close_reason();

        int closeCode = con_->get_remote_close_code();

        if (auto listener = listener_.lock()) {
            listener->onClose(closeCode, _errorReason);
        }
    }

    void client_no_tls::onMessage(client* c, websocketpp::connection_hdl, client::message_ptr msg)
    {
        if (auto listener = listener_.lock()) {
            if (msg->get_opcode() == websocketpp::frame::opcode::text) {

                listener->onTextMessage(msg->get_payload());
            }
            else if (msg->get_opcode() == websocketpp::frame::opcode::binary) {

                std::vector<uint8_t> data(msg->get_payload().begin(), msg->get_payload().end());
                listener->onBinaryMessage(data);
            }
        }
    }

    void client_no_tls::uninit()
    {

    }

    void client_no_tls::destory()
    {
    }

}

