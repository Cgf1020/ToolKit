#ifndef EVENT_LOOP_BOOST_H_
#define EVENT_LOOP_BOOST_H_

#include "base/eventloop/event_loop_interface.h"
#include <atomic>
#include <boost/asio/io_context.hpp>
#include <chrono>
#include <boost/version.hpp>
#include <memory>
#include <optional>
#include <thread>
#include <unordered_map>

// 仅按 Boost 版本切换新旧 work guard 接口。
#if BOOST_VERSION >= 108000
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/any_io_executor.hpp>
#endif
#include <boost/asio/steady_timer.hpp>

namespace itflee {

namespace detail {
struct SignalSlot;
}

class EventLoopBoost final
    : public EventLoopInterface
    , public std::enable_shared_from_this<EventLoopBoost> {
public:
    EventLoopBoost();
    ~EventLoopBoost() override;

    void Run() override;
    void Stop() override;
    bool IsRunning() const noexcept override;
    bool IsLoopThread() const noexcept override;
    void Post(Task task) override;
    void Dispatch(Task task) override;
    void PostEvent(EventCallback cb) override;
    CancelToken ScheduleAfter(Duration delay, Task task) override;
    CancelToken ScheduleEvery(Duration period, Task task) override;
    CancelToken OnSignal(int signum, SignalHandler handler) override;
    Metrics GetMetrics() const noexcept override;
    void ResetMetrics() noexcept override;
    std::any GetNativeExecutor() override;

private:
    void reset_work_in_loop();
    void signal_unregister(int signum, uint64_t handler_id);
    void execute_task(Task task);
    void on_task_enqueued() noexcept;
    void on_task_executed(std::chrono::steady_clock::time_point enqueued_at) noexcept;
    void on_task_failed(std::chrono::steady_clock::time_point enqueued_at) noexcept;
    void on_task_canceled() noexcept;

    enum class RunState : uint8_t {
        kIdle = 0,
        kRunning = 1,
        kStopping = 2
    };

    boost::asio::io_context ioc_;
    std::atomic<RunState> state_{RunState::kIdle};

#if BOOST_VERSION >= 108000
    using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
#else
    using WorkGuard = boost::asio::io_context::work;
#endif
    std::optional<WorkGuard> work_;
    std::thread::id loop_thread_id_{};

    std::unordered_map<int, std::shared_ptr<detail::SignalSlot>> signal_slots_;
    std::atomic<uint64_t> next_signal_id_{1};

    std::atomic<uint64_t> posted_tasks_{0};
    std::atomic<uint64_t> completed_tasks_{0};
    std::atomic<uint64_t> failed_tasks_{0};
    std::atomic<uint64_t> pending_tasks_{0};
    std::atomic<uint64_t> max_pending_tasks_{0};
    std::atomic<uint64_t> queue_delay_sum_us_{0};
    std::atomic<uint64_t> queue_delay_samples_{0};
    std::atomic<uint64_t> max_queue_delay_us_{0};
};

} // namespace itflee

#endif
