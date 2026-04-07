#pragma once

#include "base/log/log_helper.h"

#include <boost/log/trivial.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup.hpp>

#include <boost/log/sources/logger.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/global_logger_storage.hpp>

#include <boost/log/attributes/current_thread_id.hpp>
#include <boost/log/attributes/attribute.hpp>
#include <boost/log/attributes/constant.hpp>
#include <boost/log/attributes/attribute_set.hpp>
#include <boost/log/support/date_time.hpp>

#include <boost/log/sinks/async_frontend.hpp>
#include <boost/log/sinks/basic_sink_backend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>

#include <boost/make_shared.hpp>
#include <fstream>
#include <atomic>
#include <boost/log/attributes.hpp>
#include <boost/signals2.hpp>



namespace itflee
{
	class LogImpl : public LogInterface
	{
	public:
		LogImpl();
		~LogImpl();

	public:
		void Init(const LogInfo& loginfo = LogInfo()) override;

		void setObserver(std::weak_ptr<LogObserver> observer) override;

		void printLog(const std::string& log, LogLevel level) override;

		void printLog(const uint64_t time,
			LogLevel level,
			const std::string& file,
			const std::string& func,
			const int line,
			const std::string& log) override;

	private:
		bool isInit_{ false };
		
		// boost log 相关成员
		boost::log::sources::severity_logger<boost::log::trivial::severity_level> logger_;

		// 线程安全的信号
		boost::signals2::signal<void(const std::string&, LogLevel)> log_signal_;
		boost::signals2::signal<void(uint64_t, LogLevel, const std::string&, const std::string&, int, const std::string&)> detailed_log_signal_;
		
		// 日志配置信息
		LogInfo logInfo_;
		
		// 文件输出流
		std::shared_ptr<std::ofstream> file_stream_;
		
		// 批量刷新计数器：每N条普通日志刷新一次（仅用于非Error/Fatal级别）
		static constexpr int BATCH_FLUSH_COUNT = 10;
		std::atomic<int> log_count_{ 0 };

	};

}


