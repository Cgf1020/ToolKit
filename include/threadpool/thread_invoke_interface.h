#ifndef THREAD_INVOKE_INTERFACE_H_
#define THREAD_INVOKE_INTERFACE_H_

#include <string>
#include <future>
#include <functional>
#include <memory>
#include "define.h"

namespace itflee {

    /**
     * abstract base class: only define the interface, not contain any synchronous member variables
     */
    class ThreadInvokeInterface {
    public:
        virtual ~ThreadInvokeInterface() = default;

        /**
         * factory method: create a concrete implementation object
         */
        static ITFLEEEXPORT std::shared_ptr<ThreadInvokeInterface> CreateThreadInvoke(const std::string& thread_name = "");

        /**
         * start the thread and run the worker loop
         */
        virtual void Start() = 0;

        /**
         * stop the thread
         */
        virtual void Stop() = 0;

        /**
         * clear the task queue
         */
        virtual void ClearTask() = 0;

        /**
         * check if the current thread is the thread of the thread invoke
         */
        virtual bool IsCurrent() const = 0;

        /**
         * generic Invoke interface (kept in the base class for external calls)
         */
        template<typename F, typename... Args>
        auto Invoke(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
            using ReturnType = std::invoke_result_t<F, Args...>;

            // use packaged_task to associate the asynchronous result 包装任务：将函数和参数绑定为packaged_task
    #if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
            auto task = std::make_shared<std::packaged_task<ReturnType()>>(
                [f = std::forward<F>(f), ...args = std::forward<Args>(args)]() mutable {
                    return std::invoke(std::move(f), std::move(args)...);
                }
            );
    #else
            auto task = std::make_shared<std::packaged_task<ReturnType()>>(
                [f = std::forward<F>(f), args_tuple = std::make_tuple(std::forward<Args>(args)...)]() mutable {
                    return std::apply(std::move(f), std::move(args_tuple));
                }
            );
    #endif

            std::future<ReturnType> res = task->get_future();
            // call the protected interface of the subclass to push the task to the task queue
            this->PushTask([task]() { (*task)(); });
            return res;
        }

        /**
         * synchronous execution and wait for the result, if the current thread is the thread of the thread invoke, execute the task directly
         */
        template<typename F, typename... Args>
        auto InvokeSync(F&& f, Args&&... args) -> std::invoke_result_t<F, Args...> {
            if (IsCurrent()) {
                return std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
            }
            return Invoke(std::forward<F>(f), std::forward<Args>(args)...).get();
        }

    protected:
        /**
         * push the task to the task queue
         */
        virtual void PushTask(std::function<void()> task) = 0;
    };

}

#endif //THREAD_INVOKE_INTERFACE_H_