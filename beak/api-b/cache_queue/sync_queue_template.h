#ifndef SYNC_QUEUE_TEMPLATE_H
#define SYNC_QUEUE_TEMPLATE_H

#include <deque>
#include <thread>
#include <memory>
#include <atomic>
#include <functional>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <condition_variable>

/**
 * @brief 终极优化版同步队列（含系统时间冗余清理）
 * 改进点：
 * 1. 系统时间监控：增加对 Data1 的绝对时间监控，防止单边数据堆积。
 * 2. 指针化存储：使用 std::shared_ptr 避免大数据拷贝。
 * 3. 异步回调：在释放锁后执行回调，提升并发吞吐量。
 */
template<typename DataType1, typename DataType2, 
         typename TimeExtractor1 = std::function<int64_t(const DataType1&)>,
         typename TimeExtractor2 = std::function<int64_t(const DataType2&)>>
class SyncQueueTemplate
{
public:
    using CallbackType = std::function<void(const DataType1& data1, const DataType2& data2)>;
    
    SyncQueueTemplate(TimeExtractor1 time_extractor1,
                     TimeExtractor2 time_extractor2,
                     int max_retention = 2000,
                     int match_threshold = 100,
                     bool enable_sync = true)
        : time_extractor1_(std::move(time_extractor1))
        , time_extractor2_(std::move(time_extractor2))
        , MAX_RETENTION_(max_retention)
        , MATCH_THRESHOLD_(match_threshold)
        , enable_sync_(enable_sync)
        , isRunning_(false)
    {}

    ~SyncQueueTemplate() { Stop();  }

    void Start(CallbackType callback) {
        if (isRunning_.exchange(true)) return;
        callback_ = std::move(callback);
        processSyncThread_ = std::make_unique<std::thread>(&SyncQueueTemplate::synchronizer, this);
    }

    void Stop() {
        isRunning_ = false;
        cv_sync_.notify_all();
        if (processSyncThread_ && processSyncThread_->joinable()) {
            processSyncThread_->join();
        }

        CleanQueue();
    }

    void PushToQueue1(std::shared_ptr<DataType1> data) {
        if (!data) return;
        {
            std::lock_guard<std::mutex> lock(mtx_data1_);
            data1Queue_.push_back(std::move(data));
        }
        data1_pushed_count_++;
        cv_sync_.notify_one();
    }

    void PushToQueue2(std::shared_ptr<DataType2> data) {
        if (!data) return;
        {
            std::lock_guard<std::mutex> lock(mtx_data2_);
            data2Queue_.push_back(std::move(data));
        }
        data2_pushed_count_++;
        cv_sync_.notify_one();
    }

    void CleanQueue()
    {
        data1Queue_.clear();
        data2Queue_.clear();
    }

    void SetSyncEnabled(bool enabled) {
        enable_sync_ = enabled;
    }
private:
    // 获取当前毫秒级系统时间戳（需确保与数据提取器的时间基准一致）
    int64_t getCurrentTimestampMS() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    void synchronizer() {
        while (isRunning_) {
            std::shared_ptr<DataType1> d1;
            
            // --- 阶段 1: 获取 Data1 并进行系统时间过期检查 ---
            {
                std::unique_lock<std::mutex> lock1(mtx_data1_);
                // 等待数据，但增加超时检查以防系统时间清理逻辑被阻塞
                cv_sync_.wait_for(lock1, std::chrono::milliseconds(100), [this] { 
                    return !data1Queue_.empty() || !isRunning_; 
                });

                if (!isRunning_) break;
                if (data1Queue_.empty()) continue;

                // 检查 Data1 是否已经相对于系统时间过期
                int64_t current_sys_ms = getCurrentTimestampMS();
                while (!data1Queue_.empty()) {
                    int64_t ts1 = time_extractor1_(*data1Queue_.front());
                    if (current_sys_ms - ts1 > MAX_RETENTION_) {
                        // 数据太旧了，即便没匹配上也丢弃
                        data1Queue_.pop_front();
                        continue; 
                    }
                    break;
                }

                if (data1Queue_.empty()) continue;

                d1 = std::move(data1Queue_.front());
                data1Queue_.pop_front();
            }

            int64_t ts1 = time_extractor1_(*d1);
            std::shared_ptr<DataType2> d2_matched = nullptr;

            // --- 阶段 2: 二分查找匹配 Data2 ---
            {
                std::unique_lock<std::mutex> lock2(mtx_data2_);
                
                // 清理 Data2 中早于 ts1 - MAX_RETENTION_ 的数据
                while (!data2Queue_.empty() && 
                       time_extractor2_(*data2Queue_.front()) < ts1 - MAX_RETENTION_) {
                    data2Queue_.pop_front();
                }

                if (!data2Queue_.empty()) {
                    auto it = std::lower_bound(data2Queue_.begin(), data2Queue_.end(), ts1,
                        [this](const std::shared_ptr<DataType2>& ptr, int64_t val) {
                            return time_extractor2_(*ptr) < val;
                        });

                    auto best_it = data2Queue_.end();
                    int64_t min_diff = INT64_MAX;

                    if (it != data2Queue_.end()) {
                        min_diff = std::abs(time_extractor2_(**it) - ts1);
                        best_it = it;
                    }
                    if (it != data2Queue_.begin()) {
                        auto prev_it = std::prev(it);
                        int64_t prev_diff = std::abs(time_extractor2_(**prev_it) - ts1);
                        if (prev_diff < min_diff) {
                            min_diff = prev_diff;
                            best_it = prev_it;
                        }
                    }

                    if (best_it != data2Queue_.end() && min_diff <= MATCH_THRESHOLD_) {
                        d2_matched = std::move(*best_it);
                        // 清除当前匹配项及其之前的所有旧数据
                        data2Queue_.erase(data2Queue_.begin(), std::next(best_it));
                    }
                }
            } 

            // --- 阶段 3: 执行回调 (不持有锁) ---
            if (d2_matched) {
                matched_count_++;
                if (callback_) {
                    callback_(*d1, *d2_matched);
                }
            }
        }
    }

private:
    std::deque<std::shared_ptr<DataType1>> data1Queue_;
    std::deque<std::shared_ptr<DataType2>> data2Queue_;
    mutable std::mutex mtx_data1_, mtx_data2_;
    std::condition_variable cv_sync_;
    
    CallbackType callback_;
    TimeExtractor1 time_extractor1_;
    TimeExtractor2 time_extractor2_;
    
    const int MAX_RETENTION_;
    const int MATCH_THRESHOLD_;
    std::atomic<bool> enable_sync_;
    std::atomic<bool> isRunning_;
    std::unique_ptr<std::thread> processSyncThread_;
    
    std::atomic<int> data1_pushed_count_{0};
    std::atomic<int> data2_pushed_count_{0};
    std::atomic<int> matched_count_{0};
};

#endif