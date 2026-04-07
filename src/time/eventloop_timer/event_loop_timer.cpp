#include "time/event_loop_timer.h"
#include "event_loop_timer_asio.h"

namespace itflee {

std::shared_ptr<EventLoopTimer> EventLoopTimer::Create(std::shared_ptr<EventLoopInterface> loop) {
    std::unique_ptr<EventLoopThread> elt;
    if (!loop) {
        elt = std::make_unique<EventLoopThread>();
        loop = elt->loop();
    }
    return std::make_shared<EventLoopTimerAsio>(std::move(loop), std::move(elt));
}

} // namespace itflee
