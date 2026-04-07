#ifndef CONNECTION_METADATA_H_
#define CONNECTION_METADATA_H_


#include <iostream>
#include <string>

#include <websocketpp/config/asio_no_tls_client.hpp>
//#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>



#include "utils/include/define.h"
#include "websocket/include/connection_listener.h"

typedef websocketpp::client<websocketpp::config::asio_client> client;



namespace itflee
{
	class ConnectionMetadata
	{
	public:

		ConnectionMetadata(int id, websocketpp::connection_hdl hdl, const std::string& url, std::shared_ptr<SocketConnectionListener> listener);

	public:

		websocketpp::connection_hdl getHdl() const;

	public:
		/*
		* @c (client*)
		* @hdl (websocketpp::connection_hdl)
		*/
		void onOpen(client* c, websocketpp::connection_hdl hdl);

		/*
		* @c (client*)
		* @hdl (websocketpp::connection_hdl)
		*/
		void onFail(client* c, websocketpp::connection_hdl hdl);

		/*
		* @c (client*)
		* @hdl (websocketpp::connection_hdl)
		*/
		void onClose(client* c, websocketpp::connection_hdl hdl);

		/*
		* @c (client*)
		* @hdl (websocketpp::connection_hdl)
		* @msg (client::message_ptr)
		*/
		void onMessage(client* c, websocketpp::connection_hdl, client::message_ptr msg);

	private:
		std::string url_;

	private:	//private members

		websocketpp::connection_hdl hdl_;

		std::shared_ptr<SocketConnectionListener> listener_;


	};
}





#endif	//CONNECTION_METADATA_H_