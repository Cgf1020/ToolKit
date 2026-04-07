#ifndef CLIENT_NO_TLS_H_
#define CLIENT_NO_TLS_H_



#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/common/thread.hpp>
#include <websocketpp/common/memory.hpp>

#include "utils/define.h"
#include "../socket_client.h"


typedef websocketpp::client<websocketpp::config::asio_client> client;


namespace itflee {

    class client_no_tls : public SocketClient
    {
    public:
        client_no_tls();
        ~client_no_tls();

    public: 
        int connect(std::string const& uri, std::shared_ptr<ISocketClientListener> listener, const std::string& subprotocol = "") override;

        int close() override;

        int sendText(const std::string& data) override;

    private:
        void onOpen(client* c, websocketpp::connection_hdl hdl);

        void onFail(client* c, websocketpp::connection_hdl hdl);

        void onClose(client* c, websocketpp::connection_hdl hdl);

        void onMessage(client* c, websocketpp::connection_hdl, client::message_ptr msg);

        void uninit();

        void destory();


    private:    //
        client client_;

        client::connection_ptr con_;

        websocketpp::lib::shared_ptr<websocketpp::lib::thread> thread_;

        //std::map<int, std::shared_ptr<ConnectionMetadata>> connectionMap_;

        std::weak_ptr<ISocketClientListener> listener_;

    };

}


#endif // CLIENT_NO_TLS_H_
