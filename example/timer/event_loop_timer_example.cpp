#if defined(_MSC_VER)
#pragma warning(disable : 4819)
#endif

/**
 * event_loop_timer 示例 + 按《测试文档指导说明.md》要点做的自检（非 GTest，退出码 0=全过）。
 *
 * 覆盖对照：
 * - 一、基础功能：边界（空 loop / 空 task / 零周期）、Create(nullptr)+Start/Stop（内部 EventLoopThread）
 * - 二、异步与生命周期：Token 立即销毁则回调不触发；Post 内临时 Token 析构会 cancel；weak_ptr 安全用法
 * - 三、并发：多线程 Post，在 loop 线程内 schedule_after；重入（Post 内再调度，token 延长存活）
 * - 四、压力：可选环境变量 ITFLEE_TIMER_STRESS_COUNT（默认 500）创建-取消循环
 * - 五、性能：可选 ITFLEE_TIMER_LATENCY_SAMPLES 打印平均延迟（默认 0 关闭）
 */

#include "base/eventloop/event_loop_interface.h"
#include "time/event_loop_timer.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

#if defined(_WIN32)
/** 将控制台设为 UTF-8，避免中文在 PowerShell/cmd 下按系统默认代码页误解析。 */
static void win32_console_utf8() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}
#endif


using SteadyClock = std::chrono::steady_clock;

using TimerH = std::shared_ptr<itflee::EventLoopTimer>;

/** 自检用：等价于已从头文件移除的 schedule_* free 函数（基于 EventLoopTimer）。 */
inline TimerH schedule_after_on_loop(
    const std::shared_ptr<itflee::EventLoopInterface>& loop,
    std::chrono::steady_clock::duration delay,
    itflee::EventLoopInterface::Task task) {
    if (!loop || !task) {
        return nullptr;
    }
    auto t = itflee::EventLoopTimer::Create(loop);
    if (!t->StartOnce(delay, std::move(task))) {
        return nullptr;
    }
    return t;
}

inline TimerH schedule_every_on_loop(
    const std::shared_ptr<itflee::EventLoopInterface>& loop,
    std::chrono::steady_clock::duration period,
    itflee::EventLoopInterface::Task task) {
    if (!loop || !task || period <= std::chrono::steady_clock::duration::zero()) {
        return nullptr;
    }
    auto t = itflee::EventLoopTimer::Create(loop);
    if (!t->StartRepeating(period, std::move(task))) {
        return nullptr;
    }
    return t;
}

int g_failures = 0;

void expect(bool ok, const char* msg) {
    if (!ok) {
        std::cerr << "[FAIL] " << msg << '\n';
        ++g_failures;
    }
}

int env_int(const char* name, int default_value) {
#if defined(_WIN32)
    char* buf = nullptr;
    size_t sz  = 0;
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

/** 独立线程 Run，析构时 Stop+Join。 */
class LoopRunner {
public:
    LoopRunner()
        : loop_(itflee::EventLoopInterface::Create())
        , th_([this]() { loop_->Run(); }) {
        const auto deadline = SteadyClock::now() + std::chrono::seconds(5);
        while (!loop_->IsRunning()) {
            if (SteadyClock::now() > deadline) {
                throw std::runtime_error("LoopRunner: timeout waiting for IsRunning");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    LoopRunner(const LoopRunner&)            = delete;
    LoopRunner& operator=(const LoopRunner&) = delete;

    ~LoopRunner() {
        try {
            if (loop_ && loop_->IsRunning()) {
                loop_->Stop();
            }
        } catch (...) {
        }
        if (th_.joinable()) {
            th_.join();
        }
    }

    std::shared_ptr<itflee::EventLoopInterface> loop() const { return loop_; }

private:
    std::shared_ptr<itflee::EventLoopInterface> loop_;
    std::thread                          th_;
};

// ---------- 一、基础功能 / 边界 ----------

void test_boundary_null_and_empty_task() {
    std::cout << "[check] 边界：空 loop / 空 task / 零周期\n";
    using namespace std::chrono_literals;

    expect(schedule_after_on_loop(nullptr, 10ms, [] {}) == nullptr, "schedule_after: null loop -> nullptr");
    expect(schedule_every_on_loop(nullptr, 10ms, [] {}) == nullptr, "schedule_every: null loop -> nullptr");

    LoopRunner r;
    auto loop = r.loop();
    expect(schedule_after_on_loop(loop, 10ms, nullptr) == nullptr, "schedule_after: null task -> nullptr");
    expect(schedule_every_on_loop(loop, 10ms, nullptr) == nullptr, "schedule_every: null task -> nullptr");
    expect(schedule_every_on_loop(loop, 0ms, [] {}) == nullptr, "schedule_every: zero period -> nullptr");

    auto tmr = itflee::EventLoopTimer::Create(loop);
    expect(!tmr->StartRepeating(std::chrono::steady_clock::duration::zero(), [] {}),
        "EventLoopTimer::StartRepeating: zero period -> false");
    expect(!tmr->StartOnce(std::chrono::milliseconds(1), nullptr), "EventLoopTimer::StartOnce: null cb -> false");
}

/** Create(nullptr) 内部持有 EventLoopThread：构造自动后台 Run()，析构自动停线程；无需外部 LoopRunner。 */
void test_create_nullptr_start_stop() {
    std::cout << "[check] Create(nullptr) auto-start/auto-stop（EventLoopThread）\n";
    using namespace std::chrono_literals;

    auto tmr = itflee::EventLoopTimer::Create();
    expect(static_cast<bool>(tmr->GetLoop()), "embedded: GetLoop non-null");

    std::atomic<bool> once_ok{false};
    expect(tmr->GetLoop()->IsRunning(), "embedded: IsRunning after Create");

    expect(tmr->StartOnceMs(100, [&]() {
               once_ok.store(true, std::memory_order_relaxed);
           }),
        "embedded: StartOnceMs");

    const auto deadline = SteadyClock::now() + std::chrono::seconds(5);
    while (!once_ok.load(std::memory_order_relaxed)) {
        if (SteadyClock::now() > deadline) {
            expect(false, "embedded: timeout waiting one-shot");
            break;
        }
        std::this_thread::sleep_for(10ms);
    }

    std::atomic<int> ticks{0};
    expect(tmr->StartRepeatingMs(30, [&]() {
               const int n = ticks.fetch_add(1, std::memory_order_relaxed) + 1;
               if (n >= 2) {
                   tmr->Cancel();
               }
           }),
        "embedded: StartRepeatingMs");
    const auto t1 = SteadyClock::now() + std::chrono::seconds(3);
    while (ticks.load(std::memory_order_relaxed) < 2 && SteadyClock::now() < t1) {
        std::this_thread::sleep_for(10ms);
    }
    expect(ticks.load(std::memory_order_relaxed) >= 2, "embedded: repeating ticks");

    tmr.reset();
}

// ---------- 二、Token 失效：立即销毁则回调不触发 ----------

void test_token_cancel_before_fire() {
    std::cout << "[check] Token 立即销毁，回调不应触发\n";
    using namespace std::chrono_literals;

    LoopRunner r;
    auto loop = r.loop();
    std::atomic<bool> fired{false};

    auto tok = schedule_after_on_loop(loop, std::chrono::seconds(30), [&]() {
        fired.store(true, std::memory_order_relaxed);
    });
    expect(static_cast<bool>(tok), "should get token");
    tok.reset();

    std::this_thread::sleep_for(150ms);
    expect(!fired.load(std::memory_order_relaxed), "canceled one-shot should not fire");
}

void test_token_lost_when_only_scheduled_inside_post() {
    std::cout << "[check] Post 回调内 schedule 若不保存 CancelToken，返回即析构并取消定时器\n";
    using namespace std::chrono_literals;

    LoopRunner r;
    auto loop = r.loop();
    std::atomic<int> fired{0};
    loop->Post([&]() {
        (void)schedule_after_on_loop(loop, 30ms, [&]() {
            fired.store(1, std::memory_order_relaxed);
        });
    });
    std::this_thread::sleep_for(200ms);
    expect(fired.load(std::memory_order_relaxed) == 0, "Post 内临时 token 不应让定时回调触发");
}

// ---------- 二、weak_ptr / 调用方提前释放 ----------

void test_weak_ptr_skip_when_destroyed() {
    std::cout << "[check] 回调内 weak_ptr 升级失败则跳过（模拟 UAF 防护用法）\n";
    using namespace std::chrono_literals;

    LoopRunner r;
    auto loop = r.loop();

    auto payload = std::make_shared<int>(42);
    std::weak_ptr<int> w = payload;

    std::atomic<bool> saw_upgrade{false};
    std::promise<void> done;
    auto tok = schedule_after_on_loop(loop, 80ms, [w, &done, &saw_upgrade]() {
        if (w.lock()) {
            saw_upgrade.store(true, std::memory_order_relaxed);
        }
        done.set_value();
    });
    payload.reset();
    (void)tok;
    expect(done.get_future().wait_for(std::chrono::seconds(2)) == std::future_status::ready, "weak_ptr test should finish");
    expect(!saw_upgrade.load(std::memory_order_relaxed), "payload 已释放，weak_ptr 不应升级成功");
}

// ---------- 一、状态 / Stop 之后不再依赖新定时完成 ----------

void test_after_stop_no_new_work_expected() {
    std::cout << "[check] Stop 后主线程仅验证已退出（不强制向已停 loop 投递）\n";
    LoopRunner r;
    auto loop = r.loop();
    loop->Stop();
    // LoopRunner 析构会 join；此处先显式等待 Run 结束
    while (loop->IsRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    expect(!loop->IsRunning(), "after Stop, IsRunning false");
}

// ---------- 三、并发：多线程 schedule ----------

void test_concurrent_schedule_after() {
    std::cout << "[check] 多线程 Post + 主线程批量定时（CancelToken 须存活至触发）\n";
    using namespace std::chrono_literals;

    auto loop = itflee::EventLoopInterface::Create();
    std::thread runner([loop]() { loop->Run(); });
    while (!loop->IsRunning()) {
        std::this_thread::sleep_for(1ms);
    }

    std::atomic<int> post_hits{0};
    const int threads = 4;
    const int per     = 100;
    const int post_expected = threads * per;

    std::vector<std::thread> workers;
    for (int t = 0; t < threads; ++t) {
        workers.emplace_back([loop, &post_hits, per]() {
            for (int i = 0; i < per; ++i) {
                loop->Post([&post_hits]() { post_hits.fetch_add(1, std::memory_order_relaxed); });
            }
        });
    }
    for (auto& w : workers) {
        w.join();
    }

    const auto t_posts = SteadyClock::now() + std::chrono::seconds(10);
    while (post_hits.load(std::memory_order_relaxed) < post_expected && SteadyClock::now() < t_posts) {
        std::this_thread::sleep_for(1ms);
    }
    expect(post_hits.load(std::memory_order_relaxed) == post_expected, "concurrent Post count mismatch");

    // 一次性定时器句柄若立即析构会 cancel；须保存在容器中直至触发。
    std::atomic<int> timer_hits{0};
    const int timer_n = 200;
    std::vector<TimerH> keep;
    keep.reserve(static_cast<size_t>(timer_n));
    for (int i = 0; i < timer_n; ++i) {
        keep.push_back(schedule_after_on_loop(loop, 2ms, [&timer_hits]() {
            timer_hits.fetch_add(1, std::memory_order_relaxed);
        }));
    }

    const auto t_timers = SteadyClock::now() + std::chrono::seconds(15);
    while (timer_hits.load(std::memory_order_relaxed) < timer_n && SteadyClock::now() < t_timers) {
        std::this_thread::sleep_for(1ms);
    }

    std::promise<void> settled;
    loop->Post([&settled, loop]() {
        settled.set_value();
        loop->Stop();
    });
    expect(settled.get_future().wait_for(std::chrono::seconds(30)) == std::future_status::ready,
           "concurrent test should stop cleanly");
    runner.join();

    expect(timer_hits.load(std::memory_order_relaxed) == timer_n, "timer batch count mismatch");
    keep.clear();
}

// ---------- 三、重入：Post 内再调度（token 须延长存活） ----------

void test_reentrant_post_with_saved_token() {
    std::cout << "[check] 重入：Post 回调内 schedule，token 存 shared_ptr 延长生命周期\n";
    using namespace std::chrono_literals;

    LoopRunner r;
    auto loop = r.loop();
    std::promise<void> inner_done;

    auto slot = std::make_shared<TimerH>();
    loop->Post([loop, slot, &inner_done]() {
        *slot = schedule_after_on_loop(loop, 15ms, [&inner_done]() { inner_done.set_value(); });
    });

    auto inner_fut = inner_done.get_future();
    expect(inner_fut.wait_for(std::chrono::seconds(2)) == std::future_status::ready,
           "reentrant scheduled callback should complete");
}

// ---------- 四、压力：创建-取消循环 ----------

void test_stress_create_cancel() {
    const int n = env_int("ITFLEE_TIMER_STRESS_COUNT", 500);
    std::cout << "[check] 压力：创建-取消 x " << n << "（可用 ITFLEE_TIMER_STRESS_COUNT 调整）\n";
    using namespace std::chrono_literals;

    LoopRunner r;
    auto loop = r.loop();
    for (int i = 0; i < n; ++i) {
        auto tok = schedule_after_on_loop(loop, std::chrono::hours(24), []() {});
        tok.reset();
    }
    std::promise<void> done;
    loop->Post([&done, loop]() {
        done.set_value();
        loop->Stop();
    });
    expect(done.get_future().wait_for(std::chrono::seconds(30)) == std::future_status::ready,
           "stress drain should complete");
}

// ---------- 五、延迟采样（可选） ----------

void test_latency_optional() {
    const int samples = env_int("ITFLEE_TIMER_LATENCY_SAMPLES", 0);
    if (samples <= 0) {
        return;
    }
    std::cout << "[check] 性能：平均延迟采样 n=" << samples << '\n';
    using namespace std::chrono_literals;

    LoopRunner r;
    auto loop = r.loop();
    long long sum_us = 0;

    for (int i = 0; i < samples; ++i) {
        const auto t0 = SteadyClock::now();
        std::promise<void> p;
        auto fut = p.get_future();
        auto tok = schedule_after_on_loop(loop, 0ms, [&p, t0, &sum_us]() {
            const auto us = std::chrono::duration_cast<std::chrono::microseconds>(SteadyClock::now() - t0).count();
            sum_us += us;
            p.set_value();
        });
        (void)tok;
        fut.wait_for(std::chrono::seconds(1));
    }

    std::promise<void> stop_p;
    auto hold_stop = schedule_after_on_loop(loop, 50ms, [&stop_p, loop]() {
        stop_p.set_value();
        loop->Stop();
    });
    (void)hold_stop;
    stop_p.get_future().wait_for(std::chrono::seconds(2));

    std::cout << "  avg latency " << (static_cast<double>(sum_us) / samples) << " us\n";
}

// ---------- 演示路径（开心路径） ----------

void demo_happy_path() {
    std::cout << "[demo] EventLoopTimer + 多次一次性定时（原 schedule_* 语义）\n";
    using namespace std::chrono_literals;

    LoopRunner r;
    auto loop = r.loop();

    auto once_tok = schedule_after_on_loop(loop, 80ms, []() {
        std::cout << "  one-shot 80ms\n";
    });

    auto tmr = itflee::EventLoopTimer::Create(loop);
    std::atomic<int> tick{0};
    tmr->StartRepeatingMs(40, [&tick]() {
        std::cout << "  StartRepeatingMs tick " << (tick.fetch_add(1) + 1) << '\n';
    });

    auto cancel_repeat_tmr = schedule_after_on_loop(loop, 200ms, [tmr]() { tmr->Cancel(); });

    int every_n = 0;
    auto every_tok = schedule_every_on_loop(loop, 60ms, [&every_n]() {
        std::cout << "  repeating #" << ++every_n << '\n';
    });

    auto cancel_every = schedule_after_on_loop(loop, 220ms, [&every_tok]() { every_tok.reset(); });

    std::promise<void> done;
    auto stop_tok = schedule_after_on_loop(loop, 450ms, [&done, loop]() {
        loop->Stop();
        done.set_value();
    });

    done.get_future().wait_for(std::chrono::seconds(10));

    (void)once_tok;
    (void)cancel_repeat_tmr;
    (void)cancel_every;
    (void)stop_tok;
}

} // namespace

int main() {
#if defined(_WIN32)
    win32_console_utf8();
#endif
    try {
        test_boundary_null_and_empty_task();
        test_create_nullptr_start_stop();
        test_token_cancel_before_fire();
        test_token_lost_when_only_scheduled_inside_post();
        test_weak_ptr_skip_when_destroyed();
        test_after_stop_no_new_work_expected();
        test_concurrent_schedule_after();
        test_reentrant_post_with_saved_token();
        test_stress_create_cancel();
        test_latency_optional();
        demo_happy_path();

        if (g_failures != 0) {
            std::cerr << "[event_loop_timer_example] " << g_failures << " check(s) failed.\n";
            return 1;
        }
        std::cout << "[event_loop_timer_example] all checks passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[event_loop_timer_example] exception: " << e.what() << '\n';
        return 1;
    }
}
