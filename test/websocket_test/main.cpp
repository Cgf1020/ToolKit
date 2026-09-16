#include <iostream>
#include <iostream>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include "base/websocket/socketconnection.h"

using namespace std::chrono_literals;

class DemoConnListener : public itflee::SocketConnectListener {
public:
    void onConnectConnected() override {
        std::cout << "[websocket] connected" << std::endl;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            opened_ = true;
        }
        cv_.notify_all();
    }

    void onConnectDisconnected(int err, const std::string& reason = "") override {
        std::cout << "[websocket] disconnected err=" << err << " reason=" << reason << std::endl;
        notify_close();
    }

    void onConnectNotification(const std::string& msg) override {
        std::cout << "[websocket] notification: " << msg << std::endl;
    }

    bool wait_open(std::chrono::milliseconds timeout = 3s) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout, [&] { return opened_; });
    }

    void wait_close(std::chrono::milliseconds timeout = 5s) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, timeout, [&] { return closed_; });
    }

private:
    void notify_close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        cv_.notify_all();
    }

    std::mutex mutex_;
    std::condition_variable cv_;
    bool opened_ = false;
    bool closed_ = false;
};

int main() {

#if !ENABLE_WEBSOCKET
    std::cout << "ENABLE_WEBSOCKET is OFF, skip websocket test." << std::endl;
    return 0;
#else
    std::cout << "WebSocket test starting..." << std::endl;

    auto conn = itflee::SocketConnectFactory::Create();
    auto listener = std::make_shared<DemoConnListener>();

    // 当前实现内部固定了地址 (47.104.168.9:51188)，此处 url 仅为占位
    conn->connect("ws://127.0.0.1:8765", listener);

    if (!listener->wait_open()) {
        std::cout << "wait open timeout." << std::endl;
    } else {
        std::cout << "输入内容回车发送，输入 exit 退出：" << std::endl;

        while (true) {
            std::string line;
            if (!std::getline(std::cin, line)) { 
                std::cin.clear();
                continue;         // 没有输入时不要立刻关闭
            }
            if (line == "exit") break;
            auto handler = std::make_shared<itflee::SocketHandler>(
                "tx1", std::make_shared<itflee::SocketCallback>(
                    [](const std::string& msg){ std::cout << "[cb] " << msg << std::endl; }));
            conn->sendText(line, handler);
        }

    }

    std::cout << "Press Enter to continue...";
	std::cin.get();
    // 最后再 close
    conn->close();
    listener->wait_close();
    return 0;
#endif
}
