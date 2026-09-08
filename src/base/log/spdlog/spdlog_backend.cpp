#include "spdlog_backend.h"

#include "timestamped_file_sink.h"

#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace itflee {
namespace log_backend {
namespace {

class UpperLevelFlag final : public spdlog::custom_flag_formatter {
public:
    void format(const spdlog::details::log_msg& msg,
                const std::tm&,
                spdlog::memory_buf_t& dest) override
    {
        const char* text = "INFO";
        switch (msg.level) {
            case spdlog::level::trace:
                text = "TRACE";
                break;
            case spdlog::level::debug:
                text = "DEBUG";
                break;
            case spdlog::level::info:
                text = "INFO";
                break;
            case spdlog::level::warn:
                text = "WARN";
                break;
            case spdlog::level::err:
                text = "ERROR";
                break;
            case spdlog::level::critical:
                text = "CRITICAL";
                break;
            case spdlog::level::off:
                text = "OFF";
                break;
            default:
                text = "INFO";
                break;
        }
        dest.append(text, text + std::char_traits<char>::length(text));
    }

    std::unique_ptr<spdlog::custom_flag_formatter> clone() const override
    {
        return std::make_unique<UpperLevelFlag>();
    }
};

spdlog::level::level_enum toSpdlog(LogLevel level)
{
    switch (level) {
        case LogLevel::Trace:
            return spdlog::level::trace;
        case LogLevel::Debug:
            return spdlog::level::debug;
        case LogLevel::Info:
            return spdlog::level::info;
        case LogLevel::Warn:
            return spdlog::level::warn;
        case LogLevel::Error:
            return spdlog::level::err;
        case LogLevel::Critical:
            return spdlog::level::critical;
        default:
            return spdlog::level::info;
    }
}

std::unique_ptr<spdlog::formatter> makeFormatter(bool with_source)
{
    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    formatter->add_flag<UpperLevelFlag>('E');
    if (with_source) {
        formatter->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%E] [%s:%#] [thread:%t] %v");
    } else {
        formatter->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%E] [%s] [thread:%t] %v");
    }
    return formatter;
}

}  // namespace

struct SpdlogBackend::Impl {
    struct ModuleState {
        std::shared_ptr<spdlog::logger> logger;
        spdlog::sink_ptr file_sink;
        spdlog::sink_ptr console_sink;  ///< 仅该模块需要控制台时非空
        std::optional<LogLevel> override_level;
        bool console_enabled{false};
        LogLevel console_level{LogLevel::Info};
    };

    mutable std::mutex mutex;
    bool initialized{false};
    LoggerConfig config;
    std::string default_name{"default"};
    bool source_enabled{false};     // 是否启用源位置信息
    spdlog::sink_ptr error_sink;
    std::unordered_map<std::string, ModuleState> modules;
    std::unordered_set<std::string> unknown_warned;

    LogLevel effectiveFileLevel(const ModuleState& state) const
    {
        return state.override_level.value_or(config.level);
    }

    void applyModuleLevels(ModuleState& state)
    {
        const LogLevel file_level = effectiveFileLevel(state);
        state.file_sink->set_level(toSpdlog(file_level));

        LogLevel logger_level = mostVerbose(file_level, LogLevel::Error);
        if (state.console_enabled && state.console_sink) {
            logger_level = mostVerbose(logger_level, state.console_level);
            state.console_sink->set_level(toSpdlog(state.console_level));
        }
        state.logger->set_level(toSpdlog(logger_level));
        state.logger->flush_on(toSpdlog(config.flush_level));
    }

    std::string joinLogPath(const std::string& file) const
    {
        return (std::filesystem::path(config.directory) / file).string();
    }

    std::shared_ptr<spdlog::logger> makeLogger(const std::string& name,
                                               std::vector<spdlog::sink_ptr> sinks)
    {
        std::shared_ptr<spdlog::logger> logger;
        if (config.async) {
            logger = std::make_shared<spdlog::async_logger>(
                name,
                sinks.begin(),
                sinks.end(),
                spdlog::thread_pool(),
                spdlog::async_overflow_policy::block);
        } else {
            logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
        }
        logger->set_formatter(makeFormatter(source_enabled));
        spdlog::register_logger(logger);
        return logger;
    }

    bool init(const LoggerConfig& in_config)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (initialized) {
            return false;
        }

        config = in_config;
        source_enabled = resolveSourceEnabled(config.source_location);
        modules.clear();
        unknown_warned.clear();
        error_sink.reset();

        if (config.max_size == 0) {
            config.max_size = 100 * 1024 * 1024;
        }
        if (config.max_files == 0) {
            config.max_files = 1;
        }
        if (config.async_queue_size < 64) {
            config.async_queue_size = 64;
        }
        if (config.async_thread_count == 0) {
            config.async_thread_count = 1;
        }
        if (config.directory.empty()) {
            config.directory = "logs";
        }

        std::vector<ModuleConfig> module_cfgs = config.modules;
        if (module_cfgs.empty()) {
            ModuleConfig fallback;
            fallback.name = "default";
            fallback.file = "default.log";
            module_cfgs.push_back(std::move(fallback));
        }

        for (auto& module : module_cfgs) {
            if (module.name.empty()) {
                module.name = "default";
            }
            if (module.file.empty()) {
                module.file = module.name + ".log";
            }
        }

        if (config.default_module.empty()) {
            default_name = module_cfgs.front().name;
        } else {
            default_name = config.default_module;
            const bool found = std::any_of(
                module_cfgs.begin(), module_cfgs.end(),
                [this](const ModuleConfig& m) { return m.name == default_name; });
            if (!found) {
                ModuleConfig extra;
                extra.name = default_name;
                extra.file = default_name + ".log";
                module_cfgs.insert(module_cfgs.begin(), std::move(extra));
            }
        }

        try {
            std::filesystem::create_directories(config.directory);
        } catch (const std::exception& e) {
            std::cerr << "[logger] create directory failed: " << e.what() << '\n';
            return false;
        }

        spdlog::drop_all();
        spdlog::set_error_handler([](const std::string& msg) {
            std::cerr << "[logger] " << msg << '\n';
        });

        if (config.async) {
            spdlog::init_thread_pool(config.async_queue_size, config.async_thread_count);
        }

        try {
            error_sink = std::make_shared<TimestampedRotatingFileSinkMt>(
                joinLogPath("error.log"), config.max_size, config.max_files);
            error_sink->set_level(spdlog::level::err);

            for (const auto& module : module_cfgs) {
                if (modules.count(module.name) != 0) {
                    std::cerr << "[logger] duplicate module ignored: " << module.name << '\n';
                    continue;
                }

                const std::string file_path = joinLogPath(module.file);
                auto file_sink = std::make_shared<TimestampedRotatingFileSinkMt>(
                    file_path, config.max_size, config.max_files);

                std::vector<spdlog::sink_ptr> sinks;
                sinks.push_back(file_sink);
                if (module.file != "error.log") {
                    sinks.push_back(error_sink);
                }

                ModuleState state;
                state.file_sink = std::move(file_sink);
                state.override_level = module.level;
                state.console_enabled = resolveModuleConsole(config, module);
                state.console_level = resolveModuleConsoleLevel(config, module);
                if (state.console_enabled) {
                    auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
                    console->set_level(toSpdlog(state.console_level));
                    state.console_sink = console;
                    sinks.push_back(std::move(console));
                }

                state.logger = makeLogger(module.name, std::move(sinks));
                applyModuleLevels(state);
                modules.emplace(module.name, std::move(state));
            }
        } catch (const std::exception& e) {
            std::cerr << "[logger] init failed: " << e.what() << '\n';
            spdlog::drop_all();
            spdlog::shutdown();
            modules.clear();
            error_sink.reset();
            return false;
        }

        if (modules.empty()) {
            spdlog::shutdown();
            return false;
        }

        initialized = true;
        return true;
    }

    void shutdown()
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!initialized) {
            return;
        }
        for (auto& [_, state] : modules) {
            if (state.logger) {
                state.logger->flush();
            }
        }
        modules.clear();
        error_sink.reset();
        unknown_warned.clear();
        initialized = false;
        spdlog::shutdown();
    }

    void flush()
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!initialized) {
            return;
        }
        for (auto& [_, state] : modules) {
            if (state.logger) {
                state.logger->flush();
            }
        }
    }

    bool isInitialized() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return initialized;
    }

    void setLevel(LogLevel level)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!initialized) {
            return;
        }
        config.level = level;
        for (auto& [_, state] : modules) {
            applyModuleLevels(state);
        }
    }

    void setLevel(std::string_view name, LogLevel level)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!initialized) {
            return;
        }
        auto it = modules.find(std::string(name));
        if (it == modules.end()) {
            return;
        }
        it->second.override_level = level;
        applyModuleLevels(it->second);
    }

    std::string resolveName(std::string_view name)
    {
        std::string fallback;
        std::string unknown;
        bool warn = false;
        {
            std::lock_guard<std::mutex> lock(mutex);
            const std::string key(name);
            if (!initialized) {
                return key.empty() ? default_name : key;
            }
            if (key.empty() || modules.count(key) != 0) {
                return key.empty() ? default_name : key;
            }
            unknown = key;
            fallback = default_name;
            warn = unknown_warned.insert(key).second;
        }
        if (warn) {
            log(fallback, LogLevel::Warn, SourceLocation{},
                std::string("Unknown logger module '") + unknown + "', fallback to '" + fallback +
                    "'");
        }
        return fallback;
    }

    bool shouldLog(std::string_view name, LogLevel level) const
    {
        std::shared_ptr<spdlog::logger> logger;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!initialized) {
                return true;
            }
            auto it = modules.find(std::string(name));
            if (it == modules.end()) {
                it = modules.find(default_name);
            }
            if (it == modules.end()) {
                return true;
            }
            logger = it->second.logger;
        }
        return logger && logger->should_log(toSpdlog(level));
    }

    void log(std::string_view name, LogLevel level, const SourceLocation& loc, std::string_view message)
    {
        std::shared_ptr<spdlog::logger> logger;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!initialized) {
                logToStderr(name.empty() ? default_name : name, level, message);
                return;
            }
            auto it = modules.find(std::string(name));
            if (it == modules.end()) {
                it = modules.find(default_name);
            }
            if (it == modules.end() || !it->second.logger) {
                logToStderr(name, level, message);
                return;
            }
            logger = it->second.logger;
        }

        const auto spd_level = toSpdlog(level);
        const spdlog::string_view_t view{message.data(), message.size()};
        if (loc.file != nullptr && loc.file[0] != '\0') {
            logger->log(spdlog::source_loc{loc.file, loc.line, loc.function}, spd_level, view);
        } else {
            logger->log(spd_level, view);
        }

        if (static_cast<int>(level) >= static_cast<int>(config.flush_level) ||
            level == LogLevel::Critical) {
            logger->flush();
        }
    }
};

SpdlogBackend::SpdlogBackend()
    : impl_(std::make_unique<Impl>())
{
}

SpdlogBackend::~SpdlogBackend()
{
    if (impl_) {
        impl_->shutdown();
    }
}

bool SpdlogBackend::init(const LoggerConfig& config)
{
    return impl_->init(config);
}

void SpdlogBackend::shutdown()
{
    impl_->shutdown();
}

void SpdlogBackend::flush()
{
    impl_->flush();
}

bool SpdlogBackend::isInitialized() const
{
    return impl_->isInitialized();
}

void SpdlogBackend::setLevel(LogLevel level)
{
    impl_->setLevel(level);
}

void SpdlogBackend::setLevel(std::string_view name, LogLevel level)
{
    impl_->setLevel(name, level);
}

std::string SpdlogBackend::resolveName(std::string_view name)
{
    return impl_->resolveName(name);
}

bool SpdlogBackend::shouldLog(std::string_view name, LogLevel level) const
{
    return impl_->shouldLog(name, level);
}

void SpdlogBackend::log(std::string_view name,
                        LogLevel level,
                        const SourceLocation& loc,
                        std::string_view message)
{
    impl_->log(name, level, loc, message);
}

std::unique_ptr<ILogBackend> createSpdlogBackend()
{
    return std::make_unique<SpdlogBackend>();
}

}  // namespace log_backend
}  // namespace itflee
