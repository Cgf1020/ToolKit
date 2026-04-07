#ifndef SOCKET_CLIENT_H_
#define SOCKET_CLIENT_H_

#include <iostream>
#include <string>
#include <memory>


namespace itflee {


	class SocketClientListener {
	public:
		virtual ~SocketClientListener() {}
	
		virtual void onOpen() = 0;
	
		virtual void onFail(int errorCode, const std::string& reason) = 0;
	
		virtual void onClose(int closeCode, const std::string& reason) = 0;
	
		virtual bool onValidate() = 0;
	
		virtual void onTextMessage(const std::string& text) = 0;
	
		virtual void onBinaryMessage(const std::vector<uint8_t>& data) = 0;
	
		virtual bool onPing(const std::string& text) = 0;
	
		virtual void onPong(const std::string& text) = 0;
	
		virtual void onPongTimeout(const std::string& text) = 0;
	};

	class SocketClient
	{
	public:
		/*
		* @return (std::shared_ptr<SocketClient>)
		*/
		static std::shared_ptr<SocketClient> getSocketClient();

		/**/
		virtual ~SocketClient() {}

		/*
		* @url (std::string) ex: "ws://192.168.0.1:12345"
		* @listener
		* @subprotocol
		* @return (int) = 0 success 
		*/
		virtual int connect(std::string const& uri, std::shared_ptr<SocketClientListener> listener, const std::string& subprotocol = "") = 0;

		/*
		* @return (int) = 0 success 
		*/
		virtual int close() = 0;

		/*
		* @data (std::string) ex: "data"
		* @return (int) = 0 success 
		*/
		virtual int sendText(const std::string& data) = 0;	
	};

}

#endif //SOCKET_CLIENT_H_