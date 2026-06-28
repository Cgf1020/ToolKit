#include "event_loop_timer_asio.h"

#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/version.hpp>

// 兼容新旧 Boost.Asio：
// - 新版通常提供 boost::asio::any_io_executor
// - 旧版可能缺失，需改用 io_context 构造 steady_timer
#if BOOST_VERSION >= 108000
#include <boost/asio/any_io_executor.hpp>
#endif

namespace itflee {
namespace {

using SteadyDuration = std::chrono::steady_clock::duration;

struct RepeatingState {
    boost::asio::steady_timer timer;
    SteadyDuration            period;
    EventLoopInterface::Task  task;

#if BOOST_VERSION >= 108000
    RepeatingState(boost::asio::any_io_executor ex, SteadyDuration p, EventLoopInterface::Task t)
        : timer(std::move(ex)), period(p), task(std::move(t)) {
        timer.expires_after(period);
    }
#else
    RepeatingState(boost::asio::io_context& ioc, SteadyDuration p, EventLoopInterface::Task t)
        : timer(ioc), period(p), task(std::move(t)) {
        timer.expires_after(period);
    }
#endif

    void arm(const std::shared_ptr<RepeatingState>& self) {
        timer.async_wait([self](const boost::system::error_code& ec) {
            if (ec == boost::asio::error::operation_aborted) {
                return;
            }
            if (ec) {
                return;
            }
            if (self->task) {
                self->task();
            }
            self->timer.expires_after(self->period);
            self->arm(self);
        });
    }
};

EventLoopInterface::CancelToken make_after_token(
    const std::shared_ptr<EventLoopInterface>& loop,
    SteadyDuration delay,
    EventLoopInterface::Task task) {
    if (!loop || !task) {
        return nullptr;
    }
#if BOOST_VERSION >= 108000
    auto ex = std::any_cast<boost::asio::io_context::executor_type>(loop->GetNativeExecutor());
    boost::asio::any_io_executor any_ex(ex);
    auto timer = std::make_shared<boost::asio::steady_timer>(any_ex);
#else
    auto ex  = std::any_cast<boost::asio::io_context::executor_type>(loop->GetNativeExecutor());
    auto& ioc = static_cast<boost::asio::io_context&>(ex.context());
    auto timer = std::make_shared<boost::asio::steady_timer>(ioc);
#endif
    timer->expires_after(delay);
    timer->async_wait([timer, t = std::move(task)](const boost::system::error_code& ec) mutable {
        if (ec == boost::asio::error::operation_aborted) {
            return;
        }
        if (ec) {
            return;
        }
        if (t) {
            t();
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

EventLoopInterface::CancelToken make_every_token(
    const std::shared_ptr<EventLoopInterface>& loop,
    SteadyDuration period,
    EventLoopInterface::Task task) {
    if (!loop || !task || period <= SteadyDuration::zero()) {
        return nullptr;
    }
#if BOOST_VERSION >= 108000
    auto ex = std::any_cast<boost::asio::io_context::executor_type>(loop->GetNativeExecutor());
    boost::asio::any_io_executor any_ex(ex);
    auto state = std::make_shared<RepeatingState>(any_ex, period, std::move(task));
#else
    auto ex  = std::any_cast<boost::asio::io_context::executor_type>(loop->GetNativeExecutor());
    auto& ioc = static_cast<boost::asio::io_context&>(ex.context());
    auto state = std::make_shared<RepeatingState>(ioc, period, std::move(task));
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

} // namespace

EventLoopTimerAsio::EventLoopTimerAsio(std::shared_ptr<EventLoopInterface> loop, std::unique_ptr<EventLoopThread> loop_thread)
    : loop_(std::move(loop))
    , loop_thread_(std::move(loop_thread)) {
    if (loop_thread_) {
        loop_thread_->start();
    }
}

EventLoopTimerAsio::EventLoopTimerAsio(EventLoopTimerAsio&& other) noexcept
    : loop_(std::move(other.loop_))
    , token_(std::move(other.token_))
    , loop_thread_(std::move(other.loop_thread_)) {}

EventLoopTimerAsio& EventLoopTimerAsio::operator=(EventLoopTimerAsio&& other) noexcept {
    if (this != &other) {
        Cancel();
        loop_        = std::move(other.loop_);
        token_       = std::move(other.token_);
        loop_thread_ = std::move(other.loop_thread_);
    }
    return *this;
}

EventLoopTimerAsio::~EventLoopTimerAsio() {
    Cancel();
    if (loop_thread_) {
        loop_thread_->stop(true);
    }
}

bool EventLoopTimerAsio::StartOnce(Duration delay, Callback cb) {
    Cancel();
    if (!loop_ || !cb) {
        return false;
    }
    token_ = make_after_token(loop_, delay, std::move(cb));
    return static_cast<bool>(token_);
}

bool EventLoopTimerAsio::StartRepeating(Duration period, Callback cb) {
    Cancel();
    if (!loop_ || !cb || period <= Duration::zero()) {
        return false;
    }
    token_ = make_every_token(loop_, period, std::move(cb));
    return static_cast<bool>(token_);
}

void EventLoopTimerAsio::Cancel() noexcept {
    token_.reset();
}

bool EventLoopTimerAsio::IsActive() const noexcept {
    return static_cast<bool>(token_);
}

std::shared_ptr<EventLoopInterface> EventLoopTimerAsio::GetLoop() const noexcept {
    return loop_;
}

} // namespace itflee
