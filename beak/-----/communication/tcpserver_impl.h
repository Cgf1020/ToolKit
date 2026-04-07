#ifndef TCPSERVER_IMPL_H
#define TCPSERVER_IMPL_H

#ifdef  _WIN32


#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <unordered_set>
#include <unordered_map>
#include <mutex>

#include "communication/communication.h"

namespace itflee {

    // Concrete TCP server implementation based on abstract interface TcpServer
    class TcpServerImpl : public TcpServer
    {
    public:
        explicit TcpServerImpl(int port);
        ~TcpServerImpl() override;

        // Start/Stop listening
        bool StartListen() override;
        void StopListen() override;

        // Set observer
        void SetObserver(std::shared_ptr<TcpServerObserver> observer) override;

        // Broadcast/Send messages
        void BroadcastMessage(const std::vector<unsigned char>& message) override;
        bool SendToClient(int clientFd, const std::vector<unsigned char>& message) override;
        bool SendToClient(int clientFd, const std::string& message) override;

    private:
        // Loop for accepting new connections
        void acceptLoop();

        // Handle a single client
        void handleClient(int clientFd, std::string clientip, int clientport);

        // Close client connection
        void closeClientConnection(int clientFd);

    private:
        int port_;                  // Listening port
        int serverFd_;              // Server socket
        std::atomic<bool> running_; // Running status

        std::unique_ptr<std::thread> acceptThread_;     // Accept thread for receiving new connections
        std::unordered_set<int> activeClients_;         // Active client list
        std::unordered_map<int, std::unique_ptr<std::thread>> client_threads_;  // Client handler thread mapping

        std::mutex clientsMutex_;                       // Client list mutex for protecting active client list and thread mapping

        std::weak_ptr<TcpServerObserver> observer_;      // Observer for receiving client data
    };

} // namespace itflee

#endif // TCPSERVER_IMPL_H

