#include "base/log/logger.h"

#include "backend.h"
#include "boost/boost_log_backend.h"
#include "spdlog/spdlog_backend.h"

#include <memory>
#include <mutex>

namespace itflee {
namespace {

std::mutex g_mutex;
std::shared_ptr<log_backend::ILogBackend> g_backend;

}  // namespace

Logger::Logger(std::string name)
    : name_(std::move(name))
{
}

bool Logger::init(const LoggerConfig& config)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_backend && g_backend->isInitialized()) {
        return false;
    }
    if (config.backend == LogBackendKind::BoostLog) {
        g_backend = log_backend::createBoostLogBackend();
    } else {
        g_backend = log_backend::createSpdlogBackend();
    }
    return g_backend->init(config);
}

void Logger::shutdown()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_backend) {
        g_backend->shutdown();
        g_backend.reset();
    }
}

void Logger::flush()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_backend) {
        g_backend->flush();
    }
}

bool Logger::isInitialized()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_backend && g_backend->isInitialized();
}

void Logger::setLevel(LogLevel level)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_backend) {
        g_backend->setLevel(level);
    }
}

void Logger::setLevel(std::string_view name, LogLevel level)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_backend) {
        g_backend->setLevel(name, level);
    }
}

Logger Logger::get(std::string_view name)
{
    std::shared_ptr<log_backend::ILogBackend> backend;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        backend = g_backend;
        if (!backend) {
            return Logger(name.empty() ? std::string("default") : std::string(name));
        }
    }
    return Logger(backend->resolveName(name));
}

Logger Logger::getDefault()
{
    return get({});
}

std::string_view Logger::name() const
{
    return name_;
}

bool Logger::shouldLog(LogLevel level) const
{
    std::shared_ptr<log_backend::ILogBackend> backend;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        backend = g_backend;
        if (!backend) {
            return true;
        }
    }
    return backend->shouldLog(name_, level);
}

void Logger::logMessage(LogLevel level, const SourceLocation& loc, std::string_view message) const
{
    std::shared_ptr<log_backend::ILogBackend> backend;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        backend = g_backend;
        if (!backend || !backend->isInitialized()) {
            log_backend::logToStderr(name_.empty() ? "default" : name_, level, message);
            return;
        }
    }
    backend->log(name_, level, loc, message);
}

}  // namespace itflee
