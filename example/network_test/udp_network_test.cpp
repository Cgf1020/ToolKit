#if defined(_MSC_VER)
#pragma warning(disable : 4819)
#endif

#include "network/udp/udp_client_interface.h"
#include "network/udp/udp_server_interface.h"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

using namespace std::chrono_literals;

int g_failures = 0;

#if defined(_WIN32)
void win32_console_utf8() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}
#endif

void expect(bool ok, const char* msg) {
    if (!ok) {
        std::cerr << "[FAIL] " << msg << '\n';
        ++g_failures;
    }
}

int env_int(const char* name, int default_value) {
#if defined(_WIN32)
    char* buf = nullptr;
    size_t sz = 0;
    if (_dupenv_s(&buf, &sz, name) != 0 || buf == nullptr) {
        return default_value;
    }
    const int out = std::atoi(buf);
    free(buf);
    return out;
#else
    const char* v = std::getenv(name);
    if (!v || !*v) {
        return default_value;
    }
    return std::atoi(v);
#endif
}

template <typename F>
bool wait_until(F&& pred, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!pred()) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        std::this_thread::sleep_for(2ms);
    }
    return true;
}

class UdpServerCb final : public itflee::UdpCallback {
public:
    std::atomic<int> recv_count{0};
    std::mutex mu;
    std::string last_message;
    std::string last_peer_ip;
    uint16_t last_peer_port{0};

    void OnMessage(const void* data, std::size_t len,
                   const std::string& peer_ip, uint16_t peer_port) override {
        recv_count.fetch_add(1, std::memory_order_relaxed);
        std::lock_guard<std::mutex> lock(mu);
        last_message.assign(static_cast<const char*>(data), len);
        last_peer_ip = peer_ip;
        last_peer_port = peer_port;
    }
};

class UdpClientCb final : public itflee::UdpCallback {
public:
    std::atomic<int> recv_count{0};
    std::mutex mu;
    std::string last_message;

    void OnMessage(const void* data, std::size_t len,
                   const std::string&, uint16_t) override {
        recv_count.fetch_add(1, std::memory_order_relaxed);
        std::lock_guard<std::mutex> lock(mu);
        last_message.assign(static_cast<const char*>(data), len);
    }
};

void test_udp_functional_and_order() {
    std::cout << "[check] UDP 基础功能 + 接口顺序\n";

    auto server_cb = std::make_shared<UdpServerCb>();
    auto client_cb = std::make_shared<UdpClientCb>();
    auto server = itflee::UdpServerInterface::Create(server_cb);
    auto client = itflee::UdpClientInterface::Create(client_cb);

    expect(client->Send("x", 1) == -1, "udp: Send before Open should fail");
    expect(client->Open("127.0.0.1", 0) == -1, "udp: invalid remote port should fail");
    expect(server->Bind(19093, "127.0.0.1") == 0, "udp: bind");
    expect(client->Open("127.0.0.1", 19093) == 0, "udp: open");

    server->Start();
    client->Start();

    expect(client->Send("u-ping") == 0, "udp: client send");
    expect(wait_until([&]() { return server_cb->recv_count.load() > 0; }, 3000ms),
           "udp: server receive");

    std::string peer_ip;
    uint16_t peer_port = 0;
    {
        std::lock_guard<std::mutex> lock(server_cb->mu);
        expect(server_cb->last_message == "u-ping", "udp: payload check");
        peer_ip = server_cb->last_peer_ip;
        peer_port = server_cb->last_peer_port;
    }

    expect(server->SendTo("u-pong", peer_ip, peer_port) == 0, "udp: server reply");
    expect(wait_until([&]() { return client_cb->recv_count.load() > 0; }, 3000ms),
           "udp: client receive");
    {
        std::lock_guard<std::mutex> lock(client_cb->mu);
        expect(client_cb->last_message == "u-pong", "udp: reply payload check");
    }

    client->Stop();
    server->Stop();
}

void test_udp_stress_create_destroy() {
    const int n = env_int("ITFLEE_NETWORK_STRESS", 200);
    std::cout << "[check] 压力：UDP Create/Destroy x " << n << '\n';
    for (int i = 0; i < n; ++i) {
        auto cb = std::make_shared<UdpClientCb>();
        auto c = itflee::UdpClientInterface::Create(cb);
        c->Stop();
    }
    expect(true, "udp stress create/destroy completed");
}

void test_udp_robustness_and_burst() {
    const int burst = env_int("ITFLEE_UDP_BURST_MSGS", 2000);
    std::cout << "[check] UDP 鲁棒性+高频冲击 burst=" << burst << '\n';

    auto server_cb = std::make_shared<UdpServerCb>();
    auto client_cb = std::make_shared<UdpClientCb>();
    auto server = itflee::UdpServerInterface::Create(server_cb);
    auto client = itflee::UdpClientInterface::Create(client_cb);

    expect(server->Bind(19110, "127.0.0.1") == 0, "udp robust: bind");
    expect(client->Open("127.0.0.1", 19110) == 0, "udp robust: open");
    server->Start();
    client->Start();

    std::vector<char> bin(512);
    for (int i = 0; i < 512; ++i) bin[i] = static_cast<char>(i % 256);
    expect(client->Send(bin.data(), bin.size()) == 0, "udp robust: binary send");

    for (int i = 0; i < burst; ++i) {
        (void)client->Send("ub-" + std::to_string(i));
    }
    expect(wait_until([&]() { return server_cb->recv_count.load() > 0; }, 5000ms),
           "udp robust: should receive under burst");

    client->Stop();
    server->Stop();
}

} // namespace

int main() {
#if defined(_WIN32)
    win32_console_utf8();
#endif
    try {
        test_udp_functional_and_order();
        test_udp_stress_create_destroy();
        test_udp_robustness_and_burst();

        if (g_failures != 0) {
            std::cerr << "[udp_network_test] " << g_failures << " check(s) failed.\n";
            return 1;
        }
        std::cout << "[udp_network_test] all checks passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[udp_network_test] exception: " << e.what() << '\n';
        return 1;
    }
}

