#include "thread_invoke_impl.h"
#include "threadpool/thread_invoke_interface.h"
#include <memory>

namespace itflee {

    
    /**
     * factory method: create a concrete implementation object
     */
    std::shared_ptr<ThreadInvokeInterface> ThreadInvokeInterface::CreateThreadInvoke(const std::string& name) {
        return std::make_shared<ThreadInvokeImpl>(name);
    }

    void ThreadInvokeImpl::Start() {
        if (_is_running.exchange(true)) return;
        _task_thread = std::make_unique<std::thread>([this]() {
            _thread_id = std::this_thread::get_id();
            this->WorkerLoop();
        });
    }

    void ThreadInvokeImpl::Stop() { 
        if (_is_running.exchange(false)) {
            _cache_convar.notify_all();
            if (_task_thread && _task_thread->joinable()) _task_thread->join();
        }
    }

    void ThreadInvokeImpl::ClearTask(){
        std::unique_lock<std::mutex> lock(_cache_queue_mutex);
        std::queue<std::function<void()>> empty;
        std::swap(_task_queue, empty);
    }

    bool ThreadInvokeImpl::IsCurrent() const {
        return std::this_thread::get_id() == _thread_id;
    }

    void ThreadInvokeImpl::WorkerLoop() {
        while (_is_running) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(_cache_queue_mutex);
                _cache_convar.wait(lock, [this] {
                    return !_task_queue.empty() || !_is_running;
                });
                if (!_is_running && _task_queue.empty()) break;
                task = std::move(_task_queue.front());
                _task_queue.pop();
            }
            if (task) task();
        }
    }

}
