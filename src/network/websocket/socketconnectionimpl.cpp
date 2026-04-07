#include "socketconnectionimpl.h"
#include <chrono>




namespace itflee
{
	std::shared_ptr<SocketConnectInterface> SocketConnectImpl::Create()
	{
		return std::static_pointer_cast<SocketConnectInterface>(std::make_shared<SocketConnectImpl>());
	}


	SocketConnectImpl::SocketConnectImpl() noexcept
		: socket_client_(SocketClient::getSocketClient())
	{}

	SocketConnectImpl::~SocketConnectImpl()
	{		
	}

	int SocketConnectImpl::connect(std::string const& uri, 
		std::shared_ptr<SocketConnectListener> lister, 
		bool auto_reconnect, 
		const std::string& subprotocol) noexcept
	{
		do
		{
			if (statu_.load(std::memory_order_acquire) == SocketConnectEnum::StatusVal::CONNECT ||
				connecting_.load(std::memory_order_acquire))
			{
				// already connected or connecting
				return -2;
			}

			lister_ = lister;
			last_uri_ = uri;
			last_subprotocol_ = subprotocol;
			auto_reconnect_.store(auto_reconnect, std::memory_order_release);
			connecting_.store(true, std::memory_order_release);
			if (socket_client_.get())
			{
				socket_client_->setAutoReconnect(auto_reconnect_.load(std::memory_order_acquire));
				int res = socket_client_->connect(uri, shared_from_this(), subprotocol);
				return res;
			}

		} while (0);
		return -1;	// socket connect failed (no socket_client_ or other error)	
	}

	int SocketConnectImpl::close() noexcept
	{
		auto_reconnect_.store(false, std::memory_order_release);
		if (socket_client_.get()) {
			socket_client_->setAutoReconnect(false);
		}
		if (socket_client_.get()) {
			socket_client_->close();
		}

		{
			std::lock_guard<std::mutex> locker(_callbackMutex);
			_callbacksMap.clear();
		}
		statu_.store(SocketConnectEnum::StatusVal::DISCONNECT, std::memory_order_release);

		return 0;
	}

	void SocketConnectImpl::sendText(const std::string& data, std::shared_ptr<SocketHandler> handler)
	{
		if (socket_client_.get())
			socket_client_->sendText(data);

		if (handler->valid()) {
			std::lock_guard<std::mutex> locker(_callbackMutex);
			_callbacksMap[handler->transaction] = handler->callback;
		}
	}

	void SocketConnectImpl::sendBinary(const std::vector<uint8_t>& data, std::shared_ptr<SocketHandler> handler)
	{
		(void)data;
		(void)handler;
	}

	void SocketConnectImpl::onOpen()
	{
		statu_.store(SocketConnectEnum::StatusVal::CONNECT, std::memory_order_release);
		connecting_.store(false, std::memory_order_release);

		std::cout << "SocketConnect::onOpen" << std::endl;

		if (auto l = lister_.lock())
		{
			l->onConnectConnected();
		}
	}

	void SocketConnectImpl::onFail(int errorCode, const std::string& reason)
	{
		statu_.store(SocketConnectEnum::StatusVal::DISCONNECT, std::memory_order_release);
		connecting_.store(false, std::memory_order_release);

		std::cout << "onFail   reason: " << reason << "  errorCode:" << errorCode << std::endl;

		if (auto l = lister_.lock())
		{
			l->onConnectDisconnected(static_cast<int>(SocketConnectEnum::ErrorVal::CONNECTFAILED), reason);
		}

		{
			std::lock_guard<std::mutex> locker(_callbackMutex);
			_callbacksMap.clear();
		}
	}

	void SocketConnectImpl::onClose(int closeCode, const std::string& reason)
	{
		(void)closeCode;
		statu_.store(SocketConnectEnum::StatusVal::DISCONNECT, std::memory_order_release);
		connecting_.store(false, std::memory_order_release);

		std::cout << "client_no_tls::onClose" << std::endl;

		if (auto l = lister_.lock())
		{
			l->onConnectDisconnected(static_cast<int>(SocketConnectEnum::ErrorVal::CONNECTFAILED), reason);
		}
	
		{
			std::lock_guard<std::mutex> locker(_callbackMutex);
			_callbacksMap.clear();
		}
	}

	bool SocketConnectImpl::onValidate()
	{
		return false;
	}

	void SocketConnectImpl::onTextMessage(const std::string& text)
	{
		std::string data = text;

		std::cout << "SocketConnectImpl::onTextMessage " << text << std::endl;

		//// process message
		//Json::Value val = ProcessMessage(data);

		//if (val == NULL)
		//	return;

		//std::cout << "SocketConnect::onTextMessage: " << val.toStyledString() << std::endl;


		//// message handling is multithreaded, use a thread pool (webrtc thread)
		////1. application event
		//{
		//	std::lock_guard<std::mutex> locker(_callbackMutex);
		//	const std::string& transaction = val["transaction"].asString();
		//	if (_callbacksMap.find(transaction) != _callbacksMap.end())
		//	{
		//		std::shared_ptr<SocketCallback> callback = _callbacksMap[transaction];
		//		if (callback)
		//			(*callback)(data);

		//		_callbacksMap.erase(transaction);
		//		return;
		//	}
		//}
		////2. notify message handling
		//{



		//}
	}

	void SocketConnectImpl::onBinaryMessage(const std::vector<uint8_t>& data)
	{
		(void)data;
	}

	bool SocketConnectImpl::onPing(const std::string& text)
	{
		(void)text;
		return false;
	}

	void SocketConnectImpl::onPong(const std::string& text)
	{
		(void)text;
	}

	void SocketConnectImpl::onPongTimeout(const std::string& text)
	{
		(void)text;
	}

}

