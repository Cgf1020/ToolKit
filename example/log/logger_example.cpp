/**
 * @file logger_example.cpp
 * @brief Logger 用法演示：填满配置项、fmt 宏、流式宏、flush、shutdown。
 *
 * 功能回归与性能对比见 test/base_test/logger_test。
 */

#include "base/log/logger.h"

#include <filesystem>
#include <stdexcept>
#include <string>

int main()
{
    namespace fs = std::filesystem;

    const fs::path log_dir = fs::absolute("logger_example_output");
    fs::create_directories(log_dir);

    itflee::LoggerConfig config;
    config.level = itflee::LogLevel::Debug;
    config.console = true;
    config.console_level = itflee::LogLevel::Info;
    config.directory = log_dir.string();
    config.max_size = 100 * 1024 * 1024;
    config.max_files = 10;
    config.async = true;
    config.async_queue_size = 8192;
    config.async_thread_count = 1;
    config.source_location = itflee::SourceLocationMode::On;
    config.flush_level = itflee::LogLevel::Warn;
    config.default_module = "application";
    config.backend = itflee::LogBackendKind::Spdlog;
    config.modules = {
        {"application", "application.log", itflee::LogLevel::Debug, true, itflee::LogLevel::Info},
        {"communication", "communication.log", itflee::LogLevel::Info, true, itflee::LogLevel::Info},
        {"business", "business.log", itflee::LogLevel::Info, true, itflee::LogLevel::Warn},
    };

    if (!itflee::Logger::init(config)) {
        LOG_E("Logger::init failed");
        return 1;
    }

    LOG_I("process started, log_dir={}", log_dir.string());
    LOG_D("debug goes to file; console_level is Info so this line is not printed");

    LOG_M_I("communication", "TCP connected: {}:{}", "127.0.0.1", 9000);
    LOG_M_I("business", "order accepted id={}", 42);
    LOG_M_W("communication", "retry connect, attempt={}", 1);
    LOG_M_E("business", "order failed id={} reason={}", 43, "timeout");

    const std::string exe = "logger_example";
    LOG_I_S(exe << " start time: " << 12345);
    LOG_M_I_S("communication", "retry " << 1 << " host=" << "127.0.0.1");
    LOG_M_W_S("business", "order id=" << 42 << " waiting payment");

    try {
        throw std::runtime_error("demo exception");
    } catch (const std::exception& e) {
        LOG_EX(e);
    }

    itflee::Logger::flush();
    LOG_I("process stopping");
    itflee::Logger::shutdown();
    return 0;
}
