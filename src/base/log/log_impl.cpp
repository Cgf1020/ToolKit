#include "log_impl.h"

#include <iostream>
#include <thread>
#include <filesystem> // Added for directory creation


// 定义自定义属性关键字
//BOOST_LOG_ATTRIBUTE_KEYWORD(user_id, "UserID", std::string)



namespace itflee
{
	LogImpl::LogImpl()
		: isInit_(false)
		, logger_()
	{
		// 初始化 boost log 核心  设置全局的过滤器，在 core 上设置的，也可以在 sink端设置
		// boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);
	}

	LogImpl::~LogImpl()
	{
		// 确保所有日志都被刷新到文件
		if (isInit_)
		{
			boost::log::core::get()->flush();
		}
		
		// 清理资源
		if (file_stream_)
		{
			file_stream_->close();
		}
		boost::log::core::get()->remove_all_sinks();
	}

	void LogImpl::Init(const LogInfo& loginfo)
	{
		if (isInit_)
		{
			return;
		}

		logInfo_ = loginfo;
		
		// 清除现有的 sink
		boost::log::core::get()->remove_all_sinks();

        // 添加日志源属性
        logger_.add_attribute("ThreadID", boost::log::attributes::current_thread_id());
        logger_.add_attribute("TimeStamp", boost::log::attributes::local_clock());
		
		// 设置日志级别过滤
		boost::log::trivial::severity_level min_level;
		switch (loginfo._level)
		{
		case LogLevel::Trace:
			min_level = boost::log::trivial::trace;
			break;
		case LogLevel::Debug:
			min_level = boost::log::trivial::debug;
			break;
		case LogLevel::Info:
			min_level = boost::log::trivial::info;
			break;
		case LogLevel::Warning:
			min_level = boost::log::trivial::warning;
			break;
		case LogLevel::Error:
			min_level = boost::log::trivial::error;
			break;
		case LogLevel::Fatal:
			min_level = boost::log::trivial::fatal;
			break;
		default:
			min_level = boost::log::trivial::info;
			break;
		}
		
         // 设置格式化器
         auto format = boost::log::expressions::stream
            << "["
            << boost::log::expressions::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")        // 时间戳
            << "] "
            << "["
            << boost::log::trivial::severity
            << "] "
            << boost::log::expressions::smessage;      // 信息(因为是封装了一层，所以文件名...都在这里包的了)

            // << "["
            // << boost::log::expressions::attr<boost::log::attributes::current_thread_id::value_type>("ThreadID")                 // 线程ID
            // << "] "

        if(1)// 文件日志
        {
            // 创建同步日志 sink
            typedef boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend> file_sink_t;

            // 确保日志与归档目录存在
            try {
                // 只在支持的环境下使用 std::filesystem
                // 若不可用，可在外部提前创建目录
                namespace fs = std::filesystem;
                fs::create_directories(fs::path(logInfo_._path));
                fs::create_directories(fs::path(logInfo_._path) / "archive");
            } catch (...) {}

            // 组合文件名；若用户未提供占位符，则自动追加 _%Y-%m-%d-%H-%M-%S 到扩展名前
            std::string file_pattern = (logInfo_._path + "/" + logInfo_._name);
            if (file_pattern.find('%') == std::string::npos)
            {
                // 解析扩展名位置
                const auto pos = file_pattern.find_last_of('.');
                if (pos == std::string::npos) {
                    file_pattern += "_%Y-%m-%d-%H-%M-%S";
                } else {
                    file_pattern.insert(pos, "_%Y-%m-%d-%H-%M-%S");
                }
            }

            boost::shared_ptr<file_sink_t> file_sink = boost::make_shared<file_sink_t>(
                boost::log::keywords::file_name = file_pattern.c_str(),             // 使用 LogInfo 的路径与文件名（可带时间占位）
                boost::log::keywords::rotation_size = 5 * 1024 * 1024,                          // 文件大小上限 5 MB
                boost::log::keywords::open_mode = std::ios_base::out | std::ios_base::app,      // 打开模式--以输出和追加方式
                boost::log::keywords::time_based_rotation = boost::log::sinks::file::rotation_at_time_point(0, 0, 0), // 每天凌晨滚动文件
                boost::log::keywords::auto_flush = true                                 // 自动刷新
            );

            // 设置文件收集器：控制文件归档和清理规则
            file_sink->locked_backend()->set_file_collector(boost::log::sinks::file::make_collector(
                boost::log::keywords::target = (logInfo_._path + "/archive").c_str(),                  // 日志归档目录
                boost::log::keywords::max_files = 10,
                boost::log::keywords::max_size = 50 * 1024 * 1024,              // 最大归档文件总大小为 50 MB
                boost::log::keywords::min_free_space = 100 * 1024 * 1024        // 磁盘最小空闲空间为 100 MB
            ));

            // 扫描并处理现有的日志文件
            file_sink->locked_backend()->scan_for_files();

            // 设置日志格式
            file_sink->set_formatter(format);

            file_sink->set_filter(boost::log::trivial::severity >= min_level);

            // 将 sink 添加到核心日志器
            boost::log::core::get()->add_sink(file_sink);
        }

		if (loginfo.isLogToConsole)    // 控制台日志
		{
            // 添加控制台sink
            boost::log::add_console_log(std::cout, 
                boost::log::keywords::auto_flush = true,
                boost::log::keywords::format = format);
        }
	
		isInit_ = true;
	}

	void LogImpl::setObserver(std::weak_ptr<LogObserver> observer)
	{
		// 使用 weak_ptr，避免信号连接把观察者“强引用住”导致无法释放
		log_signal_.connect([w = observer](const std::string& log, LogLevel level) {
			if (auto obs = w.lock()) {
				obs->onLog(log, level);
			}
		});
		
		detailed_log_signal_.connect([w = std::move(observer)](uint64_t time, LogLevel level, const std::string& file,
			const std::string& func, int line, const std::string& log) {
			if (auto obs = w.lock()) {
				obs->onLog(time, level, file, func, line, log);
			}
		});
	}

	void LogImpl::printLog(const std::string& log, LogLevel level)
	{
		if (!isInit_)
		{
			return;
		}

		// 转换日志级别
		boost::log::trivial::severity_level boost_level;
		switch (level)
		{
		case LogLevel::Trace:
			boost_level = boost::log::trivial::trace;
			break;
		case LogLevel::Debug:
			boost_level = boost::log::trivial::debug;
			break;
		case LogLevel::Info:
			boost_level = boost::log::trivial::info;
			break;
		case LogLevel::Warning:
			boost_level = boost::log::trivial::warning;
			break;
		case LogLevel::Error:
			boost_level = boost::log::trivial::error;
			break;
		case LogLevel::Fatal:
			boost_level = boost::log::trivial::fatal;
			break;
		default:
			boost_level = boost::log::trivial::info;
			break;
		}

		// 记录日志
		BOOST_LOG_SEV(logger_, boost_level) << log;
		
		// 刷新策略：
		// 1. Error/Fatal级别：立即刷新，确保关键日志不丢失
		// 2. 其他级别：批量刷新（每BATCH_FLUSH_COUNT条刷新一次），平衡性能和可靠性
		if (level == LogLevel::Error || level == LogLevel::Fatal)
		{
			boost::log::core::get()->flush();
		}
		else
		{
			// 批量刷新：每N条日志刷新一次
			int count = ++log_count_;
			if (count >= BATCH_FLUSH_COUNT)
			{
				log_count_ = 0;
				boost::log::core::get()->flush();
			}
		}
		
		// 触发观察者信号（线程安全）
		log_signal_(log, level);
	}

	void LogImpl::printLog(const uint64_t time, LogLevel level,
		const std::string& file,
		const std::string& func,
		const int line,
		const std::string& log)
	{
		if (!isInit_)
		{
			return;
		}

		// 转换日志级别
		boost::log::trivial::severity_level boost_level;
		switch (level)
		{
		case LogLevel::Trace:
			boost_level = boost::log::trivial::trace;
			break;
		case LogLevel::Debug:
			boost_level = boost::log::trivial::debug;
			break;
		case LogLevel::Info:
			boost_level = boost::log::trivial::info;
			break;
		case LogLevel::Warning:
			boost_level = boost::log::trivial::warning;
			break;
		case LogLevel::Error:
			boost_level = boost::log::trivial::error;
			break;
		case LogLevel::Fatal:
			boost_level = boost::log::trivial::fatal;
			break;
		default:
			boost_level = boost::log::trivial::info;
			break;
		}

		// 创建日志记录并添加自定义属性
		auto rec = logger_.open_record(boost::log::keywords::severity = boost_level);
		if (rec)
		{
			boost::log::record_ostream rec_stream(rec);
			rec_stream << log;
			
			// 添加自定义属性
			rec.attribute_values().insert("FileName", boost::log::attributes::make_attribute_value(file));
			rec.attribute_values().insert("FunctionName", boost::log::attributes::make_attribute_value(func));
			rec.attribute_values().insert("LineNumber", boost::log::attributes::make_attribute_value(line));
			rec.attribute_values().insert("Timestamp", boost::log::attributes::make_attribute_value(time));
			
			logger_.push_record(boost::move(rec));
		}
		
		// 刷新策略：
		// 1. Error/Fatal级别：立即刷新，确保关键日志不丢失
		// 2. 其他级别：批量刷新（每BATCH_FLUSH_COUNT条刷新一次），平衡性能和可靠性
		if (level == LogLevel::Error || level == LogLevel::Fatal)
		{
			boost::log::core::get()->flush();
		}
		else
		{
			// 批量刷新：每N条日志刷新一次
			int count = ++log_count_;
			if (count >= BATCH_FLUSH_COUNT)
			{
				log_count_ = 0;
				boost::log::core::get()->flush();
			}
		}
		
		// 触发详细观察者信号（线程安全）
		detailed_log_signal_(time, level, file, func, line, log);
	}
}


