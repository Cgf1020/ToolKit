#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <numeric>
#include <stdexcept>
#include <mutex>

#include "threadpool/threadpool_invoke_interface.h"

using itflee::ThreadPoolInvokeInterface;
using itflee::TaskPriority;
using itflee::WaitDeadlockError;

static void expect(bool cond, const char* msg) {
    if (!cond) {
        throw std::runtime_error(msg);
    }
}

static void basic_functionality_test(const std::shared_ptr<ThreadPoolInvokeInterface>& pool) {
    std::cout << "[threadpool test] basic functionality..." << std::endl;

    auto f1 = pool->Invoke([] { return 42; });
    auto f2 = pool->Invoke([](int x, int y) { return x + y; }, 10, 32);

    expect(f1.get() == 42, "basic_functionality_test: f1 != 42");
    expect(f2.get() == 42, "basic_functionality_test: f2 != 42");
}

static void parallel_accumulate_test(const std::shared_ptr<ThreadPoolInvokeInterface>& pool) {
    std::cout << "[threadpool test] parallel accumulate..." << std::endl;

    const std::size_t n = 100000;
    std::vector<int> data(n);
    std::iota(data.begin(), data.end(), 1);

    const std::size_t num_tasks = 16;
    std::vector<std::future<long long>> futures;
    futures.reserve(num_tasks);

    const std::size_t block_size = (n + num_tasks - 1) / num_tasks;
    for (std::size_t i = 0; i < num_tasks; ++i) {
        const std::size_t begin = i * block_size;
        if (begin >= n) break;
        const std::size_t end = std::min(n, begin + block_size);
        futures.emplace_back(
            pool->Invoke([begin, end, &data]() {
                long long sum = 0;
                for (std::size_t j = begin; j < end; ++j) {
                    sum += data[j];
                }
                return sum;
            })
        );
    }

    long long total = 0;
    for (auto& f : futures) {
        total += f.get();
    }

    const long long expected = static_cast<long long>(n) * (n + 1) / 2;
    expect(total == expected, "parallel_accumulate_test: wrong sum");
}

static void invoke_sync_reentrancy_test(const std::shared_ptr<ThreadPoolInvokeInterface>& pool) {
    std::cout << "[threadpool test] InvokeSync reentrancy..." << std::endl;

    std::atomic<int> counter{0};
    const int depth = 4;

    std::function<void(int)> recurse;
    recurse = [&](int d) {
        counter.fetch_add(1, std::memory_order_relaxed);
        if (d <= 0) return;
        // 在线程池 worker 线程内再次调用 InvokeSync，应直接执行，避免死锁
        pool->InvokeSync(recurse, d - 1);
    };

    pool->InvokeSync(recurse, depth);

    const int expected = depth + 1;
    expect(counter.load(std::memory_order_relaxed) == expected,
           "invoke_sync_reentrancy_test: counter mismatch");
}

static void wait_and_clear_test(const std::shared_ptr<ThreadPoolInvokeInterface>& pool) {
    std::cout << "[threadpool test] Wait & ClearTask..." << std::endl;

    std::atomic<int> finished{0};

    for (int i = 0; i < 8; ++i) {
        pool->Invoke([&finished] {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            finished.fetch_add(1, std::memory_order_relaxed);
        });
    }

    pool->Wait();
    expect(finished.load(std::memory_order_relaxed) == 8,
           "wait_and_clear_test: Wait did not wait for all tasks");

    // 再提交一些任务，然后 ClearTask，验证不会死锁且 Wait 能返回
    for (int i = 0; i < 8; ++i) {
        pool->Invoke([] {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        });
    }

    pool->ClearTask();
    pool->Wait();
}

static void priority_order_test(const std::shared_ptr<ThreadPoolInvokeInterface>& pool) {
    std::cout << "[threadpool test] priority order..." << std::endl;

    std::mutex mtx;
    std::vector<int> order;

    // 为了避免 worker 在我们还没把所有任务入队前就开始消费，先暂停池，
    // 等所有任务都入队后再 Unpause，这样能够更稳定地体现优先级队列的顺序。
    pool->Pause();

    // 先提交低优先级任务，再提交高优先级任务，观察执行顺序
    for (int i = 0; i < 4; ++i) {
        pool->InvokeWithPriority(TaskPriority::Low, [&mtx, &order, i] {
            std::lock_guard<std::mutex> lock(mtx);
            order.push_back(100 + i); // 低优先级标记为 100+
        });
    }
    for (int i = 0; i < 4; ++i) {
        pool->InvokeWithPriority(TaskPriority::High, [&mtx, &order, i] {
            std::lock_guard<std::mutex> lock(mtx);
            order.push_back(i); // 高优先级标记为 0..3
        });
    }

    pool->Unpause();
    pool->Wait();

    // 只要队列有任务，高优先级会优先被取出执行。
    // 由于我们在入队期间暂停了池，所有任务都会先进入优先级队列，然后再由 worker 消费，
    // 因此正常情况下应该在出现第一个低优先级任务之前就看到高优先级任务。
    expect(!order.empty(), "priority_order_test: order empty");
    bool low_seen = false;
    bool high_before_low = false;
    for (int v : order) {
        if (v >= 100) {
            // 低优先级
            low_seen = true;
        } else {
            // 高优先级
            if (!low_seen) {
                high_before_low = true;
            }
        }
    }
    expect(high_before_low, "priority_order_test: no high priority executed before low");
}

static void pause_unpause_test(const std::shared_ptr<ThreadPoolInvokeInterface>& pool) {
    std::cout << "[threadpool test] pause / unpause..." << std::endl;

    std::atomic<int> counter{0};

    for (int i = 0; i < 20; ++i) {
        pool->Invoke([&counter] {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }

    // 暂停后，Wait 会因为 paused 条件而尽快返回
    pool->Pause();
    pool->Wait();
    expect(pool->IsPaused(), "pause_unpause_test: pool not paused");

    // 解除暂停，再等所有任务跑完
    pool->Unpause();
    pool->Wait();
    expect(counter.load(std::memory_order_relaxed) == 20,
           "pause_unpause_test: not all tasks finished after unpause");
}

static void reset_test(const std::shared_ptr<ThreadPoolInvokeInterface>& pool) {
    std::cout << "[threadpool test] reset thread count..." << std::endl;

    std::size_t original = pool->GetThreadCount();
    std::size_t new_count = (original > 1) ? (original - 1) : (original + 1);

    pool->Reset(new_count);
    expect(pool->GetThreadCount() == new_count, "reset_test: thread count not updated");

    // 提交一些任务，确认新配置正常工作
    std::atomic<int> counter{0};
    for (int i = 0; i < 16; ++i) {
        pool->Invoke([&counter] {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }
    pool->Wait();
    expect(counter.load(std::memory_order_relaxed) == 16,
           "reset_test: tasks not all executed");

    // 还原
    pool->Reset(original);
}

static void deadlock_detection_test(const std::shared_ptr<ThreadPoolInvokeInterface>& pool) {
    std::cout << "[threadpool test] deadlock detection..." << std::endl;

    pool->EnableWaitDeadlockCheck(true);

    std::atomic<bool> caught{false};

    auto fut = pool->Invoke([&pool, &caught] {
        try {
            pool->Wait(); // 在 worker 线程里调用 Wait，应抛 WaitDeadlockError
        } catch (const WaitDeadlockError&) {
            caught.store(true, std::memory_order_relaxed);
        }
    });

    fut.get();
    expect(caught.load(std::memory_order_relaxed), "deadlock_detection_test: WaitDeadlockError not thrown");
}

static void deadlock_detection_stress_test(const std::shared_ptr<ThreadPoolInvokeInterface>& pool) {
    std::cout << "[threadpool test] deadlock detection (stress)..." << std::endl;

    pool->EnableWaitDeadlockCheck(true);

    const int rounds = 50;
    std::atomic<int> caught_count{0};

    for (int r = 0; r < rounds; ++r) {
        auto fut = pool->Invoke([&pool, &caught_count] {
            try {
                pool->Wait();
            } catch (const WaitDeadlockError&) {
                caught_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
        fut.get();
    }

    expect(caught_count.load(std::memory_order_relaxed) == rounds,
           "deadlock_detection_stress_test: not all Wait calls detected as deadlock");
}

static void is_current_test(const std::shared_ptr<ThreadPoolInvokeInterface>& pool) {
    std::cout << "[threadpool test] IsCurrent..." << std::endl;

    // 主线程不应在池内
    expect(!pool->IsCurrent(), "is_current_test: main thread reported as pool worker");

    std::atomic<bool> in_worker{false};
    auto fut = pool->Invoke([&pool, &in_worker] {
        if (pool->IsCurrent()) {
            in_worker.store(true, std::memory_order_relaxed);
        }
    });
    fut.get();
    expect(in_worker.load(std::memory_order_relaxed),
           "is_current_test: worker thread not reported as pool worker");
}

static void exception_propagation_test(const std::shared_ptr<ThreadPoolInvokeInterface>& pool) {
    std::cout << "[threadpool test] exception propagation..." << std::endl;

    auto fut = pool->Invoke([]() -> int {
        throw std::runtime_error("test exception");
    });

    bool thrown = false;
    try {
        (void)fut.get();
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    expect(thrown, "exception_propagation_test: exception not propagated via future");
}

static void stress_test(const std::shared_ptr<ThreadPoolInvokeInterface>& pool) {
    std::cout << "[threadpool test] stress..." << std::endl;

    const int tasks = 1000;
    std::atomic<int> sum{0};

    for (int i = 0; i < tasks; ++i) {
        pool->Invoke([&sum] {
            sum.fetch_add(1, std::memory_order_relaxed);
        });
    }
    pool->Wait();
    expect(sum.load(std::memory_order_relaxed) == tasks,
           "stress_test: sum mismatch");
}

static void benchmark_test(const std::shared_ptr<ThreadPoolInvokeInterface>& pool) {
    std::cout << "[threadpool test] benchmark (simple)..." << std::endl;

    const int rounds = 5;
    const int tasks_per_round = 20000;

    for (int r = 0; r < rounds; ++r) {
        auto start = std::chrono::steady_clock::now();

        std::atomic<int> counter{0};
        for (int i = 0; i < tasks_per_round; ++i) {
            pool->Invoke([&counter] {
                // 做一点点工作，避免被完全优化掉
                counter.fetch_add(1, std::memory_order_relaxed);
            });
        }
        pool->Wait();

        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        expect(counter.load(std::memory_order_relaxed) == tasks_per_round,
               "benchmark_test: not all tasks executed");

        std::cout << "  round " << r + 1 << "/" << rounds
                  << " : " << tasks_per_round << " tasks in "
                  << ms << " ms" << std::endl;
    }
}

int main() {
    try {
        auto pool = ThreadPoolInvokeInterface::CreateThreadPool(
            std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : 4,
            "itflee_threadpool_full_test");

        basic_functionality_test(pool);
        parallel_accumulate_test(pool);
        invoke_sync_reentrancy_test(pool);
        wait_and_clear_test(pool);
        priority_order_test(pool);
        pause_unpause_test(pool);
        reset_test(pool);
        deadlock_detection_test(pool);
        deadlock_detection_stress_test(pool);
        is_current_test(pool);
        exception_propagation_test(pool);
        stress_test(pool);
        benchmark_test(pool);

        std::cout << "[threadpool test] all tests passed." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[threadpool test] FAILED: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "[threadpool test] FAILED: unknown exception" << std::endl;
        return 1;
    }

    // 默认不等待输入，避免脚本/CI 卡死；需要交互时可设置环境变量打开。
    const char* wait = std::getenv("ITFLEE_THREADPOOL_WAIT_ENTER");
    if (wait && *wait && (std::string(wait) == "1" || std::string(wait) == "true")) {
        std::cout << "Press Enter to continue...";
        std::cin.get();
    }

    return 0;
}

