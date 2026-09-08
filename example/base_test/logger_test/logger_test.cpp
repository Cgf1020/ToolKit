#include "base/log/logger.h"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

namespace {

int g_failed = 0;
int g_passed = 0;
std::vector<std::string> g_fail_details;

itflee::LogBackendKind g_backend = itflee::LogBackendKind::Spdlog;

const char* backendTag(itflee::LogBackendKind backend)
{
    return backend == itflee::LogBackendKind::BoostLog ? "Boost.Log" : "spdlog";
}

const char* backendDirTag(itflee::LogBackendKind backend)
{
    return backend == itflee::LogBackendKind::BoostLog ? "boost" : "spdlog";
}

struct CaseResult {
    std::string name;
    bool ok{false};
    int passed{0};
    int failed{0};
    double ms{0};
    std::vector<std::string> failures;
};

struct PerfResult {
    std::string scenario;
    std::string backend;
    bool async{false};
    int threads{1};
    int count{0};
    double call_ms{0};
    double shutdown_ms{0};
    double total_ms{0};
    double enqueue_per_sec{0};
    double durable_per_sec{0};
    double ns_per_call{0};
    std::uint64_t bytes{0};
    bool data_ok{false};
    std::string note;
};

struct BackendSection {
    std::string backend;
    std::vector<CaseResult> cases;
    std::vector<PerfResult> perfs;
};

std::vector<BackendSection> g_sections;
BackendSection* g_current = nullptr;
CaseResult* g_live_case = nullptr;
std::string g_current_case;

void expect(bool cond, const std::string& msg)
{
    if (cond) {
        ++g_passed;
        return;
    }
    ++g_failed;
    g_fail_details.push_back(std::string("[") + backendTag(g_backend) + "] " + g_current_case +
                             " / " + msg);
    if (g_live_case != nullptr) {
        g_live_case->failures.push_back(msg);
    }
    std::cerr << "FAIL: [" << backendTag(g_backend) << "] " << g_current_case << " / " << msg
              << '\n';
}

std::string readFile(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::vector<fs::path> listModuleLogs(const fs::path& dir, const std::string& stem)
{
    std::vector<fs::path> files;
    if (!fs::exists(dir)) {
        return files;
    }
    const std::string prefix = stem + "_";
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::string name = entry.path().filename().string();
        if (name.size() > prefix.size() && name.compare(0, prefix.size(), prefix) == 0 &&
            entry.path().extension() == ".log") {
            files.push_back(entry.path());
        }
    }
    return files;
}

std::string readModuleLogs(const fs::path& dir, const std::string& stem)
{
    std::string body;
    for (const auto& path : listModuleLogs(dir, stem)) {
        body += readFile(path);
    }
    return body;
}

bool moduleContains(const fs::path& dir, const std::string& stem, const std::string& needle)
{
    return readModuleLogs(dir, stem).find(needle) != std::string::npos;
}

std::uint64_t dirBytes(const fs::path& dir)
{
    std::uint64_t sum = 0;
    if (!fs::exists(dir)) {
        return 0;
    }
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            std::error_code ec;
            const auto n = entry.file_size(ec);
            if (!ec) {
                sum += n;
            }
        }
    }
    return sum;
}

std::size_t countLines(const std::string& body)
{
    std::size_t lines = 0;
    for (char c : body) {
        if (c == '\n') {
            ++lines;
        }
    }
    return lines;
}

std::string makeDir(const std::string& suffix)
{
    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    fs::path dir = fs::path("logger_test_output") /
                   (std::string(backendDirTag(g_backend)) + "_" + suffix + "_" + std::to_string(stamp));
    fs::remove_all(dir);
    fs::create_directories(dir);
    return dir.string();
}

itflee::LoggerConfig baseConfig(const std::string& dir)
{
    itflee::LoggerConfig config;
    config.level = itflee::LogLevel::Info;
    config.console = false;
    config.directory = dir;
    config.max_size = 100 * 1024 * 1024;
    config.max_files = 5;
    config.async = false;
    config.source_location = itflee::SourceLocationMode::Off;
    config.flush_level = itflee::LogLevel::Warn;
    config.default_module = "application";
    config.modules = {
        {"application", "application.log", {}},
        {"communication", "communication.log", {}},
        {"business", "business.log", {}},
    };
    config.backend = g_backend;
    return config;
}

void testUninitWritesStderr()
{
    itflee::Logger::shutdown();
    expect(!itflee::Logger::isInitialized(), "uninit: not initialized");
    LOG_WARN("uninit-warning-visible");
}

void testModuleFilesAndErrorSink()
{
    const std::string dir = makeDir("modules");
    auto config = baseConfig(dir);
    expect(itflee::Logger::init(config), "modules: init");
    expect(itflee::Logger::isInitialized(), "modules: initialized");

    LOG_INFO("Server started");
    LOG_M_INFO("communication", "TCP connected: {}:{}", "192.168.1.10", 9000);
    LOG_M_INFO("business", "User login");
    LOG_M_ERROR("communication", "Connect failed: {}", "Connection refused");
    LOG_M_CRITICAL("business", "Business fatal");
    itflee::Logger::shutdown();

    const fs::path root(dir);
    expect(moduleContains(root, "application", "Server started"), "application.info");
    expect(moduleContains(root, "application", "[logger_test.cpp]"), "application.filename");
    expect(!moduleContains(root, "application", "[application]"), "application.no_module");
    expect(!moduleContains(root, "application", "TCP connected"), "application.no_comm");

    expect(moduleContains(root, "communication", "TCP connected: 192.168.1.10:9000"),
           "communication.info");
    expect(moduleContains(root, "communication", "Connect failed"), "communication.error");
    expect(moduleContains(root, "communication", "[logger_test.cpp]"), "communication.filename");

    expect(moduleContains(root, "business", "User login"), "business.info");
    expect(moduleContains(root, "business", "Business fatal"), "business.critical");

    const std::string errors = readModuleLogs(root, "error");
    expect(errors.find("Connect failed") != std::string::npos, "error.collect_comm");
    expect(errors.find("Business fatal") != std::string::npos, "error.collect_business");
    expect(errors.find("Server started") == std::string::npos, "error.no_info");
    expect(errors.find("[ERROR]") != std::string::npos, "error.level_name");
    expect(errors.find("[CRITICAL]") != std::string::npos, "error.critical_name");
}

void testDebugSkippedWhenInfo()
{
    const std::string dir = makeDir("level");
    auto config = baseConfig(dir);
    expect(itflee::Logger::init(config), "level: init");

    LOG_DEBUG("DEBUG_SHOULD_NOT_APPEAR");
    LOG_INFO("INFO_SHOULD_APPEAR");
    itflee::Logger::setLevel(itflee::LogLevel::Debug);
    LOG_DEBUG("DEBUG_AFTER_SET_LEVEL");
    itflee::Logger::shutdown();

    expect(!moduleContains(dir, "application", "DEBUG_SHOULD_NOT_APPEAR"), "level.debug_filtered");
    expect(moduleContains(dir, "application", "INFO_SHOULD_APPEAR"), "level.info_present");
    expect(moduleContains(dir, "application", "DEBUG_AFTER_SET_LEVEL"), "level.debug_after_set");
}

void testRotation()
{
    const std::string dir = makeDir("rotate");
    auto config = baseConfig(dir);
    config.max_size = 1024;
    config.max_files = 3;
    config.modules = {{"communication", "communication.log", {}}};
    config.default_module = "communication";
    expect(itflee::Logger::init(config), "rotate: init");

    const std::string payload(80, 'x');
    for (int i = 0; i < 80; ++i) {
        LOG_INFO("rotate {} {}", i, payload);
    }
    itflee::Logger::shutdown();

    const auto files = listModuleLogs(dir, "communication");
    expect(files.size() >= 2, "rotate.multiple");
    expect(files.size() <= 4, "rotate.pruned");
    expect(!fs::exists(fs::path(dir) / "communication.log"), "rotate.no_plain_name");
    expect(!fs::exists(fs::path(dir) / "communication.1.log"), "rotate.no_index_name");
    for (const auto& path : files) {
        const std::string name = path.filename().string();
        expect(name.rfind("communication_", 0) == 0, "rotate.prefix");
        expect(name.find("_20") != std::string::npos, "rotate.has_date");
    }
}

void testAsyncShutdownFlushes()
{
    const std::string dir = makeDir("async");
    auto config = baseConfig(dir);
    config.async = true;
    config.modules = {{"application", "application.log", {}}};
    expect(itflee::Logger::init(config), "async: init");

    constexpr int kCount = 500;
    for (int i = 0; i < kCount; ++i) {
        LOG_INFO("async-line-{}", i);
    }
    itflee::Logger::shutdown();

    const std::string body = readModuleLogs(dir, "application");
    expect(countLines(body) >= static_cast<std::size_t>(kCount), "async.flush_all");
    expect(body.find("async-line-0") != std::string::npos, "async.first");
    expect(body.find("async-line-499") != std::string::npos, "async.last");
}

void testConcurrent()
{
    const std::string dir = makeDir("conc");
    auto config = baseConfig(dir);
    config.async = true;
    config.modules = {{"application", "application.log", {}}};
    expect(itflee::Logger::init(config), "conc: init");

    constexpr int kThreads = 4;
    constexpr int kEach = 200;
    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([t, kEach]() {
            for (int i = 0; i < kEach; ++i) {
                LOG_INFO("thread-{} line-{}", t, i);
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    itflee::Logger::shutdown();

    const std::string body = readModuleLogs(dir, "application");
    expect(countLines(body) >= static_cast<std::size_t>(kThreads * kEach), "conc.line_count");
    expect(body.find("thread-0 line-0") != std::string::npos, "conc.sample0");
    expect(body.find("thread-3 line-199") != std::string::npos, "conc.sample3");
}

void testSourceLocationAndThread()
{
    const std::string dir = makeDir("src");
    auto config = baseConfig(dir);
    config.source_location = itflee::SourceLocationMode::On;
    config.modules = {{"application", "application.log", {}}};
    expect(itflee::Logger::init(config), "src: init");
    LOG_INFO("with-source");
    itflee::Logger::shutdown();

    const std::string body = readModuleLogs(dir, "application");
    expect(body.find("with-source") != std::string::npos, "src.message");
    expect(body.find("logger_test.cpp:") != std::string::npos, "src.file_line");
    expect(body.find("[thread:") != std::string::npos, "src.thread");
}

void testUnknownModuleFallback()
{
    const std::string dir = makeDir("unknown");
    auto config = baseConfig(dir);
    expect(itflee::Logger::init(config), "unknown: init");
    LOG_M_INFO("not_registered", "fallback-message");
    itflee::Logger::shutdown();

    expect(moduleContains(dir, "application", "fallback-message"), "unknown.fallback");
    expect(moduleContains(dir, "application", "Unknown logger module"), "unknown.warn");
}

void testExceptionMacro()
{
    const std::string dir = makeDir("exc");
    auto config = baseConfig(dir);
    expect(itflee::Logger::init(config), "exc: init");
    try {
        throw std::runtime_error("boom");
    } catch (const std::exception& e) {
        LOG_EXCEPTION(e);
    }
    itflee::Logger::shutdown();
    expect(moduleContains(dir, "application", "Exception: boom"), "exc.text");
    expect(moduleContains(dir, "error", "Exception: boom"), "exc.error_file");
}

void testPerModuleConsole()
{
    const std::string dir = makeDir("modcfg");
    auto config = baseConfig(dir);
    config.console = true;
    config.console_level = itflee::LogLevel::Info;
    config.level = itflee::LogLevel::Warn;
    config.modules = {
        {"application", "application.log", {}, true, itflee::LogLevel::Debug},
        {"communication", "communication.log", {}, false},
        {"business", "business.log", {}},
    };
    expect(itflee::Logger::init(config), "modcfg: init");

    const auto app = itflee::Logger::get("application");
    const auto comm = itflee::Logger::get("communication");
    const auto biz = itflee::Logger::get("business");
    expect(app.shouldLog(itflee::LogLevel::Debug), "modcfg.app_debug_via_console");
    expect(!comm.shouldLog(itflee::LogLevel::Debug), "modcfg.comm_no_console_debug");
    expect(comm.shouldLog(itflee::LogLevel::Warn), "modcfg.comm_file_warn");
    expect(!biz.shouldLog(itflee::LogLevel::Debug), "modcfg.biz_console_info_skips_debug");
    expect(biz.shouldLog(itflee::LogLevel::Info), "modcfg.biz_console_info");

    LOG_M_DEBUG("application", "APP_DEBUG");
    LOG_M_DEBUG("communication", "COMM_DEBUG");
    LOG_M_DEBUG("business", "BIZ_DEBUG");
    LOG_M_WARN("communication", "COMM_WARN");
    itflee::Logger::shutdown();

    expect(!moduleContains(dir, "application", "APP_DEBUG"), "modcfg.app_debug_not_in_file");
    expect(!moduleContains(dir, "communication", "COMM_DEBUG"), "modcfg.comm_debug_absent");
    expect(moduleContains(dir, "communication", "COMM_WARN"), "modcfg.comm_warn_in_file");
    expect(!moduleContains(dir, "business", "BIZ_DEBUG"), "modcfg.biz_debug_absent");
}

void testShutdownThenLogGoesToStderr()
{
    const std::string dir = makeDir("after_sd");
    auto config = baseConfig(dir);
    config.modules = {{"application", "application.log", {}}};
    expect(itflee::Logger::init(config), "after_sd: init");

    std::vector<std::thread> threads;
    threads.reserve(2);
    for (int t = 0; t < 2; ++t) {
        threads.emplace_back([t]() {
            for (int i = 0; i < 50; ++i) {
                LOG_INFO("pre-shutdown t{} i{}", t, i);
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    itflee::Logger::shutdown();
    expect(!itflee::Logger::isInitialized(), "after_sd: not initialized");

    LOG_INFO("AFTER_SHUTDOWN_MUST_NOT_BE_IN_FILE");
    LOG_WARN("after-shutdown-visible-on-stderr");

    expect(moduleContains(dir, "application", "pre-shutdown t0 i0"), "after_sd.pre_kept");
    expect(moduleContains(dir, "application", "pre-shutdown t1 i49"), "after_sd.pre_last");
    expect(!moduleContains(dir, "application", "AFTER_SHUTDOWN_MUST_NOT_BE_IN_FILE"),
           "after_sd.post_not_in_file");
}

void testSmallAsyncQueueStillFlushes()
{
    const std::string dir = makeDir("q64");
    auto config = baseConfig(dir);
    config.async = true;
    config.async_queue_size = 64;
    config.async_thread_count = 1;
    config.flush_level = itflee::LogLevel::Critical;
    config.modules = {{"application", "application.log", {}}};
    expect(itflee::Logger::init(config), "q64: init");

    constexpr int kCount = 4000;
    for (int i = 0; i < kCount; ++i) {
        LOG_INFO("q64-line-{}", i);
    }
    itflee::Logger::shutdown();

    const std::string body = readModuleLogs(dir, "application");
    expect(countLines(body) >= static_cast<std::size_t>(kCount), "q64.flush_all");
    expect(body.find("q64-line-0") != std::string::npos, "q64.first");
    expect(body.find("q64-line-3999") != std::string::npos, "q64.last");
}

void runCase(const char* name, void (*fn)())
{
    g_current_case = name;
    if (g_current == nullptr) {
        fn();
        return;
    }
    g_current->cases.push_back(CaseResult{});
    g_live_case = &g_current->cases.back();
    g_live_case->name = name;
    const int fail_before = g_failed;
    const int pass_before = g_passed;
    const auto t0 = Clock::now();
    fn();
    g_live_case->ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
    g_live_case->passed = g_passed - pass_before;
    g_live_case->failed = g_failed - fail_before;
    g_live_case->ok = g_live_case->failed == 0;
    g_live_case = nullptr;
}

PerfResult benchWrite(const char* scenario, bool async, int threads, int each, const char* note)
{
    PerfResult r;
    r.scenario = scenario;
    r.backend = backendTag(g_backend);
    r.async = async;
    r.threads = threads;
    r.count = threads * each;
    r.note = note;

    const std::string dir = makeDir(std::string("perf_") + scenario);
    auto config = baseConfig(dir);
    config.async = async;
    config.flush_level = itflee::LogLevel::Critical;
    config.modules = {{"application", "application.log", {}}};
    config.default_module = "application";
    if (!itflee::Logger::init(config)) {
        r.note = "init 失败";
        return r;
    }

    for (int i = 0; i < 200; ++i) {
        LOG_INFO("warmup {}", i);
    }
    itflee::Logger::flush();

    const auto t_call0 = Clock::now();
    if (threads <= 1) {
        for (int i = 0; i < each; ++i) {
            LOG_INFO("perf-line-{}", i);
        }
    } else {
        std::vector<std::thread> workers;
        workers.reserve(static_cast<std::size_t>(threads));
        for (int t = 0; t < threads; ++t) {
            workers.emplace_back([t, each]() {
                for (int i = 0; i < each; ++i) {
                    LOG_INFO("perf-t{}-{}", t, i);
                }
            });
        }
        for (auto& th : workers) {
            th.join();
        }
    }
    const auto t_call1 = Clock::now();
    itflee::Logger::shutdown();
    const auto t_end = Clock::now();

    r.call_ms = std::chrono::duration<double, std::milli>(t_call1 - t_call0).count();
    r.shutdown_ms = std::chrono::duration<double, std::milli>(t_end - t_call1).count();
    r.total_ms = std::chrono::duration<double, std::milli>(t_end - t_call0).count();
    if (r.call_ms > 0) {
        r.enqueue_per_sec = static_cast<double>(r.count) * 1000.0 / r.call_ms;
        r.ns_per_call = r.call_ms * 1.0e6 / static_cast<double>(r.count);
    }
    if (r.total_ms > 0) {
        r.durable_per_sec = static_cast<double>(r.count) * 1000.0 / r.total_ms;
    }
    r.bytes = dirBytes(dir);
    const auto lines = countLines(readModuleLogs(dir, "application"));
    r.data_ok = lines >= static_cast<std::size_t>(r.count);
    expect(r.data_ok, std::string(scenario) + ": 落盘行数不足");
    return r;
}

PerfResult benchFilteredDebug(int count)
{
    PerfResult r;
    r.scenario = "filtered_debug";
    r.backend = backendTag(g_backend);
    r.async = false;
    r.threads = 1;
    r.count = count;
    r.note = "全局 Info，打 Debug；应跳过格式化与落盘";

    const std::string dir = makeDir("perf_filter");
    auto config = baseConfig(dir);
    config.level = itflee::LogLevel::Info;
    config.modules = {{"application", "application.log", {}}};
    if (!itflee::Logger::init(config)) {
        r.note = "init 失败";
        return r;
    }

    for (int i = 0; i < 100; ++i) {
        LOG_DEBUG("warmup-debug {}", i);
    }

    const auto t0 = Clock::now();
    for (int i = 0; i < count; ++i) {
        LOG_DEBUG("filtered-{}", i);
    }
    const auto t1 = Clock::now();
    itflee::Logger::shutdown();
    const auto t2 = Clock::now();

    r.call_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    r.shutdown_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
    r.total_ms = std::chrono::duration<double, std::milli>(t2 - t0).count();
    if (r.call_ms > 0) {
        r.enqueue_per_sec = static_cast<double>(r.count) * 1000.0 / r.call_ms;
        r.ns_per_call = r.call_ms * 1.0e6 / static_cast<double>(r.count);
    }
    r.durable_per_sec = r.enqueue_per_sec;
    r.bytes = dirBytes(dir);
    r.data_ok = !moduleContains(dir, "application", "filtered-0");
    expect(r.data_ok, "filtered_debug: Debug 不应落盘");
    return r;
}

void runSuite(itflee::LogBackendKind backend)
{
    g_backend = backend;
    BackendSection section;
    section.backend = backendTag(backend);
    g_sections.push_back(std::move(section));
    g_current = &g_sections.back();

    std::cout << "\n======== 功能测试  backend=" << g_current->backend << " ========\n";
    runCase("未初始化写 stderr", testUninitWritesStderr);
    runCase("模块分流与 error 汇聚", testModuleFilesAndErrorSink);
    runCase("Info 级别过滤 Debug", testDebugSkippedWhenInfo);
    runCase("按大小滚动与时间戳文件名", testRotation);
    runCase("异步 shutdown 刷空", testAsyncShutdownFlushes);
    runCase("四线程并发写", testConcurrent);
    runCase("源文件名与线程号", testSourceLocationAndThread);
    runCase("未知模块回退", testUnknownModuleFallback);
    runCase("LOG_EXCEPTION", testExceptionMacro);
    runCase("按模块覆盖控制台", testPerModuleConsole);
    runCase("shutdown 后再打日志走 stderr", testShutdownThenLogGoesToStderr);
    runCase("小异步队列仍完整落盘", testSmallAsyncQueueStillFlushes);

    std::cout << "======== 性能测试  backend=" << g_current->backend << " ========\n";
    g_current_case = "perf";
    g_current->perfs.push_back(
        benchWrite("sync_20k", false, 1, 20000, "同步单线程 INFO，含格式化与写盘"));
    g_current->perfs.push_back(
        benchWrite("async_50k", true, 1, 50000, "异步单线程 INFO；调用耗时=入队，合计含 shutdown 刷盘"));
    g_current->perfs.push_back(
        benchWrite("async_4x10k", true, 4, 10000, "异步 4 线程各 1 万条"));
    g_current->perfs.push_back(benchFilteredDebug(200000));
}

std::string nowLocal()
{
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm) == 0) {
        return {};
    }
    return buf;
}

std::string fmtNum(double v, int prec)
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(prec) << v;
    return ss.str();
}

std::string fmtInt(double v)
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(0) << v;
    return ss.str();
}

std::string buildMode()
{
#ifdef NDEBUG
    return "Release";
#else
    return "Debug";
#endif
}

std::string compilerId()
{
#ifdef _MSC_VER
    return "MSVC " + std::to_string(_MSC_VER);
#elif defined(__clang__)
    return "Clang";
#elif defined(__GNUC__)
    return "GCC";
#else
    return "unknown";
#endif
}

const PerfResult* findPerf(const BackendSection& sec, const std::string& scenario)
{
    for (const auto& p : sec.perfs) {
        if (p.scenario == scenario) {
            return &p;
        }
    }
    return nullptr;
}

std::string compareSpeed(double a, double b)
{
    if (a <= 0 || b <= 0) {
        return "-";
    }
    if (a >= b) {
        return std::string("spdlog 约为 Boost.Log 的 ") + fmtNum(a / b, 2) + " 倍";
    }
    return std::string("Boost.Log 约为 spdlog 的 ") + fmtNum(b / a, 2) + " 倍";
}

std::string renderReport()
{
    std::ostringstream out;
    out << "# Logger V2 测试报告\n\n";
    out << "- 时间：" << nowLocal() << "\n";
#ifdef _WIN32
    out << "- 平台：Windows\n";
#else
    out << "- 平台：POSIX\n";
#endif
    out << "- 构建：" << buildMode() << "（" << compilerId() << "）\n";
    out << "- 硬件并发：" << std::thread::hardware_concurrency() << "\n";
    out << "- 断言合计：通过 " << g_passed << "，失败 " << g_failed << "\n";
    out << "- 功能覆盖：模块分流、error 汇聚、级别过滤、时间戳滚动、异步 flush、并发、"
           "shutdown 后再 LOG、小异步队列落盘、源位置、未知模块、异常宏、按模块控制台\n";
    out << "- 性能口径：调用耗时 = 最后一条 LOG_* 返回；shutdown 耗时 = 刷盘并释放；"
           "入队吞吐按调用耗时，落盘吞吐按调用+shutdown\n\n";

    out << "## 1. 功能测试\n\n";
    for (const auto& sec : g_sections) {
        int ok = 0;
        for (const auto& c : sec.cases) {
            if (c.ok) {
                ++ok;
            }
        }
        out << "### " << sec.backend << "（" << ok << "/" << sec.cases.size() << " 用例通过）\n\n";
        out << "| 用例 | 结果 | 断言 | 耗时 |\n";
        out << "| --- | --- | --- | --- |\n";
        for (const auto& c : sec.cases) {
            out << "| " << c.name << " | " << (c.ok ? "通过" : "失败") << " | " << c.passed << "/"
                << (c.passed + c.failed) << " | " << fmtNum(c.ms, 1) << " ms |\n";
        }
        out << "\n";
        for (const auto& c : sec.cases) {
            if (!c.ok) {
                out << "- **" << c.name << "** 失败项：";
                for (std::size_t i = 0; i < c.failures.size(); ++i) {
                    if (i != 0) {
                        out << "；";
                    }
                    out << '`' << c.failures[i] << '`';
                }
                out << "\n";
            }
        }
    }

    out << "## 2. 性能\n\n";
    out << "写盘场景正文为短 INFO；控制台关闭；`flush_level=Critical`，避免每条 WARN 同步刷盘。\n";
    out << buildMode()
        << " 构建下的绝对值会明显慢于 Release，**对比两个后端时请看同一行的相对倍数**。\n\n";

    out << "| 场景 | 后端 | 条数 | 调用耗时 | shutdown | 入队吞吐 | 落盘吞吐 | 单次调用 | 落盘体积 | 数据完整 |\n";
    out << "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |\n";
    for (const auto& sec : g_sections) {
        for (const auto& p : sec.perfs) {
            out << "| " << p.scenario << " | " << p.backend << " | " << p.count << " | "
                << fmtNum(p.call_ms, 1) << " ms | " << fmtNum(p.shutdown_ms, 1) << " ms | "
                << fmtInt(p.enqueue_per_sec) << " /s | " << fmtInt(p.durable_per_sec) << " /s | "
                << fmtInt(p.ns_per_call) << " ns | " << p.bytes << " B | "
                << (p.data_ok ? "是" : "否") << " |\n";
        }
    }
    out << "\n";

    for (const auto& sec : g_sections) {
        out << "### " << sec.backend << " 场景说明\n\n";
        for (const auto& p : sec.perfs) {
            out << "- `" << p.scenario << "`：" << p.note << "\n";
        }
        out << "\n";
    }

    if (g_sections.size() >= 2) {
        out << "## 3. 双后端对比（入队吞吐）\n\n";
        out << "| 场景 | spdlog | Boost.Log | 相对 |\n";
        out << "| --- | --- | --- | --- |\n";
        const char* scenarios[] = {"sync_20k", "async_50k", "async_4x10k", "filtered_debug"};
        for (const char* sc : scenarios) {
            const PerfResult* a = findPerf(g_sections[0], sc);
            const PerfResult* b = findPerf(g_sections[1], sc);
            if (a == nullptr || b == nullptr) {
                continue;
            }
            const bool spd_first = g_sections[0].backend == "spdlog";
            const PerfResult* spd = spd_first ? a : b;
            const PerfResult* bst = spd_first ? b : a;
            out << "| " << sc << " | " << fmtInt(spd->enqueue_per_sec) << " /s | "
                << fmtInt(bst->enqueue_per_sec) << " /s | "
                << compareSpeed(spd->enqueue_per_sec, bst->enqueue_per_sec) << " |\n";
        }
        out << "\n";
    }

    out << "## 4. 结论\n\n";
    if (g_failed == 0) {
        out << "- 功能：spdlog 与 Boost.Log 均通过分流、error 汇聚、滚动文件名、异步刷盘与并发用例。\n";
    } else {
        out << "- 功能：**存在失败**，见上表与 FAIL 日志。\n";
        for (const auto& line : g_fail_details) {
            out << "  - " << line << "\n";
        }
    }
    out << "- 热路径：`filtered_debug` 表示级别不够时只做 shouldLog，不应产生文件内容。\n";
    out << "- 异步：入队吞吐通常高于同步；真正落盘看「落盘吞吐」和 shutdown 耗时。\n";
    out << "- Boost.Log 异步为每 sink 一条投递线程、队列 8192 满则阻塞；spdlog 为共享线程池。\n";
    return out.str();
}

void writeReportFile(const std::string& text, const fs::path& path)
{
    fs::create_directories(path.parent_path().empty() ? fs::path(".") : path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::cerr << "无法写入报告: " << path.string() << '\n';
        return;
    }
    const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    out.write(reinterpret_cast<const char*>(bom), 3);
    out << text;
    std::cout << "报告已写入: " << fs::absolute(path).string() << '\n';
}

}  // namespace

int main(int argc, char** argv)
{
    fs::remove_all("logger_test_output");

    runSuite(itflee::LogBackendKind::Spdlog);
    runSuite(itflee::LogBackendKind::BoostLog);

    const std::string report = renderReport();
    std::cout << '\n' << report;

    std::vector<fs::path> outputs;
    outputs.emplace_back("logger_test_report.md");
#ifdef LOGGER_TEST_REPORT_PATH
    outputs.emplace_back(LOGGER_TEST_REPORT_PATH);
#endif
    if (argc >= 2) {
        outputs.emplace_back(argv[1]);
    }
    for (const auto& path : outputs) {
        writeReportFile(report, path);
    }

    if (g_failed == 0) {
        fs::remove_all("logger_test_output");
    }

    return g_failed == 0 ? 0 : 1;
}
