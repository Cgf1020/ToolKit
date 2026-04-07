#include "uv_timer.h"

namespace itflee {

	UVTimer::UVTimer(std::string timer_name, int timeoutMs, bool repeat, 
				std::function<void(std::string)> task,
				std::function<void(std::string)> destory)
		: timer_name_(std::move(timer_name))
		, timeout_ms_(timeoutMs)
		, repeat_(repeat)
		, task_(std::move(task))
		, destory_(std::move(destory))
	{
		uv_loop_init(&loop_);
		uv_timer_init(&loop_, &timer_);
		timer_.data = this;  // this指向当前对象

		uv_async_init(&loop_, &async_stop_, OnAsyncStop);
		async_stop_.data = this;
	}

	UVTimer::~UVTimer()
	{
		Invalidate();
	}

	void UVTimer::StartTimer()
	{
		const auto timeout = static_cast<uint64_t>(timeout_ms_.load());
		stopping_.store(false, std::memory_order_release);
		uv_timer_start(&timer_, OnTimerTimeout, timeout, repeat_.load() ? timeout : 0);

		timerThread_ = std::make_unique<std::thread>([this]() {
			uv_run(&loop_, UV_RUN_DEFAULT);
			uv_loop_close(&loop_);
		});
	}

	void UVTimer::Invalidate()
	{
		if (stopping_.exchange(true, std::memory_order_acq_rel)) {
			return;
		}

		// request stop on loop thread (libuv handles are not thread-safe)
		uv_async_send(&async_stop_);

		if (timerThread_ && timerThread_->joinable()) {
			timerThread_->join();
			timerThread_.reset();
		}
	}

	bool UVTimer::IsActive() const
	{
		return uv_is_active((uv_handle_t*)&timer_);
	}

	void UVTimer::OnTimerTimeout(uv_timer_t* handle)
	{
		auto* self = reinterpret_cast<UVTimer*>(handle->data);
		self->processTask();
	}

	void UVTimer::OnAsyncStop(uv_async_t* handle)
	{
		auto* self = reinterpret_cast<UVTimer*>(handle->data);
		self->StopInLoop();
	}

	void UVTimer::StopInLoop()
	{
		if (uv_is_active(reinterpret_cast<uv_handle_t*>(&timer_))) {
			uv_timer_stop(&timer_);
		}

		// close handles then stop loop
		uv_close(reinterpret_cast<uv_handle_t*>(&timer_), nullptr);
		uv_close(reinterpret_cast<uv_handle_t*>(&async_stop_), nullptr);
		uv_stop(&loop_);
	}

	void UVTimer::processTask()
	{
		if (task_)
			task_(timer_name_);

		if (!repeat_)
		{
			uv_timer_stop(&timer_);		//停止重复的定时器
			//删除定时器
			if (destory_) {
				destory_(timer_name_);
			}
		}		
	}
}