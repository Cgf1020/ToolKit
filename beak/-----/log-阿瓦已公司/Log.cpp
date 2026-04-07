#include "Log.h"

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/attributes/current_thread_id.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

namespace Utils {
    bool LogHelper::logToConsole = true;

    void LogHelper::Initialize(const std::string& logFile) {
        boost::log::register_simple_formatter_factory<boost::log::trivial::severity_level, char>("Severity");

        // 控制台输出
        if (logToConsole) {
            boost::log::add_console_log(std::cout,
                boost::log::keywords::format = boost::log::expressions::stream
                    << boost::log::expressions::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f") << " [" << boost::log::trivial::severity << "] "
                    << boost::log::expressions::smessage /*"[%TimeStamp%] [%ThreadID%] [%Severity%]: %Message%"*/);

            // 创建控制台输出目标
            typedef boost::log::sinks::text_ostream_backend backend_t;
            boost::shared_ptr<backend_t> backend = boost::make_shared<backend_t>();
            backend->add_stream(boost::shared_ptr<std::ostream>(&std::clog, boost::null_deleter()));
            typedef boost::log::sinks::synchronous_sink<backend_t> sink_t;
            boost::shared_ptr<sink_t> sink(new sink_t(backend));

            // 将控制台输出目标添加到日志核心
            boost::log::core::get()->add_sink(sink);
        }

        // 文件输出
        if (!logFile.empty()) {
            auto sink = boost::log::add_file_log(boost::log::keywords::target = "logs", // 日志文件夹路径
                boost::log::keywords::file_name = logFile, // 日志文件名
                boost::log::keywords::rotation_size = 10 * 1024 * 1024, // 日志文件大小
                boost::log::keywords::auto_flush = true,
                boost::log::keywords::format = boost::log::expressions::stream
                    << boost::log::expressions::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f") << " [" << boost::log::trivial::severity << "] "
                    << boost::log::expressions::smessage); // "[%TimeStamp%] [%ThreadID%] [%Severity%]: %Message%");

        }
        boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);
        boost::log::add_common_attributes();
    }

    void LogHelper::UnInitialize() {
        boost::log::core::get()->flush(); // 释放Boost::Log资源
    }

    void LogHelper::EnableConsoleLogging(bool enable) {
        logToConsole = enable;
    }

    void LogHelper::Log(const std::string& message, LogLevel level) {
        switch (level) {
        case Trace:
            BOOST_LOG_TRIVIAL(trace) << message;
            break;
        case Debug:
            BOOST_LOG_TRIVIAL(debug) << message;
            break;
        case Info:
            BOOST_LOG_TRIVIAL(info) << message;
            break;
        case Warning:
            BOOST_LOG_TRIVIAL(warning) << message;
            break;
        case Error:
            BOOST_LOG_TRIVIAL(error) << message;
            break;
        default:
            BOOST_LOG_TRIVIAL(info) << message;
            break;
        }


        //BOOST_LOG_ATTRIBUTE_KEYWORD()
    }



} // namespace Utils
