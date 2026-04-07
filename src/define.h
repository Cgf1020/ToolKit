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


#ifdef _WIN32

    #define ITFLEEEXPORT __declspec(dllexport) // DLL 内部：导出符号
    // #define ITFLEEEXPORT __declspec(dllimport)  // 客户端：导入符号
    
#else
    #define ITFLEEEXPORT __attribute__((visibility("default")))
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