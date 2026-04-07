
#include "cache_queue/spsc_queue.h"
#include "cache_queue/spsc_queue_2.h"

#include <thread>
#include <memory>
#include <queue>
#include <mutex>

#define QUEUE_SIZE 10
#define NUM_ITEMS 9000000
using namespace std;

class TimeStamp {
public:

    void mark(string name) {
        auto now = chrono::high_resolution_clock::now();
        timePoints[name].push_back(now);
    };

    void summary() {
        std::cout << "**********************************************************************\n";
        int count = 1;
        timePoints.reserve(timePoints.size());
        for (const auto& entry : timePoints) {
            const string& name = entry.first;
            const auto& points = entry.second;
            if (points.size() < 2) continue; // 至少需要两个时间点来计算持续时间

            auto start = points.front();
            auto end = points.back();
            auto duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();
            cout << count++ << ". " << name << ": " << duration << " ms" << endl;
        }
        std::cout << "**********************************************************************\n";

    };

private:
    unordered_map<string, std::vector<chrono::high_resolution_clock::time_point>> timePoints;

};

// Producer-Consumer functions
template<typename T>
void producer(T& queue) {
    for (int i = 0; i < NUM_ITEMS; ++i) {
        queue.enqueue(i);
    }
}

// Consumer function
template<typename T>
void consumer(T& queue) {
    for (int i = 0; i < NUM_ITEMS; ++i) {
		int item = 0;
        auto res = queue.dequeue(item);
        // std::cout << res << " ";
    }
    // std::cout << std::endl;
}


class lock_queue {
private:
    std::queue<int> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_Empty;
    std::condition_variable m_Full;
    const size_t max_size = QUEUE_SIZE;

public:
    void enqueue(int item) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_Full.wait(lock, [this]() { return m_queue.size() < max_size; });
        m_queue.push(item);
        m_Empty.notify_one();
    }

    int dequeue(int i) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_Empty.wait(lock, [this]() { return !m_queue.empty(); });
        auto item = m_queue.front();
        m_queue.pop();
        m_Full.notify_one();
        return item;
    }
};



static int queue_test() {
    lock_queue lq;
    itflee::SPSCLockFreeQueue<int> uq(10);
	itflee::SPSCLockFreeQueue_2<int> uq2(10);


    TimeStamp ts;

    ts.mark("lockqueue");
    std::thread producer1(producer<lock_queue>, std::ref(lq));
    std::thread consumer1(consumer<lock_queue>, std::ref(lq));
    producer1.join();
    consumer1.join();
    ts.mark("lockqueue");

    //ts.mark("unlockqueue1");
    //std::thread producer2(producer<itflee::SPSCLockFreeQueue<int>>, std::ref(uq));
    //std::thread consumer2(consumer<itflee::SPSCLockFreeQueue<int>>, std::ref(uq));
    //producer2.join();
    //consumer2.join();
    //ts.mark("unlockqueue1");

    
    ts.mark("unlockqueue2");
    std::thread producer2(producer<itflee::SPSCLockFreeQueue_2<int>>, std::ref(uq2));
    std::thread consumer2(consumer<itflee::SPSCLockFreeQueue_2<int>>, std::ref(uq2));
    producer2.join();
    consumer2.join();
    ts.mark("unlockqueue2");

    ts.summary();

    return 0;
}