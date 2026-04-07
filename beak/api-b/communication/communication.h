#ifndef ITFLEE_COMMUNICATION_H
#define ITFLEE_COMMUNICATION_H

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "define.h"

namespace itflee {
    enum class ConnectStatus {
        Connecting = 0,
        DisConnect
    };

    class ITFLEEEXPORT TcpClientObserver {
    public:
        virtual ~TcpClientObserver() = default;

        // Connection status change callback
        virtual void OnConnectStatus(ConnectStatus status, 
            const std::string& host, 
            const int port, 
            const std::string& reason = "") = 0;

        // Receive data callback
        virtual void OnReceiveData(const std::string& data, 
            const std::string& host, 
            const int port) = 0;

        // Send data result callback
        // success: true if all bytes were sent, false otherwise
        // reason: optional error description when failed
        virtual void OnSendResult(bool success,
            const std::string& host,
            const int port,
            const std::string& reason = "") = 0;
    };

    class ITFLEEEXPORT TcpClient {
    public:
        virtual ~TcpClient() = default;

        // Connect to server (optional auto-reconnect)
        virtual bool Connect(const std::string& host,
                             int port,
                             std::shared_ptr<TcpClientObserver> observer,
                             bool auto_reconnect = true) = 0;

        // Disconnect
        virtual bool DisConnect() = 0;

        // Stop auto-reconnect
        virtual void StopAutoReconnect() = 0;

        // Send data
        virtual bool Send(const std::string& data) = 0;
        virtual bool Send(const char* data, size_t length) = 0;


        // Check if currently connected
        virtual bool IsConnected() const noexcept = 0;

        // Static creation interface (implementation selected internally by library)
        static std::shared_ptr<TcpClient> Create();
    };

    class ITFLEEEXPORT TcpServerObserver {
    public:
        virtual ~TcpServerObserver() = default;

        // Client connection status change callback
        virtual void OnClientConnectStatus(int clientFd, 
            const std::string& clientip,
            const int clientport,
            ConnectStatus status, 
            const std::string& reason = "") = 0;

        // Receive data callback
        virtual void OnReceiveData(int clientFd, 
            const std::string& clientip,
            const int clientport,
            const std::string& data) = 0;
    };

    class ITFLEEEXPORT TcpServer {
    public:
        virtual ~TcpServer() = default;

        // Start listening
        virtual bool StartListen() = 0;

        // Stop listening
        virtual void StopListen() = 0;

        // Set observer (for receiving client connection status and data callbacks)
        virtual void SetObserver(std::shared_ptr<TcpServerObserver> observer) = 0;

        // Broadcast message to all clients
        virtual void BroadcastMessage(const std::vector<unsigned char>& message) = 0;

        // Send message to specified client
        virtual bool SendToClient(int clientFd, const std::vector<unsigned char>& message) = 0;
        virtual bool SendToClient(int clientFd, const std::string& message) = 0;

        // Static creation interface (implementation selected internally by library)
        static std::shared_ptr<TcpServer> Create(int port);
    };

} // namespace itflee

#endif // ITFLEE_COMMUNICATION_H


