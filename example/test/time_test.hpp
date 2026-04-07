#ifndef TIME_TEST_HPP
#define TIME_TEST_HPP

#include "time/timer.h"

[[maybe_unused]] static int time_test()
{
    auto timer = itflee::Timer::CreateTimer("test", 1000, true, [](const std::string& name) {
        std::cout << "timer213: " << name << std::endl;
    }, [](const std::string& name) {
        std::cout << "timer342: " << name << std::endl;
    });

	timer->StartTimer();

    return true;
}




#endif