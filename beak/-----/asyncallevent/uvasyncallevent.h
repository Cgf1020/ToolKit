#ifndef UVAsynCallEvent_H_
#define UVAsynCallEvent_H_

/*使用libuv来进行事件驱动的队列的编写*/

#include "ToolKit/asyncallevent.h"

#include "uv.h"

#include <queue>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

namespace itflee {

	class UVAsynCallEvent : public AsynCallEvent
	{
	public:
		UVAsynCallEvent();
		~UVAsynCallEvent();

	public:
		void Start() final;
		void Post(std::function<void()> task) final;
		void Stop() final;
		size_t Size() const final;
		bool empty() const final;

	private:
		static void uv_async_callback(uv_async_t* handle);
		/*处理实际的事件*/
		void processTasks();

	private:
		std::queue<std::function<void()>>  tasks;
		uv_loop_t loop;
		uv_async_t async_t;
		std::atomic<bool> _closed{ false };
		mutable std::mutex _lock;
		std::unique_ptr<std::thread> _worker;
	};


}

#endif