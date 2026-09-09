#include "boost_log_backend.h"

#include <boost/core/null_deleter.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/log/attributes/clock.hpp>
#include <boost/log/attributes/constant.hpp>
#include <boost/log/attributes/current_thread_id.hpp>
#include <boost/log/attributes/scoped_attribute.hpp>
#include <boost/log/attributes/value_extraction.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/async_frontend.hpp>
#include <boost/log/sinks/basic_sink_frontend.hpp>
#include <boost/log/sinks/block_on_overflow.hpp>
#include <boost/log/sinks/bounded_fifo_queue.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace attrs = boost::log::attributes;
namespace expr = boost::log::expressions;
namespace keywords = boost::log::keywords;

namespace itflee {

/**
 * @brief Boost.Log 过滤器与格式化需要能把级别写入流。
 */
std::ostream& operator<<(std::ostream& strm, LogLevel level)
{
    strm << log_backend::levelName(level);
    return strm;
}

namespace log_backend {
namespace {

constexpr std::size_t kBoostAsyncQueueSize = 8192;

using FileBackend = sinks::text_file_backend;
using ConsoleBackend = sinks::text_ostream_backend;
using SyncFileSink = sinks::synchronous_sink<FileBackend>;
using AsyncFileSink =
    sinks::asynchronous_sink<FileBackend, sinks::bounded_fifo_queue<kBoostAsyncQueueSize, sinks::block_on_overflow>>;
using SyncConsoleSink = sinks::synchronous_sink<ConsoleBackend>;
using AsyncConsoleSink = sinks::asynchronous_sink<ConsoleBackend,
                                                  sinks::bounded_fifo_queue<kBoostAsyncQueueSize, sinks::block_on_overflow>>;

std::string timestampedPattern(const std::string& directory, const std::string& file)
{
    const std::filesystem::path as_path(file);
    std::string stem = as_path.stem().string();
    std::string ext = as_path.extension().string();
    if (stem.empty()) {
        stem = "default";
    }
    if (ext.empty()) {
        ext = ".log";
    }
    // %N：同一秒内多次滚动时追加序号，避免覆盖；产品主名仍含 stem_YYYY-MM-DD_HH-MM-SS
    const std::filesystem::path full =
        std::filesystem::path(directory) / (stem + "_%Y-%m-%d_%H-%M-%S_%N" + ext);
    return full.generic_string();
}

void appendTimestampMs(logging::formatting_ostream& strm, const boost::posix_time::ptime& t)
{
    if (t.is_not_a_date_time()) {
        strm << "1970-01-01 00:00:00.000";
        return;
    }
    const auto d = t.date();
    const auto tod = t.time_of_day();
    const unsigned ms = static_cast<unsigned>(tod.total_milliseconds() % 1000);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03u",
                  static_cast<int>(d.year()), static_cast<int>(d.month()), static_cast<int>(d.day()),
                  static_cast<int>(tod.hours()), static_cast<int>(tod.minutes()),
                  static_cast<int>(tod.seconds()), ms);
    strm << buf;
}

void formatRecord(const logging::record_view& rec, logging::formatting_ostream& strm)
{
    const auto ts = logging::extract<boost::posix_time::ptime>("TimeStamp", rec);
    const auto sev = logging::extract<LogLevel>("Severity", rec);
    const auto file = logging::extract<std::string>("FileTag", rec);
    const auto tid = logging::extract<attrs::current_thread_id::value_type>("ThreadID", rec);
    const auto msg = rec[expr::smessage];

    strm << '[';
    if (ts) {
        appendTimestampMs(strm, *ts);
    }
    strm << "] [";
    if (sev) {
        strm << levelName(*sev);
    }
    strm << "] [";
    if (file) {
        strm << *file;
    }
    strm << "] [thread:";
    if (tid) {
        strm << *tid;
    }
    strm << "] ";
    if (msg) {
        strm << *msg;
    }
}

}  // namespace

struct BoostLogBackend::Impl {
    struct ModuleState {
        boost::shared_ptr<sinks::basic_sink_frontend> file_sink;
        std::optional<LogLevel> override_level;
        bool console_enabled{false};
        LogLevel console_level{LogLevel::Info};
    };

    mutable std::mutex mutex;
    bool initialized{false};
    LoggerConfig config;
    std::string default_name{"default"};
    bool source_enabled{false};
    src::severity_channel_logger_mt<LogLevel, std::string> logger;
    std::vector<boost::shared_ptr<sinks::basic_sink_frontend>> sinks;
    boost::shared_ptr<sinks::basic_sink_frontend> console_sink;
    std::unordered_map<std::string, ModuleState> modules;
    std::unordered_set<std::string> unknown_warned;

    LogLevel effectiveFileLevel(const ModuleState& state) const
    {
        return state.override_level.value_or(config.level);
    }

    void applyModuleFilter(ModuleState& state, const std::string& channel)
    {
        const LogLevel file_level = effectiveFileLevel(state);
        state.file_sink->set_filter(expr::attr<std::string>("Channel") == channel &&
                                    expr::attr<LogLevel>("Severity") >= file_level);
    }

    /**
     * @brief 按当前各模块的 console / console_level 重建控制台过滤器。
     * @note 前提：已持有 mutex。
     */
    void applyConsoleFilter()
    {
        if (!console_sink) {
            return;
        }
        auto allow = std::make_shared<std::unordered_map<std::string, LogLevel>>();
        for (const auto& [name, state] : modules) {
            if (state.console_enabled) {
                (*allow)[name] = state.console_level;
            }
        }
        console_sink->set_filter([allow](const logging::attribute_value_set& attrs) {
            const auto ch = logging::extract<std::string>("Channel", attrs);
            const auto sev = logging::extract<LogLevel>("Severity", attrs);
            if (!ch || !sev) {
                return false;
            }
            const auto it = allow->find(ch.get());
            if (it == allow->end()) {
                return false;
            }
            return static_cast<int>(sev.get()) >= static_cast<int>(it->second);
        });
    }

    void removeAllSinks()
    {
        auto core = logging::core::get();
        for (auto& sink : sinks) {
            if (sink) {
                sink->flush();
                core->remove_sink(sink);
            }
        }
        sinks.clear();
        modules.clear();
        console_sink.reset();
    }

    boost::shared_ptr<FileBackend> makeFileBackend(const std::string& logical_file)
    {
        auto backend = boost::make_shared<FileBackend>(
            keywords::file_name = timestampedPattern(config.directory, logical_file),
            keywords::rotation_size = config.max_size,
            keywords::auto_flush = false,
            keywords::open_mode = std::ios_base::out | std::ios_base::app);
        backend->set_file_collector(sinks::file::make_collector(
            keywords::target = config.directory, keywords::max_files = config.max_files + 1));
        try {
            backend->scan_for_files();
        } catch (const std::exception&) {
            // 目录为空或尚无历史文件时忽略
        }
        return backend;
    }

    boost::shared_ptr<sinks::basic_sink_frontend> wrapFileSink(const boost::shared_ptr<FileBackend>& backend)
    {
        if (config.async) {
            auto sink = boost::make_shared<AsyncFileSink>(backend);
            sink->set_formatter(&formatRecord);
            return sink;
        }
        auto sink = boost::make_shared<SyncFileSink>(backend);
        sink->set_formatter(&formatRecord);
        return sink;
    }

    boost::shared_ptr<sinks::basic_sink_frontend> makeConsoleSink()
    {
        auto backend = boost::make_shared<ConsoleBackend>();
        backend->add_stream(boost::shared_ptr<std::ostream>(&std::cout, boost::null_deleter()));
        backend->auto_flush(false);
        if (config.async) {
            auto sink = boost::make_shared<AsyncConsoleSink>(backend);
            sink->set_formatter(&formatRecord);
            return sink;
        }
        auto sink = boost::make_shared<SyncConsoleSink>(backend);
        sink->set_formatter(&formatRecord);
        return sink;
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
        sinks.clear();

        if (config.max_size == 0) {
            config.max_size = 100 * 1024 * 1024;
        }
        if (config.max_files == 0) {
            config.max_files = 1;
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

        auto core = logging::core::get();
        core->add_global_attribute("TimeStamp", attrs::local_clock());
        core->add_global_attribute("ThreadID", attrs::current_thread_id());
        core->set_logging_enabled(true);

        try {
            auto error_backend = makeFileBackend("error.log");
            auto error_sink = wrapFileSink(error_backend);
            error_sink->set_filter(expr::attr<LogLevel>("Severity") >= LogLevel::Error);
            core->add_sink(error_sink);
            sinks.push_back(error_sink);

            bool any_console = false;
            for (const auto& module : module_cfgs) {
                if (modules.count(module.name) != 0) {
                    std::cerr << "[logger] duplicate module ignored: " << module.name << '\n';
                    continue;
                }

                auto file_backend = makeFileBackend(module.file);
                auto file_sink = wrapFileSink(file_backend);
                ModuleState state;
                state.file_sink = file_sink;
                state.override_level = module.level;
                state.console_enabled = resolveModuleConsole(config, module);
                state.console_level = resolveModuleConsoleLevel(config, module);
                any_console = any_console || state.console_enabled;
                applyModuleFilter(state, module.name);
                core->add_sink(file_sink);
                sinks.push_back(file_sink);
                modules.emplace(module.name, std::move(state));
            }

            if (any_console) {
                console_sink = makeConsoleSink();
                applyConsoleFilter();
                core->add_sink(console_sink);
                sinks.push_back(console_sink);
            }
        } catch (const std::exception& e) {
            std::cerr << "[logger] init failed: " << e.what() << '\n';
            removeAllSinks();
            return false;
        }

        if (modules.empty()) {
            removeAllSinks();
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
        logging::core::get()->flush();
        removeAllSinks();
        unknown_warned.clear();
        initialized = false;
    }

    void flush()
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!initialized) {
            return;
        }
        logging::core::get()->flush();
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
        for (auto& [name, state] : modules) {
            applyModuleFilter(state, name);
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
        applyModuleFilter(it->second, it->first);
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
        const LogLevel file_level = effectiveFileLevel(it->second);
        LogLevel logger_level = mostVerbose(file_level, LogLevel::Error);
        if (it->second.console_enabled) {
            logger_level = mostVerbose(logger_level, it->second.console_level);
        }
        return static_cast<int>(level) >= static_cast<int>(logger_level);
    }

    void log(std::string_view name, LogLevel level, const SourceLocation& loc, std::string_view message)
    {
        bool do_flush = false;
        std::string tag;
        std::string channel;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!initialized) {
                logToStderr(name.empty() ? default_name : name, level, message);
                return;
            }
            channel = std::string(name);
            if (modules.count(channel) == 0) {
                channel = default_name;
            }
            if (modules.count(channel) == 0) {
                logToStderr(name, level, message);
                return;
            }
            tag = sourceFileTag(loc, source_enabled);
            do_flush = static_cast<int>(level) >= static_cast<int>(config.flush_level) ||
                       level == LogLevel::Critical;
        }

        auto sentry =
            logging::add_scoped_thread_attribute("FileTag", attrs::constant<std::string>(std::move(tag)));
        BOOST_LOG_CHANNEL_SEV(logger, channel, level) << message;
        if (do_flush) {
            logging::core::get()->flush();
        }
        (void)sentry;
    }
};

BoostLogBackend::BoostLogBackend()
    : impl_(std::make_unique<Impl>())
{
}

BoostLogBackend::~BoostLogBackend()
{
    if (impl_) {
        impl_->shutdown();
    }
}

bool BoostLogBackend::init(const LoggerConfig& config)
{
    return impl_->init(config);
}

void BoostLogBackend::shutdown()
{
    impl_->shutdown();
}

void BoostLogBackend::flush()
{
    impl_->flush();
}

bool BoostLogBackend::isInitialized() const
{
    return impl_->isInitialized();
}

void BoostLogBackend::setLevel(LogLevel level)
{
    impl_->setLevel(level);
}

void BoostLogBackend::setLevel(std::string_view name, LogLevel level)
{
    impl_->setLevel(name, level);
}

std::string BoostLogBackend::resolveName(std::string_view name)
{
    return impl_->resolveName(name);
}

bool BoostLogBackend::shouldLog(std::string_view name, LogLevel level) const
{
    return impl_->shouldLog(name, level);
}

void BoostLogBackend::log(std::string_view name,
                          LogLevel level,
                          const SourceLocation& loc,
                          std::string_view message)
{
    impl_->log(name, level, loc, message);
}

std::unique_ptr<ILogBackend> createBoostLogBackend()
{
    return std::make_unique<BoostLogBackend>();
}

}  // namespace log_backend
}  // namespace itflee
