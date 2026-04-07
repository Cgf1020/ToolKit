#ifndef CACHE_QUEUE_TEST_HPP
#define CACHE_QUEUE_TEST_HPP

#include "cache_queue/cache_queue.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <string>

namespace itflee {
namespace test {

// 生产者-消费者测试：测试所有接口
[[maybe_unused]] static int cache_queue_test() {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "    CacheQueue 生产者-消费者测试\n";
    std::cout << "========================================\n";
    
    // 创建队列，设置最大容量为10
    CacheQueue<int> queue(10);
    
    std::cout << "队列最大容量: " << queue.MaxSize() << std::endl;
    std::cout << "初始队列大小: " << queue.Size() << std::endl;
    std::cout << "初始队列是否为空: " << (queue.Empty() ? "是" : "否") << std::endl;
    std::cout << "初始队列是否已满: " << (queue.Full() ? "是" : "否") << std::endl;
    std::cout << "\n";
    
    // 统计变量
    std::atomic<int> produced_count{0};
    std::atomic<int> consumed_count{0};
    std::atomic<int> push_success_count{0};
    std::atomic<int> push_fail_count{0};
    std::atomic<int> try_pop_success_count{0};
    std::atomic<int> try_pop_fail_count{0};
    std::atomic<bool> producer_done{false};
    std::atomic<bool> consumer_done{false};
    
    const int total_items = 100;  // 总共生产100个元素
    
    // 生产者线程：测试所有推入接口
    std::thread producer([&]() {
        std::cout << "[生产者] 线程启动，开始生产数据...\n";
        
        for (int i = 0; i < total_items; ++i) {
            // 测试 Push (左值)
            if (i % 4 == 0) {
                int value = i;
                if (queue.Push(value)) {
                    push_success_count++;
                    produced_count++;
                } else {
                    push_fail_count++;
                    // 如果失败，使用 WaitAndPush 阻塞等待
                    queue.WaitAndPush(value);
                    produced_count++;
                }
            }
            // 测试 Push (右值)
            else if (i % 4 == 1) {
                int value = i;
                if (queue.Push(std::move(value))) {
                    push_success_count++;
                    produced_count++;
                } else {
                    push_fail_count++;
                    queue.WaitAndPush(std::move(value));
                    produced_count++;
                }
            }
            // 测试 Emplace
            else if (i % 4 == 2) {
                if (queue.Emplace(i)) {
                    push_success_count++;
                    produced_count++;
                } else {
                    push_fail_count++;
                    // Emplace 失败时，使用 WaitAndPush
                    queue.WaitAndPush(i);
                    produced_count++;
                }
            }
            // 测试 WaitAndPush (阻塞等待)
            else {
                queue.WaitAndPush(i);
                produced_count++;
            }
            
            // 定期输出状态
            if ((i + 1) % 20 == 0) {
                std::cout << "[生产者] 已生产: " << produced_count.load() 
                          << ", 队列大小: " << queue.Size() 
                          << ", 是否已满: " << (queue.Full() ? "是" : "否") << std::endl;
            }
            
            // 模拟生产速度
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        producer_done = true;
        std::cout << "[生产者] 生产完成，总共生产: " << produced_count.load() << " 个元素\n";
        std::cout << "[生产者] Push成功: " << push_success_count.load() 
                  << ", Push失败: " << push_fail_count.load() << std::endl;
    });
    
    // 消费者线程：测试所有弹出和访问接口
    std::thread consumer([&]() {
        std::cout << "[消费者] 线程启动，开始消费数据...\n";
        
        int consumed = 0;
        while (true) {
            // 检查退出条件：生产者已完成且队列为空
            if (producer_done && queue.Empty()) {
                break;
            }
            
            // 测试 TryFront (非阻塞访问)
            auto front = queue.TryFront();
            if (front.has_value()) {
                // 有数据，尝试弹出
                
                // 测试 TryPop (非阻塞弹出)
                if (queue.TryPop()) {
                    try_pop_success_count++;
                    consumed_count++;
                    consumed++;
                } else {
                    try_pop_fail_count++;
                    // TryPop 失败，使用 WaitAndPop
                    int value = 0;
                    if (queue.WaitAndPop(value)) {
                        consumed_count++;
                        consumed++;
                    } else {
                        // WaitAndPop 被中断，检查是否应该退出
                        if (producer_done && queue.Empty()) {
                            break;
                        }
                    }
                }
            } else {
                // 队列为空
                if (producer_done) {
                    // 生产者已完成，再检查一次队列是否真的为空
                    if (queue.Empty()) {
                        break;
                    }
                } else {
                    // 生产者还在运行，使用 WaitAndFront 等待
                    auto result = queue.WaitAndFront(100);  // 等待100ms
                    if (result.has_value()) {
                        // 测试 WaitAndPop (阻塞等待并获取值)
                        int value = 0;
                        if (queue.WaitAndPop(value)) {
                            consumed_count++;
                            consumed++;
                        }
                    }
                    // 如果 WaitAndFront 超时，继续循环检查
                }
            }
            
            // 定期输出状态
            if (consumed % 20 == 0 && consumed > 0) {
                std::cout << "[消费者] 已消费: " << consumed_count.load() 
                          << ", 队列大小: " << queue.Size() 
                          << ", 是否为空: " << (queue.Empty() ? "是" : "否") << std::endl;
            }
            
            // 测试 TryBack (非阻塞访问队尾)
            auto back = queue.TryBack();
            if (back.has_value() && consumed % 10 == 0 && consumed > 0) {
                std::cout << "[消费者] 当前队尾元素: " << back.value() << std::endl;
            }
            
            // 模拟消费速度
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
        
        consumer_done = true;
        std::cout << "[消费者] 消费完成，总共消费: " << consumed_count.load() << " 个元素\n";
        std::cout << "[消费者] TryPop成功: " << try_pop_success_count.load() 
                  << ", TryPop失败: " << try_pop_fail_count.load() << std::endl;
    });
    
    // 等待生产者完成
    producer.join();
    std::cout << "\n[主线程] 生产者线程已结束\n";
    
    // 等待消费者完成
    consumer.join();
    std::cout << "[主线程] 消费者线程已结束\n";
    
    // 输出最终统计
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "           测试结果统计\n";
    std::cout << "========================================\n";
    std::cout << "总生产数量: " << produced_count.load() << std::endl;
    std::cout << "总消费数量: " << consumed_count.load() << std::endl;
    std::cout << "Push成功次数: " << push_success_count.load() << std::endl;
    std::cout << "Push失败次数: " << push_fail_count.load() << std::endl;
    std::cout << "TryPop成功次数: " << try_pop_success_count.load() << std::endl;
    std::cout << "TryPop失败次数: " << try_pop_fail_count.load() << std::endl;
    std::cout << "最终队列大小: " << queue.Size() << std::endl;
    std::cout << "最终队列是否为空: " << (queue.Empty() ? "是" : "否") << std::endl;
    std::cout << "最终队列是否已满: " << (queue.Full() ? "是" : "否") << std::endl;
    
    // 测试 Clear
    std::cout << "\n测试 Clear() 功能...\n";
    if (!queue.Empty()) {
        std::cout << "清空前队列大小: " << queue.Size() << std::endl;
        queue.Clear();
        std::cout << "清空后队列大小: " << queue.Size() << std::endl;
        std::cout << "清空后队列是否为空: " << (queue.Empty() ? "是" : "否") << std::endl;
    }
    
    // 测试 Abort 和 Reset
    std::cout << "\n测试 Abort() 和 Reset() 功能...\n";
    std::atomic<bool> wait_interrupted{false};
    std::thread wait_thread([&]() {
        int value;
        if (!queue.WaitAndPop(value)) {
            wait_interrupted = true;
        }
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    queue.Abort();
    std::cout << "调用 Abort()，中断标志: " << (queue.IsAborted() ? "是" : "否") << std::endl;
    
    wait_thread.join();
    std::cout << "等待线程是否被中断: " << (wait_interrupted ? "是" : "否") << std::endl;
    
    queue.Reset();
    std::cout << "调用 Reset()，中断标志: " << (queue.IsAborted() ? "是" : "否") << std::endl;
    
    // 测试 swap
    std::cout << "\n测试 swap() 功能...\n";
    CacheQueue<int> queue2(5);
    queue2.Push(100);
    queue2.Push(200);
    std::cout << "交换前 queue1 大小: " << queue.Size() << ", queue2 大小: " << queue2.Size() << std::endl;
    queue.swap(queue2);
    std::cout << "交换后 queue1 大小: " << queue.Size() << ", queue2 大小: " << queue2.Size() << std::endl;
    auto front = queue.TryFront();
    if (front.has_value()) {
        std::cout << "交换后 queue1 队首元素: " << front.value() << std::endl;
    }
    
    std::cout << "\n========================================\n";
    std::cout << "    CacheQueue 测试完成！\n";
    std::cout << "========================================\n";
    
    return 0;
}

} // namespace test
} // namespace itflee

#endif // CACHE_QUEUE_TEST_HPP

