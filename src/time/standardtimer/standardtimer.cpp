#include "standardtimer.h"

#include <chrono>

namespace itflee {



	StandardTimer::StandardTimer(std::string timer_name, int timeoutMs, bool repeat, 
												std::function<void(std::string)> task,
												std::function<void(std::string)> destory)
		: expired_(true)
		, try_to_expire_(false)
		, timer_name_(std::move(timer_name))
		, timeout_ms_(timeoutMs)
		, repeat_(repeat)
		, task_(std::move(task))
		, destory_(std::move(destory))
	{}

	StandardTimer::~StandardTimer()
	{
		Invalidate();
	}

	void StandardTimer::StartTimer()
	{
		if (expired_ == false)
		{
			return;
		}
		expired_ = false;
		timerThread_ = std::make_unique<std::thread>([this]() {
			int64_t cnsNext = 0;		// 等待时间
			while (!try_to_expire_)
			{
				if (!cnsNext)
				{
					cnsNext = 1000 * timeout_ms_.load();	// convert ms to microseconds for wait_for timeout
				}
				std::unique_lock<std::mutex> locker(mutex_);

				//std::chrono::system_clock::time_point s_time = std::chrono::system_clock::now() + std::chrono::microseconds(cnsNext);
				if (expired_cond_.wait_for(locker, std::chrono::microseconds(cnsNext)) == std::cv_status::timeout)
				{
					// timeout
					//int64_t	pos = tk::timeMicros();
					if (!try_to_expire_ && task_)
					{
						task_(timer_name_);
					}
					//pos += tk::kNumMicrosecsPerMillisec * timeout_ms_;
					//cnsNext = pos - tk::timeMicros();
				}

				if (repeat_ == false)
				{
					break;
				}
			};

			{
				std::unique_lock<std::mutex> locker(mutex_);
				expired_ = true;
				expired_cond_.notify_all();
			}

			// notify external destroy callback (optional)
			// if (destory_)
			// {
			// 	destory_(timer_name_);
			// }

			});
	}

	void StandardTimer::Invalidate()
	{
		if (expired_ || try_to_expire_)
		{
			return;
		}
		try_to_expire_ = true;
		expired_cond_.notify_all();
		{
			std::unique_lock<std::mutex> locker(mutex_);
			if (!expired_)
			{
				expired_cond_.wait(locker, [this] { return expired_ == true; });
			}
			try_to_expire_ = false;
		}

		if (timerThread_ && timerThread_->joinable())
		{
			timerThread_->join();
			timerThread_.reset();
		}
	}

	bool StandardTimer::IsActive() const
	{
		return !expired_.load();
	}

}
