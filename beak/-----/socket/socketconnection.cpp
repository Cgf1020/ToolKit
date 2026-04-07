#include "ToolKit/socketconnection.h"

#include "socketconnectionimpl.h"


namespace itflee
{
	std::shared_ptr<SocketConnectInterface> SocketConnectFactory::Create()
	{
		return std::shared_ptr<SocketConnectImpl>();
	}

}



