#ifndef AsynCallEvent_H_
#define AsynCallEvent_H_

#include <functional>
#include <memory>
#include "define.h"
//using task_type = std::function<void(void)>;
/*
* 主要作为事件驱动来使用
*/

namespace itflee {

	class ITFLEEEXPORT AsynCallEvent
	{
	public:
		~AsynCallEvent() {}
		virtual void Start() = 0;
		virtual void Post(std::function<void()> task) = 0;
		virtual void Stop() = 0;
		virtual size_t Size() const = 0;
		virtual bool empty() const = 0;

		static std::shared_ptr<AsynCallEvent> instance();
	};

}

#endif



