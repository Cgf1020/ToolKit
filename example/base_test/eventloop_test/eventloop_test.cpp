#if defined(_MSC_VER)
#pragma warning(disable : 4819) // 源文件字符集警告：不影响功能，避免噪音
#endif

#include <atomic>
#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <future>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <time.h>

#include "base/eventloop/event_loop_interface.h"
#include "time/event_loop_timer.h"

#ifndef _WIN32
#include <boost/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#endif

using itflee::EventLoopInterface;

namespace el_test_timer {

using TimerHandle = itflee::EventLoopInterface::CancelToken;

/** 测试用：调用 EventLoopInterface::ScheduleAfter。 */
inline TimerHandle schedule_after_on_loop(
    const std::shared_ptr<itflee::EventLoopInterface>& loop,
    std::chrono::steady_clock::duration delay,
    itflee::EventLoopInterface::Task task) {
    if (!loop || !task) {
        return nullptr;
    }
    return loop->ScheduleAfter(delay, std::move(task));
}

/** 测试用：调用 EventLoopInterface::ScheduleEvery。 */
inline TimerHandle schedule_every_on_loop(
    const std::shared_ptr<itflee::EventLoopInterface>& loop,
    std::chrono::steady_clock::duration period,
    itflee::EventLoopInterface::Task task) {
    if (!loop || !task || period <= std::chrono::steady_clock::duration::zero()) {
        return nullptr;
    }
    return loop->ScheduleEvery(period, std::move(task));
}

} // namespace el_test_timer

static void expect(bool cond, const char* msg) {
    if (!cond) {
        throw std::runtime_error(msg);
    }
}

static std::string env_str(const char* name) {
#ifdef _WIN32
    // getenv_s: 返回 errno_t；输出到 buffer
    char buf[256] = {0};
    size_t outSize = 0;
    errno_t err = getenv_s(&outSize, buf, sizeof(buf), name);
    if (err != 0) {
        return {};
    }
    return std::string(buf);
#else
    const char* v = std::getenv(name);
    if (!v || !*v) return {};
    return std::string(v);
#endif
}

static int env_int(const char* name, int default_value) {
    const auto s = env_str(name);
    if (s.empty()) return default_value;
    return std::atoi(s.c_str());
}

[[maybe_unused]] static long long env_ll(const char* name, long long default_value) {
    const auto s = env_str(name);
    if (s.empty()) return default_value;
    return std::atoll(s.c_str());
}

static bool env_bool(const char* name, bool default_value) {
    const auto s0 = env_str(name);
    if (s0.empty()) return default_value;
    std::string s = s0;
    if (s == "1" || s == "true" || s == "TRUE" || s == "True") return true;
    if (s == "0" || s == "false" || s == "FALSE" || s == "False") return false;
    return default_value;
}

static std::chrono::steady_clock::time_point now_tp() {
    return std::chrono::steady_clock::now();
}

class LoopRunner {
public:
    LoopRunner()
        : loop_(EventLoopInterface::Create())
        , runner_([this] {
            loop_->Run();
        }) {
        // 等待 loop 进入运行态，避免后续 Schedule/Post 早于 Run() 执行。
        const auto deadline = now_tp() + std::chrono::seconds(5);
        while (!loop_->IsRunning()) {
            if (now_tp() > deadline) {
                throw std::runtime_error("LoopRunner: loop did not start in time");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    ~LoopRunner() {
        if (runner_.joinable()) {
            try {
                if (loop_ && loop_->IsRunning()) {
                    loop_->Stop();
                }
            } catch (...) {
                // 析构不抛异常
            }
            runner_.join();
        }
    }

    std::shared_ptr<EventLoopInterface> loop() const { return loop_; }

    void Join() {
        if (runner_.joinable()) {
            runner_.join();
        }
    }

    void StopAndJoin() {
        if (loop_ && loop_->IsRunning()) {
            loop_->Stop();
        }
        Join();
    }

private:
    std::shared_ptr<EventLoopInterface> loop_;
    std::thread runner_;
};

// 等待 promise 完成后立刻将 loop 停止，确保测试按预期退出。
[[maybe_unused]] static void stop_loop_when_done(const std::shared_ptr<EventLoopInterface>& loop,
                                                 std::promise<void>& done_promise) {
    // 该函数仅用于帮助封装调用点，不执行实际 stop。
    (void)loop;
    (void)done_promise;
}

static void post_dispatch_order_test() {
    std::cout << "[eventloop test] Post/Dispatch order..." << std::endl;

    // 1) Dispatch 在 loop 线程内应尽可能立即执行。
    {
        LoopRunner r;
        auto loop = r.loop();

        std::vector<std::string> order;
        std::mutex mtx;
        std::promise<void> done;

        loop->Post([&] {
            {
                std::lock_guard<std::mutex> lock(mtx);
                order.push_back("P_start");
            }

            loop->Dispatch([&] {
                std::lock_guard<std::mutex> lock(mtx);
                order.push_back("Dispatch_inline");
            });

            {
                std::lock_guard<std::mutex> lock(mtx);
                order.push_back("P_end");
            }

            done.set_value();
            loop->Stop();
        });

        done.get_future().get();
        r.Join();

        expect(order.size() == 3, "post_dispatch_order_test: dispatch order size mismatch");
        expect(order[0] == "P_start", "post_dispatch_order_test: dispatch order [0] mismatch");
        expect(order[1] == "Dispatch_inline", "post_dispatch_order_test: dispatch order [1] mismatch");
        expect(order[2] == "P_end", "post_dispatch_order_test: dispatch order [2] mismatch");
    }

    // 2) Post 在 loop 线程内应不会在当前回调里“就地执行”（应排队到后续循环）。
    {
        LoopRunner r;
        auto loop = r.loop();

        std::vector<std::string> order;
        std::mutex mtx;
        std::promise<void> done;

        loop->Post([&] {
            {
                std::lock_guard<std::mutex> lock(mtx);
                order.push_back("After_Post_call");
            }

            loop->Post([&] {
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    order.push_back("Post_from_loop_thread");
                }
                done.set_value();
                loop->Stop();
            });
        });

        done.get_future().get();
        r.Join();

        expect(order.size() == 2, "post_dispatch_order_test: post order size mismatch");
        expect(order[0] == "After_Post_call", "post_dispatch_order_test: post order [0] mismatch");
        expect(order[1] == "Post_from_loop_thread", "post_dispatch_order_test: post order [1] mismatch");
    }
}

static void post_event_test() {
    std::cout << "[eventloop test] PostEvent..." << std::endl;

    LoopRunner r;
    auto loop = r.loop();

    std::promise<void> done;
    std::atomic<bool> on_loop{false};
    static int user_tag;

    loop->PostEvent([&done, loop, &on_loop](itflee::Event* ev) {
        expect(ev != nullptr, "post_event_test: null Event*");
        ev->setUserData(&user_tag);
        expect(ev->userData() == static_cast<void*>(&user_tag), "post_event_test: userData");
        on_loop.store(loop->IsLoopThread(), std::memory_order_relaxed);
        done.set_value();
        loop->Stop();
    });

    done.get_future().get();
    r.Join();
    expect(on_loop.load(std::memory_order_relaxed), "post_event_test: not on loop thread");
}

static void event_loop_timer_wrapper_test() {
    std::cout << "[eventloop test] EventLoopTimer wrapper..." << std::endl;

    LoopRunner r;
    auto loop = r.loop();

    std::promise<void> done;
    std::atomic<int> ticks{0};

    auto tmr = itflee::EventLoopTimer::Create(loop);
    expect(!tmr->IsActive(), "event_loop_timer_wrapper_test: should start inactive");

    expect(tmr->StartOnceMs(30, [&] {
               ticks.fetch_add(1, std::memory_order_relaxed);
           }),
        "event_loop_timer_wrapper_test: StartOnceMs failed");
    expect(tmr->IsActive(), "event_loop_timer_wrapper_test: active after start");

    loop->Post([&] {
        tmr->Cancel();
        expect(!tmr->IsActive(), "event_loop_timer_wrapper_test: inactive after cancel");
        tmr->StartRepeatingMs(20, [&] {
            const int n = ticks.fetch_add(1, std::memory_order_relaxed) + 1;
            if (n >= 3) {
                tmr->Cancel();
                done.set_value();
                loop->Stop();
            }
        });
        expect(tmr->IsActive(), "event_loop_timer_wrapper_test: repeating armed");
    });

    done.get_future().get();
    r.Join();
    expect(ticks.load(std::memory_order_relaxed) >= 3, "event_loop_timer_wrapper_test: tick count");
}

/** Create(nullptr) 内部 EventLoopThread：构造自动 Run()，析构自动停线程。 */
static void event_loop_timer_embedded_thread_test() {
    std::cout << "[eventloop test] EventLoopTimer Create() auto-start/auto-stop (EventLoopThread)..." << std::endl;

    auto tmr = itflee::EventLoopTimer::Create();
    expect(tmr->GetLoop() != nullptr, "embedded_thread: GetLoop");

    std::promise<void> once_done;
    std::atomic<int> ticks{0};

    expect(tmr->GetLoop()->IsRunning(), "embedded_thread: IsRunning after Create");

    expect(tmr->StartOnceMs(50, [&] {
               ticks.fetch_add(1, std::memory_order_relaxed);
               once_done.set_value();
           }),
        "embedded_thread: StartOnceMs");

    const auto st = once_done.get_future().wait_for(std::chrono::seconds(5));
    expect(st == std::future_status::ready, "embedded_thread: one-shot timeout");
    expect(ticks.load(std::memory_order_relaxed) >= 1, "embedded_thread: one-shot tick");

    std::promise<void> repeat_done;
    expect(tmr->StartRepeatingMs(25, [&] {
               const int n = ticks.fetch_add(1, std::memory_order_relaxed) + 1;
               if (n >= 4) {
                   tmr->Cancel();
                   repeat_done.set_value();
               }
           }),
        "embedded_thread: StartRepeatingMs");

    const auto st_rep = repeat_done.get_future().wait_for(std::chrono::seconds(5));
    expect(st_rep == std::future_status::ready, "embedded_thread: repeat timeout");
    expect(ticks.load(std::memory_order_relaxed) >= 4, "embedded_thread: tick count");

    tmr.reset();
}

static void schedule_after_precision_test() {
    std::cout << "[eventloop test] ScheduleAfter precision..." << std::endl;

    const int delay_ms = env_int("ITFLEE_EVENTLOOP_DELAY_AFTER_MS", 100);
    const int tol_ms = env_int("ITFLEE_EVENTLOOP_SCHEDULE_AFTER_ERROR_TOLERANCE_MS",
#ifdef _WIN32
                               50
#else
                               10
#endif
    );

    LoopRunner r;
    auto loop = r.loop();

    std::promise<void> done;
    auto scheduled_at = now_tp();

    auto token = el_test_timer::schedule_after_on_loop(loop, std::chrono::milliseconds(delay_ms), [&] {
        const auto actual = now_tp();
        const auto target = scheduled_at + std::chrono::milliseconds(delay_ms);
        const auto err_ms = std::chrono::duration_cast<std::chrono::milliseconds>(actual - target).count();
        std::cout << "  delay expected=" << delay_ms << "ms, actual_error=" << err_ms << "ms" << std::endl;
        expect(std::llabs(err_ms) <= tol_ms, "schedule_after_precision_test: error exceeds tolerance");
        done.set_value();
        loop->Stop();
    });
    (void)token; // 保持 token 存活直到 test 结束

    done.get_future().get();
    r.Join();
}

static void schedule_every_drift_test() {
    std::cout << "[eventloop test] ScheduleEvery drift..." << std::endl;

    const int period_ms = env_int("ITFLEE_EVENTLOOP_EVERY_PERIOD_MS", 10);
    const int count = env_int("ITFLEE_EVENTLOOP_EVERY_COUNT", 100);
    // Windows 下非实时调度会产生额外开销，但不应出现“相邻间隔严重偏离 period”的系统性漂移。
    const int interval_avg_tol_ms = env_int(
        "ITFLEE_EVENTLOOP_EVERY_INTERVAL_AVG_TOLERANCE_MS",
#ifdef _WIN32
        10
#else
        5
#endif
    );
    const int interval_max_tol_ms = env_int(
        "ITFLEE_EVENTLOOP_EVERY_INTERVAL_MAX_TOLERANCE_MS",
#ifdef _WIN32
        25
#else
        15
#endif
    );

    LoopRunner r;
    auto loop = r.loop();

    std::promise<void> done;
    std::vector<std::chrono::steady_clock::time_point> fire_times;
    fire_times.reserve(count);

    std::atomic<int> fired{0};

    auto token = el_test_timer::schedule_every_on_loop(loop, std::chrono::milliseconds(period_ms), [&] {
        const int idx = fired.fetch_add(1) + 1; // idx is 1-based
        fire_times.push_back(now_tp());
        if (idx >= count) {
            // 触发次数到达后停止，避免后续回调继续累积。
            done.set_value();
            loop->Stop();
        }
    });
    (void)token;

    done.get_future().get();
    r.Join();

    expect(static_cast<int>(fire_times.size()) >= count, "schedule_every_drift_test: not enough callbacks");

    // 只取前 count 次数据，避免超出次数导致的统计偏差。
    fire_times.resize(static_cast<size_t>(count));

    std::vector<long long> interval_ms;
    interval_ms.reserve(static_cast<size_t>(count - 1));

    for (int i = 1; i < count; ++i) {
        const auto dt = std::chrono::duration_cast<std::chrono::microseconds>(fire_times[i] - fire_times[i - 1]).count();
        const long long ms = dt / 1000; // 下取整足够用于容差判断
        interval_ms.push_back(ms);
    }

    long long sum_dev_ms = 0;
    long long max_dev_ms = 0;
    for (auto ms : interval_ms) {
        const long long dev = std::llabs(ms - static_cast<long long>(period_ms));
        sum_dev_ms += dev;
        max_dev_ms = std::max(max_dev_ms, dev);
    }

    const double avg_dev_ms = static_cast<double>(sum_dev_ms) / static_cast<double>(std::max(1, count - 1));
    std::cout << "  period=" << period_ms << "ms, count=" << count
              << ", avg_interval_dev=" << avg_dev_ms << "ms"
              << ", max_interval_dev=" << max_dev_ms << "ms" << std::endl;

    expect(avg_dev_ms <= static_cast<double>(interval_avg_tol_ms),
           "schedule_every_drift_test: average interval deviation too large");
    expect(max_dev_ms <= interval_max_tol_ms,
           "schedule_every_drift_test: max interval deviation too large");
}

static void signal_handling_test() {
    std::cout << "[eventloop test] Signal handling..." << std::endl;

    if (env_bool("ITFLEE_EVENTLOOP_SKIP_SIGNALS", false)) {
        std::cout << "  skipped (ITFLEE_EVENTLOOP_SKIP_SIGNALS=1)" << std::endl;
        return;
    }

    [[maybe_unused]] const bool enable_sigterm = !env_bool("ITFLEE_EVENTLOOP_SKIP_SIGTERM_ON_WINDOWS", false);
    const auto timeout_ms = env_int("ITFLEE_EVENTLOOP_SIGNAL_TIMEOUT_MS", 1500);

    LoopRunner r;
    auto loop = r.loop();

    std::atomic<bool> called_int{false};
    std::atomic<bool> called_term{false};
    std::atomic<bool> finished{false};

    std::promise<void> done;

    auto finish_if_ready = [&] {
        const bool ok_int = called_int.load(std::memory_order_relaxed);
#ifdef _WIN32
        const bool ok_term = enable_sigterm ? called_term.load(std::memory_order_relaxed) : true;
#else
        const bool ok_term = called_term.load(std::memory_order_relaxed);
#endif
        if (ok_int && ok_term) {
            if (!finished.exchange(true)) {
                done.set_value();
                loop->Stop();
            }
        }
    };

    auto sigint_token = loop->OnSignal(SIGINT, [&](int /*signum*/) {
        called_int.store(true, std::memory_order_relaxed);
        finish_if_ready();
    });
    (void)sigint_token;

#ifndef _WIN32
    auto sigterm_token = loop->OnSignal(SIGTERM, [&](int /*signum*/) {
        called_term.store(true, std::memory_order_relaxed);
        finish_if_ready();
    });
    (void)sigterm_token;
#else
    EventLoopInterface::CancelToken sigterm_token;
    if (enable_sigterm) {
        sigterm_token = loop->OnSignal(SIGTERM, [&](int /*signum*/) {
            called_term.store(true, std::memory_order_relaxed);
            finish_if_ready();
        });
    }
    (void)sigterm_token;
#endif

    // 超时兜底，避免测试卡死在信号丢失的情况下。
    auto timeout_token = el_test_timer::schedule_after_on_loop(loop, std::chrono::milliseconds(timeout_ms), [&] {
        if (!finished.exchange(true)) {
            done.set_value();
            loop->Stop();
        }
    });
    (void)timeout_token;

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::raise(SIGINT);
#ifndef _WIN32
    std::raise(SIGTERM);
#else
    if (enable_sigterm) {
        std::raise(SIGTERM);
    }
#endif

    done.get_future().get();
    r.Join();

    expect(called_int.load(std::memory_order_relaxed), "signal_handling_test: SIGINT callback not called");
#ifndef _WIN32
    expect(called_term.load(std::memory_order_relaxed), "signal_handling_test: SIGTERM callback not called");
#else
    if (enable_sigterm) {
        expect(called_term.load(std::memory_order_relaxed), "signal_handling_test: SIGTERM callback not called (win)");
    }
#endif
}

static void stop_logic_test() {
    std::cout << "[eventloop test] Stop logic..." << std::endl;

    LoopRunner r;
    auto loop = r.loop();
    expect(loop->IsRunning(), "stop_logic_test: IsRunning should be true after Run starts");

    std::promise<void> done;
    loop->Post([&] {
        loop->Stop();
        done.set_value();
    });

    done.get_future().get();
    r.Join();
    expect(!loop->IsRunning(), "stop_logic_test: IsRunning should be false after Stop + Run return");
}

static void cancel_token_invalidation_test() {
    std::cout << "[eventloop test] CancelToken invalidation..." << std::endl;

    const int seconds = env_int("ITFLEE_EVENTLOOP_TOKEN_INVALIDATE_SECONDS", 5);
    LoopRunner r;
    auto loop = r.loop();

    std::atomic<bool> fired{false};
    std::promise<void> done;

    auto token = el_test_timer::schedule_after_on_loop(loop, std::chrono::seconds(seconds), [&] {
        fired.store(true, std::memory_order_relaxed);
    });
    token.reset(); // 立即销毁，触发取消

    auto stop_token = el_test_timer::schedule_after_on_loop(
        loop, std::chrono::seconds(seconds) + std::chrono::milliseconds(300), [&] {
            done.set_value();
            loop->Stop();
        });
    (void)stop_token;

    done.get_future().get();
    r.Join();

    expect(!fired.load(std::memory_order_relaxed), "cancel_token_invalidation_test: canceled timer still fired");
}

static void object_lifetime_uaf_prevention_test() {
    std::cout << "[eventloop test] Object lifetime safety..." << std::endl;

    const int delay_ms = env_int("ITFLEE_EVENTLOOP_OBJECT_LIFETIME_DELAY_MS", 50);
    LoopRunner r;
    auto loop = r.loop();

    std::promise<void> done;

    size_t use_count_in_cb = 0;
    std::weak_ptr<int> weak;
    auto sp = std::make_shared<int>(123);
    weak = sp;

    // 保持 CancelToken 存活直至回调触发。
    auto token = el_test_timer::schedule_after_on_loop(loop, std::chrono::milliseconds(delay_ms),
        [sp, &use_count_in_cb, &done, loop] {
            // sp 在回调中捕获，保证对象生命周期不被提前释放。
            use_count_in_cb = sp.use_count();
            expect(*sp == 123, "object_lifetime_uaf_prevention_test: value mismatch");
            done.set_value();
            loop->Stop();
        });

    done.get_future().get();

    // 等 loop 线程退出，确保异步回调已完全结束。
    r.Join();
    token.reset();

    expect(weak.lock() != nullptr, "object_lifetime_uaf_prevention_test: object destroyed too early");
    sp.reset();
    expect(weak.lock() == nullptr, "object_lifetime_uaf_prevention_test: object should be destroyed after callback");
    expect(use_count_in_cb >= 2, "object_lifetime_uaf_prevention_test: shared_ptr use_count unexpected");
}

static void signal_unregister_test() {
    std::cout << "[eventloop test] Signal unregister..." << std::endl;

    if (env_bool("ITFLEE_EVENTLOOP_SKIP_SIGNALS", false)) {
        std::cout << "  skipped (ITFLEE_EVENTLOOP_SKIP_SIGNALS=1)" << std::endl;
        return;
    }

    const int rounds = env_int("ITFLEE_EVENTLOOP_SIGNAL_UNREGISTER_ROUNDS", 200);
    const int wait_after_ms = env_int("ITFLEE_EVENTLOOP_SIGNAL_UNREGISTER_WAIT_MS", 5);

    LoopRunner r;
    auto loop = r.loop();

    std::atomic<int> called{0};

    for (int i = 0; i < rounds; ++i) {
        auto token = loop->OnSignal(SIGINT, [&](int /*signum*/) {
            called.fetch_add(1, std::memory_order_relaxed);
        });
        token.reset(); // 取消订阅
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_after_ms));
    }

    // 给异步注销一点时间
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::raise(SIGINT);

    // 稍后停止 loop
    std::promise<void> done;
    auto stop_token = el_test_timer::schedule_after_on_loop(loop, std::chrono::milliseconds(200), [&] {
        done.set_value();
        loop->Stop();
    });
    (void)stop_token;

    done.get_future().get();
    r.Join();

    expect(called.load(std::memory_order_relaxed) == 0, "signal_unregister_test: canceled handler was still called");
}

static void cross_thread_post_test() {
    std::cout << "[eventloop test] Cross-thread Post..." << std::endl;

    const int threads = env_int("ITFLEE_EVENTLOOP_CROSS_POST_THREADS", 10);
    const int per_thread = env_int("ITFLEE_EVENTLOOP_CROSS_POST_PER_THREAD", 10000);
    const int expected = threads * per_thread;

    LoopRunner r;
    auto loop = r.loop();

    std::atomic<int> counter{0};
    std::promise<void> done;

    std::vector<std::thread> workers;
    workers.reserve(threads);

    for (int t = 0; t < threads; ++t) {
        workers.emplace_back([&] {
            for (int i = 0; i < per_thread; ++i) {
                loop->Post([&counter] { counter.fetch_add(1, std::memory_order_relaxed); });
            }
        });
    }

    for (auto& th : workers) {
        th.join();
    }

    loop->Post([&] {
        expect(counter.load(std::memory_order_relaxed) == expected, "cross_thread_post_test: task count mismatch");
        done.set_value();
        loop->Stop();
    });

    done.get_future().get();
    r.Join();
}

static void reentrancy_post_callback_test() {
    std::cout << "[eventloop test] Re-entrancy (Post/ScheduleAfter)..." << std::endl;

    const int delay_ms = env_int("ITFLEE_EVENTLOOP_REENTRANCY_DELAY_MS", 30);

    LoopRunner r;
    auto loop = r.loop();

    std::atomic<int> step{0};
    std::promise<void> done;
    el_test_timer::TimerHandle after_timer;

    loop->Post([&] {
        expect(step.load(std::memory_order_relaxed) == 0, "reentrancy_post_callback_test: step init mismatch");
        step.store(1, std::memory_order_relaxed);

        // 在线程回调中再次 Post：应排队，不应死锁。
        loop->Post([&] {
            step.store(2, std::memory_order_relaxed);
        });

        // 在线程回调中定时调度：应可正常调度。
        after_timer = el_test_timer::schedule_after_on_loop(loop, std::chrono::milliseconds(delay_ms), [&] {
            step.store(3, std::memory_order_relaxed);
            done.set_value();
            loop->Stop();
        });
    });

    done.get_future().get();
    r.Join();

    expect(step.load(std::memory_order_relaxed) == 3, "reentrancy_post_callback_test: step did not reach end");
}

static void metrics_basic_test() {
    std::cout << "[eventloop test] Metrics basic..." << std::endl;

    LoopRunner r;
    auto loop = r.loop();

    loop->ResetMetrics();

    std::promise<void> done;
    std::atomic<int> step{0};
    el_test_timer::TimerHandle every_token;

    loop->Post([&] {
        step.fetch_add(1, std::memory_order_relaxed);
    });
    loop->Dispatch([&] {
        step.fetch_add(1, std::memory_order_relaxed);
    });
    loop->PostEvent([&](itflee::Event*) {
        step.fetch_add(1, std::memory_order_relaxed);
    });

    auto after_token = loop->ScheduleAfter(std::chrono::milliseconds(20), [&] {
        step.fetch_add(1, std::memory_order_relaxed);
    });
    expect(static_cast<bool>(after_token), "metrics_basic_test: ScheduleAfter token is null");

    every_token = loop->ScheduleEvery(std::chrono::milliseconds(10), [&] {
        const int n = step.fetch_add(1, std::memory_order_relaxed) + 1;
        if (n >= 6) {
            every_token.reset();
            done.set_value();
            loop->Stop();
        }
    });
    expect(static_cast<bool>(every_token), "metrics_basic_test: ScheduleEvery token is null");

    done.get_future().get();
    r.Join();

    const auto m = loop->GetMetrics();
    expect(m.posted_tasks >= 6, "metrics_basic_test: posted_tasks too small");
    expect(m.completed_tasks >= 6, "metrics_basic_test: completed_tasks too small");
    expect(m.pending_tasks == 0, "metrics_basic_test: pending_tasks should be zero");
    expect(m.max_pending_tasks >= 1, "metrics_basic_test: max_pending_tasks invalid");
    expect(m.avg_queue_delay_us <= m.max_queue_delay_us, "metrics_basic_test: avg/max delay relation invalid");

    loop->ResetMetrics();
    const auto m2 = loop->GetMetrics();
    expect(m2.posted_tasks == 0, "metrics_basic_test: posted_tasks reset failed");
    expect(m2.completed_tasks == 0, "metrics_basic_test: completed_tasks reset failed");
    expect(m2.failed_tasks == 0, "metrics_basic_test: failed_tasks reset failed");
    expect(m2.pending_tasks == 0, "metrics_basic_test: pending_tasks reset failed");
    expect(m2.max_pending_tasks == 0, "metrics_basic_test: max_pending_tasks reset failed");
    expect(m2.avg_queue_delay_us == 0, "metrics_basic_test: avg_queue_delay_us reset failed");
    expect(m2.max_queue_delay_us == 0, "metrics_basic_test: max_queue_delay_us reset failed");
}

static void reentrancy_stop_from_post_callback_test() {
    std::cout << "[eventloop test] Re-entrancy (Stop from Post)..." << std::endl;

    LoopRunner r;
    auto loop = r.loop();

    std::promise<void> done;
    loop->Post([&] {
        loop->Stop();
        done.set_value();
    });

    done.get_future().get();
    r.Join();
    expect(!loop->IsRunning(), "reentrancy_stop_from_post_callback_test: loop should stop");
}

static void cancel_token_race_test() {
    std::cout << "[eventloop test] Concurrent cancel race..." << std::endl;

    const int iterations = env_int("ITFLEE_EVENTLOOP_CANCEL_RACE_ITER", 20);
    const int delay_ms = env_int("ITFLEE_EVENTLOOP_CANCEL_RACE_DELAY_MS", 80);
    const int cancel_jitter_ms = env_int("ITFLEE_EVENTLOOP_CANCEL_RACE_JITTER_MS", 3);

    LoopRunner r;
    auto loop = r.loop();

    std::atomic<int> fired_count{0};

    for (int i = 0; i < iterations; ++i) {
        std::atomic<bool> fired{false};
        auto token = el_test_timer::schedule_after_on_loop(loop, std::chrono::milliseconds(delay_ms), [&] {
            fired.store(true, std::memory_order_relaxed);
            fired_count.fetch_add(1, std::memory_order_relaxed);
        });

        // 将 token 移交给 canceller 线程，确保 main 线程不再额外持有引用，能够真正触发取消。
        std::thread canceller([token = std::move(token), delay_ms, cancel_jitter_ms]() mutable {
            // 尽可能靠近触发点释放 token。
            const int sleep_ms = std::max(0, delay_ms - cancel_jitter_ms);
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
            token.reset();
        });
        canceller.join();

        // 等待本轮 timer 回调/取消处理落地。
        const auto deadline = now_tp() + std::chrono::milliseconds(delay_ms + 30);
        while (!fired.load(std::memory_order_relaxed) && now_tp() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // 结束 loop，验证不崩溃即通过。
    r.StopAndJoin();
    std::cout << "  timer fired count=" << fired_count.load(std::memory_order_relaxed)
              << " / iterations=" << iterations << std::endl;
}

static void task_backlog_test() {
    std::cout << "[eventloop test] Task backlog stress..." << std::endl;

    const bool skip_heavy = env_bool("ITFLEE_EVENTLOOP_SKIP_HEAVY", false);
    if (skip_heavy) {
        std::cout << "  skipped (ITFLEE_EVENTLOOP_SKIP_HEAVY=1)" << std::endl;
        return;
    }

    const int tasks = env_int("ITFLEE_EVENTLOOP_BACKLOG_TASKS", 1000000);
    const int final_timeout_ms = env_int("ITFLEE_EVENTLOOP_BACKLOG_TIMEOUT_MS", 30000);

    LoopRunner r;
    auto loop = r.loop();

    std::atomic<int> counter{0};
    std::promise<void> done;

    const auto start = now_tp();
    for (int i = 0; i < tasks; ++i) {
        loop->Post([&counter] { counter.fetch_add(1, std::memory_order_relaxed); });
    }

    loop->Post([&] {
        expect(counter.load(std::memory_order_relaxed) == tasks, "task_backlog_test: task count mismatch");
        done.set_value();
        loop->Stop();
    });

    auto fut = done.get_future();
    if (fut.wait_for(std::chrono::milliseconds(final_timeout_ms)) != std::future_status::ready) {
        throw std::runtime_error("task_backlog_test: timeout waiting for completion");
    }

    fut.get();
    r.Join();

    const auto end = now_tp();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "  posted=" << tasks << ", total_time=" << ms << "ms" << std::endl;
}

static void signal_storm_test() {
    std::cout << "[eventloop test] Signal storm..." << std::endl;

    if (env_bool("ITFLEE_EVENTLOOP_SKIP_SIGNALS", false)) {
        std::cout << "  skipped (ITFLEE_EVENTLOOP_SKIP_SIGNALS=1)" << std::endl;
        return;
    }

    const bool skip_heavy = env_bool("ITFLEE_EVENTLOOP_SKIP_HEAVY", false);
    if (skip_heavy) {
        std::cout << "  skipped (ITFLEE_EVENTLOOP_SKIP_HEAVY=1)" << std::endl;
        return;
    }

    const int count = env_int("ITFLEE_EVENTLOOP_SIGNAL_STORM_COUNT", 500);
    const int duration_ms = env_int("ITFLEE_EVENTLOOP_SIGNAL_STORM_DURATION_MS", 1000);

    const int min_expected = env_int("ITFLEE_EVENTLOOP_SIGNAL_STORM_MIN_EXPECTED", 1);

    LoopRunner r;
    auto loop = r.loop();

    std::atomic<int> called{0};
    std::promise<void> done;
    std::atomic<bool> finished{false};

    auto sigint_token = loop->OnSignal(SIGINT, [&](int /*signum*/) {
        called.fetch_add(1, std::memory_order_relaxed);
    });
    (void)sigint_token;

    auto timeout_token = el_test_timer::schedule_after_on_loop(loop, std::chrono::milliseconds(duration_ms + 200), [&] {
        if (!finished.exchange(true)) {
            done.set_value();
            loop->Stop();
        }
    });
    (void)timeout_token;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const auto start = now_tp();
    for (int i = 0; i < count; ++i) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now_tp() - start).count() > duration_ms) break;
        std::raise(SIGINT);
    }

    done.get_future().get();
    r.Join();

    std::cout << "  received callbacks=" << called.load(std::memory_order_relaxed) << std::endl;
    expect(called.load(std::memory_order_relaxed) >= min_expected,
           "signal_storm_test: no signal callbacks observed");
}

#ifdef _WIN32
#include <windows.h>
#endif

static void empty_turn_power_test() {
    std::cout << "[eventloop test] Empty run (power/no busy wait)..." << std::endl;

    const bool skip_heavy = env_bool("ITFLEE_EVENTLOOP_SKIP_HEAVY", false);
    if (skip_heavy) {
        std::cout << "  skipped (ITFLEE_EVENTLOOP_SKIP_HEAVY=1)" << std::endl;
        return;
    }

    const int seconds = env_int("ITFLEE_EVENTLOOP_EMPTY_RUN_SECONDS", 5);

    LoopRunner r;
    auto loop = r.loop();

    // 为了兼容文档描述：这里不投递任何 Task，仅等待一段时间后 Stop。
    auto cpu_before = 0.0;
    auto cpu_after = 0.0;

    auto measure_cpu = [&]() -> double {
#ifdef _WIN32
        FILETIME creation, exit, kernel, user;
        if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) return 0.0;
        ULONGLONG k = (static_cast<ULONGLONG>(kernel.dwHighDateTime) << 32) | kernel.dwLowDateTime;
        ULONGLONG u = (static_cast<ULONGLONG>(user.dwHighDateTime) << 32) | user.dwLowDateTime;
        // 100ns units -> seconds
        return static_cast<double>(k + u) / 1e7;
#else
        timespec ts{};
        if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) != 0) return 0.0;
        return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1e9;
#endif
    };

    cpu_before = measure_cpu();

    std::cout << "  waiting " << seconds
              << "s with no tasks (expected quiet period, not a hang)..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    loop->Stop();

    r.Join();

    cpu_after = measure_cpu();
    const double cpu_sec = std::max(0.0, cpu_after - cpu_before);
    const double load = cpu_sec / static_cast<double>(seconds);
    std::cout << "  empty_run_seconds=" << seconds << ", approx CPU seconds=" << cpu_sec
              << ", load_ratio=" << load << std::endl;

    std::cout << "  Note: 若在 Linux 可用 top/perf 观察 CPU 接近 0%" << std::endl;
}

static void fd_exhaustion_tcp_listen_test() {
    std::cout << "[eventloop test] FD exhaustion (tcp listen)..." << std::endl;

#ifdef _WIN32
    std::cout << "  skipped on Windows (fd exhaustion simulation not reliable)" << std::endl;
    return;
#else
    const bool skip_heavy = env_bool("ITFLEE_EVENTLOOP_SKIP_HEAVY", false);
    if (skip_heavy) {
        std::cout << "  skipped (ITFLEE_EVENTLOOP_SKIP_HEAVY=1)" << std::endl;
        return;
    }

    // 打开大量 /dev/null，尽量逼近 FD 上限（失败即停止，不强行占满，避免系统不稳定）。
    std::vector<int> devnull_fds;
    devnull_fds.reserve(1024);

    const int cap = env_int("ITFLEE_EVENTLOOP_FD_EXHAUSTION_CAP", 10000);
    for (int i = 0; i < cap; ++i) {
        FILE* f = std::fopen("/dev/null", "r");
        if (!f) break;
        devnull_fds.push_back(fileno(f));
        // 这里不 fclose，故意持有直到测试结束。
    }

    LoopRunner r;
    auto loop = r.loop();

    std::promise<void> done;
    std::atomic<bool> threw{false};
    std::atomic<bool> caught_error{false};

    loop->Post([&] {
        try {
            // 在 eventloop 的执行器上尝试创建 acceptor 并 listen。
            using executor_type = boost::asio::io_context::executor_type;
            executor_type ex = std::any_cast<executor_type>(loop->GetNativeExecutor());
#if BOOST_VERSION >= 108000
            boost::asio::any_io_executor any_ex(ex);
            boost::asio::ip::tcp::acceptor acc(any_ex);
#else
            auto& ioc = static_cast<boost::asio::io_context&>(ex.context());
            boost::asio::ip::tcp::acceptor acc(ioc);
#endif
            boost::system::error_code ec;
            acc.open(boost::asio::ip::tcp::v4(), ec);
            if (ec) {
                caught_error.store(true, std::memory_order_relaxed);
            } else {
                boost::asio::ip::tcp::endpoint ep(boost::asio::ip::tcp::v4(), 0);
                acc.bind(ep, ec);
                if (!ec) {
                    acc.listen(boost::asio::socket_base::max_listen_connections, ec);
                }
                if (ec) {
                    caught_error.store(true, std::memory_order_relaxed);
                }
            }
        } catch (...) {
            threw.store(true, std::memory_order_relaxed);
        }
        done.set_value();
        loop->Stop();
    });

    done.get_future().get();
    r.Join();

    // fd 占满不一定一定触发错误，所以这里只验证“不会抛到 main 直接 abort”，并且错误可被捕获则更好。
    expect(!threw.load(std::memory_order_relaxed), "fd_exhaustion_tcp_listen_test: exception escaped loop task");
    std::cout << "  caught_error=" << (caught_error.load(std::memory_order_relaxed) ? "true" : "false") << std::endl;

    for (int fd : devnull_fds) {
        (void)fd;
    }
#endif
}

static void latency_post_average_test() {
    std::cout << "[eventloop test] Post latency avg..." << std::endl;

    const bool skip_heavy = env_bool("ITFLEE_EVENTLOOP_SKIP_PERF", false);
    if (skip_heavy) {
        std::cout << "  skipped (ITFLEE_EVENTLOOP_SKIP_PERF=1)" << std::endl;
        return;
    }

    const int tasks = env_int("ITFLEE_EVENTLOOP_LATENCY_TASKS", 50000);

    LoopRunner r;
    auto loop = r.loop();

    std::atomic<long long> sum_us{0};
    std::atomic<int> done_count{0};
    std::promise<void> done;

    for (int i = 0; i < tasks; ++i) {
        const auto post_time = now_tp();
        loop->Post([&, post_time] {
            const auto exec_time = now_tp();
            const auto us = std::chrono::duration_cast<std::chrono::microseconds>(exec_time - post_time).count();
            sum_us.fetch_add(us, std::memory_order_relaxed);
            const int c = done_count.fetch_add(1, std::memory_order_relaxed) + 1;
            if (c == tasks) {
                const double avg = static_cast<double>(sum_us.load(std::memory_order_relaxed)) / tasks;
                std::cout << "  avg latency=" << avg << " us" << std::endl;
                done.set_value();
                loop->Stop();
            }
        });
    }

    done.get_future().get();
    r.Join();

    expect(done_count.load(std::memory_order_relaxed) == tasks, "latency_post_average_test: not all tasks executed");
}

static void throughput_empty_tasks_test() {
    std::cout << "[eventloop test] Throughput empty tasks..." << std::endl;

    const bool skip_heavy = env_bool("ITFLEE_EVENTLOOP_SKIP_PERF", false);
    if (skip_heavy) {
        std::cout << "  skipped (ITFLEE_EVENTLOOP_SKIP_PERF=1)" << std::endl;
        return;
    }

    const int tasks = env_int("ITFLEE_EVENTLOOP_THROUGHPUT_TASKS", 200000);
    LoopRunner r;
    auto loop = r.loop();

    std::promise<void> done;
    std::atomic<int> done_count{0};

    const auto start = now_tp();
    for (int i = 0; i < tasks; ++i) {
        loop->Post([&, i] {
            (void)i;
            const int c = done_count.fetch_add(1, std::memory_order_relaxed) + 1;
            if (c == tasks) {
                const auto end = now_tp();
                const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                const double seconds = static_cast<double>(ms) / 1000.0;
                const double tps = seconds > 0 ? tasks / seconds : static_cast<double>(tasks);
                std::cout << "  " << tasks << " empty tasks in " << ms << " ms, throughput=" << tps
                          << " tasks/s" << std::endl;
                done.set_value();
                loop->Stop();
            }
        });
    }

    done.get_future().get();
    r.Join();

    expect(done_count.load(std::memory_order_relaxed) == tasks, "throughput_empty_tasks_test: not all tasks executed");
}

static void high_freq_timer_context_switch_test() {
    std::cout << "[eventloop test] High-freq timer CPU load..." << std::endl;

    const bool skip_heavy = env_bool("ITFLEE_EVENTLOOP_SKIP_PERF", false);
    if (skip_heavy) {
        std::cout << "  skipped (ITFLEE_EVENTLOOP_SKIP_PERF=1)" << std::endl;
        return;
    }

    const int period_ms = env_int("ITFLEE_EVENTLOOP_HIGH_FREQ_PERIOD_MS", 1);
    const int duration_seconds = env_int("ITFLEE_EVENTLOOP_HIGH_FREQ_DURATION_SECONDS", 2);

    LoopRunner r;
    auto loop = r.loop();

    std::atomic<int> fired{0};
    std::promise<void> done;

    auto cpu_before = 0.0;
    auto cpu_after = 0.0;

    auto measure_cpu = [&]() -> double {
#ifdef _WIN32
        FILETIME creation, exit, kernel, user;
        if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) return 0.0;
        ULONGLONG k = (static_cast<ULONGLONG>(kernel.dwHighDateTime) << 32) | kernel.dwLowDateTime;
        ULONGLONG u = (static_cast<ULONGLONG>(user.dwHighDateTime) << 32) | user.dwLowDateTime;
        return static_cast<double>(k + u) / 1e7;
#else
        timespec ts{};
        if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) != 0) return 0.0;
        return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1e9;
#endif
    };

    cpu_before = measure_cpu();

    auto every_token = el_test_timer::schedule_every_on_loop(loop, std::chrono::milliseconds(period_ms), [&] {
        fired.fetch_add(1, std::memory_order_relaxed);
    });
    (void)every_token;

    auto after_token = el_test_timer::schedule_after_on_loop(loop, std::chrono::seconds(duration_seconds), [&] {
        cpu_after = measure_cpu();
        done.set_value();
        loop->Stop();
    });
    (void)after_token;

    done.get_future().get();
    r.Join();

    const double cpu_sec = std::max(0.0, cpu_after - cpu_before);
    const double load = cpu_sec / static_cast<double>(duration_seconds);
    std::cout << "  fired=" << fired.load(std::memory_order_relaxed)
              << ", approx CPU load ratio=" << load << std::endl;
}

int main() {
    try {
        // 1. 核心功能测试
        post_dispatch_order_test();
        post_event_test();
        event_loop_timer_wrapper_test();
        event_loop_timer_embedded_thread_test();
        schedule_after_precision_test();
        schedule_every_drift_test();
        signal_handling_test();
        stop_logic_test();

        // 2. 异步生命周期与内存安全
        cancel_token_invalidation_test();
        object_lifetime_uaf_prevention_test();
        signal_unregister_test();

        // 3. 多线程与并发竞争
        cross_thread_post_test();
        reentrancy_post_callback_test();
        reentrancy_stop_from_post_callback_test();
        cancel_token_race_test();
        metrics_basic_test();

        // 4. 压力与边界条件
        task_backlog_test();
        signal_storm_test();
        empty_turn_power_test();
        fd_exhaustion_tcp_listen_test();

        // 5. 性能指标
        latency_post_average_test();
        throughput_empty_tasks_test();
        high_freq_timer_context_switch_test();

        std::cout << "[eventloop test] all tests passed." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[eventloop test] FAILED: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "[eventloop test] FAILED: unknown exception" << std::endl;
        return 1;
    }

    // 默认不等待输入，避免脚本/CI 里卡死。
    const bool wait_enter = env_bool("ITFLEE_EVENTLOOP_WAIT_ENTER", false);
    if (wait_enter) {
        std::cout << "Press Enter to continue...";
        std::cin.get();
    }
    return 0;
}

