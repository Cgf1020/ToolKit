/**
 * Simple custom test for BS::thread_pool.
 * Build: add executable BS_thread_pool_custom_test in this directory's CMakeLists.txt.
 */
#include "BS_thread_pool.hpp"
#include <atomic>
#include <future>
#include <iostream>
#include <string>
#include <vector>

int main()
{
    BS::thread_pool pool;

    // 1. Fire-and-forget tasks
    std::atomic<int> counter{0};
    for (int i = 0; i < 5; ++i)
        pool.detach_task([&counter, i] { counter += i; });
    pool.wait();
    std::cout << "detach_task sum(0..4) = " << counter.load() << std::endl;

    // 2. Submit and get result
    auto fut = pool.submit_task([] { return 42; });
    std::cout << "submit_task result = " << fut.get() << std::endl;

    // 3. Multiple futures
    std::vector<std::future<std::string>> futures;
    for (int i = 0; i < 3; ++i)
        futures.push_back(pool.submit_task([i] { return "task_" + std::to_string(i); }));
    std::cout << "submit_task strings: ";
    for (auto& f : futures)
        std::cout << f.get() << " ";
    std::cout << std::endl;

    std::cout << "Custom test done." << std::endl;
    return 0;
}
