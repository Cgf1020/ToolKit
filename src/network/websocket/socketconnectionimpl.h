#ifndef WEBSOCKET_CONNECT_H_
#define WEBSOCKET_CONNECT_H_

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <thread>
#include <mutex>
#include <functional>
#include <atomic>

#include "base/websocket/socketconnection.h"
#include "client/socket_client.h"

namespace itflee {

	class SocketConnectImpl :	public SocketConnectInterface,
								public SocketClientListener, 
								public std::enable_shared_from_this<SocketConnectImpl>
	{
	public:		//static function
		static std::shared_ptr<SocketConnectInterface> Create();

	public:		//Constructor
		SocketConnectImpl() noexcept;
		virtual ~SocketConnectImpl();

	public:		//SocketConnectInterface
		int connect(std::string const& uri, 
			std::shared_ptr<SocketConnectListener> lister, 
			bool auto_reconnect = true, 
			const std::string& subprotocol = "") noexcept override;
		int close() noexcept override;
		void sendText(const std::string& data, std::shared_ptr<SocketHandler> handler) override;
		void sendBinary(const std::vector<uint8_t>& data, std::shared_ptr<SocketHandler> handler) override;

	public:		// ISocketClientListener
		void onOpen() override;
		void onFail(int errorCode, const std::string& reason) override;
		void onClose(int closeCode, const std::string& reason) override;
		bool onValidate() override;
		void onTextMessage(const std::string& text) override;
		void onBinaryMessage(const std::vector<uint8_t>& data) override;
		bool onPing(const std::string& text) override;
		void onPong(const std::string& text) override;
		void onPongTimeout(const std::string& text) override;

	private:
		std::shared_ptr<SocketClient> socket_client_;
		std::weak_ptr<SocketConnectListener> lister_;
		std::mutex _callbackMutex;
		std::unordered_map<std::string, std::shared_ptr<SocketCallback>> _callbacksMap;

	private:
		std::atomic<SocketConnectEnum::StatusVal> statu_{ SocketConnectEnum::StatusVal::IDLE };
		std::string last_uri_;
		std::string last_subprotocol_;
		std::atomic<bool> auto_reconnect_{ true };
		std::atomic<bool> connecting_{ false };
	};
}
#endif //WEBSOCKET_CONNECT_H_