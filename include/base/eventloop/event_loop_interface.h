#ifndef EVENT_LOOP_INTERFACE_H_
#define EVENT_LOOP_INTERFACE_H_

#include "define.h"
#include "base/eventloop/event.h"
#include <chrono>
#include <any>
#include <cstdint>
#include <csignal>
#include <functional>
#include <memory>

namespace itflee {

/**
 * @brief 单线程事件循环抽象接口（典型实现基于 Boost.Asio io_context）。
 * @note 若要在独立线程中自动运行 Run()（定时器、Post 等无需在调用方线程 Run），请使用 EventLoopThread（见 base/eventloop/event_loop_thread.h）。
 * @note EventLoopTimer::Create(nullptr) 会由对象内部自动管理 EventLoopThread 生命周期；TcpServerInterface::Create(listener,nullptr) 则仍通过 Start()/Stop() 驱动 Run()。
 * @note Run() 在调用线程阻塞；「loop 线程」即正在执行 Run() 的那条线程。
 * @note Post / PostEvent / Dispatch 投递的任务、OnSignal 的 handler、以及 EventLoopTimer（StartOnce/StartRepeating）的定时器回调，
 *       均在 loop 线程上串行执行。在这些回调里可以再次调用 Post、PostEvent、Dispatch、Stop、OnSignal
 *       （以及基于 GetNativeExecutor 的定时器注册）；禁止在回调里调用 Run()（也不可重入 Run）。
 * @note 其它线程仅应通过 Post / Dispatch / PostEvent / Stop 等与 loop 交互，勿在非 loop 线程直接跑业务回调。
 * @note 对外仅暴露 IsRunning() 等状态查询接口；内部生命周期状态由实现维护。
 */
class ITFLEEEXPORT EventLoopInterface {
public:
    using Task          = std::function<void()>;
    using Duration      = std::chrono::steady_clock::duration;
    using CancelToken   = std::shared_ptr<void>;
    using SignalHandler = std::function<void(int signum)>;
    struct Metrics {
        uint64_t posted_tasks{0};
        uint64_t completed_tasks{0};
        uint64_t failed_tasks{0};
        uint64_t max_pending_tasks{0};
        uint64_t pending_tasks{0};
        uint64_t avg_queue_delay_us{0};
        uint64_t max_queue_delay_us{0};
    };

    /**
     * @brief 创建事件循环实例。
     * @return 事件循环接口的共享指针
     */
    static std::shared_ptr<EventLoopInterface> Create();

    virtual ~EventLoopInterface() = default;

    /**
     * @brief 在当前调用线程上运行事件循环（阻塞直至 Stop() 后退出）。
     * @note 不可在 Post/定时器等 loop 回调内调用，否则死锁或重入未定义。
     */
    virtual void Run() = 0;

    /**
     * @brief 请求停止事件循环，使 Run() 尽快结束阻塞并返回。
     * @note 可在任意线程或 loop 回调内调用；实现会安全收尾 io_context（含 work 与 signal 槽）。
     */
    virtual void Stop() = 0;

    /**
     * @brief 查询事件循环是否处于运行中（Run() 已开始且尚未完全退出）。
     */
    virtual bool IsRunning() const noexcept = 0;

    /**
     * @brief 判断当前调用线程是否为事件循环所在线程。
     * @return 若当前线程即为执行回调的 loop 线程则为 true
     */
    virtual bool IsLoopThread() const noexcept = 0;

    /**
     * @brief 将任务异步投递到事件循环队列，在 loop 线程中执行。
     * @param task 可调用对象；若为空则无操作
     * @note 可在 loop 回调内调用（嵌套 Post 仅入队，下一轮到点执行）。
     */
    virtual void Post(Task task) = 0;

    /**
     * @brief 在 loop 线程上调度任务；若当前已在 loop 线程则尽可能立即执行，否则行为类似 Post。
     * @param task 可调用对象；若为空则无操作
     * @note 在 loop 回调内调用可能同步执行 task，避免无限自递归。
     */
    virtual void Dispatch(Task task) = 0;

    /**
     * @brief 投递自定义事件：在 loop 线程构造 Event 并调用 cb(Event*)（语义类似 hv::EventLoop::postEvent，实现为 Asio post）。
     * @param cb 若为空则无操作
     * @note 可在 loop 回调内调用；cb 内亦可再调 Post / PostEvent / Dispatch / Stop 等。
     */
    virtual void PostEvent(EventCallback cb) = 0;

    /**
     * @brief 在指定延迟后于 loop 线程执行一次性任务。
     * @param delay 相对 steady_clock 的延迟时长
     * @param task 到点后执行的任务；若为空则返回空令牌
     * @return 取消令牌；销毁该令牌可取消尚未触发的定时器
     */
    virtual CancelToken ScheduleAfter(Duration delay, Task task) = 0;

    /**
     * @brief 按固定周期在 loop 线程重复执行任务。
     * @param period 周期间隔，必须大于 0
     * @param task 每次周期到达时执行的任务；若为空或 period 非法则返回空令牌
     * @return 取消令牌；销毁该令牌可取消周期性定时器
     */
    virtual CancelToken ScheduleEvery(Duration period, Task task) = 0;

    /**
     * @brief 注册操作系统信号回调（如控制台 Ctrl+C 对应 SIGINT）。
     * @param signum 信号编号（如 SIGINT、SIGTERM）
     * @param handler 收到信号时在 loop 线程调用的处理函数
     * @return 取消令牌；销毁该令牌可注销该处理器；同一信号允许多个处理器
     */
    virtual CancelToken OnSignal(int signum, SignalHandler handler) = 0;

    /**
     * @brief 获取事件循环运行统计。
     */
    virtual Metrics GetMetrics() const noexcept = 0;

    /**
     * @brief 清零事件循环运行统计。
     */
    virtual void ResetMetrics() noexcept = 0;

    /**
     * @brief 获取底层 native 执行器（以 std::any 封装，便于与网络等模块对接）。
     * @return 具体类型由实现决定，常见为 boost::asio::io_context::executor_type
     */
    virtual std::any GetNativeExecutor() = 0;
};

} // namespace itflee

#endif
