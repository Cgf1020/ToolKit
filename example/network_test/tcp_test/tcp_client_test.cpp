#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include "network/tcp/tcp_connection.h"
#include "network/tcp/tcp_client_interface.h"

namespace {
constexpr int kHeartbeatIntervalMs = 5000;
constexpr int kHeartbeatTimeoutMs = 15000;
const std::string kHeartbeatPayload = "PING\n";
std::mutex g_log_mutex;

void SafeLog(const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    std::cout << msg << std::endl;
}
} // namespace

class TcpClientCb final : public itflee::TcpConnectionCallback {
public:
    void OnConnectionStateChanged(const itflee::TcpConnectionPtr& conn,
                                  itflee::ConnectionState state) override {
        SafeLog("OnConnectionStateChanged: " + itflee::ConnectionStateToString(state));
        if (state == itflee::ConnectionState::kConnected && conn) {
            // int ret = conn->EnableHeartbeat(kHeartbeatIntervalMs, kHeartbeatTimeoutMs, kHeartbeatPayload);
            // std::ostringstream oss;
            // oss << "EnableHeartbeat ret=" << ret
            //     << " interval_ms=" << kHeartbeatIntervalMs
            //     << " timeout_ms=" << kHeartbeatTimeoutMs
            //     << " payload=" << kHeartbeatPayload;
            // SafeLog(oss.str());
        }
    }
    void OnMessage(const itflee::TcpConnectionPtr& conn,
                   const void* data, std::size_t len) override {
        const std::string msg(static_cast<const char*>(data), len);
        if (msg == "PING\n" && conn) {
            conn->Write("PONG\n");
            SafeLog("OnMessage: PING (auto reply PONG)");
            return;
        }
        if (msg == "PONG\n") {
            SafeLog("OnMessage: PONG");
            return;
        }
        
        const int ret = conn ? conn->Write("wait") : -1;
        std::cout << "Send role info, result: " << (ret == 0 ? "true" : "false") << std::endl;
        SafeLog("OnMessage: " + msg);
    }
};

int main() {
    auto client_cb = std::make_shared<TcpClientCb>();
    auto client = itflee::TcpClientInterface::Create(client_cb);
    constexpr int kDefaultPort = 23456;
    constexpr int kReconnectIntervalMs = 3000;
    constexpr int kReconnectMaxRetries = -1; // -1 表示无限重连

    SafeLog("tcp_client_test interactive mode");
    SafeLog("请先输入 connect <host> [port] 发起连接（port 省略时为 " +
            std::to_string(kDefaultPort) + "）");
    SafeLog("commands:");
    SafeLog("  connect <host> [port]");
    SafeLog("  send <message>");
    SafeLog("  close");
    SafeLog("  status");
    SafeLog("  help");
    SafeLog("  quit");
    {
        std::ostringstream oss;
        oss << "[heartbeat] interval=" << kHeartbeatIntervalMs
            << "ms timeout=" << kHeartbeatTimeoutMs
            << "ms payload=" << kHeartbeatPayload;
        SafeLog(oss.str());
    }

    client->SetReconnect(kReconnectIntervalMs, kReconnectMaxRetries);
    {
        std::ostringstream oss;
        oss << "[default] reconnect interval=" << kReconnectIntervalMs
            << "ms, max_retries=" << kReconnectMaxRetries;
        SafeLog(oss.str());
    }

    std::string line;
    while (std::cout << "> ", std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd.empty()) {
            continue;
        }

        if (cmd == "connect") {
            std::string host;
            iss >> host;
            if (host.empty()) {
                std::cout << "usage: connect <host> [port]  (port 默认 " << kDefaultPort << ")\n";
                continue;
            }
            int port = kDefaultPort;
            int p = 0;
            if (iss >> p) {
                if (p <= 0 || p > 65535) {
                    std::cout << "invalid port\n";
                    continue;
                }
                port = p;
            }
            if (client->IsConnected()) {
                SafeLog("already connected; close first if you want reconnect");
                continue;
            }
            const int ret = client->Connect(host, port);
            SafeLog("connect submit ret=" + std::to_string(ret));
        } else if (cmd == "send") {
            std::string msg;
            std::getline(iss >> std::ws, msg);
            if (msg.empty()) {
                std::cout << "usage: send <message>\n";
                continue;
            }
            const int ret = client->Send(msg.data(), msg.size());
            SafeLog("send ret=" + std::to_string(ret));
        } else if (cmd == "close") {
            client->Close();
            SafeLog("client closed");
        } else if (cmd == "status") {
            SafeLog(std::string("connected=") + (client->IsConnected() ? "true" : "false"));
        } else if (cmd == "help") {
            std::cout << "commands: connect <host> [port] | send <message> | close | status | quit\n";
        } else if (cmd == "quit") {
            client->Close();
            break;
        } else {
            std::cout << "unknown command: " << cmd << '\n';
        }
    }

    return 0;
}