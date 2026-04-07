#include "communication/communication.h"
#include "tcpclient_impl.h"
#include "tcpserver_impl.h"

namespace itflee {

    std::shared_ptr<TcpClient> TcpClient::Create()
    {
        return std::make_shared<TcpClientImpl>();
    }

    std::shared_ptr<TcpServer> TcpServer::Create(int port)
    {
        return std::make_shared<TcpServerImpl>(port);
    }

} // namespace itflee


