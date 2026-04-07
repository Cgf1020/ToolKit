#ifndef EVENT_LOOP_TIMER_ASIO_H_
#define EVENT_LOOP_TIMER_ASIO_H_

#include "time/event_loop_timer.h"

#include <memory>

namespace itflee {

/**
 * @brief 基于 Boost.Asio steady_timer 的 EventLoopTimer 实现（仅供库内编译单元使用）。
 */
class EventLoopTimerAsio final : public EventLoopTimer {
public:
    EventLoopTimerAsio(std::shared_ptr<EventLoopInterface> loop, std::unique_ptr<EventLoopThread> loop_thread);

    EventLoopTimerAsio(EventLoopTimerAsio&& other) noexcept;
    EventLoopTimerAsio& operator=(EventLoopTimerAsio&& other) noexcept;

    ~EventLoopTimerAsio() override;

    bool StartOnce(Duration delay, Callback cb) override;
    bool StartRepeating(Duration period, Callback cb) override;
    void Cancel() noexcept override;
    bool IsActive() const noexcept override;
    std::shared_ptr<EventLoopInterface> GetLoop() const noexcept override;

private:
    std::shared_ptr<EventLoopInterface>  loop_;
    EventLoopInterface::CancelToken      token_;
    std::unique_ptr<EventLoopThread>     loop_thread_;
};

} // namespace itflee

#endif
