#include "socket_client.h"

#include "boost_impl/boost_socket.h"

namespace itflee {

	std::shared_ptr<SocketClient> SocketClient::getSocketClient()
	{
		return std::make_shared<boost_client>();
	}

}
