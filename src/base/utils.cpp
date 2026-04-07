#include "define.h"
#include "base/utils.h"

#ifdef _WIN32
#include <Windows.h>
#include <Shellapi.h>
#include <TlHelp32.h>
#pragma warning(disable : 4244)
#else
#include <csignal>
#include <filesystem>
#include <fstream>
#include <limits.h>
#include <unistd.h>
#endif

#include <cctype>
#include <fstream>
#include <random>

namespace utils {

std::wstring s2ws(const std::string& str) {
#ifdef _WIN32
    if (str.empty()) {
        return std::wstring();
    }
    const int wide_len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (wide_len <= 0) {
        return std::wstring(str.begin(), str.end());
    }
    std::wstring out(static_cast<size_t>(wide_len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, out.data(), wide_len);
    if (!out.empty() && out.back() == L'\0') {
        out.pop_back();
    }
    return out;
#else
    return std::wstring(str.begin(), str.end());
#endif
}

std::string ws2s(const std::wstring& s) {
#ifdef _WIN32
    if (s.empty()) {
        return std::string();
    }
    const int utf8_len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8_len <= 0) {
        return std::string(s.begin(), s.end());
    }
    std::string out(static_cast<size_t>(utf8_len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, out.data(), utf8_len, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0') {
        out.pop_back();
    }
    return out;
#else
    return std::string(s.begin(), s.end());
#endif
}

std::string generateRandomString(std::size_t length) {
    static const char chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::string result;
    std::random_device randomDevice;
    std::default_random_engine random(randomDevice());
    for (std::size_t i = 0; i < length; i++) {
        result += chars[random() % 36];
    }
    return result;
}

std::string GetModulePath() {
#ifdef _WIN32
    wchar_t work_path[_MAX_PATH] = {0};
    GetModuleFileName(nullptr, work_path, _MAX_PATH);
    std::string path = ws2s(work_path);
    const auto pos = path.find_last_of('\\');
    if (pos == std::string::npos) {
        return "";
    }
    return path.substr(0, pos + 1);
#elif defined(__linux__)
    char work_path[PATH_MAX] = {0};
    const ssize_t len = ::readlink("/proc/self/exe", work_path, sizeof(work_path) - 1);
    if (len <= 0) {
        return "";
    }
    std::string path(work_path, static_cast<size_t>(len));
    const auto pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        return "";
    }
    return path.substr(0, pos + 1);
#else
    return std::filesystem::current_path().string() + "/";
#endif
}

uint32_t GetProcessIdByName(const std::string& processName) {
#ifdef _WIN32
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }
    if (!Process32First(snapshot, &entry)) {
        CloseHandle(snapshot);
        return 0;
    }
    do {
        std::wstring currentProcessName = entry.szExeFile;
        if (currentProcessName == s2ws(processName)) {
            CloseHandle(snapshot);
            return entry.th32ProcessID;
        }
    } while (Process32Next(snapshot, &entry));
    CloseHandle(snapshot);
    return 0;
#elif defined(__linux__)
    namespace fs = std::filesystem;
    for (const auto& entry : fs::directory_iterator("/proc")) {
        if (!entry.is_directory()) {
            continue;
        }
        const auto pid_str = entry.path().filename().string();
        if (pid_str.empty() || !std::isdigit(static_cast<unsigned char>(pid_str[0]))) {
            continue;
        }
        std::ifstream comm_file(entry.path() / "comm");
        if (!comm_file.is_open()) {
            continue;
        }
        std::string comm;
        std::getline(comm_file, comm);
        if (comm == processName) {
            return static_cast<uint32_t>(std::stoul(pid_str));
        }
    }
    return 0;
#else
    (void)processName;
    return 0;
#endif
}

bool KillProcess(uint32_t processID) {
#ifdef _WIN32
    HANDLE killHandle = OpenProcess(
        PROCESS_TERMINATE | PROCESS_ALL_ACCESS | PROCESS_QUERY_INFORMATION |
            PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE,
        FALSE,
        processID);
    if (killHandle == nullptr) {
        return false;
    }
    const BOOL ok = TerminateProcess(killHandle, 0);
    CloseHandle(killHandle);
    return ok != 0;
#elif defined(__linux__)
    if (processID == 0) {
        return false;
    }
    return ::kill(static_cast<pid_t>(processID), SIGTERM) == 0;
#else
    (void)processID;
    return false;
#endif
}

std::string base64_encode(const char* bytes_to_encode, unsigned int in_len) {
    const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            for (i = 0; i < 4; i++) {
                ret += base64_chars[char_array_4[i]];
            }
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++) {
            char_array_3[j] = '\0';
        }
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;
        for (j = 0; j < i + 1; j++) {
            ret += base64_chars[char_array_4[j]];
        }
        while (i++ < 3) {
            ret += '=';
        }
    }
    return ret;
}

static inline bool is_base64(unsigned char c) {
    return (std::isalnum(c) || (c == '+') || (c == '/'));
}

std::string base64_decode(const std::string& encoded_string) {
    const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    size_t in_len = encoded_string.size();
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::string ret;

    while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
        char_array_4[i++] = encoded_string[in_];
        in_++;
        if (i == 4) {
            for (i = 0; i < 4; i++) {
                char_array_4[i] = static_cast<unsigned char>(base64_chars.find(char_array_4[i]) & 0xff);
            }
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            for (i = 0; i < 3; i++) {
                ret += static_cast<char>(char_array_3[i]);
            }
            i = 0;
        }
    }

    if (i) {
        for (j = 0; j < i; j++) {
            char_array_4[j] = static_cast<unsigned char>(base64_chars.find(char_array_4[j]) & 0xff);
        }
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        for (j = 0; j < i - 1; j++) {
            ret += static_cast<char>(char_array_3[j]);
        }
    }
    return ret;
}

std::string jpegToBase64(const std::string& filename) {
    std::ifstream is(filename, std::ios::in | std::ios::binary);
    if (!is.is_open()) {
        return "";
    }
    is.seekg(0, is.end);
    const int length = static_cast<int>(is.tellg());
    is.seekg(0, is.beg);
    if (length <= 0) {
        return "";
    }
    std::string buffer(static_cast<size_t>(length), '\0');
    is.read(buffer.data(), length);
    return base64_encode(buffer.data(), static_cast<unsigned int>(length));
}

std::string imgToBase64(const char* bytes_to_encode, unsigned int in_len, std::string img_base64_header) {
    if (!bytes_to_encode || in_len == 0) {
        return "";
    }
    std::string imgBase64 = base64_encode(bytes_to_encode, in_len);
    if (!imgBase64.empty() && !img_base64_header.empty()) {
        imgBase64 = img_base64_header + imgBase64;
    }
    return imgBase64;
}

} // namespace utils

namespace tempfun {

void musicffplay() {
#ifdef _WIN32
    std::wstring exeName = L"E:\\alva_work_dir\\resource\\RainbowSDK\\x64\\Release\\ffplay.exe";
    std::wstring parma = L"-nodisp -i E:\\alva_work_dir\\resource\\RainbowSDK\\x64\\Release\\test.mp3";

    HINSTANCE hInstance = ShellExecute(0, 0, exeName.c_str(), parma.c_str(), 0, SW_HIDE);
    if (hInstance < (HANDLE)32) {
        std::cout << "failed " << std::endl;
    }
    Sleep(5000);
    DWORD id = utils::GetProcessIdByName("ffplay.exe");
    utils::KillProcess(id);
#endif
}

constexpr const std::string_view testConstexprFun() {
    return "";
}

} // namespace tempfun

