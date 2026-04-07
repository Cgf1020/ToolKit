#include "tcpclient_impl.h"

#include <iostream>

// 平台相关的头文件和类型定义
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "Ws2_32.lib")

    // 在实现内部使用 int 作为 socket 句柄，使用 -1 作为无效值
    constexpr int INVALID_SOCKET_INT = -1;
    inline bool is_invalid_socket_int(int s) { return s == INVALID_SOCKET_INT; }

    #define CLOSE_SOCKET_INT(s) closesocket(static_cast<SOCKET>(s))
    #define GET_LAST_ERROR WSAGetLastError()
    #define IS_INVALID_SOCKET(s) (is_invalid_socket_int(s))
#else
    #include <sys/select.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>

    constexpr int INVALID_SOCKET_INT = -1;
    #define CLOSE_SOCKET_INT ::close
    #define GET_LAST_ERROR errno
    #define IS_INVALID_SOCKET(s) ((s) < 0)
#endif

#define CONNECT_STATUS_FAILE(reason) \
    if(auto ptr = observer_.lock()) \
    { \
        ptr->OnConnectStatus(ConnectStatus::DisConnect, connect_host_, connect_port_, reason); \
    }

namespace itflee 
{  

    TcpClientImpl::TcpClientImpl()
        : m_socket(INVALID_SOCKET_INT)
    {
        // 初始化服务器地址结构
        m_serverAddr = new sockaddr_in();
        std::memset(m_serverAddr, 0, sizeof(sockaddr_in));
        m_serverAddr->sin_family = AF_INET;

#ifdef _WIN32
        // 初始化 WinSock
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            std::cerr << "WSAStartup failed: " << result << std::endl;
        }
#endif
    }

    TcpClientImpl::~TcpClientImpl()
    {
        DisConnect();
        delete m_serverAddr;
        m_serverAddr = nullptr;
#ifdef _WIN32
        WSACleanup();
#endif
    }

    bool TcpClientImpl::Connect(const std::string &host, int port, std::shared_ptr<TcpClientObserver> observer, bool auto_reconnect)
    {
        State current = state_.load();
        if (current != State::Idle) {
            return false;
        } 

        observer_ = observer;
        connect_host_ = host;
        connect_port_ = port;
        auto_reconnect_ = auto_reconnect;
        reconnect_attempts_ = 0;
        should_stop_connect_ = false;

        // Execute connection in independent thread to avoid blocking
        connectThread_ = std::make_unique<std::thread>(&TcpClientImpl::connectThreadFunc, this);
        return true;
    }

    std::optional<std::string> TcpClientImpl::doConnect()
    {
        std::string reason = "";    
        do{
            // In Windows, ::socket returns SOCKET, here convert to int for storage
            m_socket = static_cast<int>(::socket(AF_INET, SOCK_STREAM, 0));
            if (IS_INVALID_SOCKET(m_socket)) {
                reason = "create socket failed";
                break;
            }

            // Set receive timeout to 100ms
#ifdef _WIN32
            int timeout_ms = 100; // milliseconds
            if (setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms)) == SOCKET_ERROR) {
                std::cerr << "Failed to set recv timeout" << std::endl;
                reason = "Failed to set recv timeout";
                break;
            }
#else
            struct timeval timeout;
            timeout.tv_sec = 0;         // seconds
            timeout.tv_usec = 100000;   // microseconds
            if (setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
                std::cerr << "Failed to set recv timeout" << std::endl;
                reason = "Failed to set recv timeout";
                break;
            }
#endif

            m_serverAddr->sin_port = htons(static_cast<uint16_t>(connect_port_));
            if (inet_pton(AF_INET, connect_host_.c_str(), &m_serverAddr->sin_addr) <= 0) {
                reason = "Invalid address: " + connect_host_;
                break ;
            }

            // Set socket to non-blocking mode, to set connection timeout
#ifdef _WIN32
            u_long mode = 1;
            if (ioctlsocket(m_socket, FIONBIO, &mode) != 0) {
                reason = "Failed to set non-blocking mode";
                break;
            }
#else
            int flags = fcntl(m_socket, F_GETFL, 0);
            if (flags < 0 || fcntl(m_socket, F_SETFL, flags | O_NONBLOCK) < 0) {
                reason = "Failed to set non-blocking mode";
                break;
            }
#endif

            // Try to connect (non-blocking)
            int connectResult = ::connect(m_socket, reinterpret_cast<struct sockaddr*>(m_serverAddr), sizeof(*m_serverAddr));
#ifdef _WIN32
            if (connectResult == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK && WSAGetLastError() != WSAEINPROGRESS) {
                reason = "connection failed";
                break;
            }
#else
            if (connectResult < 0 && errno != EINPROGRESS) {
                reason = "connection failed";
                break;
            }
#endif

            // Use select to wait for connection to complete, set 500ms timeout (non-blocking connection)
            fd_set writefds;
            FD_ZERO(&writefds);
            FD_SET(m_socket, &writefds);
            struct timeval connectTimeout;
            connectTimeout.tv_sec = 0;
            connectTimeout.tv_usec = 500000;  // 500ms

            int selectResult = select(static_cast<int>(m_socket) + 1, nullptr, &writefds, nullptr, &connectTimeout);
            if (selectResult <= 0) {
                reason = "connection timeout";
                break;
            }

            // Check if socket is really connected successfully
            int socketError = 0;
            socklen_t len = sizeof(socketError);
            if (getsockopt(m_socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&socketError), &len) < 0 || socketError != 0) {
                reason = "connection failed";
                break;
            }

            // Restore blocking mode
#ifdef _WIN32
            mode = 0;
            if (ioctlsocket(m_socket, FIONBIO, &mode) != 0) {
                reason = "Failed to restore blocking mode";
                break;
            }
#else
            flags = fcntl(m_socket, F_GETFL, 0);
            if (flags < 0 || fcntl(m_socket, F_SETFL, flags & ~O_NONBLOCK) < 0) {
                reason = "Failed to restore blocking mode";
                break;
            }
#endif
            // Connection successful, get IP address
            // char ipStr[INET_ADDRSTRLEN] = {0};
            // if (inet_ntop(AF_INET, &m_serverAddr.sin_addr, ipStr, INET_ADDRSTRLEN) != nullptr) {
            //     connect_ip_ = std::string(ipStr);
            // } else {
            //     connect_ip_ = connect_host_;  // If failed to get, use host as IP
            // }
            return std::nullopt;
        }while(0);

        // Connection failed, clean up resources
        uninit();     
        // Connection failed, return false, reconnection will be handled by connectThreadFunc
        return reason;
    }

    bool TcpClientImpl::DisConnect()
    {
        State expected = State::Connected;
        if (!state_.compare_exchange_strong(expected, State::Disconnecting)) {
            // If not in Connected state, try to set to Disconnecting
            State current = state_.load();
            if (current != State::Disconnecting && current != State::Idle) {
                state_.store(State::Disconnecting);
            }
        }

        auto_reconnect_ = false;
        should_stop_connect_ = true;
        should_stop_receive_ = true;
        
        // Notify condition variable to interrupt reconnect delay
        reconnect_cv_.notify_all();  
        // Wait for connection thread to finish
        if (connectThread_ && connectThread_->joinable()) {
            connectThread_->join();
            connectThread_.reset();
        }

        // Wait for receive thread to finish
        if (receiveThread_ && receiveThread_->joinable()) {
            std::thread::id currentThreadId = std::this_thread::get_id();
            std::thread::id receiveThreadId = receiveThread_->get_id();

            if (currentThreadId != receiveThreadId) {
                receiveThread_->join();
            } else {
                receiveThread_.release();  // Release ownership, avoid duplicate join when destructing
            }
            receiveThread_.reset();
        }

        // Clean up resources
        uninit();
		return true;
    }

    void TcpClientImpl::StopAutoReconnect()
    {
        auto_reconnect_ = false;
        // Notify condition variable to interrupt reconnect delay
        reconnect_cv_.notify_all();
    }

    bool TcpClientImpl::Send(const std::string &data)
    {
        return Send(data.c_str(), data.length());
    }

    bool TcpClientImpl::Send(const char *data, size_t length)
    {
        if (state_.load() != State::Connected || IS_INVALID_SOCKET(m_socket)) {
            if (auto obs = observer_.lock()) {
                obs->OnSendResult(false, connect_host_, connect_port_, "not connected");
            }
            return false;
        }

        size_t totalSent = 0;

        // 阻塞模式下循环发送，直到全部发送完毕或遇到不可恢复错误
        while (totalSent < length) {
#ifdef _WIN32
            int sent = ::send(
                m_socket,
                data + totalSent,
                static_cast<int>(length - totalSent),
                0);
#else
            ssize_t sent = ::send(
                m_socket,
                data + totalSent,
                length - totalSent,
                0);
#endif
            if (sent > 0) {
                totalSent += static_cast<size_t>(sent);
                continue;
            }

            // sent <= 0: 处理错误
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK || err == WSAEINTR) {
                // 暂时不可写，稍作休眠后重试，避免 busy-loop
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                // 暂时不可写 / 被信号打断，短暂休眠后重试
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
#endif
            // 真正发送错误：回调失败并断开连接
            if (auto obs = observer_.lock()) {
                obs->OnSendResult(false, connect_host_, connect_port_, "send error");
            }
            DisConnect();
            return false;
        }

        // 全部发送成功
        if (auto obs = observer_.lock()) {
            obs->OnSendResult(true, connect_host_, connect_port_, "");
        }
        return true;
    }

    bool TcpClientImpl::IsConnected() const noexcept
    {
        return state_.load() == State::Connected;
    }

    void TcpClientImpl::receiveDataFunc()
    {
        const size_t bufferSize = 4096;  // Buffer size
        char buffer[bufferSize];

        while (!should_stop_receive_ && state_.load() == State::Connected) {
            std::memset(buffer, 0, bufferSize);
#ifdef _WIN32
            int bytesRead = ::recv(m_socket, buffer, static_cast<int>(bufferSize - 1), 0);  // Leave 1 byte to avoid overflow
#else
            ssize_t bytesRead = ::recv(m_socket, buffer, bufferSize - 1, 0);  // Leave 1 byte to avoid overflow
#endif

            if (bytesRead > 0) {
                if (auto observer = observer_.lock()) {
                    std::string recedata(buffer, static_cast<unsigned long>(bytesRead));
                    observer->OnReceiveData(recedata, connect_host_, connect_port_);
                }
            }
            else if (bytesRead == 0) {
                // Server actively disconnected (recv returns 0 means connection closed normally)
                state_.store(State::Idle);
                CONNECT_STATUS_FAILE("Server disconnected normally");
                
                // Clean up connection resources
                uninit();
                break;  // Exit receive loop
            }
            else {
                // Handle error cases (bytesRead < 0)
#ifdef _WIN32
                int err = WSAGetLastError();
                // Note:
                // - WSAEWOULDBLOCK / WSAEINTR: no data or interrupted, can retry
                // - WSAETIMEDOUT: recv timeout when SO_RCVTIMEO is set, should NOT be treated as fatal error
                if (err == WSAEWOULDBLOCK || err == WSAEINTR || err == WSAETIMEDOUT) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
#else
                // 非 Windows：暂时无数据 / 被信号打断，稍作休眠后重试，避免 busy-loop
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
#endif
                // Real receive error occurred
                state_.store(State::Idle);
                CONNECT_STATUS_FAILE("Receive error");
                
                // Clean up connection resources
                uninit();
                break;  // Exit receive loop
            }
        }
    }

    void TcpClientImpl::connectThreadFunc()
    {
        static bool is_first_connection = true;
        // At least try to connect once; if auto_reconnect_ is true, reconnect in loop after failure
        do {       
            // Connection failed, wait for a period of time and then reconnect
            // Use condition variable for interruptible wait
            std::unique_lock<std::mutex> lock(reconnect_mutex_);
            reconnect_cv_.wait_for(lock, std::chrono::seconds(reconnect_delay_), [this]() {
                //std::cout << "Waiting for connection..." << should_stop_connect_ << ", " << is_first_connection << std::endl;
                return should_stop_connect_.load() || is_first_connection;
            });
            lock.unlock();

            if (should_stop_connect_) {
                break;
            }

            // Try to set state to Connecting
            State expected = State::Idle;
            if (!state_.compare_exchange_strong(expected, State::Connecting)) {
                continue;
            }

            // 1. It is the first connection
            // 2. It is not the first connection and auto-reconnect is enabled
            if(is_first_connection)
            {
                is_first_connection = false;
            }
            if (!is_first_connection && !auto_reconnect_) {
                continue;
            }

            auto reason = doConnect();
            if (!reason.has_value()) {    
                // Connection successful, update state
                state_.store(State::Connected);
                if (auto ptr = observer_.lock()) {
                    ptr->OnConnectStatus(ConnectStatus::Connecting, connect_host_, connect_port_);
                }

                // Clean up old receive thread if it exists (should not happen, but be safe)
                should_stop_receive_ = true;
                if (receiveThread_) {
                    if (receiveThread_->joinable()) {
                        // This should not happen in normal flow, but handle it safely
                        std::cerr << "Warning: Old receive thread still exists, waiting for it to finish" << std::endl;
                        receiveThread_->join();
                    }
                    receiveThread_.reset();
                }

                // Start data receive thread
                should_stop_receive_ = false;
                receiveThread_ = std::make_unique<std::thread>(&TcpClientImpl::receiveDataFunc, this);

                continue;
            }

            // fail state_ set Idle
            state_.store(State::Idle);
            CONNECT_STATUS_FAILE(reason.value());

            // Check reconnect attempts limit
            // When max_reconnect_attempts_ < 0, it means infinite retries
            int attempts = reconnect_attempts_.fetch_add(1) + 1;
            if (max_reconnect_attempts_ >= 0 && attempts >= max_reconnect_attempts_) {
                state_.store(State::Idle);
                if (auto ptr = observer_.lock()) {
                    ptr->OnConnectStatus(ConnectStatus::DisConnect, connect_host_, connect_port_, "Max reconnect attempts reached");
                }
                break;
            }

        } while (!should_stop_connect_);
        
        state_.store(State::Idle);
    }

    void TcpClientImpl::uninit()
    {
        // Close socket
        if (!IS_INVALID_SOCKET(m_socket)) {
            // Graceful shutdown: first shutdown, then close socket
#ifdef _WIN32
            ::shutdown(static_cast<SOCKET>(m_socket), SD_BOTH);
#else
            ::shutdown(m_socket, SHUT_RDWR);
#endif
            CLOSE_SOCKET_INT(m_socket);
            m_socket = INVALID_SOCKET_INT;
        }

        // Reset state and IP address (keep host for reconnection)
        state_.store(State::Idle);
        connect_ip_.clear();
    }


}