#ifndef THREADPOOL_INVOKE_IMPL_H_
#define THREADPOOL_INVOKE_IMPL_H_

#include "threadpool/threadpool_invoke_interface.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <atomic>
#include <optional>
namespace itflee {

    class ThreadPoolInvokeImpl;

    class ThisThread
    {
        friend class ThreadPoolInvokeImpl;
    
    public:
        /**
         * @brief Get the index of the current thread. If this thread belongs to a `BS::thread_pool` object, the return value will be an index in the range `[0, N)` where `N == BS::thread_pool::get_thread_count()`. Otherwise, for example if this thread is the main thread or an independent thread not in any pools, `std::nullopt` will be returned.
         *
         * @return An `std::optional` object, optionally containing a thread index.
         */
        static std::optional<std::thread::id> get_index() noexcept
        {
            return thread_id_;
        }
    
        /**
         * @brief Get a pointer to the thread pool that owns the current thread. If this thread belongs to a `BS::thread_pool` object, the return value will be a `void` pointer to that object. Otherwise, for example if this thread is the main thread or an independent thread not in any pools, `std::nullopt` will be returned.
         *
         * @return An `std::optional` object, optionally containing a pointer to a thread pool. Note that this will be a `void` pointer, so it must be cast to the desired instantiation of the `BS::thread_pool` template in order to use any member functions.
         */
        static std::optional<void*> get_pool() noexcept
        {
            return thread_pool_ptr_;
        }

    private:
        inline static thread_local std::optional<std::thread::id> thread_id_ = std::nullopt;
        inline static thread_local std::optional<void*> thread_pool_ptr_ = std::nullopt;
    };


    class ThreadPoolInvokeImpl : public ThreadPoolInvokeInterface {
    public:
        ThreadPoolInvokeImpl(std::size_t num_threads, const std::string& name);
        ~ThreadPoolInvokeImpl() override;

        void ClearTask() override;
        void Wait() override;
        bool IsCurrent() const override;
        bool IsPaused() const override;
        std::size_t GetThreadCount() const override;
        void Reset(std::size_t num_threads) override;
        void Pause() override;
        void Unpause() override;
        void EnableWaitDeadlockCheck(bool enable) override;


    protected:
        void PushTask(Task&& task) override;

    private:
        void WorkerLoop();
        void CreateThreads();
        void DestroyThreads();


    private:
        /** The flag to indicate if the pool is running. */
        std::atomic<bool> is_running_{ false };

        /** The flag to indicate if the pool is paused. */
        std::atomic<bool> paused_{ false };

        /** The flag to indicate if the pool is waiting. */
        std::atomic<bool>       waiting_{ false };

        /** The flag to indicate if the wait deadlock check is enabled. */
        std::atomic<bool>       wait_deadlock_check_enabled_{ true };

        /** Number of tasks currently running in worker threads. */
        std::atomic<std::size_t> tasks_running_{ 0 };

        /** The mutex for the task queue and counters. */
        std::mutex              queue_mutex_;

        /** Signalled when tasks_running_ / tasks_total_ reach a state that Wait() cares about. */
        std::condition_variable wait_cv_;

        /** The name of the pool. */
        std::string             pool_name_{ "itflee_thread_pool" };

        /** The number of threads in the pool. */
        std::size_t             num_threads_;

        /** The threads in the pool. */
        std::vector<std::thread> workers_;

        /** The task queue. */
        std::priority_queue<Task> task_queue_;

        /** The condition variable for the task queue. */
        std::condition_variable task_cv_;
    };

}

#endif // THREADPOOL_INVOKE_IMPL_H_
