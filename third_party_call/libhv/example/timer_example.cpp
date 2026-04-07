// Simple timer usage example using libhv EventLoop.
// Features:
// 1) Multiple one-shot timers with different delays.
// 2) Multiple interval timers with different periods.
// 3) Demonstration of "pause" and "resume" for both one-shot and interval timers.

#include <iostream>
#include <cstdint>

#include "hv/EventLoop.h"

// Use simple global state to keep the example straightforward.
static hv::EventLoop* g_loop                 = nullptr;
static std::uint64_t  g_oneshot_pause        = static_cast<std::uint64_t>(-1);
static std::uint64_t  g_interval_demo        = static_cast<std::uint64_t>(-1);

// ---------- One-shot timers ----------

void on_one_shot_1s(std::uint64_t) {
    std::cout << "[one-shot] 1s timeout" << std::endl;
}

void on_one_shot_2s(std::uint64_t) {
    std::cout << "[one-shot] 2s timeout" << std::endl;
}

void on_one_shot_5s(std::uint64_t) {
    std::cout << "[one-shot] 5s timeout" << std::endl;
}

// One-shot timer that will be "paused" (actually reset with new timeout).
void on_oneshot_pause_fire(std::uint64_t) {
    std::cout << "[one-shot-pause] fired (after reset)" << std::endl;
}

void on_oneshot_pause_control(std::uint64_t) {
    if (g_loop && g_oneshot_pause != static_cast<std::uint64_t>(-1)) {
        std::cout << "[control] reset one-shot timer at 3s, delay another 5s" << std::endl;
        // Reset remaining time to 5000 ms.
        g_loop->resetTimer(g_oneshot_pause, 5000);
    }
}

// ---------- Interval timers ----------

void on_interval_500ms(std::uint64_t) {
    static int count_500ms = 0;
    std::cout << "[interval] 500ms tick, count=" << ++count_500ms << std::endl;
}

void on_interval_1000ms(std::uint64_t) {
    static int count_1000ms = 0;
    std::cout << "[interval] 1000ms tick, count=" << ++count_1000ms << std::endl;
}

void on_interval_2000ms(std::uint64_t) {
    static int count_2000ms = 0;
    std::cout << "[interval] 2000ms tick, count=" << ++count_2000ms << std::endl;
}

// Interval timer that can be paused / resumed.

void on_interval_demo_tick(std::uint64_t) {
    static int count = 0;
    std::cout << "[interval-pause] 1500ms tick, count=" << ++count << std::endl;
}

void start_interval_demo() {
    if (!g_loop) return;
    g_interval_demo = g_loop->setInterval(1500, on_interval_demo_tick);
}

void on_interval_demo_pause(std::uint64_t) {
    if (g_loop && g_interval_demo != static_cast<std::uint64_t>(-1)) {
        std::cout << "[control] pause 1500ms interval timer at ~6s" << std::endl;
        g_loop->killTimer(g_interval_demo);
        g_interval_demo = static_cast<std::uint64_t>(-1);
    }
}

void on_interval_demo_resume(std::uint64_t) {
    std::cout << "[control] resume 1500ms interval timer at 10s" << std::endl;
    start_interval_demo();
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    hv::EventLoop loop;
    g_loop = &loop;

    std::cout << "timer_example started." << std::endl;

    // 1) Multiple one-shot timers with different delays.
    loop.setTimeout(1000, on_one_shot_1s);
    loop.setTimeout(2000, on_one_shot_2s);
    loop.setTimeout(5000, on_one_shot_5s);

    // One-shot timer that we will "pause" (reset) later.
    // g_oneshot_pause = loop.setTimeout(8000, on_oneshot_pause_fire);
    // // After 3 seconds, reset that one-shot timer to fire 5 seconds later.
    // loop.setTimeout(3000, on_oneshot_pause_control);

    // // 2) Multiple interval timers with different periods.
    // loop.setInterval(500, on_interval_500ms);
    // loop.setInterval(1000, on_interval_1000ms);
    // loop.setInterval(2000, on_interval_2000ms);

    // 3) Interval timer pause / resume demo.
    start_interval_demo();
    loop.setTimeout(6000, on_interval_demo_pause);   // pause at ~6s
    loop.setTimeout(10000, on_interval_demo_resume); // resume at 10s

    std::cout << "entering event loop, press Ctrl+C to exit." << std::endl;

    loop.run();

    return 0;
}