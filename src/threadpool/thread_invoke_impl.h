#ifndef THREAD_INVOKE_IMPL_H_
#define THREAD_INVOKE_IMPL_H_


#include "threadpool/thread_invoke_interface.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

namespace itflee {

class ThreadInvokeImpl : public ThreadInvokeInterface {
public:
    explicit ThreadInvokeImpl(const std::string& name) : _thread_name(name) {}
    ~ThreadInvokeImpl() { Stop(); }

    void Start() override;

    void Stop() override;

    void ClearTask() override;

    bool IsCurrent() const override;

protected:
    /**
     * push the task to the task queue
     */
    void PushTask(std::function<void()> task) override {
        {
            std::unique_lock<std::mutex> lock(_cache_queue_mutex);
            _task_queue.emplace(std::move(task));
        }
        _cache_convar.notify_one();
    }

private:
    void WorkerLoop();

private:
    std::string _thread_name;
    std::queue<std::function<void()>> _task_queue;
    std::mutex _cache_queue_mutex;
    std::condition_variable _cache_convar;
    std::unique_ptr<std::thread> _task_thread;
    std::atomic<bool> _is_running{ false };
    std::thread::id _thread_id;
};


}

#endif //THREAD_INVOKE_IMPL_H_