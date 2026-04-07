#include "base/eventloop/event_loop_thread.h"
#include "status.h"

#include <chrono>

namespace itflee {

EventLoopThread::EventLoopThread(std::shared_ptr<EventLoopInterface> loop) {
    status_ = std::make_shared<Status>();
    status_->setStatus(Status::kInitializing);
    loop_ = loop ? std::move(loop) : EventLoopInterface::Create();
    status_->setStatus(Status::kInitialized);
}

EventLoopThread::~EventLoopThread() {
    stop();
    join();
}

std::shared_ptr<EventLoopInterface> EventLoopThread::loop() const noexcept {
    return loop_;
}

void EventLoopThread::start(bool wait_thread_started, Functor pre, Functor post) {
    if (loop_ && loop_->IsRunning()) {
        return;
    }
    if (thread_ && thread_->joinable()) {
        join();
    }
    const auto st = status_->status();
    if (st >= Status::kStarting && st < Status::kStopped) {
        return;
    }
    status_->setStatus(Status::kStarting);

    // 捕获 shared_ptr 而非裸 this：即使 EventLoopThread 先被析构（析构时
    // 在 loop 线程上、只能 detach 而不能 join），线程体内的 status_ / loop_
    // 仍有效，不会 use-after-free。
    auto status_ref = status_;
    auto loop_ref   = loop_;
    thread_ = std::make_shared<std::thread>(
        [status_ref, loop_ref,
         pre  = std::move(pre),
         post = std::move(post)]() mutable {
            EventLoopThread::loop_thread_body(
                status_ref, loop_ref, std::move(pre), std::move(post));
        });

    if (wait_thread_started) {
        while (loop_ && !loop_->IsRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void EventLoopThread::stop(bool wait_thread_stopped) {
    const auto st = status_->status();
    if (st < Status::kStarting || st >= Status::kStopping) {
        return;
    }
    status_->setStatus(Status::kStopping);

    if (loop_) {
        loop_->Stop();
    }

    if (wait_thread_stopped) {
        if (loop_ && loop_->IsLoopThread()) {
            return;
        }
        join();
    }
}

void EventLoopThread::join() {
    if (thread_ && thread_->joinable()) {
        if (loop_ && loop_->IsLoopThread()) {
            // 当前线程就是 loop 线程本身（通常来自析构路径被 loop 上的回调触发）。
            // join-self 是 UB；stop() 已调用 loop_->Stop()，Run() 会在本回调
            // 返回后退出，线程将自然结束。
            // 因为线程体捕获的是 shared_ptr（非 this），detach 后不会 UAF。
            thread_->detach();
        } else {
            thread_->join();
        }
        thread_.reset();
    }
}

// static
void EventLoopThread::loop_thread_body(
    std::shared_ptr<Status>             status,
    std::shared_ptr<EventLoopInterface> loop,
    Functor                             pre,
    Functor                             post)
{
    status->setStatus(Status::kStarted);

    if (pre && loop) {
        loop->Post([l = loop, p = std::move(pre)]() mutable {
            if (p() != 0) {
                l->Stop();
            }
        });
    }

    if (loop) {
        loop->Run();
    }

    if (post) {
        post();
    }

    status->setStatus(Status::kStopped);
}

} // namespace itflee
