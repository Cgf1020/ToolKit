#pragma once
#include <string>
#include <sstream>

#include <boost/log/trivial.hpp>

namespace Utils {
    class LogHelper {
    public:
        enum LogLevel { Trace, Debug, Info, Warning, Error };

    public:
        static void Initialize(const std::string& logFile = "log.txt");
        static void EnableConsoleLogging(bool enable);
        static void Log(const std::string& message, LogLevel level = LogLevel::Info);
        static void UnInitialize();

        template <typename... Args> static void Log(const std::string& format, Args... args) {
            std::ostringstream oss;
            FormatAndLog(oss, format, args...);
            BOOST_LOG_TRIVIAL(info) << oss.str();
        }

    private:
        static bool logToConsole;
        template <typename T> static void FormatAndLog(std::ostringstream& oss, const std::string& format, const T& value) {
            size_t pos = format.find("{}");
            if (pos != std::string::npos) {
                oss << format.substr(0, pos) << value;
                FormatAndLog(oss, format.substr(pos + 2), value);
            } else {
                oss << format;
            }
        }

        template <typename T, typename... Args> static void FormatAndLog(std::ostringstream& oss, const std::string& format, const T& value, Args... args) {
            size_t pos = format.find("{}");
            if (pos != std::string::npos) {
                oss << format.substr(0, pos) << value;
                FormatAndLog(oss, format.substr(pos + 2), args...);
            } else {
                oss << format;
            }
        }
    };
} // namespace Utils
