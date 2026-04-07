#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <optional>
#include <chrono>

// IT逃离者 itflee
namespace itflee {

template <typename T>
class CacheQueue
{
public:
	// 构造函数：可设置最大容量（0表示无限制）
	explicit CacheQueue(size_t max_size = 0) 
		: max_size_(max_size) 
	{}

	~CacheQueue() {
		Clear();
	}

	/* 是否为空（线程安全）
	*/
	bool Empty() const {
		std::lock_guard<std::mutex> l(cache_queue_mutex_);
		return data_queue_.empty();
	}

	/* 队列是否已满（线程安全）
	* return true: yes   false：no
	*/
	bool Full() const {
		if (max_size_ == 0) {
			return false;
		}
		std::lock_guard<std::mutex> l(cache_queue_mutex_);
		return data_queue_.size() >= max_size_;
	}

	/* 获取数据的长度（线程安全）
	*/
	size_t Size() const {
		std::lock_guard<std::mutex> l(cache_queue_mutex_);
		return data_queue_.size();
	}

	/* 获取最大容量
	*/
	size_t MaxSize() const {
		return max_size_;
	}

	/* 设置最大容量
	* @param max_size 新的最大容量，0表示无限制
	* @return true: 成功  false: 新容量小于当前队列大小
	*/
	bool SetMaxSize(size_t max_size) {
		std::unique_lock<std::mutex> l(cache_queue_mutex_);
		size_t current_size = data_queue_.size();
		
		// 如果新容量小于当前队列大小，设置失败
		if (max_size > 0 && current_size > max_size) {
			return false;
		}
		
		max_size_ = max_size;
		
		// 如果之前队列已满，现在容量增加了，唤醒等待的推入线程
		if (max_size == 0 || current_size < max_size) {
			cache_convar_.notify_all();
		}
		
		return true;
	}

	/**
	 * @brief 向队列中推入新元素，左值版本
	 * @param t 待推入的元素
	 * @return true: 成功  false: 队列已满
	 */
	bool Push(const T& t) {
		std::unique_lock<std::mutex> l(cache_queue_mutex_);
		if (max_size_ > 0 && data_queue_.size() >= max_size_) {
			return false;  // 队列已满
		}
		data_queue_.push(t);
		cache_convar_.notify_one();		//通知pop
		return true;
	}

	/**
	 * @brief 向队列中推入新元素，右值版本
	 * @param t 待推入的元素
	 * @return true: 成功  false: 队列已满
	 */
	bool Push(T&& t) {
		std::unique_lock<std::mutex> l(cache_queue_mutex_);
		if (max_size_ > 0 && data_queue_.size() >= max_size_) {
			return false;  // 队列已满
		}
		data_queue_.push(std::forward<T>(t));
		cache_convar_.notify_one();		//通知pop
		return true;
	}

	/**
	 * @brief 向队列中推入新元素，emplace版本
	 * @param args 构造参数
	 * @return true: 成功  false: 队列已满
	 */
	template <typename... Args>
	bool Emplace(Args&&... args)
	{
		std::unique_lock<std::mutex> l(cache_queue_mutex_);
		if (max_size_ > 0 && data_queue_.size() >= max_size_) {
			return false;  // 队列已满
		}
		data_queue_.emplace(std::forward<Args>(args)...);
		cache_convar_.notify_one();		//通知pop
		return true;
	}

	/**
	 * @brief 阻塞等待直到可以推入元素
	 * @param t 待推入的元素
	 */
	void WaitAndPush(const T& t) {
		std::unique_lock<std::mutex> l(cache_queue_mutex_);
		if (max_size_ > 0) {
			cache_convar_.wait(l, [this] {
				return data_queue_.size() < max_size_ || abort_;
			});
			if (abort_) return;
		}
		data_queue_.push(t);
		cache_convar_.notify_one();
	}

	/**
	 * @brief 阻塞等待直到可以推入元素（右值版本）
	 * @param t 待推入的元素
	 */
	void WaitAndPush(T&& t) {
		std::unique_lock<std::mutex> l(cache_queue_mutex_);
		if (max_size_ > 0) {
			cache_convar_.wait(l, [this] {
				return data_queue_.size() < max_size_ || abort_;
			});
			if (abort_) return;
		}
		data_queue_.push(std::forward<T>(t));
		cache_convar_.notify_one();
	}

	/* 尝试弹出数据（非阻塞）
	* @return true: 成功弹出  false: 队列为空
	*/
	bool TryPop() {
		std::lock_guard<std::mutex> l(cache_queue_mutex_);
		if (data_queue_.empty()) {
			return false;
		}
		data_queue_.pop();
		return true;
	}

	/* 弹出数据（阻塞等待）
	* @return true: 成功弹出  false: 被中断
	*/
	bool WaitAndPop() {
		std::unique_lock<std::mutex> l(cache_queue_mutex_);
		cache_convar_.wait(l, [this] {
			return !data_queue_.empty() || abort_;
		});
		if (abort_ || data_queue_.empty()) {
			return false;
		}
		data_queue_.pop();
		return true;
	}

	/* 弹出数据并返回元素（阻塞等待）
	* @param value 弹出的元素
	* @return true: 成功弹出  false: 被中断
	*/
	bool WaitAndPop(T& value) {
		std::unique_lock<std::mutex> l(cache_queue_mutex_);
		cache_convar_.wait(l, [this] {
			return !data_queue_.empty() || abort_;
		});
		if (abort_ || data_queue_.empty()) {
			return false;
		}
		value = std::move(data_queue_.front());
		data_queue_.pop();
		return true;
	}

	/* 尝试访问队首元素（非阻塞）
	* @return std::optional<T> 如果队列为空返回 std::nullopt
	*/
	std::optional<T> TryFront() const {
		std::lock_guard<std::mutex> l(cache_queue_mutex_);
		if (data_queue_.empty()) {
			return std::nullopt;
		}
		return data_queue_.front();
	}

	/* 访问队首元素（阻塞等待）
	* @param timeout_ms 超时时间（毫秒），0表示无限等待
	* @return std::optional<T> 如果超时或中断返回 std::nullopt
	*/
	std::optional<T> WaitAndFront(int timeout_ms = 0) const {
		std::unique_lock<std::mutex> l(cache_queue_mutex_);
		if (timeout_ms > 0) {
			bool success = cache_convar_.wait_for(l, std::chrono::milliseconds(timeout_ms), [this] {
				return !data_queue_.empty() || abort_;
			});
			if (!success || abort_ || data_queue_.empty()) {
				return std::nullopt;
			}
		} else {
			cache_convar_.wait(l, [this] {
				return !data_queue_.empty() || abort_;
			});
			if (abort_ || data_queue_.empty()) {
				return std::nullopt;
			}
		}
		return data_queue_.front();
	}

	/* 访问队尾元素（非阻塞）
	* @return std::optional<T> 如果队列为空返回 std::nullopt
	*/
	std::optional<T> TryBack() const {
		std::lock_guard<std::mutex> l(cache_queue_mutex_);
		if (data_queue_.empty()) {
			return std::nullopt;
		}
		return data_queue_.back();
	}

	/* 交换两个队列
	*/
	void swap(CacheQueue<T>& other)
	{
		if (this == &other)
		{
			return;
		}
		std::unique_lock<std::mutex> l1(cache_queue_mutex_, std::defer_lock);
		std::unique_lock<std::mutex> l2(other.cache_queue_mutex_, std::defer_lock);
		std::lock(l1, l2);
		data_queue_.swap(other.data_queue_);
		std::swap(max_size_, other.max_size_);
		cache_convar_.notify_all();
		other.cache_convar_.notify_all();
	}

	/* 清空队列
	*/
	void Clear()
	{
		abort_ = true;
		cache_convar_.notify_all();  // 唤醒所有等待的线程

		std::unique_lock<std::mutex> l(cache_queue_mutex_);
		while (!data_queue_.empty())
		{
			data_queue_.pop();
		}

		abort_ = false;
	}

	/* 中断所有等待操作（用于优雅关闭）
	*/
	void Abort() {
		abort_ = true;
		cache_convar_.notify_all();
	}

	/* 重置中断标志
	*/
	void Reset() {
		abort_ = false;
	}

	/* 检查是否被中断
	*/
	bool IsAborted() const {
		return abort_.load();
	}

private:
	mutable std::mutex cache_queue_mutex_;			//队列锁
	mutable std::condition_variable cache_convar_;	//队列条件变量
	std::queue<T> data_queue_;						//内存队列
	std::atomic<bool> abort_{ false };				//中断标志
	size_t max_size_{ 0 };							//最大容量（0表示无限制）
};
}//namespace itflee