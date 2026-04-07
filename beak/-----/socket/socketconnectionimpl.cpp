#include "socketconnectionimpl.h"

//#define CHECK_CONNECT_VAL \
//if (!initialized_) { \
//	TKLOG_E("[rtc-sdk] TKRoomManager instance has not been initialized. function: " << __func__); \
//	return; \
//}



namespace itflee
{
	std::shared_ptr<SocketConnectInterface> SocketConnectImpl::Create()
	{
		return std::static_pointer_cast<SocketConnectInterface>(std::make_shared<SocketConnectImpl>());
	}


	SocketConnectImpl::SocketConnectImpl() noexcept
		: socket_client_(SocketClient::getSocketClient())
		, statu_(SocketConnectEnum::StatusVal::IDLE)
	{}

	SocketConnectImpl::~SocketConnectImpl()
	{		
	}

	int SocketConnectImpl::connect(std::string const& uri, std::shared_ptr<SocketConnectListener> lister, const std::string& subprotocol) noexcept
	{
		do
		{
			if (statu_ == SocketConnectEnum::StatusVal::CONNECT)
			{
				//return static_cast<int>(SocketConnectEnum::ErrorVal::CONNECTED);
				break;
			}

			lister_ = lister;
			if (socket_client_.get())
			{
				int res = socket_client_->connect(uri, shared_from_this(), subprotocol);
				//statu = res == 0 ? SocketConnectEnum::StatusVal::CONNECT : SocketConnectEnum::StatusVal::NOLL;
				return 0;
			}

		} while (0);
	
		if (lister_.lock())
		{
			lister_.lock()->onConnectDisconnected(static_cast<int>(SocketConnectEnum::ErrorVal::CONNECTED));
		}
		
		return 1;	//socket连接失败	
	}

	int SocketConnectImpl::close() noexcept
	{
		if (socket_client_.get())
			socket_client_->close();

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

	}

	void SocketConnectImpl::onOpen()
	{
		statu_ = SocketConnectEnum::StatusVal::CONNECT;

		if (lister_.lock())
		{
			lister_.lock()->onConnectConnected();
		}

		std::cout << "SocketConnect::onOpen" << std::endl;
	}

	void SocketConnectImpl::onFail(int errorCode, const std::string& reason)
	{
		statu_ = SocketConnectEnum::StatusVal::DISCONNECT;

		if (lister_.lock())
		{
			lister_.lock()->onConnectDisconnected(static_cast<int>(SocketConnectEnum::ErrorVal::CONNECTFAILED), reason);
		}

		std::cout << "onFail   reason: " << reason << "  errorCode:" << errorCode << std::endl;
	}

	void SocketConnectImpl::onClose(int closeCode, const std::string& reason)
	{
		statu_ = SocketConnectEnum::StatusVal::DISCONNECT;

		if (lister_.lock())
		{
			lister_.lock()->onConnectDisconnected(static_cast<int>(SocketConnectEnum::ErrorVal::CONNECTFAILED), reason);
		}

		std::cout << "client_no_tls::onClose" << std::endl;
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


		//// 处理消息的逻辑是多线程的，使用线程池的方式，线程池去处理消息，使用webrtc线程
		////1. Ӧ���¼�
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
		////2. 通知消息，处理消息
		//{



		//}
	}

	void SocketConnectImpl::onBinaryMessage(const std::vector<uint8_t>& data)
	{
	}

	bool SocketConnectImpl::onPing(const std::string& text)
	{
		return false;
	}

	void SocketConnectImpl::onPong(const std::string& text)
	{
	}

	void SocketConnectImpl::onPongTimeout(const std::string& text)
	{
	}

}

