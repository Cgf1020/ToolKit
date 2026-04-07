#include "base/websocket/socketconnection.h"

#include "socketconnectionimpl.h"


namespace itflee
{
	std::shared_ptr<SocketConnectInterface> SocketConnectFactory::Create()
	{
		return SocketConnectImpl::Create();
	}

}



