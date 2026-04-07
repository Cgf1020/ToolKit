#ifndef ITFLEE_EVENT_H_
#define ITFLEE_EVENT_H_

#include <functional>
#include <memory>

namespace itflee {

/**
 * @brief 自定义事件载体，回调在 loop 线程执行并接收本对象指针（设计参考 hv::Event，无 libhv 依赖）。
 * @note 与 EventLoopInterface::PostEvent 配合使用；可经 userData 挂载任意上下文。
 */
class Event {
public:
    using Callback = std::function<void(Event*)>;

    explicit Event(Callback cb = nullptr);

    Callback cb;
    void setUserData(void* data) noexcept { user_data_ = data; }
    void* userData() const noexcept { return user_data_; }

private:
    void* user_data_{nullptr};
};

using EventCallback = Event::Callback;
using EventPtr      = std::shared_ptr<Event>;

inline Event::Event(Callback cb) : cb(std::move(cb)) {}

} // namespace itflee

#endif
