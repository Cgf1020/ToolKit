#ifndef SPSC_QUEUE_H
#define SPSC_QUEUE_H

////////////////////////////////////////////////////////////////////////////////
//
// File Name:	NewTec.hpp
// Class Name:
// Description:	手动实现一些单生产者单消费者的无锁队列
// Author:		常高峰
// Date:		2025年9月8日
////////////////////////////////////////////////////////////////////////////////

// 说明：
// 1. 该队列只能有一个生产者线程和一个消费者线程同时操作
// 2. 该队列不支持动态扩容，容量在构造时指定
// 3. 该队列是无锁的，使用原子操作保证线程安全
// 4. 该队列使用环形缓冲区实现
// 5. 该队列不支持拷贝和移动操作，防止误用导致线程安全问题


#include <atomic>
#include <vector>
#include <cstddef>
#include <stdexcept>

// IT逃离者 itflee
namespace itflee {
    // 单生产者单消费者无锁队列
    // 单生产者单消费者无锁队列
    template<typename T>
    class SPSCLockFreeQueue {
    public:
        // 构造函数，指定队列容量
        explicit SPSCLockFreeQueue(size_t capacity)
            : capacity_(capacity + 1)   // 额外一个位置用于区分满和空
            , buffer_(capacity + 1)     // 环形缓冲区
            , head_(0)                  // 消费者指针（指向队首元素）
            , tail_(0)                  // 生产者指针（指向队尾下一个位置）
        {
            if (capacity == 0) {
                throw std::invalid_argument("队列容量不能为0");
            }
            static_assert(std::is_move_assignable<T>::value || std::is_copy_assignable<T>::value,  "T 必须可赋值（可移动或可拷贝）以支持入队/出队赋值");
        }

        // 禁用拷贝和移动操作，避免线程安全问题
        SPSCLockFreeQueue(const SPSCLockFreeQueue&) = delete;
        SPSCLockFreeQueue& operator=(const SPSCLockFreeQueue&) = delete;
        SPSCLockFreeQueue(SPSCLockFreeQueue&&) = delete;
        SPSCLockFreeQueue& operator=(SPSCLockFreeQueue&&) = delete;

        // 入队操作（只能由单个生产者线程调用）
        bool enqueue(const T& item) noexcept(noexcept(std::declval<T&>() = std::declval<const T&>())) {
            const size_t current_tail = tail_.load(std::memory_order_relaxed);
            const size_t next_tail = (current_tail + 1) % capacity_;

            // 检查队列是否已满（使用acquire确保能看到消费者最新的head）
            if (next_tail == head_.load(std::memory_order_acquire)) {
                return false;  // 队列已满，入队失败
            }

            // 存储元素到缓冲区
            buffer_[current_tail] = item;

            // 更新tail指针（使用release确保生产者的写入对消费者可见）
            tail_.store(next_tail, std::memory_order_release);
            return true;  // 入队成功
        }

        // 入队（移动版本，仅生产者）
        bool enqueue(T&& item) noexcept(noexcept(std::declval<T&>() = std::declval<T&&>())) {
            const size_t current_tail = tail_.load(std::memory_order_relaxed);
            const size_t next_tail = (current_tail + 1) % capacity_;

            if (next_tail == head_.load(std::memory_order_acquire)) {
                return false;
            }

            buffer_[current_tail] = std::move(item);
            tail_.store(next_tail, std::memory_order_release);
            return true;
        }

        // 原位构造（仅生产者）
        template<typename... Args>
        bool emplace(Args&&... args) noexcept(
            std::is_nothrow_constructible<T, Args...>::value&&
            std::is_nothrow_move_assignable<T>::value
            ) {
            const size_t current_tail = tail_.load(std::memory_order_relaxed);
            const size_t next_tail = (current_tail + 1) % capacity_;

            if (next_tail == head_.load(std::memory_order_acquire)) {
                return false;
            }

            T temp(std::forward<Args>(args)...);
            buffer_[current_tail] = std::move(temp);
            tail_.store(next_tail, std::memory_order_release);
            return true;
        }

        // 出队操作（只能由单个消费者线程调用）
        bool dequeue(T& item) noexcept(noexcept(std::declval<T&>() = std::declval<const T&>())) {
            const size_t current_head = head_.load(std::memory_order_relaxed);

            // 检查队列是否为空（使用acquire确保能看到生产者最新的tail）
            if (current_head == tail_.load(std::memory_order_acquire)) {
                return false;  // 队列为空，出队失败
            }

            // 从缓冲区读取元素
            item = buffer_[current_head];

            // 更新head指针（使用release确保消费者的读取对生产者可见）
            head_.store((current_head + 1) % capacity_, std::memory_order_release);
            return true;  // 出队成功
        }

        // 获取队首元素（不删除，只能由消费者线程调用）
        bool front(T& item) const noexcept(noexcept(std::declval<T&>() = std::declval<const T&>())) {
            const size_t current_head = head_.load(std::memory_order_relaxed);

            // 检查队列是否为空
            if (current_head == tail_.load(std::memory_order_acquire)) {
                return false;  // 队列为空
            }

            // 读取队首元素（head_指向的位置）
            item = buffer_[current_head];
            return true;
        }

        // 获取队尾元素（不删除，只能由生产者线程调用）
        bool back(T& item) const noexcept(noexcept(std::declval<T&>() = std::declval<const T&>())) {
            const size_t current_tail = tail_.load(std::memory_order_relaxed);

            // 检查队列是否为空
            if (current_tail == head_.load(std::memory_order_acquire)) {
                return false;  // 队列为空
            }

            // 计算队尾元素位置（tail_的前一个位置）
            const size_t back_pos = (current_tail == 0) ? (capacity_ - 1) : (current_tail - 1);
            item = buffer_[back_pos];
            return true;
        }

        // 清空队列
        void clear() noexcept {
            // 直接将head和tail指针重置
            head_.store(0, std::memory_order_release);
            tail_.store(0, std::memory_order_release);
        }

        // 检查队列是否为空
        [[nodiscard]] bool empty() const noexcept {
            // 使用acquire确保内存可见性
            return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
        }

        // 检查队列是否已满
        [[nodiscard]] bool full() const noexcept {
            const size_t current_tail = tail_.load(std::memory_order_acquire);
            const size_t next_tail = (current_tail + 1) % capacity_;
            return next_tail == head_.load(std::memory_order_acquire);
        }

        // 获取当前队列中的元素数量
        [[nodiscard]] size_t size() const noexcept {
            const size_t current_tail = tail_.load(std::memory_order_acquire);
            const size_t current_head = head_.load(std::memory_order_acquire);

            if (current_tail >= current_head) {
                return current_tail - current_head;
            }
            else {
                return (capacity_ - current_head) + current_tail;
            }
        }

        // 获取队列的最大容量
        [[nodiscard]] size_t max_capacity() const noexcept {
            return capacity_ - 1;  // 减去用于区分满和空的额外位置
        }

        // 当前可用容量
        [[nodiscard]] size_t capacity() const noexcept {
            return capacity_ - 1;
        }

    private:
        const size_t capacity_;          // 缓冲区总容量（包含额外位置）
        std::vector<T> buffer_;          // 环形缓冲区，使用vector管理内存

        // 自动适配当前CPU的缓存行大小
        alignas(std::hardware_destructive_interference_size) std::atomic<size_t> head_;  // 消费者指针
        alignas(std::hardware_destructive_interference_size) std::atomic<size_t> tail_;  // 生产者指针
    };

}




#endif // !SPSC_QUEUE_H
