// Example: Reactor-style processing of two in-memory queues using libhv.
// - Two producer threads push data into two separate queues.
// - Producers notify a central hv::EventLoop via postEvent (reactor pattern).
// - EventLoop callbacks drain the queues and dispatch work to libhv's global thread pool.

#include <iostream>
#include <string>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>

#include "hv/EventLoop.h"
#include "hv/hasync.h"

struct Task {
    int         id;
    std::string source;   // "queue1" or "queue2"
    std::string payload;
};

// Shared queues and synchronization primitives.
static std::queue<Task> g_queue1;
static std::queue<Task> g_queue2;
static std::mutex       g_mtx1;
static std::mutex       g_mtx2;

static std::atomic<bool> g_running{true};

// EventLoop pointer so producer threads can post events into the reactor.
static hv::EventLoop* g_loop = nullptr;

// Helper: pop all tasks from one queue under its mutex.
static void drain_queue(std::queue<Task>& q, std::mutex& mtx, std::vector<Task>& out) {
    std::lock_guard<std::mutex> lock(mtx);
    while (!q.empty()) {
        out.push_back(std::move(q.front()));
        q.pop();
    }
}

// Event handler: drain one specific queue and submit its tasks to libhv's global thread pool.
static void on_queue_event(std::queue<Task>& q, std::mutex& mtx, const char* queue_name, hv::Event* /*ev*/) {
    std::vector<Task> tasks;
    tasks.reserve(32);

    drain_queue(q, mtx, tasks);

    for (const auto& task : tasks) {
        hv::async([task, queue_name]() {
            // Simulate some work.
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            std::cout << "[worker] processed task " << task.id
                      << " from " << queue_name
                      << ", payload=" << task.payload << std::endl;
        });
    }
}

// Producer thread function for a given queue.
static void producer_thread(std::queue<Task>& q, std::mutex& mtx,
                            const std::string& source_name, int start_id) {
    int id = start_id;
    while (g_running.load(std::memory_order_relaxed)) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            Task t;
            t.id      = id++;
            t.source  = source_name;
            t.payload = "data_" + std::to_string(t.id);
            q.push(std::move(t));
        }

        // Notify the reactor (EventLoop) that this queue has new data.
        hv::EventLoop* loop = g_loop;
        if (loop) {
            if (source_name == "queue1") {
                loop->postEvent([](hv::Event* ev) {
                    on_queue_event(g_queue1, g_mtx1, "queue1", ev);
                });
            } else {
                loop->postEvent([](hv::Event* ev) {
                    on_queue_event(g_queue2, g_mtx2, "queue2", ev);
                });
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    std::cout << "queue_threadpool_example (reactor + libhv async) started." << std::endl;

    // Start libhv global thread pool.
    hv::async::startup();

    hv::EventLoop loop;
    g_loop = &loop;

    // Start two producer threads.
    std::thread th1(producer_thread, std::ref(g_queue1), std::ref(g_mtx1), "queue1", 1);
    std::thread th2(producer_thread, std::ref(g_queue2), std::ref(g_mtx2), "queue2", 1000);

    // Stop everything after 10 seconds via EventLoop timer.
    loop.setTimeout(10000, [&loop](hv::TimerID /*timer_id*/) {
        std::cout << "[main] stopping after 10 seconds." << std::endl;
        g_running.store(false, std::memory_order_relaxed);
        loop.stop();
    });

    std::cout << "entering event loop, press Ctrl+C to exit earlier." << std::endl;
    loop.run();

    // Join producers.
    if (th1.joinable()) th1.join();
    if (th2.joinable()) th2.join();

    // Cleanup global thread pool.
    hv::async::cleanup();

    std::cout << "queue_threadpool_example finished." << std::endl;
    return 0;
}

