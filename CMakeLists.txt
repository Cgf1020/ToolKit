cmake_minimum_required(VERSION 3.16)
project(ToolKit VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # 确保使用C++17标准


# 设置默认构建类型（单配置生成器，如 Unix Makefiles）
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# 多配置生成器（如 Visual Studio）：配置列表，Release 放在首位
if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Release;Debug;RelWithDebInfo;MinSizeRel" CACHE STRING "Available build types" FORCE)
endif()

# 设置库类型选项 
# option(BUILD_SHARED_LIBS "Build shared libraries" ON)
# set(LIBRARY_TYPE ${BUILD_SHARED_LIBS})
# 全局设置：默认生成动态库
set(BUILD_SHARED_LIBS ON)

#-----------------------------功能模块选项----------------------------------#
# 是否启用 WebSocket 模块
# 注意：如果缓存中已有值，需要删除 build/CMakeCache.txt 或使用命令行 -DENABLE_WEBSOCKET=OFF 来覆盖
option(ENABLE_WEBSOCKET "Enable WebSocket module (requires Boost Beast and OpenSSL)" OFF)
if(ENABLE_WEBSOCKET)
    message(STATUS "WebSocket module: ENABLED")
else()
    message(STATUS "WebSocket module: DISABLED")
endif()

#-----------------------------添加编译的源文件----------------------------------#
file(GLOB_RECURSE SRC_FILES 
    ${PROJECT_SOURCE_DIR}/src/base/json/json_helper.cpp
    ${PROJECT_SOURCE_DIR}/src/base/log/log_helper.cpp
    ${PROJECT_SOURCE_DIR}/src/base/log/log_impl.cpp
    ${PROJECT_SOURCE_DIR}/src/base/utils.cpp
    ${PROJECT_SOURCE_DIR}/src/config/*.c
    ${PROJECT_SOURCE_DIR}/src/config/*.cpp
    ${PROJECT_SOURCE_DIR}/src/time/*.cpp
    ${PROJECT_SOURCE_DIR}/src/time/standardtimer/*.cpp
    ${PROJECT_SOURCE_DIR}/src/time/uvtimer/*.cpp
    ${PROJECT_SOURCE_DIR}/src/threadpool/*.cpp
    ${PROJECT_SOURCE_DIR}/src/base/eventloop/*.cpp
    ${PROJECT_SOURCE_DIR}/src/time/eventloop_timer/event_loop_timer.cpp
    ${PROJECT_SOURCE_DIR}/src/time/eventloop_timer/event_loop_timer_asio.cpp
    ${PROJECT_SOURCE_DIR}/src/network/tcp/tcp_connection.cpp
    ${PROJECT_SOURCE_DIR}/src/network/tcp/tcp_server.cpp
    ${PROJECT_SOURCE_DIR}/src/network/tcp/tcp_client.cpp
    ${PROJECT_SOURCE_DIR}/src/network/udp/udp_server.cpp
    ${PROJECT_SOURCE_DIR}/src/network/udp/udp_client.cpp
)

# 条件性添加 WebSocket 源文件
if(ENABLE_WEBSOCKET)
    file(GLOB_RECURSE WEBSOCKET_SRC_FILES 
        ${PROJECT_SOURCE_DIR}/src/network/websocket/*.cpp
    )
    list(APPEND SRC_FILES ${WEBSOCKET_SRC_FILES})
endif()

# 检查是否找到源文件
if(NOT SRC_FILES)
    message(FATAL_ERROR "No source files found in src/ directory")
endif()

#输出源文件路径 换行显示
message(STATUS "Source files:") 
foreach(FILE ${SRC_FILES})
    message(STATUS "  ${FILE}")
endforeach()

# 输出库类型信息
if(BUILD_SHARED_LIBS)
    message(STATUS "Library type: SHARED (Dynamic)")
    add_library(ToolKit ${SRC_FILES})
else()
    message(STATUS "Library type: STATIC")
    add_library(ToolKit STATIC ${SRC_FILES})
endif()

# 对外头文件在 include/；实现依赖的私有头文件在各 src 子目录
target_include_directories(ToolKit PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_include_directories(ToolKit PRIVATE
        ${PROJECT_SOURCE_DIR}/src/base/json
        ${PROJECT_SOURCE_DIR}/src/base/log
        ${PROJECT_SOURCE_DIR}/src/time
        ${PROJECT_SOURCE_DIR}/src/time/standardtimer
        ${PROJECT_SOURCE_DIR}/src/time/uvtimer
        ${PROJECT_SOURCE_DIR}/src/config
        ${PROJECT_SOURCE_DIR}/src/config/simpleini
        ${PROJECT_SOURCE_DIR}/src/threadpool
        ${PROJECT_SOURCE_DIR}/src/base/eventloop
        ${PROJECT_SOURCE_DIR}/src/time/eventloop_timer
        ${PROJECT_SOURCE_DIR}/src/network/tcp
        ${PROJECT_SOURCE_DIR}/src/network/udp
)

# 条件性添加 WebSocket 包含目录
if(ENABLE_WEBSOCKET)
    target_include_directories(ToolKit PRIVATE
        ${PROJECT_SOURCE_DIR}/src/network/websocket
        ${PROJECT_SOURCE_DIR}/src/network/websocket/client
        ${PROJECT_SOURCE_DIR}/src/network/websocket/client/boost_impl
    )
endif()



#-----------------------------编译生成库设置----------------------------------#
# 设置库属性
set_target_properties(ToolKit PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON  # 设置库为位置无关代码
)

# 如果是动态库，设置版本信息（仅 Windows，Linux 不设置版本以只生成单个 .so 文件）
if(BUILD_SHARED_LIBS AND WIN32)
    set_target_properties(ToolKit PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
endif()

# 设置导出宏
target_compile_definitions(ToolKit PUBLIC
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

# 导出宏控制：
# - 构建动态库本体时：导出符号
# - 构建静态库时：导出宏置空
target_compile_definitions(ToolKit
    PRIVATE $<$<BOOL:${BUILD_SHARED_LIBS}>:ITFLEE_BUILDING_LIBRARY>
    PUBLIC  $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:ITFLEE_STATIC_LIBRARY>
)

# 条件性添加 WebSocket 宏定义
if(ENABLE_WEBSOCKET)
    target_compile_definitions(ToolKit PUBLIC
        ENABLE_WEBSOCKET=1
    )
else()
    target_compile_definitions(ToolKit PUBLIC
        ENABLE_WEBSOCKET=0
    )
endif()

# Windows特定设置
if(WIN32)
    target_compile_definitions(ToolKit PUBLIC
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _WIN32_WINNT=0x0601
        UNICODE
        _UNICODE
    )
    
    # 链接Windows库
    target_link_libraries(ToolKit PUBLIC
        ws2_32
        winmm
        crypt32
        advapi32
        shell32
        dbghelp
        iphlpapi
        userenv
        psapi
    )
endif()

#-----------------------------第三方库查找函数---------------------------------------#
# 查找并链接 libuv 库的函数
function(linux_find_and_link_libuv target_name)
    # 首先尝试使用 find_package（如果 libuv 提供了 CMake 配置文件）
    find_package(libuv QUIET)
    
    if(libuv_FOUND)
        # 如果找到了 libuv 的 CMake 包，使用导入目标
        message(STATUS "libuv found via find_package: ${libuv_VERSION}")
        target_link_libraries(${target_name} PUBLIC libuv::uv)
    else()
        # 否则使用 find_path 和 find_library 手动查找
        find_path(LIBUV_INCLUDE_DIR
            NAMES uv.h
            PATHS
                /usr/include
                /usr/local/include
                /opt/local/include
        )
        
        find_library(LIBUV_LIBRARY
            NAMES uv libuv
            PATHS
                /usr/lib
                /usr/local/lib
                /opt/local/lib
                /usr/lib/x86_64-linux-gnu
                /usr/lib/aarch64-linux-gnu
                /usr/lib/arm-linux-gnueabihf
        )
        
        if(LIBUV_INCLUDE_DIR AND LIBUV_LIBRARY)
            message(STATUS "libuv found:")
            message(STATUS "  Include dir: ${LIBUV_INCLUDE_DIR}")
            message(STATUS "  Library: ${LIBUV_LIBRARY}")
            target_include_directories(${target_name} PUBLIC ${LIBUV_INCLUDE_DIR})
            target_link_libraries(${target_name} PUBLIC ${LIBUV_LIBRARY})
        else()
            message(FATAL_ERROR "libuv not found. Please install libuv-dev:\n"
                                "  sudo apt-get install libuv1-dev")
        endif()
    endif()
endfunction()

#-----------------------------添加第三方库配置---------------------------------------#
if(WIN32)
    set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/../../third_party")
    message(STATUS "THIRD_PARTY_DIR: ${THIRD_PARTY_DIR}")

    # 添加boost（Windows 本地预编译路径）；PUBLIC 以便链接 ToolKit 的目标能编译含 Boost.Asio 的公开头文件（如 event_loop_timer.h）
    target_include_directories(ToolKit PUBLIC ${THIRD_PARTY_DIR}/boost_1_82_0/include)

    # 目录按配置切换
    target_link_directories(ToolKit PRIVATE
        "$<$<CONFIG:Debug>:${THIRD_PARTY_DIR}/boost_1_82_0/lib_win/debug/x64>"
        "$<$<CONFIG:Release>:${THIRD_PARTY_DIR}/boost_1_82_0/lib_win/release/x64>"
    )

    # 库名按配置切换（MSVC 命名）
    target_link_libraries(ToolKit PRIVATE
        "$<$<CONFIG:Debug>:libboost_log-vc143-mt-gd-x64-1_82;libboost_system-vc143-mt-gd-x64-1_82;libboost_thread-vc143-mt-gd-x64-1_82>"
        "$<$<CONFIG:Release>:libboost_log-vc143-mt-x64-1_82;libboost_system-vc143-mt-x64-1_82;libboost_thread-vc143-mt-x64-1_82>"
    )

    # 添加libuv（Windows 本地预编译路径）
    target_include_directories(ToolKit PUBLIC ${THIRD_PARTY_DIR}/libuv/include)

    # 目录按配置切换
    target_link_directories(ToolKit PUBLIC
        "$<$<CONFIG:Debug>:${THIRD_PARTY_DIR}/libuv/dll/x64/Debug>"
        "$<$<CONFIG:Release>:${THIRD_PARTY_DIR}/libuv/dll/x64/Release>"
    )

    # 库名按配置切换
    target_link_libraries(ToolKit PUBLIC
        "$<$<CONFIG:Debug>:uv>"
        "$<$<CONFIG:Release>:uv>"
    )

    # 条件性添加 OpenSSL（仅当启用 WebSocket 时，Boost Beast SSL需要）
    if(ENABLE_WEBSOCKET)
        target_include_directories(ToolKit PRIVATE ${THIRD_PARTY_DIR}/openssl-1.1/include)

        # OpenSSL 库目录（尝试多个可能的路径）
        # OpenSSL 1.1 通常库文件在 lib 目录下，可能没有 Debug/Release 子目录
        target_link_directories(ToolKit PRIVATE
            "${THIRD_PARTY_DIR}/openssl-1.1/lib"
            "$<$<CONFIG:Debug>:${THIRD_PARTY_DIR}/openssl-1.1/lib/x64>"
            "$<$<CONFIG:Release>:${THIRD_PARTY_DIR}/openssl-1.1/lib/x64>"
        )

        # OpenSSL 库名（OpenSSL 1.1 通常使用 libssl 和 libcrypto）
        # 如果链接失败，可能需要根据实际库文件名调整（如 ssleay32, libeay32 等）
        target_link_libraries(ToolKit PRIVATE
            libssl
            libcrypto
        )
    endif()
    
    # 抑制 PDB 警告
    target_link_options(ToolKit PRIVATE "/ignore:4099")   
else()
    # 非 Windows：使用系统/包管理器安装的 Boost
    find_package(Threads REQUIRED)
    find_package(Boost 1.71 REQUIRED COMPONENTS log system thread)
    if(Boost_FOUND)
        message(STATUS "Boost found: ${Boost_VERSION}")
        message(STATUS "Boost include dirs: ${Boost_INCLUDE_DIRS}")
        message(STATUS "Boost library dirs: ${Boost_LIBRARY_DIRS}")
        target_link_libraries(ToolKit PRIVATE Boost::log Boost::system Boost::thread Threads::Threads)
        target_include_directories(ToolKit PUBLIC ${Boost_INCLUDE_DIRS})
    else()
        message(FATAL_ERROR "Boost not found. Please install libboost-all-dev or specific components:\n"
                            "  sudo apt-get install libboost-all-dev\n"
                            "Or set BOOST_ROOT environment variable to point to your Boost installation.")
    endif()
    
    # 查找并链接 libuv 库
    linux_find_and_link_libuv(ToolKit)

    # 条件性添加 OpenSSL（仅当启用 WebSocket 时，Boost Beast SSL需要）
    if(ENABLE_WEBSOCKET)
        find_package(OpenSSL REQUIRED)
        if(OPENSSL_FOUND)
            message(STATUS "OpenSSL found: ${OPENSSL_VERSION}")
            message(STATUS "OpenSSL include dirs: ${OPENSSL_INCLUDE_DIRS}")
            message(STATUS "OpenSSL library dirs: ${OPENSSL_LIBRARY_DIRS}")
            target_include_directories(ToolKit PRIVATE ${OPENSSL_INCLUDE_DIRS})
            target_link_libraries(ToolKit PRIVATE ${OPENSSL_LIBRARIES})
        else()
            message(FATAL_ERROR "OpenSSL not found. Please install libssl-dev or specific components:\n"
                                "  sudo apt-get install libssl-dev\n"
                                "Or set OPENSSL_ROOT environment variable to point to your OpenSSL installation.")
        endif()
    endif()
endif()

#-----------------------------设置编译选项,区分debug和release---------------------------------------#
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")  # 设置警告级别为3

    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL") # 设置多线程库为动态链接

    # 源文件字符集：避免 C4819（源文件里含非当前代码页字符）
    target_compile_options(ToolKit PRIVATE
        /source-charset:utf-8
        /execution-charset:utf-8
    )

    # Debug 模式：调试符号+无优化（MSVC）
    # Release 模式：优化（MSVC）
    target_compile_options(ToolKit PUBLIC
        # 允许更多节数（解决大文件编译问题）
        /bigobj
        # Debug 模式：调试符号+无优化（MSVC）
        $<$<CONFIG:Debug>:/Zi;/Od>
        # Release 模式：优化（MSVC）
        $<$<CONFIG:Release>:/O2>
    )
else() 
    # # 指定 C 编译器
    # set(CMAKE_C_COMPILER "/usr/bin/clang")  # 完整路径
    # # 指定 C++ 编译器
    # set(CMAKE_CXX_COMPILER "/usr/bin/clang++")  # 完整路径

    #下面的写法是属于生成表达式
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")  # 设置警告级别为Wall和Wextra
    # 设置调试模式和优化模式，区分debug和release
    target_compile_options(ToolKit PUBLIC
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O3>
    )

    # 设置clang和gnu的警告级别
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(ToolKit PUBLIC 
            $<$<CONFIG:Debug>:-Wno-unused-parameter>
            $<$<CONFIG:Debug>:-Wno-unused-function>
        )
    endif()
endif()


# 父工程：定义统一输出目录的通用函数
function(set_common_output_dir target)
    set(_BIN_DIR ${CMAKE_BINARY_DIR}/bin)
        set_target_properties(${target} PROPERTIES
            # 可执行文件/Windows DLL 输出目录
            RUNTIME_OUTPUT_DIRECTORY ${_BIN_DIR}
            # Linux/macOS 动态库输出目录
            LIBRARY_OUTPUT_DIRECTORY ${_BIN_DIR}
            # 静态库输出目录
            ARCHIVE_OUTPUT_DIRECTORY ${_BIN_DIR}
            # 分配置目录（Debug）
            RUNTIME_OUTPUT_DIRECTORY_DEBUG ${_BIN_DIR}/Debug
            LIBRARY_OUTPUT_DIRECTORY_DEBUG ${_BIN_DIR}/Debug
            ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${_BIN_DIR}/Debug
            # 分配置目录（Release）
            RUNTIME_OUTPUT_DIRECTORY_RELEASE ${_BIN_DIR}/Release
            LIBRARY_OUTPUT_DIRECTORY_RELEASE ${_BIN_DIR}/Release
            ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${_BIN_DIR}/Release
        )
endfunction()

# 可执行文件与 libToolKit.so 同目录（bin/Release 等）时，优先加载该目录下的 .so，避免误用系统旧库。
function(toolkit_set_origin_rpath target)
    if(APPLE)
        set_target_properties(${target} PROPERTIES
            MACOSX_RPATH ON
            BUILD_RPATH "@loader_path"
            INSTALL_RPATH "@loader_path")
    elseif(UNIX)
        # 说明：
        # - 默认情况下，GNU ld 往往会生成 DT_RUNPATH（new-dtags），其优先级低于 LD_LIBRARY_PATH；
        #   环境里若存在同名 so，可能“看起来 RPATH 不生效”。
        # - 这里通过 --disable-new-dtags 强制生成 DT_RPATH，使其优先级高于 LD_LIBRARY_PATH。
        # - 必须对 $ORIGIN 进行转义，避免被 CMake 当作变量展开。
        target_link_options(${target} PRIVATE "-Wl,--disable-new-dtags")
        set_target_properties(${target} PROPERTIES
            BUILD_RPATH "$ORIGIN"
            INSTALL_RPATH "$ORIGIN")
    endif()
endfunction()

set_common_output_dir(ToolKit)  # 应用配置

#-----------------------------安装规则---------------------------------------#
# # 安装动态库
# install(TARGETS ToolKit
#     ARCHIVE DESTINATION lib
#     LIBRARY DESTINATION lib
#     RUNTIME DESTINATION bin
# )

# # 安装头文件
# install(DIRECTORY src/ DESTINATION include/ToolKit
#     FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
# )

# # 创建pkg-config文件
# configure_file(
#     "${CMAKE_SOURCE_DIR}/ToolKit.pc.in"
#     "${CMAKE_BINARY_DIR}/ToolKit.pc"
#     @ONLY
# )

#-----------------------------打印一些变量----------------------------------#
message(STATUS "编译器CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}")
message(STATUS "编译器版本CMAKE_CXX_COMPILER_VERSION: ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "编译器标志CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message(STATUS "编译器标志DEBUG模式CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "编译器标志RELEASE模式CMAKE_CXX_FLAGS_RELEASE: ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "编译模式CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")




#-----------------------------添加测试项目---------------------------------------#
# 添加测试子目录（存在的目录才添加）
# if(EXISTS "${PROJECT_SOURCE_DIR}/example/test")
#     add_subdirectory(example/test)
# endif()

# if(EXISTS "${PROJECT_SOURCE_DIR}/example/com_test")
#     add_subdirectory(example/com_test)
# endif()

if(EXISTS "${PROJECT_SOURCE_DIR}/example/CMakeLists.txt")
    add_subdirectory(example)
endif()