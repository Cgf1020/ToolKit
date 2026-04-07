#ifndef UV_TIMER_H
#define UV_TIMER_H


#include "time/timer.h"
#include "uv.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace itflee {

	class UVTimer : public Timer
	{
	public:
		UVTimer(std::string timer_name, int timeoutMs, bool repeat, 
					std::function<void(std::string)> task = nullptr,
					std::function<void(std::string)> destory = nullptr);
		~UVTimer();
	public:
		void StartTimer() override;
		void Invalidate() override;
		bool IsActive() const override;
	private:
		static void OnTimerTimeout(uv_timer_t* handle);
		static void OnAsyncStop(uv_async_t* handle);

		void processTask();
		void StopInLoop();
	private:
		std::string timer_name_;
		std::atomic<int> timeout_ms_{ 0 };
		std::atomic<bool> repeat_{ false };
		std::atomic<bool> stopping_{ false };
		std::function<void(std::string)> task_;
		std::function<void(std::string)> destory_;
		std::unique_ptr<std::thread> timerThread_;
		uv_loop_t loop_;
		uv_timer_t timer_;
		uv_async_t async_stop_;
	};

}


#endif