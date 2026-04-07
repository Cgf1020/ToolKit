#ifndef THREAD_INVOKE_TEST_HPP
#define THREAD_INVOKE_TEST_HPP

#include "threadpool/thread_invoke_interface.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <future>

namespace itflee {
    namespace test {

        // 简单使用示例
        [[maybe_unused]] static void thread_invoke_test_simple() {
            std::cout << "\n========================================\n";
            std::cout << "    ThreadInvoke 简单使用示例 \n";
            std::cout << "========================================\n\n";
            
            auto thread = ThreadInvokeInterface::CreateThreadInvoke("WorkerThread");
            thread->Start();


            // 示例1: 无参数无返回值
            std::cout << "[示例1] 无参数无返回值\n";
            thread->Invoke([]() {
                std::cout << "  执行线程ID  1: " << std::this_thread::get_id() << "\n";
                std::cout << "  任务 1 执行完成\n";
            });


            // 示例3: 无参数有返回值
            std::cout << "\n[示例3] 无参数有返回值\n";
            auto future1 = thread->Invoke([]() -> int {
                std::cout << "  执行线程ID 3 : " << std::this_thread::get_id() << "\n";
                return 100;
                });

            
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::cout << "1 - 3 \n";
            
            // 示例2: 有参数无返回值
            std::cout << "\n[示例2] 有参数无返回值\n";
            thread->Invoke([](int a, const std::string& msg) {
                std::cout << "  执行线程ID 2: " << std::this_thread::get_id() << "\n";
                std::cout << "  任务2  参数: a=" << a << ", msg=\"" << msg << "\"\n";
            }, 42, "Hello World");
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            

            int result1 = future1.get();
            std::cout << " 任务 3 返回值: " << result1 << "\n";
            
            // 示例4: 有参数有返回值
            std::cout << "\n[示例4] 有参数有返回值\n";
            auto future2 = thread->Invoke([](int a, int b) -> int {
                std::cout << "  执行线程ID 4: " << std::this_thread::get_id() << "\n";
                return a + b;
            }, 10, 20);
            int result2 = future2.get();
            std::cout << " 任务 4  返回值: 10 + 20 = " << result2 << "\n";
            
            thread->Stop();
            std::cout << "\n线程已停止\n";
        }

        // InvokeSync 使用示例
        [[maybe_unused]] static void thread_invoke_test_sync() {
            std::cout << "\n========================================\n";
            std::cout << "    ThreadInvoke InvokeSync 使用示例\n";
            std::cout << "========================================\n\n";
            
            auto thread = ThreadInvokeInterface::CreateThreadInvoke("WorkerThread");
            thread->Start();
            
            // 示例1: InvokeSync 无参数有返回值（同步等待）
            std::cout << "[示例1] InvokeSync 无参数有返回值\n";
            std::cout << "  主线程ID: " << std::this_thread::get_id() << "\n";
            int result1 = thread->InvokeSync([]() -> int {
                std::cout << "  执行线程ID: " << std::this_thread::get_id() << "\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                return 200;
            });
            std::cout << "  返回值: " << result1 << "\n";
            
            // 示例2: InvokeSync 有参数有返回值（同步等待）
            std::cout << "\n[示例2] InvokeSync 有参数有返回值\n";
            int result2 = thread->InvokeSync([](int a, int b) -> int {
                std::cout << "  执行线程ID: " << std::this_thread::get_id() << "\n";
                return a * b;
            }, 5, 6);
            std::cout << "  返回值: 5 * 6 = " << result2 << "\n";
            
            // 示例3: 在工作线程中调用 InvokeSync（直接执行，不排队）
            std::cout << "\n[示例3] 在工作线程中调用 InvokeSync\n";
            thread->InvokeSync([](std::shared_ptr<ThreadInvokeInterface> t) {
                std::cout << "  工作线程ID: " << std::this_thread::get_id() << "\n";
                std::cout << "  是否在工作线程: " << (t->IsCurrent() ? "是" : "否") << "\n";
                
                // 在工作线程中调用 InvokeSync，会直接执行而不排队
                int result3 = t->InvokeSync([]() -> int {
                    std::cout << "  直接执行线程ID: " << std::this_thread::get_id() << "\n";
                    return 300;
                });
                std::cout << "  直接执行返回值: " << result3 << "\n";
            }, thread);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            thread->Stop();
            std::cout << "\n线程已停止\n";
        }

    } // namespace test
} // namespace itflee

#endif // THREAD_INVOKE_TEST_HPP
