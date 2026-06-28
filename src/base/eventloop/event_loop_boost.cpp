#include "event_loop_boost.h"

#include <algorithm>
#if BOOST_VERSION >= 108000
#include <boost/asio/any_io_executor.hpp>
#endif
#include <boost/asio/dispatch.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <exception>

namespace itflee {

    namespace detail {

    struct SignalSlot : std::enable_shared_from_this<SignalSlot> {
        boost::asio::signal_set sig;
        std::vector<std::pair<uint64_t, EventLoopInterface::SignalHandler>> handlers;
        bool waiting{false};

        SignalSlot(boost::asio::io_context& ioc, int s) : sig(ioc) {
            sig.add(s);
        }

        void remove_handler(uint64_t id) {
            handlers.erase(
                std::remove_if(handlers.begin(), handlers.end(),
                    [id](const auto& p) { return p.first == id; }),
                handlers.end());
        }

        void begin_wait(std::weak_ptr<EventLoopBoost> loop_weak);
    };

    void SignalSlot::begin_wait(std::weak_ptr<EventLoopBoost> loop_weak) {
        if (handlers.empty() || waiting) {
            return;
        }
        waiting = true;
        sig.async_wait([self = shared_from_this(), loop_weak](boost::system::error_code ec, int received) {
            self->waiting = false;
            if (ec == boost::asio::error::operation_aborted) {
                return;
            }
            if (ec) {
                return;
            }
            if (!loop_weak.lock()) {
                return;
            }
            auto copy = self->handlers;
            for (auto& kv : copy) {
                if (kv.second) {
                    try {
                        kv.second(received);
                    } catch (...) {
                        // 吞掉用户信号回调异常，避免 run() 退出。
                    }
                }
            }
            if (!self->handlers.empty()) {
                self->begin_wait(loop_weak);
            }
        });
    }

    } // namespace detail

    std::shared_ptr<EventLoopInterface> EventLoopInterface::Create() {
        return std::make_shared<EventLoopBoost>();
    }

    EventLoopBoost::EventLoopBoost() {
    }

    EventLoopBoost::~EventLoopBoost() {
        Stop();
    }

    void EventLoopBoost::reset_work_in_loop() {
        work_.reset();
    }

    void EventLoopBoost::signal_unregister(int signum, uint64_t handler_id) {
        auto it = signal_slots_.find(signum);
        if (it == signal_slots_.end()) {
            return;
        }
        const auto& slot = it->second;
        slot->remove_handler(handler_id);
        if (slot->handlers.empty()) {
            boost::system::error_code ec;
            slot->sig.cancel(ec);
            signal_slots_.erase(it);
        }
    }

    void EventLoopBoost::Run() {
        RunState expected = RunState::kIdle;
        if (!state_.compare_exchange_strong(expected, RunState::kRunning, std::memory_order_acq_rel)) {
            return;
        }
        loop_thread_id_ = std::this_thread::get_id();
#if BOOST_VERSION >= 108000
        work_.emplace(boost::asio::make_work_guard(ioc_.get_executor()));
#else
        work_.emplace(ioc_);
#endif
        ioc_.restart();
        ioc_.run();
        work_.reset();
        loop_thread_id_ = std::thread::id{};
        state_.store(RunState::kIdle, std::memory_order_release);
    }

    void EventLoopBoost::Stop() {
        const RunState old = state_.exchange(RunState::kStopping, std::memory_order_acq_rel);
        if (old == RunState::kIdle) {
            return;
        }

        boost::asio::post(ioc_, [this]() {
            signal_slots_.clear();
            reset_work_in_loop();
            // Stop 过程中队列里可能存在未再回调的取消项（例如 ioc_.stop() 直接终止等待），
            // 这里统一归零 pending，避免指标长期残留。
            pending_tasks_.store(0, std::memory_order_relaxed);
            ioc_.stop();
        });
    }

    bool EventLoopBoost::IsLoopThread() const noexcept {
        if (!IsRunning()) {
            return false;
        }
        return std::this_thread::get_id() == loop_thread_id_;
    }

    bool EventLoopBoost::IsRunning() const noexcept {
        return state_.load(std::memory_order_acquire) == RunState::kRunning;
    }

    void EventLoopBoost::Post(Task task) {
        if (!task) {
            return;
        }
        on_task_enqueued();
        const auto enqueued_at = std::chrono::steady_clock::now();
        boost::asio::post(ioc_, [this, task = std::move(task), enqueued_at]() mutable {
            try {
                execute_task(std::move(task));
                on_task_executed(enqueued_at);
            } catch (...) {
                on_task_failed(enqueued_at);
            }
        });
    }

    void EventLoopBoost::Dispatch(Task task) {
        if (!task) {
            return;
        }
        on_task_enqueued();
        const auto enqueued_at = std::chrono::steady_clock::now();
        boost::asio::dispatch(ioc_, [this, task = std::move(task), enqueued_at]() mutable {
            try {
                execute_task(std::move(task));
                on_task_executed(enqueued_at);
            } catch (...) {
                on_task_failed(enqueued_at);
            }
        });
    }

    void EventLoopBoost::PostEvent(EventCallback cb) {
        if (!cb) {
            return;
        }
        auto ev = std::make_shared<Event>(std::move(cb));
        on_task_enqueued();
        const auto enqueued_at = std::chrono::steady_clock::now();
        boost::asio::post(ioc_, [this, ev, enqueued_at]() {
            try {
                if (ev->cb) {
                    ev->cb(ev.get());
                }
                on_task_executed(enqueued_at);
            } catch (...) {
                on_task_failed(enqueued_at);
            }
        });
    }

    EventLoopInterface::CancelToken EventLoopBoost::ScheduleAfter(Duration delay, Task task) {
        if (!task) {
            return nullptr;
        }
#if BOOST_VERSION >= 108000
        auto ex = std::any_cast<boost::asio::io_context::executor_type>(GetNativeExecutor());
        auto timer = std::make_shared<boost::asio::steady_timer>(boost::asio::any_io_executor(ex));
#else
        auto ex  = std::any_cast<boost::asio::io_context::executor_type>(GetNativeExecutor());
        auto& ioc = static_cast<boost::asio::io_context&>(ex.context());
        auto timer = std::make_shared<boost::asio::steady_timer>(ioc);
#endif
        timer->expires_after(delay);
        on_task_enqueued();
        const auto enqueued_at = std::chrono::steady_clock::now();
        timer->async_wait([this, timer, task = std::move(task), enqueued_at](const boost::system::error_code& ec) mutable {
            if (ec == boost::asio::error::operation_aborted) {
                on_task_canceled();
                return;
            }
            if (ec) {
                on_task_failed(enqueued_at);
                return;
            }
            try {
                execute_task(std::move(task));
                on_task_executed(enqueued_at);
            } catch (...) {
                on_task_failed(enqueued_at);
            }
        });
        return std::shared_ptr<void>(timer.get(), [timer](void*) noexcept {
#if BOOST_VERSION >= 108900
            timer->cancel();
#else
            boost::system::error_code cancel_ec;
            timer->cancel(cancel_ec);
#endif
        });
    }

    EventLoopInterface::CancelToken EventLoopBoost::ScheduleEvery(Duration period, Task task) {
        if (!task || period <= Duration::zero()) {
            return nullptr;
        }
        struct RepeatingState {
            boost::asio::steady_timer timer;
            Duration period;
            Task task;
            EventLoopBoost* owner{nullptr};
            std::chrono::steady_clock::time_point enqueued_at;
#if BOOST_VERSION >= 108000
            RepeatingState(boost::asio::any_io_executor ex,
                           Duration p,
                           Task t,
                           EventLoopBoost* loop)
                : timer(std::move(ex))
                , period(p)
                , task(std::move(t))
                , owner(loop)
                , enqueued_at(std::chrono::steady_clock::now()) {
                timer.expires_after(period);
            }
#else
            RepeatingState(boost::asio::io_context& ioc,
                           Duration p,
                           Task t,
                           EventLoopBoost* loop)
                : timer(ioc)
                , period(p)
                , task(std::move(t))
                , owner(loop)
                , enqueued_at(std::chrono::steady_clock::now()) {
                timer.expires_after(period);
            }
#endif
            void arm(const std::shared_ptr<RepeatingState>& self) {
                self->owner->on_task_enqueued();
                self->enqueued_at = std::chrono::steady_clock::now();
                self->timer.async_wait([self](const boost::system::error_code& ec) {
                    if (ec == boost::asio::error::operation_aborted) {
                        self->owner->on_task_canceled();
                        return;
                    }
                    if (ec) {
                        self->owner->on_task_failed(self->enqueued_at);
                        return;
                    }
                    try {
                        self->owner->execute_task(self->task);
                        self->owner->on_task_executed(self->enqueued_at);
                    } catch (...) {
                        self->owner->on_task_failed(self->enqueued_at);
                    }
                    self->timer.expires_after(self->period);
                    self->arm(self);
                });
            }
        };
#if BOOST_VERSION >= 108000
        auto ex = std::any_cast<boost::asio::io_context::executor_type>(GetNativeExecutor());
        auto state = std::make_shared<RepeatingState>(boost::asio::any_io_executor(ex), period, std::move(task), this);
#else
        auto ex  = std::any_cast<boost::asio::io_context::executor_type>(GetNativeExecutor());
        auto& ioc = static_cast<boost::asio::io_context&>(ex.context());
        auto state = std::make_shared<RepeatingState>(ioc, period, std::move(task), this);
#endif
        state->arm(state);
        return std::shared_ptr<void>(state.get(), [state](void*) noexcept {
#if BOOST_VERSION >= 108900
            state->timer.cancel();
#else
            boost::system::error_code cancel_ec;
            state->timer.cancel(cancel_ec);
#endif
        });
    }

    EventLoopInterface::CancelToken EventLoopBoost::OnSignal(int signum, SignalHandler handler) {
        if (!handler) {
            return nullptr;
        }

        const uint64_t hid = next_signal_id_.fetch_add(1, std::memory_order_relaxed);
        const auto self = shared_from_this();

        boost::asio::post(ioc_, [self, signum, hid, h = std::move(handler)]() mutable {
            auto& slot_ptr = self->signal_slots_[signum];
            if (!slot_ptr) {
                slot_ptr = std::make_shared<detail::SignalSlot>(self->ioc_, signum);
            }
            slot_ptr->handlers.push_back({hid, std::move(h)});
            slot_ptr->begin_wait(std::weak_ptr<EventLoopBoost>(self));
        });

        struct Tag {
            int signum;
            uint64_t id;
        };
        auto tag = std::make_shared<Tag>();
        tag->signum = signum;
        tag->id = hid;

        return std::shared_ptr<void>(tag.get(), [weak_loop = std::weak_ptr<EventLoopBoost>(self), tag](void*) noexcept {
            if (auto loop = weak_loop.lock()) {
                boost::asio::post(loop->ioc_, [loop, signum = tag->signum, hid = tag->id]() {
                    loop->signal_unregister(signum, hid);
                });
            }
        });
    }

    EventLoopInterface::Metrics EventLoopBoost::GetMetrics() const noexcept {
        Metrics m;
        m.posted_tasks = posted_tasks_.load(std::memory_order_relaxed);
        m.completed_tasks = completed_tasks_.load(std::memory_order_relaxed);
        m.failed_tasks = failed_tasks_.load(std::memory_order_relaxed);
        m.max_pending_tasks = max_pending_tasks_.load(std::memory_order_relaxed);
        m.pending_tasks = pending_tasks_.load(std::memory_order_relaxed);
        const auto samples = queue_delay_samples_.load(std::memory_order_relaxed);
        const auto sum_us = queue_delay_sum_us_.load(std::memory_order_relaxed);
        m.avg_queue_delay_us = (samples == 0) ? 0 : (sum_us / samples);
        m.max_queue_delay_us = max_queue_delay_us_.load(std::memory_order_relaxed);
        return m;
    }

    void EventLoopBoost::ResetMetrics() noexcept {
        posted_tasks_.store(0, std::memory_order_relaxed);
        completed_tasks_.store(0, std::memory_order_relaxed);
        failed_tasks_.store(0, std::memory_order_relaxed);
        pending_tasks_.store(0, std::memory_order_relaxed);
        max_pending_tasks_.store(0, std::memory_order_relaxed);
        queue_delay_sum_us_.store(0, std::memory_order_relaxed);
        queue_delay_samples_.store(0, std::memory_order_relaxed);
        max_queue_delay_us_.store(0, std::memory_order_relaxed);
    }

    void EventLoopBoost::execute_task(Task task) {
        if (!task) {
            return;
        }
        task();
    }

    void EventLoopBoost::on_task_enqueued() noexcept {
        posted_tasks_.fetch_add(1, std::memory_order_relaxed);
        const uint64_t pending_now = pending_tasks_.fetch_add(1, std::memory_order_relaxed) + 1;
        uint64_t old_max = max_pending_tasks_.load(std::memory_order_relaxed);
        while (pending_now > old_max
               && !max_pending_tasks_.compare_exchange_weak(old_max, pending_now, std::memory_order_relaxed)) {
        }
    }

    void EventLoopBoost::on_task_executed(std::chrono::steady_clock::time_point enqueued_at) noexcept {
        completed_tasks_.fetch_add(1, std::memory_order_relaxed);
        pending_tasks_.fetch_sub(1, std::memory_order_relaxed);
        const auto delay_us = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - enqueued_at)
                .count());
        queue_delay_sum_us_.fetch_add(delay_us, std::memory_order_relaxed);
        queue_delay_samples_.fetch_add(1, std::memory_order_relaxed);

        uint64_t old_max = max_queue_delay_us_.load(std::memory_order_relaxed);
        while (delay_us > old_max
               && !max_queue_delay_us_.compare_exchange_weak(old_max, delay_us, std::memory_order_relaxed)) {
        }
    }

    void EventLoopBoost::on_task_failed(std::chrono::steady_clock::time_point enqueued_at) noexcept {
        failed_tasks_.fetch_add(1, std::memory_order_relaxed);
        pending_tasks_.fetch_sub(1, std::memory_order_relaxed);
        const auto delay_us = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - enqueued_at)
                .count());
        queue_delay_sum_us_.fetch_add(delay_us, std::memory_order_relaxed);
        queue_delay_samples_.fetch_add(1, std::memory_order_relaxed);

        uint64_t old_max = max_queue_delay_us_.load(std::memory_order_relaxed);
        while (delay_us > old_max
               && !max_queue_delay_us_.compare_exchange_weak(old_max, delay_us, std::memory_order_relaxed)) {
        }
    }

    void EventLoopBoost::on_task_canceled() noexcept {
        pending_tasks_.fetch_sub(1, std::memory_order_relaxed);
    }

    std::any EventLoopBoost::GetNativeExecutor() {
        // 隐式转换为 std::any
        return ioc_.get_executor(); 
    }

} // namespace itflee
