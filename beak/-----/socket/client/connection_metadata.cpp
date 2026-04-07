#include "connection_metadata.h"



namespace itflee
{
	ConnectionMetadata::ConnectionMetadata(int id, websocketpp::connection_hdl hdl, const std::string& url, std::shared_ptr<IConnectionListener> listener)
		: hdl_(hdl)
		, url_(url)
		, listener_(listener)
	{


	}

	websocketpp::connection_hdl ConnectionMetadata::getHdl() const
	{
		return hdl_;
	}

	void ConnectionMetadata::onOpen(client* c, websocketpp::connection_hdl hdl)
	{
		if (listener_)
			listener_->onOpen();
		std::cout << "ConnectionMetadata::onOpen" << std::endl;
	}

	void ConnectionMetadata::onFail(client* c, websocketpp::connection_hdl hdl)
	{
		client::connection_ptr con = c->get_con_from_hdl(hdl);
		std::string _server = con->get_response_header("Server");
		std::string _errorReason = con->get_ec().message();
		int errorCode = con->get_ec().value();


		if (listener_)
			listener_->onFail(1, "");
		std::cout << "ConnectionMetadata::onFail code = " <<  errorCode << "   reason = " << _errorReason << std::endl;
	}

	void ConnectionMetadata::onClose(client* c, websocketpp::connection_hdl hdl)
	{
		if (listener_)
			listener_->onClose(1, "");
		std::cout << "ConnectionMetadata::onClose" << std::endl;
	}

	void ConnectionMetadata::onMessage(client* c, websocketpp::connection_hdl, client::message_ptr msg)
	{
		if (listener_)
			listener_->onTextMessage("");
		std::cout << "ConnectionMetadata::onMessage" << std::endl;
	}

}
