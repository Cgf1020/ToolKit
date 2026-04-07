#include "base/log/log_helper.h"
#include <chrono>


class Obser : public itflee::LogObserver
{
public:
	void onLog(const std::string& log, itflee::LogInterface::LogLevel level) override
	{
		std::cout << "Observer-log: " << log;
		std::cout << "Observer-level: " << level << std::endl;
	}

	void onLog(const uint64_t time,
		itflee::LogInterface::LogLevel level,
		const std::string& file,
		const std::string& func,
		const int line,
		const std::string& log) override
	{
		std::cout << " onLog() " << time << " " << level << " " << file << " " << func << " " << line << " " << log << std::endl;
	}
};

static void logHelperTest()
{
	std::shared_ptr<Obser> o(new Obser());

	// 配置日志信息
	itflee::LogInterface::LogInfo logInfo;
	logInfo._path = "logs";
	logInfo._name = "test_log.txt";
	logInfo._level = itflee::LogInterface::LogLevel::Debug;
	logInfo.limitsize = 10; // 10MB
	logInfo.isLogToConsole = false;

	// 初始化日志系统
	itflee::LogInterface::instance()->Init(logInfo);
	itflee::LogInterface::instance()->setObserver(o);


	std::cout << "----------------开始测试日志功能..." << std::endl;
	
	// 测试不同级别的日志
	LOG_T("这是 Trace 级别的日志");
	LOG_D("这是 Debug 级别的日志");
	LOG_I("这是 Info 级别的日志");
	LOG_W("这是 Warning 级别的日志");
	LOG_E("这是 Error 级别的日志");

	// 测试带 JSON 参数的日志
	itflee::JSON json;
	json["user_id"] = "12345";
	json["action"] = "login";
	LOG_I("用户登录操作, 详情: " << json.to_string());
	//itflee::LogInterface::instance()->printLog("用户登录操作", itflee::LogInterface::LogLevel::Info);

	// 测试带详细信息的日志
	auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
	itflee::LogInterface::instance()->printLog(now, itflee::LogInterface::LogLevel::Info, __FILE__, __FUNCTION__, __LINE__, "带详细信息的日志测试");

	std::cout << "-------------------日志功能测试完成！" << std::endl;
}