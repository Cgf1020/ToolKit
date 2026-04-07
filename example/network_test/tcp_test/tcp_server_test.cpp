#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include "network/tcp/tcp_connection.h"
#include "network/tcp/tcp_server_interface.h"

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

class TcpServerCb final : public itflee::TcpConnectionCallback {
public:
    void OnConnectionStateChanged(const itflee::TcpConnectionPtr& conn,
                                  itflee::ConnectionState state) override {
        SafeLog("OnConnectionStateChanged: " + itflee::ConnectionStateToString(state));
        if (state == itflee::ConnectionState::kConnected && conn) {
            // const int ret = conn->EnableHeartbeat(kHeartbeatIntervalMs, kHeartbeatTimeoutMs, kHeartbeatPayload);
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
        if (msg == "PING\n") {
            SafeLog("OnMessage: PING (auto reply PONG)");
            if (conn) {
                conn->Write("PONG\n");
            }
            return;
        }
        if (msg == "PONG\n") {
            SafeLog("OnMessage: PONG");
            return;
        }

        if (msg == "wait") {
            SafeLog("OnMessage: wait");
            return;
        }
        SafeLog("OnMessage: " + msg);
        if (conn) {
            conn->Write("server:" + msg);
        }
    }
};

int main() {
    auto server_cb = std::make_shared<TcpServerCb>();
    auto server = itflee::TcpServerInterface::Create(server_cb);
    constexpr int kDefaultPort = 23456;

    SafeLog("tcp_server_test interactive mode");
    SafeLog("请先输入 listen <host> [port] 开始监听（port 省略时为 " +
            std::to_string(kDefaultPort) + "）");
    SafeLog("commands:");
    SafeLog("  listen <host> [port]");
    SafeLog("  broadcast <message>");
    SafeLog("  conn");
    SafeLog("  help");
    SafeLog("  quit");
    {
        std::ostringstream oss;
        oss << "[heartbeat] interval=" << kHeartbeatIntervalMs
            << "ms timeout=" << kHeartbeatTimeoutMs
            << "ms payload=" << kHeartbeatPayload;
        SafeLog(oss.str());
    }

    bool listened = false;
    std::string line;
    while (std::cout << "> ", std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd.empty()) {
            continue;
        }

        if (cmd == "listen") {
            std::string host;
            iss >> host;
            if (host.empty()) {
                std::cout << "usage: listen <host> [port]  (port 默认 " << kDefaultPort << ")\n";
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
            const int ret = server->Listen(port, host);
            SafeLog("listen ret=" + std::to_string(ret));
            listened = (ret == 0);
        } else if (cmd == "broadcast") {
            std::string msg;
            std::getline(iss >> std::ws, msg);
            if (msg.empty()) {
                std::cout << "usage: broadcast <message>\n";
                continue;
            }
            if (!listened) {
                SafeLog("server not listening; run listen first");
                continue;
            }
            const int n = server->Broadcast(msg);
            SafeLog("broadcast to " + std::to_string(n) + " connection(s)");
        } else if (cmd == "conn") {
            SafeLog("connection count=" + std::to_string(server->ConnectionCount()));
        } else if (cmd == "help") {
            std::cout << "commands: listen <host> [port] | broadcast <message> | conn | quit\n";
        } else if (cmd == "quit") {
            break;
        } else {
            std::cout << "unknown command: " << cmd << '\n';
        }
    }

    return 0;
}
