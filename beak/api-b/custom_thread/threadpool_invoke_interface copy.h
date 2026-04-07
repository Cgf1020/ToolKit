#ifndef THREADPOOL_INVOKE_INTERFACE_H_
#define THREADPOOL_INVOKE_INTERFACE_H_

#include <string>
#include <future>
#include <functional>
#include <memory>
#include <cstddef>
#include "define.h"

namespace itflee {

    /**
     * Thread pool interface: multiple worker threads, shared task queue.
     */
    class ThreadPoolInvokeInterface {
    public:
        virtual ~ThreadPoolInvokeInterface() = default;

        /**
         * Factory: create a thread pool with the given number of worker threads.
         */
        static ITFLEEEXPORT std::shared_ptr<ThreadPoolInvokeInterface> CreateThreadPool(
            std::size_t num_threads,
            const std::string& pool_name = "");

        /**
         * Start all worker threads.
         */
        virtual void Start() = 0;

        /**
         * Stop all workers and wait for them to finish.
         */
        virtual void Stop() = 0;

        /**
         * Clear pending tasks in the queue.
         */
        virtual void ClearTask() = 0;

        /**
         * Block until all submitted tasks have finished (queue empty and no task running).
         */
        virtual void Wait() = 0;

        /**
         * True if the current thread is one of the pool's worker threads.
         */
        virtual bool IsCurrent() const = 0;

        /**
         * Submit a task and return a future for its result.
         */
        template<typename F, typename... Args>
        auto Invoke(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
            using ReturnType = std::invoke_result_t<F, Args...>;

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
            this->PushTask([task]() { (*task)(); });
            return res;
        }

        /**
         * Submit and wait for result; if current thread is a pool worker, run directly.
         */
        template<typename F, typename... Args>
        auto InvokeSync(F&& f, Args&&... args) -> std::invoke_result_t<F, Args...> {
            if (IsCurrent()) {
                return std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
            }
            return Invoke(std::forward<F>(f), std::forward<Args>(args)...).get();
        }

    protected:
        virtual void PushTask(std::function<void()> task) = 0;
    };

}

#endif // THREADPOOL_INVOKE_INTERFACE_H_
