#ifndef ITFLEE_EVENTLOOP_STATUS_INTERNAL_H_
#define ITFLEE_EVENTLOOP_STATUS_INTERNAL_H_

#include <atomic>

namespace itflee {

/**
 * @brief EventLoop 内部生命周期状态（从公开头中下沉，避免 ABI/导出问题）。
 *
 * @note 仅供库内部实现文件使用；对外暴露的状态接口由 EventLoopInterface::IsRunning() 等提供。
 */
class Status {
public:
    enum KStatus {
        kNull         = 0,
        kInitializing = 1,
        kInitialized  = 2,
        kStarting     = 3,
        kStarted      = 4,
        kRunning      = 5,
        kPause        = 6,
        kStopping     = 7,
        kStopped      = 8,
        kDestroyed    = 9,
    };

    Status() noexcept : status_(kNull) {}
    ~Status() { setStatus(kDestroyed); }

    KStatus status() const noexcept {
        return status_.load(std::memory_order_acquire);
    }

    void setStatus(KStatus s) noexcept { status_.store(s, std::memory_order_release); }

    bool isRunning() const noexcept { return status() == kRunning; }
    bool isPause() const noexcept { return status() == kPause; }
    bool isStopped() const noexcept { return status() == kStopped; }

    /**
     * @brief 从 kInitialized 或 kStopped 原子切换到 kRunning；已在运行或其它状态则返回 false。
     */
    bool tryMarkRunning() noexcept {
        KStatus expected = kInitialized;
        if (status_.compare_exchange_strong(expected, kRunning, std::memory_order_acq_rel)) {
            return true;
        }
        expected = kStopped;
        return status_.compare_exchange_strong(expected, kRunning, std::memory_order_acq_rel);
    }

    void markStopped() noexcept { setStatus(kStopped); }

private:
    std::atomic<KStatus> status_;
};

} // namespace itflee

#endif

