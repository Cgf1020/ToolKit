#ifndef SOCKETCONNECTION_H_
#define SOCKETCONNECTION_H_

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "define.h"

namespace itflee {
	using SocketCallback = std::function<void(const std::string& json)>;

	class SocketConnectEnum
	{
	public:
		// socket status codes
		enum class StatusVal
		{
			IDLE = 0,
			CONNECT,
			DISCONNECT,	// connection closed
		};

		// socket error codes
		enum class ErrorVal
		{
			NOTCONNECT = 0,	// not connected
			CONNECTFAILED,	// connect failed
			URLERROR,		// invalid url format
			CONNECTED,		// connect success
		};

	};

	struct SocketHandler {
		SocketHandler() = default;
		/*
		* @trans (std::string) ex: "trans"
		* @cb (std::shared_ptr<SocketCallback>)
		*/
		SocketHandler(std::string trans, std::shared_ptr<SocketCallback> cb)
			: transaction(trans)
			, callback(cb) {
		}

		/*
		* @return (bool) true if valid, false otherwise
		*/
		bool valid() {
			return !transaction.empty() && nullptr != callback;
		}
		std::string transaction;
		std::shared_ptr<SocketCallback> callback;
	};

	// message listener
	class SocketConnectListener
	{
	public:
		virtual ~SocketConnectListener() {}
		/**
		* The connection to room is ready.
		*/
		virtual void onConnectConnected() = 0;

		/**
		* The connection to room is lost or disconnected.
		*/
		virtual void onConnectDisconnected(int err, const std::string& reason = "") = 0;

		//virtual void onConnectConnectionLost() = 0;

		virtual void onConnectNotification(const std::string& msg) = 0;
	};

	// socket interface
	class SocketConnectInterface
	{
	public:
		virtual ~SocketConnectInterface() {}

		/*
		* @url (std::string) ex: "ws://192.168.0.1:12345"
		* @lister (std::shared_ptr<SocketConnectListener>)
		* @auto_reconnect (bool) ex: true, if true, the socket will reconnect when the connection is lost
		* @subprotocol (std::string) ex: "subprotocol"
		* @return (int) = 0 success 
		*/
		virtual int connect(std::string const& url, 
			std::shared_ptr<SocketConnectListener> lister, 
			bool auto_reconnect = true, 
			const std::string& subprotocol = "") = 0;

		/*
		* @return (int) = 0 success 
		*/
		virtual int close() = 0;

		/*
		* @data (std::string) ex: "data"
		* @handler (std::shared_ptr<SocketHandler>)
		*/
		virtual void sendText(const std::string& data, std::shared_ptr<SocketHandler> handler) = 0;

		/*
		* @data (std::vector<uint8_t>) ex: "data"
		* @handler (std::shared_ptr<SocketHandler>)
		*/
		virtual void sendBinary(const std::vector<uint8_t>& data, std::shared_ptr<SocketHandler> handler) = 0;
	};

	class SocketConnectFactory
	{
	public:
		ITFLEEEXPORT static std::shared_ptr<SocketConnectInterface> Create();

	private:
		~SocketConnectFactory() {}
	};

}
#endif //SOCKETCONNECTION_H_