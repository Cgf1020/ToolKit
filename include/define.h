#ifndef DEFINE_H_
#define DEFINE_H_


#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <thread>
#include <variant>
#include <chrono>


// =========================
// 库导出/导入宏（ITFLEEEXPORT）
// 需要配合编译定义：
// 1) 构建动态库本体：ITFLEE_BUILDING_LIBRARY
// 2) 使用动态库：不定义上述宏（Windows 自动 dllimport）
// 3) 构建或使用静态库：ITFLEE_STATIC_LIBRARY
// =========================
#if defined(ITFLEE_STATIC_LIBRARY)
    #define ITFLEEEXPORT
#elif defined(_WIN32)
    #if defined(ITFLEE_BUILDING_LIBRARY)
        #define ITFLEEEXPORT __declspec(dllexport)
    #else
        #define ITFLEEEXPORT __declspec(dllimport)
    #endif
#else
    #if __GNUC__ >= 4
        #define ITFLEEEXPORT __attribute__((visibility("default")))
    #else
        #define ITFLEEEXPORT
    #endif
#endif




// 区分是debug还是release
#ifdef DEBUG
// #define JSON_DEBUG_LOG   // 打印JSON的调试日志
#endif

#ifndef NDEBUG

#endif


// 区分平台 windows linux macos; linux 区分 arm64 x86_64
#ifdef _WIN32
    #ifndef _WINDOWS
        #define _WINDOWS
    #endif
#endif

#ifdef __linux__
#define _LINUX
#ifdef __arm64__
#define _LINUX_ARM64
#endif
#ifdef __x86_64__
#define _LINUX_X86_64
#endif
#endif

#ifdef __APPLE__
#define _MACOS
#endif



#endif //ALVADEFINE_H_