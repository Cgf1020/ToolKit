#include "uvasyncallevent.h"


namespace itflee {
	static std::shared_ptr<AsynCallEvent> _asyncallevent(new UVAsynCallEvent());

	std::shared_ptr<AsynCallEvent> AsynCallEvent::instance()
	{
		if (!_asyncallevent)
		{
			_asyncallevent = std::static_pointer_cast<AsynCallEvent>(std::make_shared<UVAsynCallEvent>());
		}

		return _asyncallevent;
	}
	UVAsynCallEvent::UVAsynCallEvent()
	{	
		uv_loop_init(&loop);
		std::cout << uv_async_init(&loop, &async_t, uv_async_callback) << std::endl;
		async_t.data = this;
	}

	UVAsynCallEvent::~UVAsynCallEvent()
	{
		Stop();
	}

	void UVAsynCallEvent::Start()
	{
		_worker = std::make_unique<std::thread>([this]() {
			uv_run(&loop, UV_RUN_DEFAULT);
		});
	}

	void UVAsynCallEvent::Post(std::function<void()> task)
	{
		if (_closed)
			return;
		{
			std::lock_guard<std::mutex> lock(_lock);
			tasks.push(std::move(task));
		}
		uv_async_send(&async_t);	// 唤醒事件循环
	}

	void UVAsynCallEvent::Stop()
	{
		_closed = true;
		uv_close(reinterpret_cast<uv_handle_t*>(&async_t), nullptr);
		uv_loop_close(&loop);
		if (_worker && _worker->joinable()) {
			_worker->join();
		}
	}

	size_t UVAsynCallEvent::Size() const
	{
		std::lock_guard<std::mutex> lock(_lock);
		return tasks.size();
	}

	bool UVAsynCallEvent::empty() const
	{
		return Size() == 0;
	}

	void UVAsynCallEvent::uv_async_callback(uv_async_t* handle)
	{
		//将void*类型的指针转换成 具体类型的
		UVAsynCallEvent* self = reinterpret_cast<UVAsynCallEvent*>(handle->data);
		self->processTasks();
	}

	void UVAsynCallEvent::processTasks()
	{
		std::queue<std::function<void()>> local_tasks;
		{
			std::unique_lock<std::mutex> lock(_lock);
			local_tasks.swap(tasks);
		}

		while (!local_tasks.empty()) {
			local_tasks.front()();
			local_tasks.pop();
		}
	}
}