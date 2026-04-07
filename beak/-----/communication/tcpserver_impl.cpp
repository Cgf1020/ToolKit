#include "tcpserver_impl.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "Ws2_32.lib")
#else
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <signal.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <cstring>  // for strerror
#endif

#include <iostream>

namespace itflee {
    
    TcpServerImpl::TcpServerImpl(int port) 
        : port_(port)
        , serverFd_(-1)
        , running_(false)
    {
#ifdef _WIN32
        // Initialize WinSock
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            std::cerr << "WSAStartup failed: " << result << std::endl;
        }
#else
        // Ignore SIGPIPE to prevent process exit when client disconnects
        signal(SIGPIPE, SIG_IGN);
#endif // _WIN32
    }

    TcpServerImpl::~TcpServerImpl()
    {
        StopListen();

#ifdef _WIN32
        WSACleanup();
#endif
    }

    bool TcpServerImpl::StartListen()
    {
        // Create socket
#ifdef _WIN32
        serverFd_ = static_cast<int>(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        if (serverFd_ == -1) {
            std::cerr << "Create socket failed, WSA error: " << WSAGetLastError() << std::endl;
            return false;
        }

        // Set address reuse
        BOOL opt = TRUE;
        if (setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR,
                       reinterpret_cast<const char*>(&opt), sizeof(opt)) == SOCKET_ERROR) {
            std::cerr << "Set socket options failed, WSA error: " << WSAGetLastError() << std::endl;
            ::closesocket(serverFd_);
            serverFd_ = -1;
            return false;
        }

        // Bind port
        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(static_cast<uint16_t>(port_));

        if (bind(serverFd_, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "Bind to port failed, WSA error: " << WSAGetLastError() << std::endl;
            ::closesocket(serverFd_);
            serverFd_ = -1;
            return false;
        }

        // Start listening
        if (listen(serverFd_, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "Listen failed, WSA error: " << WSAGetLastError() << std::endl;
            ::closesocket(serverFd_);
            serverFd_ = -1;
            return false;
        }

        // Set to non-blocking mode
        u_long mode = 1;
        if (ioctlsocket(serverFd_, FIONBIO, &mode) != NO_ERROR) {
            std::cerr << "Set non-blocking mode failed, WSA error: " << WSAGetLastError() << std::endl;
            ::closesocket(serverFd_);
            serverFd_ = -1;
            return false;
        }
#else
        serverFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (serverFd_ < 0) {
            std::cerr << "Create socket failed: " << strerror(errno) << std::endl;
            return false;
        }

        // Set socket options, address reuse
        int opt = 1;
        if (setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            std::cerr << "Set socket options failed: " << strerror(errno) << std::endl;
            ::close(serverFd_);
            serverFd_ = -1;
            return false;
        }

        // Bind port
        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(static_cast<uint16_t>(port_));

        if (bind(serverFd_, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
            std::cerr << "Bind to port failed: " << strerror(errno) << std::endl;
            ::close(serverFd_);
            serverFd_ = -1;
            return false;
        }

        // Start listening
        if (listen(serverFd_, 10) < 0) {
            std::cerr << "Listen failed: " << strerror(errno) << std::endl;
            ::close(serverFd_);
            serverFd_ = -1;
            return false;
        }

        // Set to non-blocking mode
        int flags = fcntl(serverFd_, F_GETFL, 0);
        fcntl(serverFd_, F_SETFL, flags | O_NONBLOCK);
#endif

        std::cout << "TCP server started on port: " << port_ << std::endl;

        // Set running flag and start accept thread
        running_ = true;
        acceptThread_ = std::make_unique<std::thread>(&TcpServerImpl::acceptLoop, this);

        return true;
    }

    void TcpServerImpl::StopListen()
    {
        if (!running_) {
            return;
        }

        // Stop running flag
        running_ = false;

        // Wait for accept thread to finish
        if (acceptThread_ && acceptThread_->joinable()) {
            acceptThread_->join();
            acceptThread_.reset();
        }

        // Wait for all client threads to finish and close connections
        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            
            // Join all client threads that are still joinable
            for (auto it = client_threads_.begin(); it != client_threads_.end();) {
                if (it->second && it->second->joinable()) {
                    try {
                        it->second->join();
                    }
                    catch (const std::exception& e) {
                        std::cerr << "Join client thread failed: " << e.what() << " (FD: " << it->first << ")" << std::endl;
                    }
                }
                // Clean up thread object regardless of whether it's joinable
                it = client_threads_.erase(it);
            }

            // Close all active client connections
            for (int clientFd : activeClients_) {
                closeClientConnection(clientFd);
            }
            activeClients_.clear();
        }

        // Close server socket
        if (serverFd_ >= 0) {
            closeClientConnection(serverFd_);
            serverFd_ = -1;
        }
        std::cout << "TCP server stopped" << std::endl;
    }

    void TcpServerImpl::SetObserver(std::shared_ptr<TcpServerObserver> observer)
    {
        observer_ = observer;
    }

    void TcpServerImpl::BroadcastMessage(const std::vector<unsigned char>& message)
    {
        // Create a copy of active clients to avoid holding lock during send
        std::vector<int> clientsToSend;
        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            if (activeClients_.empty()) {
                std::cout << "No clients connected, broadcast message failed" << std::endl;
                return;
            }
            clientsToSend.assign(activeClients_.begin(), activeClients_.end());
        }

        std::cout << "Broadcast message to " << clientsToSend.size() << " clients: " << message.size() << std::endl;

        // Send message to all clients
        std::vector<int> failedClients;
        for (int clientFd : clientsToSend) {
            // Check if client is still active before sending
            {
                std::lock_guard<std::mutex> lock(clientsMutex_);
                if (activeClients_.find(clientFd) == activeClients_.end()) {
                    continue;  // Client already disconnected
                }
            }
            
            if (!SendToClient(clientFd, message)) {
                failedClients.push_back(clientFd);
            }
        }

        // Remove failed clients
        if (!failedClients.empty()) {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            for (int clientFd : failedClients) {
                // Double-check client is still in active list
                if (activeClients_.find(clientFd) != activeClients_.end()) {
                    std::cerr << "Send message to client failed, close connection (FD: " << clientFd << ")" << std::endl;
                    closeClientConnection(clientFd);
                    activeClients_.erase(clientFd);
                }
            }
        }
    }

    bool TcpServerImpl::SendToClient(int clientFd, const std::vector<unsigned char>& message)
    {
        try {
            if (message.empty()) {
                return true;
            }

            size_t totalSent = 0;
            size_t remaining = message.size();

            while (totalSent < message.size()) {
#ifdef _WIN32
                int sent = ::send(clientFd,
                                  reinterpret_cast<const char*>(message.data() + totalSent),
                                  static_cast<int>(remaining),
                                  0);
                if (sent == SOCKET_ERROR) {
                    int err = WSAGetLastError();
                    if (err == WSAEWOULDBLOCK) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                        continue;
                    }
                    return false;
                }
#else
                ssize_t sent = ::send(clientFd,
                                      reinterpret_cast<const char*>(message.data() + totalSent),
                                      remaining,
                                      0);
                if (sent <= 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // Temporarily not writable in non-blocking mode, retry later
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                        continue;
                    }
                    return false;
                }
#endif           
                totalSent += static_cast<size_t>(sent);
                remaining -= static_cast<size_t>(sent);
            }

            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "Send message to client failed: " << e.what() << " (FD: " << clientFd << ")" << std::endl;
            return false;
        }
    }

    bool TcpServerImpl::SendToClient(int clientFd, const std::string& message)
    {
        std::vector<unsigned char> data(message.begin(), message.end());
        return SendToClient(clientFd, data);
    }

    void TcpServerImpl::acceptLoop()
    {
#ifdef _WIN32
        sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);
        while (running_) {
            try {
                SOCKET clientSocket = ::accept(serverFd_,
                                               reinterpret_cast<sockaddr*>(&clientAddr),
                                               &clientAddrLen);
                if (clientSocket == INVALID_SOCKET) {
                    int err = WSAGetLastError();
                    if (err == WSAEWOULDBLOCK) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        continue;
                    }

                    std::cerr << "Accept new client failed, WSA error: " << err << std::endl;
                    continue;
                }

                int clientFd = static_cast<int>(clientSocket);

                // Set client to non-blocking mode
                u_long mode = 1;
                ioctlsocket(clientSocket, FIONBIO, &mode);

                char clientIP[INET_ADDRSTRLEN] = {0};
                inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
                int clientPort = ntohs(clientAddr.sin_port);

                std::cout << "New client connected: " << clientIP << ":" << clientPort
                          << " (FD: " << clientFd << ")" << std::endl;

                {
                    std::unique_ptr<std::thread> oldThread;
                    {
                        std::lock_guard<std::mutex> lock(clientsMutex_);
                        // Check if this FD was previously used and clean up old thread
                        auto it = client_threads_.find(clientFd);
                        if (it != client_threads_.end() && it->second) {
                            oldThread = std::move(it->second);
                            client_threads_.erase(it);
                            oldThread->detach();
                        }
                        
                        activeClients_.insert(clientFd);
                        // Create and store client handler thread
                        auto clientThread = std::make_unique<std::thread>(&TcpServerImpl::handleClient, this, clientFd, clientIP, clientPort);
                        client_threads_[clientFd] = std::move(clientThread);
                    }
                }
                // Notify observer: client connected
                if (auto obs = observer_.lock()) {
                    obs->OnClientConnectStatus(clientFd, clientIP, clientPort, ConnectStatus::Connecting, "");
                }
            } catch (const std::exception& e) {
                std::cerr << "Handle client failed: " << e.what() << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
#else
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        while (running_) {
            try {
                // Accept new client
                int clientFd = ::accept(serverFd_,
                                        reinterpret_cast<sockaddr*>(&clientAddr),
                                        &clientAddrLen);

                if (clientFd < 0) {
                    // In non-blocking mode, no connection returns EAGAIN/EWOULDBLOCK
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        continue;
                    }

                    std::cerr << "Accept new client failed: " << strerror(errno) << std::endl;
                    continue;
                }

                // Set client socket to non-blocking mode
                int flags = fcntl(clientFd, F_GETFL, 0);
                fcntl(clientFd, F_SETFL, flags | O_NONBLOCK);

                // Get client information
                char clientIP[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
                int clientPort = ntohs(clientAddr.sin_port);

                std::cout << "New client connected: " << clientIP << ":" << clientPort
                          << " (FD: " << clientFd << ")" << std::endl;

                // Start a new thread to handle the client
                {
                    std::unique_ptr<std::thread> oldThread;
                    {
                        std::lock_guard<std::mutex> lock(clientsMutex_);
                        // Check if this FD was previously used and clean up old thread
                        auto it = client_threads_.find(clientFd);
                        if (it != client_threads_.end() && it->second) {
                            oldThread = std::move(it->second);
                            client_threads_.erase(it);
                            oldThread->detach();
                        }
                        
                        activeClients_.insert(clientFd);
                        // Create and store client handler thread
                        auto clientThread = std::make_unique<std::thread>(&TcpServerImpl::handleClient, this, clientFd, clientIP, clientPort);
                        client_threads_[clientFd] = std::move(clientThread);
                    }
                }

                // Notify observer: client connected
                if (auto obs = observer_.lock()) {
                    obs->OnClientConnectStatus(clientFd, clientIP, clientPort, ConnectStatus::Connecting, "");
                }
            }
            catch (const std::exception& e) {
                std::cerr << "Handle client failed: " << e.what() << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
#endif // _WIN32
    }

    void TcpServerImpl::handleClient(int clientFd, std::string clientip, int clientport)
    {
        // Note: On Windows, each thread has only 1MB default stack, cannot put 1MB array on stack, otherwise Stack overflow.
        // Here we allocate buffer on heap to avoid server thread crash due to insufficient stack space.
        const int bufferSize = 1024 * 1024;   // 1MB buffer
        std::vector<char> buffer(bufferSize);

        try {
            while (running_) {
                // Read data from client
                std::fill(buffer.begin(), buffer.end(), 0);
#ifdef _WIN32
                int bytesRead = ::recv(clientFd, buffer.data(), bufferSize - 1, 0);
#else
                int bytesRead = ::recv(clientFd, buffer.data(), bufferSize - 1, 0);
#endif // _WIN32

                if (bytesRead > 0) {
                    // Convert to std::string and notify observer
                    if (auto observer = observer_.lock()) {
                        observer->OnReceiveData(clientFd, clientip, clientport, std::string(buffer.data(), buffer.data() + bytesRead));
                    }
                }
                else if (bytesRead == 0) {
                    // Client disconnected
                    std::cout << "Client disconnected: " << clientip << ":" << clientport 
                              << " (FD: " << clientFd << ")" << std::endl;

                    // Notify observer: client disconnected
                    if (auto obs = observer_.lock()) {
                        obs->OnClientConnectStatus(clientFd, clientip, clientport, ConnectStatus::DisConnect, "client closed");
                    }
                    break;
                }
                else {
#ifdef _WIN32
                    int err = WSAGetLastError();
                    if (err == WSAEWOULDBLOCK) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        continue;
                    }
#else
                    // In non-blocking mode, no data available
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        continue;
                    }
#endif
                    // Real receive error occurred, treat as client disconnect
                    std::cout << "Client recv error, disconnect: " << clientip << ":" << clientport 
                              << " (FD: " << clientFd << ")" << std::endl;
                    if (auto obs = observer_.lock()) {
                        obs->OnClientConnectStatus(clientFd, clientip, clientport, ConnectStatus::DisConnect, "recv error");
                    }
                    break;
                }
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Handle client failed: " << e.what() 
                      << " (" << clientip << ":" << clientport << ", FD: " << clientFd << ")" << std::endl;
        }

        // Close client connection and remove from tracking
        // First remove from active clients to prevent other threads from accessing it
        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            activeClients_.erase(clientFd);
            // Don't remove thread object from client_threads_ here
            // The thread will be cleaned up by acceptLoop when a new connection uses the same FD
            // or by StopListen when the server stops
        }

        // Close socket after removing from active clients to avoid race conditions
        closeClientConnection(clientFd);
        
        // Thread will naturally exit here, don't try to join ourselves (would cause deadlock)
    }

    void TcpServerImpl::closeClientConnection(int clientFd)
    {
        if (clientFd < 0) {
            return;  // Invalid file descriptor
        }

#ifdef _WIN32
        // closesocket doesn't throw exceptions, but may return error
        int result = ::closesocket(clientFd);
        if (result == SOCKET_ERROR) {
            int err = WSAGetLastError();
            std::cerr << "Close client connection failed (Windows), WSA error: " << err
                      << " (FD: " << clientFd << ")" << std::endl;
        }
#else
        // close doesn't throw exceptions, but may return error
        int result = ::close(clientFd);
        if (result < 0) {
            std::cerr << "Close client connection failed: " << strerror(errno)
                      << " (FD: " << clientFd << ")" << std::endl;
        }
#endif // _WIN32
    }


} // namespace itflee