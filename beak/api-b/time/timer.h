#ifndef UTILS_TIMER_H
#define UTILS_TIMER_H

#include "define.h"
#include <string>
#include <memory>
#include <functional>

namespace itflee {
    class ITFLEEEXPORT Timer
    {
    public:
        //using TimerCallback = std::function<void(Timer*)>;

        /* 创建计时器
         * @param timer_name: 计时器的名称
         * @param timeoutMs: 计时时间
         * @param repeat: 是否重复执行
         * @param task: 计时到执行的函数
         * @param destory: 计时器的销毁函数
         * @return: 计时器对象的智能指针
         */
        static std::shared_ptr<Timer> CreateTimer(std::string timer_name, int timeoutMs, bool repeat, 
                                                        std::function<void(std::string)> task = nullptr,
                                                        std::function<void(std::string)> destory = nullptr);

        virtual ~Timer() {}
        /* 开始计时
         */
        virtual void StartTimer() = 0;

        /* 获取计时
         */
        virtual void Invalidate() = 0;

        // 判断计时是否有效
        virtual bool IsActive() const = 0;
    };

}
#endif

