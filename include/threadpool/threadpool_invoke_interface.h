#ifndef THREADPOOL_INVOKE_INTERFACE_H_
#define THREADPOOL_INVOKE_INTERFACE_H_

#include <string>
#include <future>
#include <functional>
#include <memory>
#include <exception>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "define.h"

namespace itflee {

    using TaskFunc         = std::function<void()>;
    using ErrorHandler     = std::function<void(std::exception_ptr)>;
    using CompleteHandler  = std::function<void()>;
    using Priority_t       = std::int8_t;

    enum class TaskPriority : Priority_t {
        Lowest  = -128,
        Low     = -64,
        Normal  = 0,
        High    = 64,
        Highest = 127,
    };

    struct Task {
        TaskFunc        func;
        CompleteHandler on_complete;
        ErrorHandler    on_error;
        Priority_t      priority = static_cast<Priority_t>(TaskPriority::Normal);

        Task() = default;

        Task(TaskFunc f, Priority_t p = static_cast<Priority_t>(TaskPriority::Normal))
            : func(std::move(f)), priority(p) {}

        Task(TaskFunc f,
             CompleteHandler complete,
             ErrorHandler error,
             Priority_t p = static_cast<Priority_t>(TaskPriority::Normal))
            : func(std::move(f))
            , on_complete(std::move(complete))
            , on_error(std::move(error))
            , priority(p) {}

        friend bool operator<(const Task& lhs, const Task& rhs) noexcept {
            return lhs.priority < rhs.priority;
        }
    };

    // class WaitDeadlockError : public std::logic_error {
    // public:
    //     explicit WaitDeadlockError(const std::string& msg)
    //         : std::logic_error(msg) {}
    // };

    class WaitDeadlockError : public std::runtime_error
    {
    public:
        explicit WaitDeadlockError(const std::string& msg) : std::runtime_error(msg) {};
    };

    template<typename T>
    class MultiFuture : public std::vector<std::future<T>> {
    public:
        using Base = std::vector<std::future<T>>;
        using Base::Base;

        std::vector<T> GetAll() {
            std::vector<T> results;
            results.reserve(this->size());
            for (auto& f : *this) {
                results.push_back(f.get());
            }
            return results;
        }

        void WaitAll() {
            for (auto& f : *this) {
                f.wait();
            }
        }
    };

    /**
     * Thread pool interface: multiple worker threads, shared task queue.
     */
    class ThreadPoolInvokeInterface {
    public:
        virtual ~ThreadPoolInvokeInterface() = default;

        /**
         * Factory: create a thread pool with the given number of worker threads.
         * @param num_threads the number of threads to create
         * @param pool_name the name of the pool
         * @return a shared pointer to the thread pool
         */
        static ITFLEEEXPORT std::shared_ptr<ThreadPoolInvokeInterface> CreateThreadPool(
            std::size_t num_threads,
            const std::string& pool_name = "");

        /**
         * Enable the wait deadlock check (if called from a pool worker, Wait() throws).
         */
        virtual void EnableWaitDeadlockCheck(bool enable) = 0;

        /**
         * Check whether the pool is currently paused (workers stop taking new tasks).
         * @return true if paused, false otherwise
         */
        virtual bool IsPaused() const = 0;

        /**
         * Pause the pool. Workers stop retrieving new tasks; running tasks continue until done.
         */
        virtual void Pause() = 0;

        /**
         * Unpause the pool. Workers resume retrieving tasks from the queue.
         */
        virtual void Unpause() = 0;

        /**
         * Clear pending tasks in the queue (purge). Running tasks are not affected.
         */
        virtual void ClearTask() = 0;

        /**
         * Get the number of worker threads in the pool.
         * @return the number of worker threads in the pool
         */
        virtual std::size_t GetThreadCount() const = 0;

        /**
         * Reset the pool with a new number of threads. Waits for all tasks to complete, then recreates threads.
         * @param num_threads the number of threads to set
         * @return void
         */
        virtual void Reset(const std::size_t num_threads) = 0;

        /**
         * Block until all submitted tasks have finished (queue empty and no task running).
         * If pool is paused, waits only for currently running tasks.
         * @return void
         */
        virtual void Wait() = 0;

        /**
         * True if the current thread is one of the pool's worker threads.
         * @return true if the current thread is one of the pool's worker threads, false otherwise
         */
        virtual bool IsCurrent() const = 0;

        /**
         * Asynchronously submit a task and return a future (default priority).
         */
        template<typename F, typename... Args>
        auto InvokeAsync(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
            return InvokeAsyncFuture(TaskPriority::Normal,
                                     std::forward<F>(f),
                                     std::forward<Args>(args)...);
        }

        /**
         * Asynchronously submit a task with priority and return a future.
         */
        template<typename F, typename... Args>
        auto InvokeAsync(TaskPriority priority,
                         F&& f, Args&&... args)
            -> std::future<std::invoke_result_t<F, Args...>>
        {
            return InvokeAsyncFuture(priority,
                                     std::forward<F>(f),
                                     std::forward<Args>(args)...);
        }

        /**
         * Asynchronously submit a task with success callback (default priority).
         * Callbacks run on the worker thread. Also returns a future.
         */
        template<typename F, typename OnSuccess, typename... Args>
        auto InvokeAsync(F&& f,
                         OnSuccess&& on_success,
                         Args&&... args)
            -> std::future<std::invoke_result_t<F, Args...>>
        {
            return InvokeAsyncWithCallbacks(TaskPriority::Normal,
                                            std::forward<F>(f),
                                            std::forward<OnSuccess>(on_success),
                                            ErrorHandler{},
                                            std::forward<Args>(args)...);
        }

        /**
         * Asynchronously submit a task with success/error callbacks (default priority).
         * Callbacks run on the worker thread. Also returns a future.
         */
        template<typename F, typename OnSuccess, typename OnError, typename... Args>
        auto InvokeAsync(F&& f,
                         OnSuccess&& on_success,
                         OnError&& on_error,
                         Args&&... args)
            -> std::future<std::invoke_result_t<F, Args...>>
        {
            return InvokeAsyncWithCallbacks(TaskPriority::Normal,
                                            std::forward<F>(f),
                                            std::forward<OnSuccess>(on_success),
                                            std::forward<OnError>(on_error),
                                            std::forward<Args>(args)...);
        }

        /**
         * Asynchronously submit a task with priority and success callback.
         * Callbacks run on the worker thread. Also returns a future.
         */
        template<typename F, typename OnSuccess, typename... Args>
        auto InvokeAsync(TaskPriority priority,
                         F&& f,
                         OnSuccess&& on_success,
                         Args&&... args)
            -> std::future<std::invoke_result_t<F, Args...>>
        {
            return InvokeAsyncWithCallbacks(priority,
                                            std::forward<F>(f),
                                            std::forward<OnSuccess>(on_success),
                                            ErrorHandler{},
                                            std::forward<Args>(args)...);
        }

        /**
         * Asynchronously submit a task with priority and success/error callbacks.
         * Callbacks run on the worker thread. Also returns a future.
         */
        template<typename F, typename OnSuccess, typename OnError, typename... Args>
        auto InvokeAsync(TaskPriority priority,
                         F&& f,
                         OnSuccess&& on_success,
                         OnError&& on_error,
                         Args&&... args)
            -> std::future<std::invoke_result_t<F, Args...>>
        {
            return InvokeAsyncWithCallbacks(priority,
                                            std::forward<F>(f),
                                            std::forward<OnSuccess>(on_success),
                                            std::forward<OnError>(on_error),
                                            std::forward<Args>(args)...);
        }

        /**
         * Submit and wait for result; if current thread is a pool worker, run directly.
         */
        template<typename F, typename... Args>
        auto InvokeSync(F&& f, Args&&... args) -> std::invoke_result_t<F, Args...> {
            if (IsCurrent()) {
                return std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
            }
            return InvokeAsync(std::forward<F>(f), std::forward<Args>(args)...).get();
        }

        /**
         * Submit a pre-built task. Callbacks run on the worker thread that executes the task.
         */
        void Submit(Task&& task) {
            PushTask(std::move(task));
        }

    protected:
        template<typename F, typename... Args>
        auto InvokeAsyncFuture(TaskPriority priority,
                               F&& f, Args&&... args)
            -> std::future<std::invoke_result_t<F, Args...>>
        {
            using ReturnType = std::invoke_result_t<F, Args...>;

            auto task_ptr = std::make_shared<std::packaged_task<ReturnType()>>(
                [f = std::forward<F>(f),
                 args_tuple = std::make_tuple(std::forward<Args>(args)...)]() mutable {
                    return std::apply(std::move(f), std::move(args_tuple));
                }
            );

            std::future<ReturnType> res = task_ptr->get_future();

            Task task{
                [task_ptr]() { (*task_ptr)(); },
                static_cast<Priority_t>(priority)
            };
            this->PushTask(std::move(task));
            return res;
        }

        template<typename F, typename OnSuccess, typename OnError, typename... Args>
        auto InvokeAsyncWithCallbacks(TaskPriority priority,
                                      F&& f,
                                      OnSuccess&& on_success,
                                      OnError&& on_error,
                                      Args&&... args)
            -> std::future<std::invoke_result_t<F, Args...>>
        {
            using ReturnType = std::invoke_result_t<F, Args...>;

            auto promise = std::make_shared<std::promise<ReturnType>>();
            std::future<ReturnType> res = promise->get_future();

            Task task{
                [promise,
                 f = std::forward<F>(f),
                 on_success = std::forward<OnSuccess>(on_success),
                 on_error = std::forward<OnError>(on_error),
                 args_tuple = std::make_tuple(std::forward<Args>(args)...)]() mutable {
                    try {
                        if constexpr (std::is_void_v<ReturnType>) {
                            std::apply(std::move(f), std::move(args_tuple));
                            promise->set_value();
                            std::invoke(std::move(on_success));
                        } else {
                            auto result = std::apply(std::move(f), std::move(args_tuple));
                            promise->set_value(result);
                            std::invoke(std::move(on_success), std::move(result));
                        }
                    } catch (...) {
                        const auto eptr = std::current_exception();
                        promise->set_exception(eptr);
                        if constexpr (std::is_same_v<std::decay_t<OnError>, ErrorHandler>) {
                            if (on_error) {
                                on_error(eptr);
                            }
                        } else {
                            std::invoke(std::move(on_error), eptr);
                        }
                    }
                },
                static_cast<Priority_t>(priority)
            };
            this->PushTask(std::move(task));
            return res;
        }

        /**
         * Push a task to the task queue.
         */
        virtual void PushTask(Task&& task) = 0;
    };

}

#endif // THREADPOOL_INVOKE_INTERFACE_H_
