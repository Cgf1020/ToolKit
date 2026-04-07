#ifndef ITFLEE_EVENT_LOOP_TIMER_H_
#define ITFLEE_EVENT_LOOP_TIMER_H_

#include "define.h"
#include "base/eventloop/event_loop_interface.h"
#include "base/eventloop/event_loop_thread.h"
#include <chrono>
#include <memory>
#include <utility>

namespace itflee {

/**
 * @brief 事件循环定时器抽象接口（典型实现 EventLoopTimerAsio：基于 Boost.Asio steady_timer）。
 * @note 回调在关联的 EventLoop 所在线程执行（与 Run() 同线程）；勿在回调内调用 Run()。
 * @note 若需取消尚未触发的定时，可调用 Cancel()，或销毁本对象（析构通常会 Cancel）。
 * @note Boost.Asio 上定时器仅把「到期」排入 io_context；须有线程对该 loop 调用 Run()（或 poll/run_one）才会触发回调。
 * @note Create(nullptr) 时内部持有 EventLoopThread 与 loop，构造时自动在后台线程 Run()，析构时自动停止并等待线程退出（外部传入 loop 时仍由调用方 Run/Stop）。
 */
class ITFLEEEXPORT EventLoopTimer {
public:
    /** @brief 定时到点时执行的回调类型（同 EventLoopInterface::Task）。 */
    using Callback = EventLoopInterface::Task;
    /** @brief 延迟/周期时长，基于 steady_clock，与 std::chrono::steady_timer 一致。 */
    using Duration = std::chrono::steady_clock::duration;

    EventLoopTimer(const EventLoopTimer&)            = delete;
    EventLoopTimer& operator=(const EventLoopTimer&) = delete;

    virtual ~EventLoopTimer() = default;

    /**
     * @brief 创建默认实现（内部使用 loop->GetNativeExecutor() 绑定 Asio 定时器）。
     * @param loop 事件循环；为 nullptr 时在内部 EventLoopInterface::Create()，并由返回的定时器对象持有其 shared_ptr。
     * @return 非空 shared_ptr；失败时理论上不应出现（实现可断言）。
     * @note 若传入 nullptr：内部 EventLoopInterface::Create() 并包在 EventLoopThread 中，创建后即自动驱动 loop。
     */
    static std::shared_ptr<EventLoopTimer> Create(std::shared_ptr<EventLoopInterface> loop = nullptr);

    /**
     * @brief 返回与该定时器绑定的 EventLoopInterface（Create 传入的或内部创建的）。
     * @return 与 Create 时一致的 loop；若实现异常可能为空（一般不出现）。
     */
    virtual std::shared_ptr<EventLoopInterface> GetLoop() const noexcept = 0;

    /**
     * @brief 在 delay 之后于 loop 线程执行一次 cb；若当前已有活动定时会先 Cancel 再重新武装。
     * @param delay 相对当前 steady 时间的延迟，须 >= 0 语义（负值行为未定义）。
     * @param cb 到点回调；为空则实现可拒绝启动并返回 false。
     * @return 成功武装返回 true；loop 无效、cb 为空等返回 false。
     * @note 再次调用会取消上一次 StartOnce/StartRepeating 尚未触发的调度。
     */
    virtual bool StartOnce(Duration delay, Callback cb) = 0;

    /**
     * @brief 按固定 period 在 loop 线程周期执行 cb；若当前已有活动定时会先 Cancel 再重新武装。
     * @param period 周期间隔，须 > 0；否则实现应返回 false。
     * @param cb 每次周期到达时调用；为空则返回 false。
     * @return 成功武装返回 true。
     * @note 再次调用会取消上一次尚未触发的调度。
     */
    virtual bool StartRepeating(Duration period, Callback cb) = 0;

    /**
     * @brief 取消当前活动的一次性或周期定时（若有），不抛异常。
     * @note 与 IsActive() 为 false 时调用应为空操作。
     */
    virtual void Cancel() noexcept = 0;

    /**
     * @brief 是否已有尚未取消且已武装的定时（一次性等待中或周期运行中）。
     */
    virtual bool IsActive() const noexcept = 0;

    /**
     * @brief StartOnce 的毫秒便捷封装。
     * @param timeout_ms 延迟毫秒数。
     * @param cb 到点回调。
     * @return 同 StartOnce。
     */
    bool StartOnceMs(int timeout_ms, Callback cb) {
        return StartOnce(std::chrono::milliseconds(timeout_ms), std::move(cb));
    }

    /**
     * @brief StartRepeating 的毫秒便捷封装。
     * @param period_ms 周期间隔毫秒数，须 > 0。
     * @param cb 周期回调。
     * @return 同 StartRepeating。
     */
    bool StartRepeatingMs(int period_ms, Callback cb) {
        return StartRepeating(std::chrono::milliseconds(period_ms), std::move(cb));
    }

protected:
    EventLoopTimer() = default;
};

} // namespace itflee

#endif
