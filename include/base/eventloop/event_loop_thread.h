#ifndef ITFLEE_EVENT_LOOP_THREAD_H_
#define ITFLEE_EVENT_LOOP_THREAD_H_

#include "define.h"
#include "base/eventloop/event_loop_interface.h"

#include <functional>
#include <memory>
#include <thread>

namespace itflee {

class Status;

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

/**
 * @brief 在独立线程中运行 EventLoopInterface::Run()（设计参考 hv::EventLoopThread）。
 * @note 构造时可传入已有 loop，否则内部 EventLoopInterface::Create()；析构时 stop() + join()。
 * @note start() 后 loop 进入运行态，定时器、Post 等由该线程驱动，调用方无需再 Run()。
 * @note 再次 start() 前须确保上一轮已结束（或已 join）；stop(false) 后若未 join，在仍为停止流程中时 start 会被忽略。
 */
class ITFLEEEXPORT EventLoopThread {
public:
    /** @brief 返回 0 表示成功；非 0 时 pre 会触发 loop->Stop()（与 hv 一致）。 */
    using Functor = std::function<int()>;

    explicit EventLoopThread(std::shared_ptr<EventLoopInterface> loop = nullptr);
    ~EventLoopThread();

    EventLoopThread(const EventLoopThread&)            = delete;
    EventLoopThread& operator=(const EventLoopThread&) = delete;

    std::shared_ptr<EventLoopInterface> loop() const noexcept;

    /**
     * @param wait_thread_started 为 true 时阻塞直至 loop 进入运行态（IsRunning()）。
     * @param pre 在 loop 线程上执行：通过 Post 排队，于 Run() 处理队列时先于其它异步源执行（实现依赖一次 Post + Run）。
     * @param post 在 Run() 返回后、于 loop 线程上执行。
     */
    void start(bool wait_thread_started = true, Functor pre = Functor(), Functor post = Functor());

    /**
     * @param wait_thread_stopped 为 true 时阻塞直至 loop 线程结束（join）；若在 loop 线程内调用则不会 join（与 hv 一致）。
     */
    void stop(bool wait_thread_stopped = false);

    void join();

private:
    static void loop_thread_body(
        std::shared_ptr<Status>             status,
        std::shared_ptr<EventLoopInterface> loop,
        Functor                             pre,
        Functor                             post);

    std::shared_ptr<EventLoopInterface> loop_;
    std::shared_ptr<std::thread>        thread_;
    std::shared_ptr<Status>             status_;
};

using EventLoopThreadPtr = std::shared_ptr<EventLoopThread>;

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

} // namespace itflee

#endif
