#ifndef SPSC_QUEUE_2_H
#define SPSC_QUEUE_2_H

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
#include <memory>
#include <type_traits>
#include <new>


// IT逃离者 itflee
namespace itflee {
    // 单生产者单消费者无锁队列
    template<typename T>
    class SPSCLockFreeQueue_2 {
	public:
		explicit SPSCLockFreeQueue_2(std::size_t capacity)
			: capacity_(capacity == 0 ? 1 : capacity)
			, buffer_(capacity_) {
			// 使用 vector 管理原始对齐存储，避免强制要求 T 可默认构造
		}

		~SPSCLockFreeQueue_2() {
			// 调用析构：仅对已构造但未销毁的元素调用。为简洁，析构时假定 T 可平凡销毁或忽略残留对象析构。
			// 若需要严格析构，请在使用中确保消费者排空后再销毁队列。

			// 析构所有已构造但未消费的对象
			std::size_t head = head_.load(std::memory_order_acquire);
			std::size_t tail = tail_.load(std::memory_order_acquire);

			for (std::size_t i = head; i < tail; ++i) {
				T* slot = ptr_at(i);
				slot->~T(); // 显式析构
			}
		}

		SPSCLockFreeQueue_2(const SPSCLockFreeQueue_2&) = delete;
		SPSCLockFreeQueue_2& operator=(const SPSCLockFreeQueue_2&) = delete;

		std::size_t capacity() const noexcept { return capacity_; }

		// 近似大小（在并发下为快照）
		std::size_t size() const noexcept {
			std::size_t head = head_.load(std::memory_order_acquire);
			std::size_t tail = tail_.load(std::memory_order_acquire);
			return tail - head;
		}

		bool empty() const noexcept { return size() == 0; }
		bool full() const noexcept { return size() >= capacity_; }

		// 当满时不覆盖，返回 false
		bool enqueue(const T& value) {
			return emplace_internal([&](void* dst) { new (dst) T(value); });
		}

		template <class... Args>
		bool emplace(Args&&... args) {
			return emplace_internal([&](void* dst) { new (dst) T(std::forward<Args>(args)...); });
		}

		// 消费者出队
		bool dequeue(T& out) {
			std::size_t head = head_.load(std::memory_order_relaxed);
			std::size_t tail = tail_.load(std::memory_order_acquire);
			if (head == tail) return false; // 空
			T* slot = ptr_at(head);
			out = *slot; // 假定可拷贝/移动
			// 可选：显式调用析构，然后不重建，简化起见不调用，交给覆盖时重建
			slot->~T(); // 显式析构已消费的对象
			head_.fetch_add(1, std::memory_order_release);
			return true;
		}

		// 仅消费者：查看队首
		bool peek_front(T& out) const {
			std::size_t head = head_.load(std::memory_order_relaxed);
			std::size_t tail = tail_.load(std::memory_order_acquire);
			if (head == tail) return false;
			const T* slot = const_ptr_at(head);
			out = *slot;
			return true;
		}

		// 仅生产者：查看队尾（最后一个已写入元素）
		bool peek_back(T& out) const {
			std::size_t tail = tail_.load(std::memory_order_relaxed);
			std::size_t head = head_.load(std::memory_order_acquire);
			if (tail == head) return false;
			const T* slot = const_ptr_at(tail - 1);
			out = *slot;
			return true;
		}

	private:
		template <class Constructor>
		bool emplace_internal(Constructor ctor) {
			static_assert(std::is_constructible<T, T&&>::value || std::is_copy_constructible<T>::value, "T must be move or copy constructible");
			std::size_t tail = tail_.load(std::memory_order_relaxed);
			std::size_t head = head_.load(std::memory_order_acquire);
			// 满则返回失败
			if (tail - head >= capacity_) {
				return false;
			}
			T* slot = ptr_at(tail);
			// 直接就地构造/覆盖
			ctor(static_cast<void*>(slot));
			tail_.store(tail + 1, std::memory_order_release);
			return true;
		}

		T* ptr_at(std::size_t seq) const noexcept {
			return reinterpret_cast<T*>(const_cast<void*>(static_cast<const void*>(&buffer_[(seq % capacity_)])));
		}

		const T* const_ptr_at(std::size_t seq) const noexcept {
			return reinterpret_cast<const T*>(static_cast<const void*>(&buffer_[(seq % capacity_)]));
		}

	private:
		const std::size_t capacity_;
		//std::vector<T> 的默认行为有一个限制：当 vector 扩容、reserve 或默认初始化时，会自动调用 T 的默认构造函数。如果 T 不满足以下条件，std::vector<T> 会直接编译失败
		/*
		 * T 没有默认构造函数（例如构造函数需要强制参数，或被 = delete 禁用）；
		 * T 的默认构造函数不可访问（例如私有）；
		 * 希望 延迟构造 T 对象（即先分配内存，后续按需构造，避免默认构造的开销）。
		 * 而 std::aligned_storage_t<sizeof(T), alignof(T)> 正是为了解决这个问题：它提供 对齐的原始内存块，
		 让vector存储「未初始化的内存」而非「已构造的 T 对象」，再通过 placement new 就地构造 T，完全绕开对 T 默认构造函数的依赖。
		*/
		std::vector<std::aligned_storage_t<sizeof(T), alignof(T)>> buffer_;	
		std::atomic<std::size_t> head_{ 0 }; // 出队序号
		std::atomic<std::size_t> tail_{ 0 }; // 入队序号
    };

}




#endif // !SPSC_QUEUE_2_H
