#if defined(_MSC_VER)
#pragma warning(disable : 4819)
#endif

#include "network/tcp/tcp_client_interface.h"
#include "network/tcp/tcp_connection.h"
#include "network/tcp/tcp_server_interface.h"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <vector>
#include <clocale>

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#elif defined(__linux__)
#include <unistd.h>
#include <fstream>
#endif

namespace {

using namespace std::chrono_literals;

int g_failures = 0;

#if defined(_WIN32)
void win32_console_utf8() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    (void)std::setlocale(LC_ALL, ".UTF-8");
}
#endif

#if !defined(_WIN32)
void console_utf8_best_effort() {
    // 依赖用户环境变量（LANG/LC_ALL 等）决定最终编码；多数 Linux 默认即 UTF-8。
    (void)std::setlocale(LC_ALL, "");
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

std::size_t process_rss_bytes() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<std::size_t>(pmc.WorkingSetSize);
    }
    return 0;
#elif defined(__linux__)
    std::ifstream fin("/proc/self/statm");
    long total_pages = 0;
    long rss_pages = 0;
    if (!(fin >> total_pages >> rss_pages)) {
        return 0;
    }
    const long page_size = sysconf(_SC_PAGESIZE);
    return static_cast<std::size_t>(rss_pages * page_size);
#else
    return 0;
#endif
}

double percentile_us(std::vector<long long> values, double p) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const double pos = (p / 100.0) * static_cast<double>(values.size() - 1);
    const auto idx = static_cast<size_t>(pos);
    return static_cast<double>(values[idx]);
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

class TcpServerCb final : public itflee::TcpConnectionCallback {
public:
    std::atomic<int> connected{0};
    std::atomic<int> disconnected{0};
    std::atomic<int> recv_count{0};

    std::mutex mu;
    std::string last_message;
    std::set<std::thread::id> callback_threads;

    void OnConnectionStateChanged(const itflee::TcpConnectionPtr&,
                                  itflee::ConnectionState state) override {
        {
            std::lock_guard<std::mutex> lock(mu);
            callback_threads.insert(std::this_thread::get_id());
        }
        if (state == itflee::ConnectionState::kConnected) {
            connected.fetch_add(1, std::memory_order_relaxed);
        }
        if (state == itflee::ConnectionState::kDisconnected ||
            state == itflee::ConnectionState::kDisconnecting) {
            disconnected.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void OnMessage(const itflee::TcpConnectionPtr& conn,
                   const void* data, std::size_t len) override {
        recv_count.fetch_add(1, std::memory_order_relaxed);
        std::string msg(static_cast<const char*>(data), len);
        {
            std::lock_guard<std::mutex> lock(mu);
            last_message = msg;
            callback_threads.insert(std::this_thread::get_id());
        }
        if (conn) {
            (void)conn->Write("echo:" + msg);
        }
    }
};

class TcpClientCb final : public itflee::TcpConnectionCallback {
public:
    std::atomic<int> connected{0};
    std::atomic<int> connect_failed{0};
    std::atomic<int> disconnected{0};
    std::atomic<int> recv_count{0};

    std::mutex mu;
    std::string last_message;

    void OnConnectionStateChanged(const itflee::TcpConnectionPtr&,
                                  itflee::ConnectionState state) override {
        if (state == itflee::ConnectionState::kConnected) {
            connected.fetch_add(1, std::memory_order_relaxed);
        } else if (state == itflee::ConnectionState::kConnectFailed) {
            connect_failed.fetch_add(1, std::memory_order_relaxed);
        } else if (state == itflee::ConnectionState::kDisconnected ||
                   state == itflee::ConnectionState::kDisconnecting) {
            disconnected.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void OnMessage(const itflee::TcpConnectionPtr&,
                   const void* data, std::size_t len) override {
        recv_count.fetch_add(1, std::memory_order_relaxed);
        std::lock_guard<std::mutex> lock(mu);
        last_message.assign(static_cast<const char*>(data), len);
    }
};

std::vector<std::shared_ptr<itflee::TcpClientInterface>> make_clients(
    int n, std::vector<std::shared_ptr<TcpClientCb>>& cbs) {
    std::vector<std::shared_ptr<itflee::TcpClientInterface>> clients;
    clients.reserve(n);
    cbs.clear();
    cbs.reserve(n);
    for (int i = 0; i < n; ++i) {
        auto cb = std::make_shared<TcpClientCb>();
        cbs.push_back(cb);
        clients.push_back(itflee::TcpClientInterface::Create(cb));
    }
    return clients;
}

void test_tcp_functional_and_order() {
    std::cout << "[check] TCP 基础功能 + 接口顺序\n";

    auto server_cb = std::make_shared<TcpServerCb>();
    auto client_cb = std::make_shared<TcpClientCb>();
    auto server = itflee::TcpServerInterface::Create(server_cb);
    auto client = itflee::TcpClientInterface::Create(client_cb);

    expect(client->Send("x", 1) == -1, "tcp: Send before Connect should fail");
    expect(client->Connect("", 19091) == -1, "tcp: empty host should fail");
    expect(server->Listen(19091, "127.0.0.1") == 0, "tcp: server listen");
    expect(client->Connect("127.0.0.1", 19091) == 0, "tcp: client connect submit");

    expect(wait_until([&]() { return client_cb->connected.load() > 0; }, 3000ms),
           "tcp: client connected callback");
    expect(wait_until([&]() { return server_cb->connected.load() > 0; }, 3000ms),
           "tcp: server connected callback");
    expect(client->IsConnected(), "tcp: IsConnected after connect");

    expect(client->Send("ping") == 0, "tcp: send ping");
    expect(wait_until([&]() { return client_cb->recv_count.load() > 0; }, 3000ms),
           "tcp: client should receive echo");
    {
        std::lock_guard<std::mutex> lock(client_cb->mu);
        expect(client_cb->last_message == "echo:ping", "tcp: echo payload check");
    }

    expect(server->Broadcast("broadcast") >= 1, "tcp: broadcast should target at least one conn");
    expect(wait_until([&]() { return client_cb->recv_count.load() >= 2; }, 3000ms),
           "tcp: client should receive broadcast");

    client->Close();
    expect(wait_until([&]() { return client_cb->disconnected.load() > 0; }, 3000ms),
           "tcp: disconnect callback after Close");
}

void test_tcp_concurrency() {
    std::cout << "[check] TCP 并发发送\n";

    auto server_cb = std::make_shared<TcpServerCb>();
    auto client_cb = std::make_shared<TcpClientCb>();
    auto server = itflee::TcpServerInterface::Create(server_cb);
    auto client = itflee::TcpClientInterface::Create(client_cb);

    expect(server->Listen(19092, "127.0.0.1") == 0, "tcp concur: listen");
    expect(client->Connect("127.0.0.1", 19092) == 0, "tcp concur: connect");
    expect(wait_until([&]() { return client_cb->connected.load() > 0; }, 3000ms),
           "tcp concur: connected");

    constexpr int kThreads = 4;
    std::atomic<int> send_ok{0};
    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([client, t, &send_ok]() {
            for (int i = 0; i < 25; ++i) {
                std::string payload = "m-" + std::to_string(t) + "-" + std::to_string(i);
                if (client->Send(payload) == 0) {
                    send_ok.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }

    const int sent = send_ok.load(std::memory_order_relaxed);
    expect(sent > 0, "tcp concur: at least one send should succeed");
    expect(wait_until([&]() {
               return server_cb->recv_count.load(std::memory_order_relaxed) >= 1;
           }, 5000ms),
           "tcp concur: server should receive at least one message");

    client->Close();
}

void test_server_options_stats_and_shutdown() {
    std::cout << "[check] TCP Server options/stats/shutdown\n";

    auto server_cb = std::make_shared<TcpServerCb>();
    auto c1_cb = std::make_shared<TcpClientCb>();
    auto c2_cb = std::make_shared<TcpClientCb>();

    itflee::TcpServerOptions opts;
    opts.worker_count = 2;
    opts.load_balance_policy = itflee::TcpLoadBalancePolicy::kLeastConn;
    opts.cb_dispatch_mode = itflee::TcpCallbackDispatchMode::kMainThread;
    opts.graceful_shutdown_timeout = std::chrono::milliseconds(1200);

    auto server = itflee::TcpServerInterface::Create(server_cb, nullptr, opts);
    auto c1 = itflee::TcpClientInterface::Create(c1_cb);
    auto c2 = itflee::TcpClientInterface::Create(c2_cb);

    expect(server->Listen(19093, "127.0.0.1") == 0, "tcp opts: listen");
    expect(c1->Connect("127.0.0.1", 19093) == 0, "tcp opts: c1 connect");
    expect(c2->Connect("127.0.0.1", 19093) == 0, "tcp opts: c2 connect");

    expect(wait_until([&]() { return c1_cb->connected.load() > 0; }, 3000ms), "tcp opts: c1 connected");
    expect(wait_until([&]() { return c2_cb->connected.load() > 0; }, 3000ms), "tcp opts: c2 connected");
    expect(wait_until([&]() { return server->ConnectionCount() >= 2; }, 3000ms), "tcp opts: server conn count");

    const auto st = server->GetStats();
    expect(st.total_connections >= 2, "tcp opts: total stats >= 2");
    expect(st.per_sub.size() == 2, "tcp opts: per_sub size == worker_count");
    size_t sum = 0;
    for (const auto& item : st.per_sub) {
        sum += item.connection_count;
    }
    expect(sum == st.total_connections, "tcp opts: per_sub sum check");

    c1->Close();
    c2->Close();
    expect(server->Shutdown(itflee::TcpShutdownMode::kGraceful, std::chrono::milliseconds(1000)) == 0,
           "tcp opts: graceful shutdown");
}

void test_tcp_round_robin_distribution() {
    std::cout << "[check] TCP RoundRobin 连接分布\n";
    auto server_cb = std::make_shared<TcpServerCb>();
    itflee::TcpServerOptions opts;
    opts.worker_count = 3;
    opts.load_balance_policy = itflee::TcpLoadBalancePolicy::kRoundRobin;
    auto server = itflee::TcpServerInterface::Create(server_cb, nullptr, opts);
    expect(server->Listen(19095, "127.0.0.1") == 0, "tcp rr: listen");

    std::vector<std::shared_ptr<TcpClientCb>> cbs;
    auto clients = make_clients(9, cbs);
    for (auto& c : clients) {
        expect(c->Connect("127.0.0.1", 19095) == 0, "tcp rr: connect submit");
    }
    expect(wait_until([&]() { return server->ConnectionCount() >= 9; }, 4000ms), "tcp rr: all connected");

    const auto st = server->GetStats();
    expect(st.per_sub.size() == 3, "tcp rr: sub count");
    size_t non_zero = 0;
    size_t min_conn = static_cast<size_t>(-1);
    size_t max_conn = 0;
    for (const auto& s : st.per_sub) {
        if (s.connection_count > 0) {
            ++non_zero;
        }
        if (s.connection_count < min_conn) {
            min_conn = s.connection_count;
        }
        if (s.connection_count > max_conn) {
            max_conn = s.connection_count;
        }
    }
    expect(non_zero == 3, "tcp rr: all sub have connections");
    expect(max_conn - min_conn <= 1, "tcp rr: distribution roughly even");
    for (auto& c : clients) {
        c->Close();
    }
    (void)server->Shutdown(itflee::TcpShutdownMode::kGraceful, 800ms);
}

void test_tcp_callback_dispatch_mode() {
    std::cout << "[check] TCP 回调线程派发模式\n";

    {
        auto server_cb = std::make_shared<TcpServerCb>();
        itflee::TcpServerOptions opts;
        opts.worker_count = 2;
        opts.cb_dispatch_mode = itflee::TcpCallbackDispatchMode::kMainThread;
        auto server = itflee::TcpServerInterface::Create(server_cb, nullptr, opts);
        expect(server->Listen(19096, "127.0.0.1") == 0, "tcp cb main: listen");

        std::vector<std::shared_ptr<TcpClientCb>> cbs;
        auto clients = make_clients(4, cbs);
        for (auto& c : clients) {
            expect(c->Connect("127.0.0.1", 19096) == 0, "tcp cb main: connect");
        }
        expect(wait_until([&]() { return server->ConnectionCount() >= 4; }, 4000ms), "tcp cb main: connected");
        for (auto& c : clients) {
            (void)c->Send("m");
        }
        expect(wait_until([&]() { return server_cb->recv_count.load() >= 4; }, 4000ms), "tcp cb main: recv");
        size_t thread_count = 0;
        {
            std::lock_guard<std::mutex> lock(server_cb->mu);
            thread_count = server_cb->callback_threads.size();
        }
        expect(thread_count == 1, "tcp cb main: callbacks should be single main thread");
        for (auto& c : clients) {
            c->Close();
        }
        (void)server->Shutdown(itflee::TcpShutdownMode::kGraceful, 800ms);
    }

    {
        auto server_cb = std::make_shared<TcpServerCb>();
        itflee::TcpServerOptions opts;
        opts.worker_count = 2;
        opts.cb_dispatch_mode = itflee::TcpCallbackDispatchMode::kSubThread;
        opts.load_balance_policy = itflee::TcpLoadBalancePolicy::kRoundRobin;
        auto server = itflee::TcpServerInterface::Create(server_cb, nullptr, opts);
        expect(server->Listen(19097, "127.0.0.1") == 0, "tcp cb sub: listen");

        std::vector<std::shared_ptr<TcpClientCb>> cbs;
        auto clients = make_clients(6, cbs);
        for (auto& c : clients) {
            expect(c->Connect("127.0.0.1", 19097) == 0, "tcp cb sub: connect");
        }
        expect(wait_until([&]() { return server->ConnectionCount() >= 6; }, 4000ms), "tcp cb sub: connected");
        for (auto& c : clients) {
            (void)c->Send("s");
        }
        expect(wait_until([&]() { return server_cb->recv_count.load() >= 6; }, 4000ms), "tcp cb sub: recv");
        size_t thread_count = 0;
        {
            std::lock_guard<std::mutex> lock(server_cb->mu);
            thread_count = server_cb->callback_threads.size();
        }
        expect(thread_count >= 2, "tcp cb sub: callbacks should run on sub threads");
        for (auto& c : clients) {
            c->Close();
        }
        (void)server->Shutdown(itflee::TcpShutdownMode::kGraceful, 800ms);
    }
}

void test_tcp_force_shutdown() {
    std::cout << "[check] TCP 强制停机\n";
    auto server_cb = std::make_shared<TcpServerCb>();
    itflee::TcpServerOptions opts;
    opts.worker_count = 2;
    auto server = itflee::TcpServerInterface::Create(server_cb, nullptr, opts);
    expect(server->Listen(19098, "127.0.0.1") == 0, "tcp force: listen");

    std::vector<std::shared_ptr<TcpClientCb>> cbs;
    auto clients = make_clients(4, cbs);
    for (auto& c : clients) {
        expect(c->Connect("127.0.0.1", 19098) == 0, "tcp force: connect");
    }
    expect(wait_until([&]() { return server->ConnectionCount() >= 4; }, 4000ms), "tcp force: connected");

    expect(server->Shutdown(itflee::TcpShutdownMode::kForce, 0ms) == 0, "tcp force: shutdown ret");
    expect(wait_until([&]() {
               int disc = 0;
               for (const auto& cb : cbs) {
                   if (cb->disconnected.load(std::memory_order_relaxed) > 0) {
                       ++disc;
                   }
               }
               return disc >= 1;
           }, 3000ms),
           "tcp force: clients should observe disconnect");
}

void test_tcp_reentrant_no_deadlock() {
    std::cout << "[check] TCP 回调重入无死锁\n";
    struct ReentrantCb final : public itflee::TcpConnectionCallback {
        std::weak_ptr<itflee::TcpServerInterface> server;
        std::atomic<int> recv_count{0};
        void OnConnectionStateChanged(const itflee::TcpConnectionPtr&,
                                      itflee::ConnectionState) override {}
        void OnMessage(const itflee::TcpConnectionPtr& conn, const void* data, std::size_t len) override {
            recv_count.fetch_add(1, std::memory_order_relaxed);
            if (auto s = server.lock()) {
                // 在回调里重入调用服务接口，验证不会锁反转死锁。
                (void)s->ConnectionCount();
                (void)s->Broadcast(data, len);
            }
            if (conn) {
                (void)conn->Write("reentrant-ok");
            }
        }
    };

    auto server_cb = std::make_shared<ReentrantCb>();
    auto server = itflee::TcpServerInterface::Create(server_cb);
    server_cb->server = server;
    auto client_cb = std::make_shared<TcpClientCb>();
    auto client = itflee::TcpClientInterface::Create(client_cb);

    expect(server->Listen(19101, "127.0.0.1") == 0, "tcp reentrant: listen");
    expect(client->Connect("127.0.0.1", 19101) == 0, "tcp reentrant: connect");
    expect(wait_until([&]() { return client_cb->connected.load() > 0; }, 3000ms), "tcp reentrant: connected");

    for (int i = 0; i < 20; ++i) {
        (void)client->Send("r-" + std::to_string(i));
    }
    // watchdog: 若有死锁，这里会超时失败。
    expect(wait_until([&]() { return server_cb->recv_count.load() > 0; }, 4000ms),
           "tcp reentrant: callback should continue progressing");
    client->Close();
    (void)server->Shutdown(itflee::TcpShutdownMode::kGraceful, 500ms);
}

void test_tcp_robustness_binary_and_long_payload() {
    std::cout << "[check] TCP 鲁棒性（二进制/长包）\n";
    auto server_cb = std::make_shared<TcpServerCb>();
    auto client_cb = std::make_shared<TcpClientCb>();
    auto server = itflee::TcpServerInterface::Create(server_cb);
    auto client = itflee::TcpClientInterface::Create(client_cb);

    expect(server->Listen(19102, "127.0.0.1") == 0, "tcp robust: listen");
    expect(client->Connect("127.0.0.1", 19102) == 0, "tcp robust: connect");
    expect(wait_until([&]() { return client_cb->connected.load() > 0; }, 3000ms), "tcp robust: connected");

    std::vector<char> bin(256);
    for (int i = 0; i < 256; ++i) bin[i] = static_cast<char>(i);
    expect(client->Send(bin.data(), bin.size()) == 0, "tcp robust: binary send");

    std::string long_msg(1024 * 256, 'L');
    expect(client->Send(long_msg.data(), long_msg.size()) == 0, "tcp robust: long payload send");

    expect(wait_until([&]() { return server_cb->recv_count.load() >= 2; }, 5000ms),
           "tcp robust: server should receive binary and long payload");
    client->Close();
}

void test_tcp_stress_churn_and_burst() {
    const int churn_rounds = env_int("ITFLEE_TCP_CHURN_ROUNDS", 30);
    const int burst_msgs = env_int("ITFLEE_TCP_BURST_MSGS", 2000);
    std::cout << "[check] 压力：连接风暴 + 高频发送 churn=" << churn_rounds
              << " burst=" << burst_msgs << '\n';

    auto server_cb = std::make_shared<TcpServerCb>();
    auto server = itflee::TcpServerInterface::Create(server_cb);
    expect(server->Listen(19103, "127.0.0.1") == 0, "tcp stress: listen");

    // 连接风暴
    for (int i = 0; i < churn_rounds; ++i) {
        auto cb = std::make_shared<TcpClientCb>();
        auto c = itflee::TcpClientInterface::Create(cb);
        (void)c->Connect("127.0.0.1", 19103);
        (void)wait_until([&]() { return cb->connected.load() > 0 || cb->connect_failed.load() > 0; }, 1000ms);
        c->Close();
    }

    auto ccb = std::make_shared<TcpClientCb>();
    auto c = itflee::TcpClientInterface::Create(ccb);
    expect(c->Connect("127.0.0.1", 19103) == 0, "tcp stress: stable connect");
    expect(wait_until([&]() { return ccb->connected.load() > 0; }, 3000ms), "tcp stress: stable connected");

    std::atomic<int> ok{0};
    std::thread t1([&]() {
        for (int i = 0; i < burst_msgs; ++i) {
            if (c->Send("b1-" + std::to_string(i)) == 0) ok.fetch_add(1, std::memory_order_relaxed);
        }
    });
    std::thread t2([&]() {
        for (int i = 0; i < burst_msgs; ++i) {
            if (c->Send("b2-" + std::to_string(i)) == 0) ok.fetch_add(1, std::memory_order_relaxed);
        }
    });
    t1.join();
    t2.join();

    expect(ok.load(std::memory_order_relaxed) > 0, "tcp stress: at least some burst sends succeed");
    expect(wait_until([&]() { return server_cb->recv_count.load(std::memory_order_relaxed) > 0; }, 5000ms),
           "tcp stress: server receives under burst load");
    c->Close();
}

void test_lifecycle_weak_ptr_guard() {
    std::cout << "[check] 生命周期：weak_ptr 防 UAF 用法示例\n";

    struct Owner : std::enable_shared_from_this<Owner> {
        std::atomic<int> hits{0};
    };

    auto owner = std::make_shared<Owner>();
    std::weak_ptr<Owner> weak = owner;

    auto cb = std::make_shared<TcpClientCb>();
    auto client = itflee::TcpClientInterface::Create(cb);
    auto guard_cb = [weak]() {
        if (auto s = weak.lock()) {
            s->hits.fetch_add(1, std::memory_order_relaxed);
        }
    };
    guard_cb();
    expect(owner->hits.load() == 1, "lifecycle: weak lock succeeds before reset");
    owner.reset();
    guard_cb();
    client->Close();
}

void test_stress_create_destroy() {
    const int n = env_int("ITFLEE_NETWORK_STRESS", 200);
    std::cout << "[check] 压力：TCP Create/Destroy x " << n << '\n';
    for (int i = 0; i < n; ++i) {
        auto cb = std::make_shared<TcpClientCb>();
        auto c = itflee::TcpClientInterface::Create(cb);
        c->Close();
    }
    expect(true, "tcp stress create/destroy completed");
}

void test_latency_optional() {
    const int samples = env_int("ITFLEE_NETWORK_LATENCY_SAMPLES", 0);
    if (samples <= 0) {
        return;
    }
    std::cout << "[check] 性能：TCP 回环平均延迟采样 n=" << samples << '\n';

    auto server_cb = std::make_shared<TcpServerCb>();
    auto client_cb = std::make_shared<TcpClientCb>();
    auto server = itflee::TcpServerInterface::Create(server_cb);
    auto client = itflee::TcpClientInterface::Create(client_cb);

    if (server->Listen(19094, "127.0.0.1") != 0 ||
        client->Connect("127.0.0.1", 19094) != 0 ||
        !wait_until([&]() { return client_cb->connected.load() > 0; }, 3000ms)) {
        expect(false, "latency: setup failed");
        return;
    }

    long long sum_us = 0;
    std::vector<long long> samples_us;
    samples_us.reserve(samples);
    for (int i = 0; i < samples; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        const std::string payload = "lat-" + std::to_string(i);
        const int before = client_cb->recv_count.load();
        (void)client->Send(payload);
        if (!wait_until([&]() { return client_cb->recv_count.load() > before; }, 3000ms)) {
            expect(false, "latency: round trip timeout");
            break;
        }
        const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::steady_clock::now() - t0)
                            .count();
        sum_us += us;
        samples_us.push_back(us);
    }
    std::cout << "  avg tcp echo latency: " << (static_cast<double>(sum_us) / samples) << " us\n";
    std::cout << "  p99 tcp echo latency: " << percentile_us(samples_us, 99.0) << " us\n";
    client->Close();
}

void test_throughput_optional() {
    const int msg_count = env_int("ITFLEE_TCP_THROUGHPUT_MSGS", 0);
    if (msg_count <= 0) {
        return;
    }
    std::cout << "[check] 性能：TCP 吞吐采样 msgs=" << msg_count << '\n';

    auto server_cb = std::make_shared<TcpServerCb>();
    auto client_cb = std::make_shared<TcpClientCb>();
    auto server = itflee::TcpServerInterface::Create(server_cb);
    auto client = itflee::TcpClientInterface::Create(client_cb);

    if (server->Listen(19104, "127.0.0.1") != 0 ||
        client->Connect("127.0.0.1", 19104) != 0 ||
        !wait_until([&]() { return client_cb->connected.load() > 0; }, 3000ms)) {
        expect(false, "throughput: setup failed");
        return;
    }

    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < msg_count; ++i) {
        (void)client->Send("tp-" + std::to_string(i));
    }
    // TCP 是字节流，回调次数不等同于 Send 次数；这里只验证高压下可持续处理。
    expect(wait_until([&]() { return server_cb->recv_count.load() > 0; }, 10000ms),
           "throughput: recv should progress");
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - t0)
                                .count();
    const double qps = elapsed_ms > 0 ? (static_cast<double>(msg_count) * 1000.0 / elapsed_ms) : 0.0;
    std::cout << "  throughput: " << qps << " msg/s elapsed_ms=" << elapsed_ms << '\n';
    client->Close();
}

void test_fd_resource_exhaustion_optional() {
    const int max_clients = env_int("ITFLEE_TCP_FD_PROBE_MAX", 0);
    if (max_clients <= 0) {
        return;
    }
    std::cout << "[check] 鲁棒：资源耗尽探测 max_clients=" << max_clients << '\n';

    auto server_cb = std::make_shared<TcpServerCb>();
    auto server = itflee::TcpServerInterface::Create(server_cb);
    expect(server->Listen(19105, "127.0.0.1") == 0, "fd probe: listen");

    std::vector<std::shared_ptr<TcpClientCb>> cbs;
    std::vector<std::shared_ptr<itflee::TcpClientInterface>> clients;
    cbs.reserve(max_clients);
    clients.reserve(max_clients);

    int connect_submit_ok = 0;
    int connect_submit_fail = 0;
    for (int i = 0; i < max_clients; ++i) {
        auto cb = std::make_shared<TcpClientCb>();
        auto c = itflee::TcpClientInterface::Create(cb);
        const int ret = c->Connect("127.0.0.1", 19105);
        if (ret == 0) {
            ++connect_submit_ok;
        } else {
            ++connect_submit_fail;
        }
        cbs.push_back(cb);
        clients.push_back(c);
    }

    // 不要求全部成功；重点是模块在资源压力下不崩溃且可继续运行。
    expect(connect_submit_ok > 0, "fd probe: at least one connect submitted");
    expect(wait_until([&]() { return server_cb->connected.load() > 0; }, 5000ms),
           "fd probe: server accepts at least one connection");
    std::cout << "  fd probe result: submit_ok=" << connect_submit_ok
              << " submit_fail=" << connect_submit_fail
              << " current_conn=" << server->ConnectionCount() << '\n';

    for (auto& c : clients) {
        c->Close();
    }
    (void)server->Shutdown(itflee::TcpShutdownMode::kGraceful, 1500ms);
}

void test_memory_stability_optional() {
    const int loops = env_int("ITFLEE_TCP_RSS_LOOPS", 0);
    if (loops <= 0) {
        return;
    }
    const int max_growth_mb = env_int("ITFLEE_TCP_RSS_MAX_GROWTH_MB", 64);
    std::cout << "[check] 资源：RSS 稳定性 loops=" << loops
              << " max_growth_mb=" << max_growth_mb << '\n';

    const std::size_t rss_before = process_rss_bytes();
    for (int i = 0; i < loops; ++i) {
        auto cb = std::make_shared<TcpClientCb>();
        auto c = itflee::TcpClientInterface::Create(cb);
        c->Close();
    }
    // 给后台线程一点收敛时间，减少测量抖动。
    std::this_thread::sleep_for(200ms);
    const std::size_t rss_after = process_rss_bytes();
    const std::size_t growth = (rss_after >= rss_before) ? (rss_after - rss_before) : 0;
    const std::size_t limit = static_cast<std::size_t>(max_growth_mb) * 1024 * 1024;
    std::cout << "  rss_before=" << rss_before << " rss_after=" << rss_after
              << " growth=" << growth << " bytes\n";
    expect(growth <= limit, "rss stability: growth should be bounded");
}

} // namespace

int main() {
#if defined(_WIN32)
    win32_console_utf8();
#else
    console_utf8_best_effort();
#endif
    try {
        test_tcp_functional_and_order();
        test_tcp_concurrency();
        test_server_options_stats_and_shutdown();
        test_tcp_round_robin_distribution();
        test_tcp_callback_dispatch_mode();
        test_tcp_force_shutdown();
        test_tcp_reentrant_no_deadlock();
        test_tcp_robustness_binary_and_long_payload();
        test_tcp_stress_churn_and_burst();
        test_lifecycle_weak_ptr_guard();
        test_stress_create_destroy();
        test_latency_optional();
        test_throughput_optional();
        test_fd_resource_exhaustion_optional();
        test_memory_stability_optional();

        if (g_failures != 0) {
            std::cerr << "[tcp_network_test] " << g_failures << " check(s) failed.\n";
            return 1;
        }
        std::cout << "[tcp_network_test] all checks passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[tcp_network_test] exception: " << e.what() << '\n';
        return 1;
    }
}

