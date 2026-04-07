#ifndef THREADPOOL_INVOKE_TEST_HPP
#define THREADPOOL_INVOKE_TEST_HPP

#include "threadpool/threadpool_invoke_interface.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <future>
#include <atomic>

namespace itflee {
namespace test {

    /** Simple usage: Invoke variants */
    [[maybe_unused]] static void threadpool_invoke_test_simple() {
        std::cout << "\n========================================\n";
        std::cout << "    ThreadPoolInvoke simple demo\n";
        std::cout << "========================================\n\n";

        auto pool = ThreadPoolInvokeInterface::CreateThreadPool(4, "TestPool");
        pool->Start();

        // [1] void, no args
        std::cout << "[1] void, no args\n";
        pool->Invoke([]() {
            std::cout << "  thread id: " << std::this_thread::get_id() << "\n";
            std::cout << "  task 1 done\n";
        });

        // [2] void, with args
        std::cout << "\n[2] void, with args\n";
        pool->Invoke([](int a, const std::string& msg) {
            std::cout << "  thread id: " << std::this_thread::get_id() << "\n";
            std::cout << "  task2 a=" << a << ", msg=\"" << msg << "\"\n";
        }, 42, "Hello Pool");

        // [3] returns value, no args
        std::cout << "\n[3] returns value, no args\n";
        auto future1 = pool->Invoke([]() -> int {
            std::cout << "  thread id: " << std::this_thread::get_id() << "\n";
            return 100;
        });
        int result1 = future1.get();
        std::cout << "  task3 result: " << result1 << "\n";

        // [4] returns value, with args
        std::cout << "\n[4] returns value, with args\n";
        auto future2 = pool->Invoke([](int a, int b) -> int {
            std::cout << "  thread id: " << std::this_thread::get_id() << "\n";
            return a + b;
        }, 10, 20);
        int result2 = future2.get();
        std::cout << "  task4 result: 10 + 20 = " << result2 << "\n";

        pool->Stop();
        std::cout << "\npool stopped\n";
    }

    /** InvokeSync demo; InvokeSync from worker runs inline */
    [[maybe_unused]] static void threadpool_invoke_test_sync() {
        std::cout << "\n========================================\n";
        std::cout << "    ThreadPoolInvoke InvokeSync demo\n";
        std::cout << "========================================\n\n";

        auto pool = ThreadPoolInvokeInterface::CreateThreadPool(2, "SyncPool");
        pool->Start();

        std::cout << "[1] InvokeSync returns value\n";
        std::cout << "  main thread: " << std::this_thread::get_id() << "\n";
        int result1 = pool->InvokeSync([]() -> int {
            std::cout << "  worker thread: " << std::this_thread::get_id() << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return 200;
        });
        std::cout << "  result: " << result1 << "\n";

        std::cout << "\n[2] InvokeSync with args\n";
        int result2 = pool->InvokeSync([](int a, int b) -> int {
            std::cout << "  worker thread: " << std::this_thread::get_id() << "\n";
            return a * b;
        }, 5, 6);
        std::cout << "  result: 5 * 6 = " << result2 << "\n";

        std::cout << "\n[3] InvokeSync from worker (runs inline, no queue)\n";
        pool->InvokeSync([](std::shared_ptr<ThreadPoolInvokeInterface> p) {
            std::cout << "  worker thread: " << std::this_thread::get_id() << "\n";
            std::cout << "  IsCurrent: " << (p->IsCurrent() ? "yes" : "no") << "\n";
            int result3 = p->InvokeSync([]() -> int {
                std::cout << "  inline thread: " << std::this_thread::get_id() << "\n";
                return 300;
            });
            std::cout << "  inline result: " << result3 << "\n";
        }, pool);

        pool->Stop();
        std::cout << "\npool stopped\n";
    }

    /** Concurrent tasks and IsCurrent / ClearTask */
    [[maybe_unused]] static void threadpool_invoke_test_concurrent() {
        std::cout << "\n========================================\n";
        std::cout << "    ThreadPoolInvoke concurrent & ClearTask\n";
        std::cout << "========================================\n\n";

        auto pool = ThreadPoolInvokeInterface::CreateThreadPool(4, "ConcurrentPool");
        pool->Start();

        std::atomic<int> done{0};
        for (int i = 0; i < 8; ++i) {
            pool->Invoke([&done, i]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                done += i;
            });
        }
        pool->InvokeSync([]() {});
        std::cout << "  sum 0..7 = " << done.load() << "\n";

        std::cout << "  main IsCurrent: " << (pool->IsCurrent() ? "yes" : "no") << "\n";
        pool->InvokeSync([&pool]() {
            std::cout << "  worker IsCurrent: " << (pool->IsCurrent() ? "yes" : "no") << "\n";
        });

        pool->Invoke([]() {});
        pool->ClearTask();
        std::cout << "  ClearTask called\n";

        pool->Stop();
        std::cout << "\npool stopped\n";
    }

    /** Wait() until all submitted tasks complete */
    [[maybe_unused]] static void threadpool_invoke_test_wait() {
        std::cout << "\n========================================\n";
        std::cout << "    ThreadPoolInvoke Wait() demo\n";
        std::cout << "========================================\n\n";

        auto pool = ThreadPoolInvokeInterface::CreateThreadPool(4, "WaitPool");
        pool->Start();

        std::atomic<int> completed{0};
        const int num_tasks = 10;
        std::cout << "  submit " << num_tasks << " tasks, then Wait()...\n";

        for (int i = 0; i < num_tasks; ++i) {
            pool->Invoke([&completed, i]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                completed += 1;
            });
        }
        std::cout << "  before Wait(), completed = " << completed.load() << "\n";
        pool->Wait();
        std::cout << "  after Wait(), completed = " << completed.load() << "\n";

        if (completed.load() == num_tasks)
            std::cout << "  OK: all " << num_tasks << " tasks finished\n";
        else
            std::cout << "  FAIL: expected " << num_tasks << ", got " << completed.load() << "\n";

        pool->Stop();
        std::cout << "\npool stopped\n";
    }

} // namespace test
} // namespace itflee

#endif // THREADPOOL_INVOKE_TEST_HPP
