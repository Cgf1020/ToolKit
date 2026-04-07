cmake_minimum_required(VERSION 3.16)

project(libhv_examples LANGUAGES CXX)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# 第三方 libhv（跨平台）：头文件 libhv/include，动态库在 libhv/bin 下按 debug/release 区分
# 目录：bin/debug/、bin/release/；库名：Windows hv.lib，macOS libhv.dylib，Linux 等 libhv.so
set(LIBHV_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(LIBHV_INCLUDE_DIR "${LIBHV_ROOT}/include")
set(LIBHV_BIN_DIR "${LIBHV_ROOT}/bin")

# 按配置选择子目录：Debug -> debug，其余（Release/RelWithDebInfo/MinSizeRel/空）-> release
set(LIBHV_SUBDIR "$<IF:$<STREQUAL:$<CONFIG>,Debug>,debug,release>")
# 库文件名按平台：Windows 导入库 hv.lib，macOS 动态库 libhv.dylib，Linux 等 libhv.so
set(LIBHV_LIB_NAME "$<IF:$<PLATFORM_ID:Windows>,hv.lib,$<IF:$<PLATFORM_ID:Darwin>,libhv.dylib,libhv.so>>")
# 库文件完整路径（生成器表达式，构建时按配置与平台展开）
set(LIBHV_LIBRARY_PATH "${LIBHV_BIN_DIR}/${LIBHV_SUBDIR}/${LIBHV_LIB_NAME}")

message(STATUS "libhv include: ${LIBHV_INCLUDE_DIR}")
message(STATUS "libhv library: bin/<debug|release>/<platform-specific>")

 # 客户端示例
add_executable(tcp_client_test
    tcp_client_test.cpp
)

# 服务端示例
add_executable(tcp_server_test
    tcp_server_test.cpp
)

# 定时器示例
add_executable(timer_example
    timer_example.cpp
)

# 队列 + 线程池 示例
add_executable(queue_threadpool_example
    queue_threadpool_example.cpp
)
set_target_properties(queue_threadpool_example PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)

foreach(target tcp_client_test tcp_server_test timer_example queue_threadpool_example)
    target_include_directories(${target} PRIVATE ${LIBHV_INCLUDE_DIR})
    # 用变量直接链接库文件路径（按 Debug/Release 选 bin/debug 或 bin/release）
    target_link_libraries(${target} PRIVATE ${LIBHV_LIBRARY_PATH} Threads::Threads)

    if(MSVC)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()

    if(COMMAND set_common_output_dir)
        set_common_output_dir(${target})
    endif()
endforeach()

