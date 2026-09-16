#include "base/eventloop/event_loop_interface.h"
#include "base/eventloop/event_loop_thread.h"
#include "time/event_loop_timer.h"

#include <chrono>
#include <future>
#include <iostream>
#include <memory>

// 本示例：主线程 loop->Run()。若希望定时器在后台线程跑、主线程不调 Run()，可用：
//   auto tmr = itflee::EventLoopTimer::Create();
//   tmr->StartOnceMs(1000, [] { /* ... */ });
//   // 销毁 tmr 时自动停止后台线程

int main() {

    auto loop = itflee::EventLoopInterface::Create();
    std::cout << "EventLoopInterface created\n";

    // PostEvent 回调在 loop 线程：内部再 Post 合法（下一事件轮次执行）
    loop->PostEvent([loop](itflee::Event* /*ev*/) {
        std::cout << "PostEvent callback\n";
        loop->Post([] {
            std::cout << "nested: Post from PostEvent callback\n";
        });
    });

    auto stop_timer = itflee::EventLoopTimer::Create(loop);
    stop_timer->StartOnce(std::chrono::seconds(10), [loop] {
        loop->Post([] {
            std::cout << "nested: Post from timer callback\n";
        });
        std::cout << "10s timeout, stopping...\n";
        loop->Stop();
    });
    (void)stop_timer;

    std::cout << "Run(); Ctrl+C or q to exit; auto Stop after 10s.\n";
    loop->Run();

    std::cout << "EventLoopInterface exited\n";

    loop.reset();

    // 2. eventloop_thread test：独立线程 Run()，主线程不再调用 Run
    {
        itflee::EventLoopThread elt;
        elt.start();

        std::promise<void> posted;
        elt.loop()->Post([&posted] {
            std::cout << "EventLoopThread: Post runs on loop thread\n";
            posted.set_value();
        });
        if (posted.get_future().wait_for(std::chrono::seconds(2)) != std::future_status::ready) {
            std::cerr << "EventLoopThread: timeout waiting for Post\n";
        }

        auto tmr = itflee::EventLoopTimer::Create(elt.loop());
        std::promise<void> timer_done;
        // 这里使用较短 delay，避免示例里 wait_for 超时造成误判/误导
        tmr->StartOnceMs(500, [&timer_done] {
            std::cout << "EventLoopThread: timer fired on same loop thread\n";
            timer_done.set_value();
        });
        
        if (timer_done.get_future().wait_for(std::chrono::seconds(2)) != std::future_status::ready) {
            std::cerr << "EventLoopThread: timeout waiting for timer\n";
        }

        elt.stop(true);
        std::cout << "EventLoopThread: stop + join done\n";
    }

    return 0;
}
