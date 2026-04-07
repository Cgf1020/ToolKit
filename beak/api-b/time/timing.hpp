#ifndef UTILS_TIMING_HPP
#define UTILS_TIMING_HPP

/* 
Timer 计时器
*/
#include "define.h"
#include <chrono>


namespace itflee {
#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable: 4251)  // 抑制 DLL 接口警告：标准库类型不需要 DLL 接口
#endif
    class ITFLEEEXPORT Timing {
    private:
        // 计时器开始时间
        using TimePoint = std::chrono::high_resolution_clock::time_point;
        // 计时器结束时间
        using Duration = std::chrono::nanoseconds;

        TimePoint start_time;   // 开始时间
        TimePoint end_time;     // 结束时间
        bool is_running;        // 计时器运行状态

    public:
        // 构造函数，初始化计时器状态
        Timing() : is_running(false) {}

        // 开始计时
        void start() {
            start_time = std::chrono::high_resolution_clock::now();
            is_running = true;
        }

        // 停止计时
        void stop() {
            if (is_running) {
                end_time = std::chrono::high_resolution_clock::now();
                is_running = false;
            }
        }

        // 获取经过的秒数
        double getElapsedSeconds() const {
            return getElapsedNanoseconds() / 1e9;
        }

        // 获取经过的毫秒数
        double getElapsedMilliseconds() const {
            return getElapsedNanoseconds() / 1e6;
        }

        // 获取经过的微秒数
        double getElapsedMicroseconds() const {
            return getElapsedNanoseconds() / 1e3;
        }

        // 获取经过的纳秒数
        long long getElapsedNanoseconds() const {
            if (is_running) {
                // 当前时间到开始时间的时间差
                auto current = std::chrono::high_resolution_clock::now();
                Duration duration = std::chrono::duration_cast<Duration>(current - start_time);
                return duration.count();
            } else {
                // 停止计时到开始计时的时间差
                Duration duration = std::chrono::duration_cast<Duration>(end_time - start_time);
                return duration.count();
            }
        }

        // 打印经过的时间（自动选择合适的单位）
        void printElapsed() const {
            long long nanos = getElapsedNanoseconds();
            
            if (nanos < 1000) {
                std::cout << nanos << " 纳秒" << std::endl;
            } else if (nanos < 1e6) {
                std::cout << nanos / 1e3 << " 微秒" << std::endl;
            } else if (nanos < 1e9) {
                std::cout << nanos / 1e6 << " 毫秒" << std::endl;
            } else {
                std::cout << nanos / 1e9 << " 秒" << std::endl;
            }
        }
    };
#ifdef _MSC_VER
    #pragma warning(pop)
#endif
}
    

#endif // UTILS_TIMING_HPP