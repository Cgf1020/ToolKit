#ifndef LOH_INTERFACE_H_
#define LOH_INTERFACE_H_
/*
* 日志对外的接口文件
* 
* 底层应该适配各种第三方的库
* 目前实现方案：boost.log
* 
* 未实现的功能
*	1. 适配网络输出sink
 */
#include <string>
#include <memory>
#include <sstream>
#include "define.h"


namespace itflee
{
	class LogObserver;
	
	class ITFLEEEXPORT LogInterface
	{
	public:
		/* 日志的等级
		 *
		 */
		enum LogLevel { Trace, Debug, Info, Warning, Error, Fatal};

		struct LogInfo
		{
			std::string _path{"."};	//
			std::string _name{"log.txt"};	//
			LogLevel _level{LogLevel::Info};	// 设置过滤的等级
			int limitsize{5};		// MB. 0 for no limit
			bool isLogToConsole{true};	// 是否输出到控制台
		};

	public:
		/* 获取日志对象的单例
		 */
		static std::shared_ptr<LogInterface> instance();

		static void destroy();

		virtual ~LogInterface() {}

	public:
		/* 初始化
		*/
		virtual void Init(const LogInfo& loginfo) = 0;

		/* 设置观察者
		 */
		virtual void setObserver(std::weak_ptr<LogObserver> observer) = 0;

		/* 打印日志
		 * @param log: 日志信息
		 * @param level: 等级
		 * @param json: 自定义属性
		 */
		virtual void printLog(const std::string& log, LogLevel level) = 0;

		/* 打印日志
		 * @param time: 时间
		 * @param level: 等级
		 * @param file: 文件名
		 * @param func: 函数名
		 * @param line: 行号
		 * @param log: 日志信息
		 */
		virtual void printLog(const uint64_t time, LogLevel level,
			const std::string& file, 
			const std::string& func, 
			const int line,
			const std::string& log) = 0;
	};

	class ITFLEEEXPORT LogObserver
	{
	public:
		virtual ~LogObserver() {}
		virtual void onLog(const std::string&, LogInterface::LogLevel) {}
		virtual void onLog(const uint64_t, LogInterface::LogLevel, const std::string&, const std::string&, const int, const std::string&) {}
	};
}

#ifdef NOUSELOG

#define LOG_PRINT(log, level) \
{\
	std::ostringstream logstream;\
	logstream << log << std::endl;\
	std::cout << logstream.str() << std::endl;
}

#else

#define LOG_PRINT(log, level) \
{ \
	std::ostringstream logstream;\
	std::string filename(__FILE__);\
	size_t pos = filename.find_last_of("/\\");\
	if (pos != std::string::npos) {\
		filename = filename.substr(pos + 1);\
	}\
	logstream << "["<< filename << " " << __FUNCTION__ << " "<< __LINE__ << "] ";\
	logstream << log << std::endl;\
	itflee::LogInterface::instance()->printLog(logstream.str(), level);\
}
#endif // DEBUG



#define LOG_T(log)	LOG_PRINT(log, itflee::LogInterface::LogLevel::Trace)
#define LOG_D(log)	LOG_PRINT(log, itflee::LogInterface::LogLevel::Debug)
#define LOG_I(log)	LOG_PRINT(log, itflee::LogInterface::LogLevel::Info)
#define LOG_W(log)	LOG_PRINT(log, itflee::LogInterface::LogLevel::Warning)
#define LOG_E(log)	LOG_PRINT(log, itflee::LogInterface::LogLevel::Error)



#endif	//LOH_INTERFACE_H_



