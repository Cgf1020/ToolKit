
#include "cache_queue/sync_queue_template.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

// 测试数据结构
struct TestData1 {
    int64_t timestamp;
    std::string data;
    
    TestData1(int64_t ts, const std::string& d) : timestamp(ts), data(d) {}
    TestData1() : timestamp(0), data("empty") {} // 默认构造函数用于非同步模式
};

struct TestData2 {
    int64_t time_ms;
    int value;
    
    TestData2(int64_t ts, int val) : time_ms(ts), value(val) {}
    TestData2() : time_ms(0), value(-1) {} // 默认构造函数用于非同步模式
};

void testSyncMode() {
    std::cout << "=== 测试同步模式 ===" << std::endl;

    SyncQueueTemplate<TestData1, TestData2> syncQueue(
        [](const TestData1& data) -> int64_t { return data.timestamp; },
        [](const TestData2& data) -> int64_t { return data.time_ms; },
        1000,  // 保留时间
        50,    // 匹配阈值
        true   // 启用同步
    );

    syncQueue.Start([](const TestData1& data1, const TestData2& data2) {
        std::cout << "[同步匹配] Data1: " << data1.data
            << " (时间: " << data1.timestamp << ")"
            << " | Data2: " << data2.value
            << " (时间: " << data2.time_ms << ")" << std::endl;
        });

    // 添加数据
    syncQueue.PushToQueue1(std::make_shared<TestData1>(1000, "sync_test1"));
    syncQueue.PushToQueue2(std::make_shared<TestData2>(1020, 42));  // 应该匹配

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    syncQueue.Stop();
}

void testNonSyncMode() {
    std::cout << "\n=== 测试非同步模式 ===" << std::endl;
    
    SyncQueueTemplate<TestData1, TestData2> syncQueue(
        [](const TestData1& data) -> int64_t { return data.timestamp; },
        [](const TestData2& data) -> int64_t { return data.time_ms; },
        1000,  // 保留时间
        50,    // 匹配阈值
        false  // 禁用同步
    );
    
    syncQueue.Start([](const TestData1& data1, const TestData2& data2) {
        std::cout << "[非同步回调] Data1: " << data1.data 
                  << " (时间: " << data1.timestamp << ")"
                  << " | Data2: " << data2.value 
                  << " (时间: " << data2.time_ms << ")" << std::endl;
    });
    
    // 添加数据 - 应该立即回调
    std::cout << "添加Data1，应该立即回调..." << std::endl;
    syncQueue.PushToQueue1(std::make_shared<TestData1>(1000, "non_sync_test1"));
    
    std::cout << "添加Data2，应该立即回调..." << std::endl;
    syncQueue.PushToQueue2(std::make_shared<TestData2>(1020, 42));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    syncQueue.Stop();
}

void testDynamicSyncToggle() {
    std::cout << "\n=== 测试动态切换同步模式 ===" << std::endl;
    
    SyncQueueTemplate<TestData1, TestData2> syncQueue(
        [](const TestData1& data) -> int64_t { return data.timestamp; },
        [](const TestData2& data) -> int64_t { return data.time_ms; },
        1000,  // 保留时间
        50,    // 匹配阈值
        true   // 初始启用同步
    );
    
    syncQueue.Start([](const TestData1& data1, const TestData2& data2) {
        std::cout << "[动态切换] Data1: " << data1.data 
                  << " (时间: " << data1.timestamp << ")"
                  << " | Data2: " << data2.value 
                  << " (时间: " << data2.time_ms << ")" << std::endl;
    });
    
    // 先测试同步模式
    std::cout << "1. 同步模式测试..." << std::endl;
    syncQueue.PushToQueue1(std::make_shared<TestData1>(1000, "sync_mode"));
    syncQueue.PushToQueue2(std::make_shared<TestData2>(1020, 100));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 切换到非同步模式
    std::cout << "2. 切换到非同步模式..." << std::endl;
    syncQueue.SetSyncEnabled(false);
    
    syncQueue.PushToQueue1(std::make_shared<TestData1>(2000, "non_sync_mode"));
    syncQueue.PushToQueue2(std::make_shared<TestData2>(2020, 200));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    syncQueue.Stop();
}

//int main() {
//    std::cout << "=== 同步队列同步开关功能测试 ===" << std::endl;
//    
//    try {
//        testSyncMode();
//        testNonSyncMode();
//        testDynamicSyncToggle();
//        
//        std::cout << "\n所有测试完成！" << std::endl;
//        
//    } catch (const std::exception& e) {
//        std::cerr << "错误: " << e.what() << std::endl;
//        return 1;
//    }
//    
//    return 0;
//}

/*
编译命令:
g++ -std=c++11 -pthread sync_queue_sync_test.cpp -o sync_queue_sync_test

运行:
./sync_queue_sync_test

功能说明:
1. 同步模式：数据会等待匹配，只有匹配成功才回调
2. 非同步模式：数据到达立即回调，另一个参数为空数据
3. 动态切换：可以在运行时切换同步模式
*/
