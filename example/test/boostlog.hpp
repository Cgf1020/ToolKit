
//#include <boost/log/trivial.hpp>
//#include <boost/log/core.hpp>
//#include <boost/log/expressions.hpp>
//#include <boost/log/utility/setup.hpp>
//
//#include <boost/log/sources/logger.hpp>
//#include <boost/log/sources/severity_logger.hpp>
//#include <boost/log/sources/record_ostream.hpp>
//#include <boost/log/sources/global_logger_storage.hpp>
//#include <boost/log/attributes/current_thread_id.hpp>
//#include <boost/log/attributes/attribute.hpp>
//#include <boost/log/attributes/constant.hpp>
//#include <boost/log/attributes/attribute_set.hpp>
//#include <boost/log/support/date_time.hpp>
//
//#include <boost/log/sinks/async_frontend.hpp>
//#include <boost/log/sinks/basic_sink_backend.hpp>
//#include <boost/log/sinks/text_ostream_backend.hpp>
//
//#include <boost/make_shared.hpp>





// 自定义后端类
//class custom_asyn_sink_backend : public boost::log::sinks::sink_backend {
//public:
//    void consume(boost::log::record_view const& rec) {
//        // 在日志 sink 中处理日志记录
//        std::cout << "Logging message: " << rec[boost::log::trivial::severity] << std::endl;
//    }
//
//    // 自定义函数
//    void custom_function() {
//        std::cout << "Calling custom function in sink backend" << std::endl;
//    }
//};

//// 自定义的 sink backend
//class custom_sync_sink_backend : public boost::log::sinks::basic_sink_backend<boost::log::sinks::synchronized_feeding> {
//public:
//
//
//    // 处理每条日志记录
//    void consume(boost::log::record_view const& rec) {
//        std::cout << "同步：threadid : " << std::this_thread::get_id() << std::endl;
//
//
//        // 提取自定义属性 UserID
//        auto id = boost::log::extract<std::string>("UserID", rec);
//        if (id) {
//            std::cout << "UserID: " << id.get() << std::endl;
//        }
//
//        // 提取时间戳
//        auto timestamp = boost::log::extract<boost::posix_time::ptime>("TimeStamp", rec);
//        if (timestamp)
//            std::cout << "同步-Timestamp: " << *timestamp << std::endl;
//
//        // 提取严重级别
//        auto severity = boost::log::extract<boost::log::trivial::severity_level>("Severity", rec);
//        if (severity)
//            std::cout << "同步-Severity: " << severity << std::endl;
//
//        // 提取线程ID
//        auto thread_id = boost::log::extract<boost::log::attributes::current_thread_id::value_type>("ThreadID", rec);
//        if (thread_id)
//            std::cout << "同步-ThreadID: " << thread_id << std::endl;
//
//        // 提取线程ID
//        auto Alarm = boost::log::extract<std::string>("Alarm", rec);
//        if (thread_id)
//            std::cout << "同步-Alarm: " << Alarm << std::endl;
//
//
//        // 获取日志消息
//        auto message = rec[boost::log::expressions::smessage];
//        std::cout << "Message: " << message << std::endl;
//    }
//};
//
//
//
//// 自定义的异步 sink backend
//class custom_asyn_sink_backend : public boost::log::sinks::basic_sink_backend<boost::log::sinks::concurrent_feeding> {
//public:
//    // 处理每条日志记录
//    void consume(boost::log::record_view const& rec) {
//
//        std::cout << "异步：threadid : " << std::this_thread::get_id() << std::endl;
//
//        // 提取自定义属性 UserID
//        auto id = boost::log::extract<std::string>("UserID", rec);
//        if (id) {
//            std::cout << "UserID: " << id.get() << std::endl;
//        }
//
//        // 提取时间戳
//        auto timestamp = boost::log::extract<boost::posix_time::ptime>("TimeStamp", rec);
//        if (timestamp)
//            std::cout << "异步-Timestamp: " << *timestamp << std::endl;
//
//        // 提取严重级别
//        auto severity = boost::log::extract<boost::log::trivial::severity_level>("Severity", rec);
//        if (severity)
//            std::cout << "异步-Severity: " << severity << std::endl;
//
//        // 提取线程ID
//        auto thread_id = boost::log::extract<boost::log::attributes::current_thread_id::value_type>("ThreadID", rec);
//        if (thread_id)
//            std::cout << "异步-ThreadID: " << thread_id << std::endl;
//
//        // 获取日志消息
//        auto message = rec[boost::log::expressions::smessage];
//        std::cout << "Message: " << message << std::endl;
//    }
//};
//
//
//
//static void log_message_handler(const std::string& message) {
//    std::cout << "Custom Log Handler: " << message << std::endl;
//}
//
//
//
//static void log_source_test() 
//{
//    // 设置格式化器
//     //添加通用属性（时间戳、线程ID等）
//    boost::log::add_common_attributes();
//
//    auto format = boost::log::expressions::stream
//        << "["
//        << boost::log::expressions::format_date_time<boost::posix_time::ptime>("TimeStamp", " %Y-%m-%d %H:%M:%S.%f")
//        << "] "
//        << "["
//        << boost::log::trivial::severity
//        << "] "
//        << "["
//        << boost::log::expressions::attr<boost::log::attributes::current_thread_id::value_type>("ThreadID")
//        << "] "
//        << boost::log::expressions::if_(boost::log::expressions::has_attr("FileName"))
//        [
//            boost::log::expressions::format("[%1%] ") % boost::log::expressions::attr<std::string>("FileName")
//        ]
//        << boost::log::expressions::if_(boost::log::expressions::has_attr("Alarm"))
//        [
//            boost::log::expressions::format("[%1%] ") % boost::log::expressions::attr<std::string>("Alarm")
//        ]
//        << boost::log::expressions::smessage;
//
//    // 创建并配置控制台 Sink
//    boost::shared_ptr<boost::log::sinks::synchronous_sink<boost::log::sinks::text_ostream_backend>> console_sink =
//        boost::make_shared<boost::log::sinks::synchronous_sink<boost::log::sinks::text_ostream_backend>>();
//
//    // 将输出流定向到控制台
//    console_sink->locked_backend()->add_stream(boost::shared_ptr<std::ostream>(&std::cout, boost::null_deleter()));
//
//    //// 使用一个流缓冲区（stringstream），将日志输出到这个流
//    //auto custom_stream = boost::make_shared<std::ostringstream>();
//    //console_sink->locked_backend()->add_stream(custom_stream);
//    console_sink->set_formatter(format);
//
//
//
//    // 创建自定义 同步sink 并将其添加到日志核心
//    //typedef boost::log::sinks::synchronous_sink<custom_asyn_sink_backend> custom_sink_t;
//    //boost::shared_ptr<custom_sink_t> custom_sink = boost::make_shared<custom_sink_t>();
//    //boost::log::core::get()->add_sink(custom_sink);
//
//    // 创建异步 sink 并将其添加到日志核心
//    typedef boost::log::sinks::asynchronous_sink<custom_asyn_sink_backend> async_sink_t;
//    boost::shared_ptr<async_sink_t> sink = boost::make_shared<async_sink_t>();
//    boost::log::core::get()->add_sink(sink);
//    sink->feed_records();
//
//    // 添加 Sink 到核心日志系统
//    boost::log::core::get()->add_sink(console_sink);
//
//
//    // 创建日志源
//    boost::log::sources::severity_logger<boost::log::trivial::severity_level> lg12;
//
//    // 添加线程 ID 属性
//    lg12.add_attribute("ThreadID", boost::log::attributes::current_thread_id());
//    lg12.add_attribute("FileName", boost::log::attributes::constant<std::string>(__FILE__));
//    lg12.add_attribute("TimeStamp", boost::log::attributes::local_clock());
//
//    auto rec = lg12.open_record(boost::log::keywords::severity = boost::log::trivial::info);
//    {
//        // 记录是有效的，可以进行操作
//        boost::log::record_ostream rec_stream(rec);
//        rec_stream << "This is a valid record.";
//        rec.attribute_values().insert("Alarm", boost::log::attributes::make_attribute_value(std::string("Connection established")));
//        lg12.push_record(boost::move(rec));
//    }
//
//
//    // 记录日志
//    //BOOST_LOG_SEV(lg12, boost::log::trivial::info) << "这是一个信息日志。";
//
//
//
//    // 将日志输出到自定义函数
//    //log_message_handler(custom_stream->str());
//}


