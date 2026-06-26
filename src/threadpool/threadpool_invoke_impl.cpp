#include "threadpool_invoke_impl.h"
#include "threadpool/threadpool_invoke_interface.h"
#include <memory>

namespace itflee {
    
    std::shared_ptr<ThreadPoolInvokeInterface> ThreadPoolInvokeInterface::CreateThreadPool(
        std::size_t num_threads,
        const std::string& pool_name)
    {
        return std::make_shared<ThreadPoolInvokeImpl>(num_threads, pool_name);
    }

    ThreadPoolInvokeImpl::ThreadPoolInvokeImpl(std::size_t num_threads, const std::string& name)
        : pool_name_(name)
        , num_threads_(num_threads > 0 ? num_threads : 1)
    {
        CreateThreads();
    }

    ThreadPoolInvokeImpl::~ThreadPoolInvokeImpl() {
        Wait();
        DestroyThreads();
    }

    void ThreadPoolInvokeImpl::CreateThreads() {
        workers_.reserve(num_threads_);
        is_running_.store(true);
        for (std::size_t i = 0; i < num_threads_; ++i) {
            workers_.emplace_back([this, i]() {
                WorkerLoop();
            });
        }
    }

    void ThreadPoolInvokeImpl::DestroyThreads() {
        is_running_.store(false);
        task_cv_.notify_all();
        for (auto& t : workers_) {
            if (t.joinable()) t.join();
        }
        workers_.clear();
    }

    void ThreadPoolInvokeImpl::ClearTask() {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        // 丢弃所有尚未开始执行的任务，不影响已经执行中的任务
        task_queue_ = decltype(task_queue_)();
        wait_cv_.notify_all();
    }

    void ThreadPoolInvokeImpl::Wait() {
        if (wait_deadlock_check_enabled_.load()) {
            auto pool = ThisThread::get_pool();
            if (pool.has_value() && pool.value() == static_cast<void*>(this)) {
                throw WaitDeadlockError("Deadlock detected: Wait() called from a pool worker thread.");
            }
        }
        std::unique_lock<std::mutex> lock(queue_mutex_);
        waiting_.store(true);
        // 未暂停：等待所有任务完成（队列为空且无运行中的任务）；
        // 暂停时：只等待运行中的任务清零（队列里的任务在暂停期间不会再被取出）。
        wait_cv_.wait(lock, [this] {
            if (paused_.load()) {
                return tasks_running_.load() == 0;
            }
            return tasks_running_.load() == 0 && task_queue_.empty();
        });
        waiting_.store(false);
    }

    bool ThreadPoolInvokeImpl::IsCurrent() const {
        auto idx = ThisThread::get_index();
        return idx.has_value() && idx.value() == std::this_thread::get_id();
    }

    void ThreadPoolInvokeImpl::PushTask(Task&& task) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            task_queue_.emplace(std::move(task));
        }
        task_cv_.notify_one();
    }

    void ThreadPoolInvokeImpl::ExecuteTask(Task& task) {
        if (!task.func) {
            return;
        }
        try {
            task.func();
            if (task.on_complete) {
                task.on_complete();
            }
        } catch (...) {
            const auto eptr = std::current_exception();
            if (task.on_error) {
                task.on_error(eptr);
            }
        }
    }

    void ThreadPoolInvokeImpl::WorkerLoop() {

        ThisThread::thread_id_ = std::this_thread::get_id();
        ThisThread::thread_pool_ptr_ = static_cast<void*>(this);

        while (is_running_.load()) {
            Task task;
            struct RunningGuard {
                ThreadPoolInvokeImpl* self{nullptr};
                bool active{false};
                ~RunningGuard() {
                    if (!active || !self) return;
                    std::lock_guard<std::mutex> lock(self->queue_mutex_);
                    self->tasks_running_.fetch_sub(1);
                    self->wait_cv_.notify_all();
                }
            } running_guard{this, false};
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                // 如果有wait，只要 在暂停 / 队列为空的   情况下 通知，避免阻塞 wait线程
                if (waiting_.load() &&
                    (paused_.load() || task_queue_.empty()))
                    wait_cv_.notify_all();

                task_cv_.wait(lock, [this] {
                    return (!task_queue_.empty() && !paused_.load()) ||
                           !is_running_.load();
                });
                if (!is_running_.load()) break;

                if (paused_.load()) {
                    continue;
                }

                task = task_queue_.top();
                task_queue_.pop();
                tasks_running_.fetch_add(1);
                running_guard.active = true;
            }
            ExecuteTask(task);
        }
    }

    std::size_t ThreadPoolInvokeImpl::GetThreadCount() const {
        return num_threads_;
    }

    void ThreadPoolInvokeImpl::Reset(std::size_t num_threads) {
        if (num_threads == 0) {
            num_threads = 1;
        }
        if (num_threads == num_threads_) return;
        Wait();
        DestroyThreads();
        num_threads_ = num_threads;
        CreateThreads();
    }

    void ThreadPoolInvokeImpl::Pause() {
        paused_.store(true);
    }

    void ThreadPoolInvokeImpl::Unpause() {
        paused_.store(false);
        task_cv_.notify_all();
    }

    bool ThreadPoolInvokeImpl::IsPaused() const {
        return paused_.load();
    }

    void ThreadPoolInvokeImpl::EnableWaitDeadlockCheck(bool enable) {
        wait_deadlock_check_enabled_.store(enable);
    }



}
