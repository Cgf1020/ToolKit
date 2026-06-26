#include "time/time_helper.h"

#include <iomanip>
#include <ctime>

#ifdef  _WIN32
    #include <Windows.h>
    #include <TlHelp32.h>       //GetProcessIdByName
    #pragma warning(disable:4244)       // suppress C4244 for wstring-to-string conversion
#endif //  _WINDOWS


#include <random>       //generateRandomString
#include <chrono>
#include <limits>



namespace utils
{
    int64_t Time64()
    {
        auto now = std::chrono::system_clock::now();
        std::chrono::milliseconds timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
        uint64_t timestampMillis = timestamp.count();

        return timestampMillis;
    }

    int32_t Time32()
    {      
        const int64_t t64 = Time64();
        if (t64 > static_cast<int64_t>(std::numeric_limits<int32_t>::max())) {
            return std::numeric_limits<int32_t>::max();
        }
        if (t64 < static_cast<int64_t>(std::numeric_limits<int32_t>::min())) {
            return std::numeric_limits<int32_t>::min();
        }
        return static_cast<int32_t>(t64);
    }

    std::string GetCurrentFormattedTime()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
        std::tm localTime;
#ifdef _WIN32
        // Windows 平台（VS 等编译器）
        localtime_s(&localTime, &currentTime);
#else
        // Linux/macOS 等类 Unix 平台（GCC、Clang 等编译器）
        localtime_r(&currentTime, &localTime);
#endif

        std::ostringstream oss;
        oss << std::put_time(&localTime, "%Y:%m:%d-%H:%M:%S");

        return  oss.str();
    }


  
}
