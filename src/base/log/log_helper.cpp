#include "base/log/log_helper.h"
#include "log_impl.h"

#include <mutex>




namespace itflee
{
    static std::shared_ptr<LogInterface> _log_interface(new LogImpl());
    static std::mutex _log_mutex;


    std::shared_ptr<LogInterface> LogInterface::instance()
    {
        std::lock_guard<std::mutex> lock(_log_mutex);
        if (!_log_interface)
        {
            _log_interface = std::static_pointer_cast<LogInterface>(std::make_shared<LogImpl>());
        }

        return _log_interface;
    }

    void LogInterface::destroy()
    {
        std::lock_guard<std::mutex> lock(_log_mutex);
        if (_log_interface)
        {
            _log_interface.reset();
            _log_interface = nullptr;
        }
    }
}