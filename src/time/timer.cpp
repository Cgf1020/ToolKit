#include "time/timer.h"
#include "standardtimer/standardtimer.h"

#include <thread>
#include <chrono>


namespace itflee {
    std::shared_ptr<Timer> Timer::CreateTimer(std::string timer_name, int timeoutMs, bool repeat, 
                                                    std::function<void(std::string)> task,
                                                    std::function<void(std::string)> destory)
    {
        return std::make_shared<StandardTimer>(
            std::move(timer_name), timeoutMs, repeat, std::move(task), std::move(destory));
    }
}
