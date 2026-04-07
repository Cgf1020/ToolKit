// #pragma once
#ifndef STANDARDTIMER_H
#define STANDARDTIMER_H

#include "time/timer.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace itflee {

	class StandardTimer : public Timer
	{
	public:
		StandardTimer(std::string timer_name, int timeoutMs, bool repeat, 
								std::function<void(std::string)> task = nullptr,
								std::function<void(std::string)> destory = nullptr);

		~StandardTimer();

	public:
		void StartTimer() override;

		void Invalidate() override;

		bool IsActive() const override;

	private:

        /* 线程同步
		*/
		std::mutex mutex_;
		std::condition_variable expired_cond_;

		/* 是否计时到执行的标志
		*/
		std::atomic<bool> expired_;

		/* 获取执行的标志
		*/
        std::atomic<bool> try_to_expire_{ false };

		/* 计时器的名称 */
		std::string timer_name_;
		/* 计时器的时长(毫秒) */
		std::atomic<int> timeout_ms_{ 0 };
		/* 是否重复执行 */
		std::atomic<bool> repeat_{ false };
		/* 计时到执行的函数(回调函数) */
		std::function<void(std::string)> task_;
		/* 计时器销毁回调 */
		std::function<void(std::string)> destory_;
		/* 计时器线程 */
		std::unique_ptr<std::thread> timerThread_;
	};

}

#endif