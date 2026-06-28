#----------------------------- Source files -----------------------------
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

if(ENABLE_WEBSOCKET)
    file(GLOB_RECURSE WEBSOCKET_SRC_FILES
        ${PROJECT_SOURCE_DIR}/src/network/websocket/*.cpp
    )
    list(APPEND SRC_FILES ${WEBSOCKET_SRC_FILES})
endif()

if(NOT SRC_FILES)
    message(FATAL_ERROR "No source files found in src/ directory")
endif()

message(STATUS "Source files:")
foreach(FILE ${SRC_FILES})
    message(STATUS "  ${FILE}")
endforeach()

if(BUILD_SHARED_LIBS)
    message(STATUS "Library type: SHARED (Dynamic)")
    add_library(ToolKit ${SRC_FILES})
else()
    message(STATUS "Library type: STATIC")
    add_library(ToolKit STATIC ${SRC_FILES})
endif()

if(TARGET update_compile_commands)
    add_dependencies(ToolKit update_compile_commands)
endif()

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

if(ENABLE_WEBSOCKET)
    target_include_directories(ToolKit PRIVATE
        ${PROJECT_SOURCE_DIR}/src/network/websocket
        ${PROJECT_SOURCE_DIR}/src/network/websocket/client
        ${PROJECT_SOURCE_DIR}/src/network/websocket/client/boost_impl
    )
endif()

set_target_properties(ToolKit PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

if(BUILD_SHARED_LIBS AND WIN32)
    set_target_properties(ToolKit PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
endif()

target_compile_definitions(ToolKit PUBLIC
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

target_compile_definitions(ToolKit
    PRIVATE $<$<BOOL:${BUILD_SHARED_LIBS}>:ITFLEE_BUILDING_LIBRARY>
    PUBLIC  $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:ITFLEE_STATIC_LIBRARY>
)

if(ENABLE_WEBSOCKET)
    target_compile_definitions(ToolKit PUBLIC ENABLE_WEBSOCKET=1)
else()
    target_compile_definitions(ToolKit PUBLIC ENABLE_WEBSOCKET=0)
endif()

if(WIN32)
    target_compile_definitions(ToolKit PUBLIC
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _WIN32_WINNT=0x0601
        UNICODE
        _UNICODE
    )

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

function(linux_find_and_link_libuv target_name)
    find_package(libuv QUIET)

    if(libuv_FOUND)
        message(STATUS "libuv found via find_package: ${libuv_VERSION}")
        target_link_libraries(${target_name} PUBLIC libuv::uv)
    else()
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

if(WIN32)
    set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/../third_party")
    message(STATUS "THIRD_PARTY_DIR: ${THIRD_PARTY_DIR}")

    target_include_directories(ToolKit PUBLIC ${THIRD_PARTY_DIR}/boost_1_82_0/include)

    # Disable Boost auto-link under clang-cl (would look for clangw19 libs)
    target_compile_definitions(ToolKit PRIVATE BOOST_ALL_NO_LIB)

    target_link_directories(ToolKit PRIVATE
        "$<$<CONFIG:Debug>:${THIRD_PARTY_DIR}/boost_1_82_0/lib_win/debug/x64>"
        "$<$<CONFIG:Release>:${THIRD_PARTY_DIR}/boost_1_82_0/lib_win/release/x64>"
    )

    target_link_libraries(ToolKit PRIVATE
        "$<$<CONFIG:Debug>:libboost_log-vc143-mt-gd-x64-1_82;libboost_log_setup-vc143-mt-gd-x64-1_82;libboost_system-vc143-mt-gd-x64-1_82;libboost_thread-vc143-mt-gd-x64-1_82;libboost_filesystem-vc143-mt-gd-x64-1_82;libboost_atomic-vc143-mt-gd-x64-1_82;libboost_chrono-vc143-mt-gd-x64-1_82>"
        "$<$<CONFIG:Release>:libboost_log-vc143-mt-x64-1_82;libboost_log_setup-vc143-mt-x64-1_82;libboost_system-vc143-mt-x64-1_82;libboost_thread-vc143-mt-x64-1_82;libboost_filesystem-vc143-mt-x64-1_82;libboost_atomic-vc143-mt-x64-1_82;libboost_chrono-vc143-mt-x64-1_82>"
    )

    target_include_directories(ToolKit PUBLIC ${THIRD_PARTY_DIR}/libuv/include)

    target_link_directories(ToolKit PUBLIC
        "$<$<CONFIG:Debug>:${THIRD_PARTY_DIR}/libuv/dll/x64/Debug>"
        "$<$<CONFIG:Release>:${THIRD_PARTY_DIR}/libuv/dll/x64/Release>"
    )

    target_link_libraries(ToolKit PUBLIC
        "$<$<CONFIG:Debug>:uv>"
        "$<$<CONFIG:Release>:uv>"
    )

    if(ENABLE_WEBSOCKET)
        target_include_directories(ToolKit PRIVATE ${THIRD_PARTY_DIR}/openssl-1.1/include)

        target_link_directories(ToolKit PRIVATE
            "${THIRD_PARTY_DIR}/openssl-1.1/lib"
            "$<$<CONFIG:Debug>:${THIRD_PARTY_DIR}/openssl-1.1/lib/x64>"
            "$<$<CONFIG:Release>:${THIRD_PARTY_DIR}/openssl-1.1/lib/x64>"
        )

        target_link_libraries(ToolKit PRIVATE
            libssl
            libcrypto
        )
    endif()

    target_link_options(ToolKit PRIVATE "/ignore:4099")
else()
    find_package(Threads REQUIRED)

    if(POLICY CMP0167)
        cmake_policy(SET CMP0167 NEW)
    endif()

    # Boost.System is header-only since 1.89; older releases still ship libboost_system.
    find_package(Boost 1.71 REQUIRED COMPONENTS log thread)
    set(_tk_boost_libs Boost::log Boost::thread)
    if(Boost_VERSION VERSION_LESS "1.89.0")
        find_package(Boost 1.71 REQUIRED COMPONENTS system)
        list(APPEND _tk_boost_libs Boost::system)
    endif()

    message(STATUS "Boost found: ${Boost_VERSION}")
    message(STATUS "Boost include dirs: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost library dirs: ${Boost_LIBRARY_DIRS}")
    target_link_libraries(ToolKit PRIVATE ${_tk_boost_libs} Threads::Threads)
    target_include_directories(ToolKit PUBLIC ${Boost_INCLUDE_DIRS})

    linux_find_and_link_libuv(ToolKit)

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

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

    target_compile_options(ToolKit PRIVATE
        /source-charset:utf-8
        /execution-charset:utf-8
    )

    target_compile_options(ToolKit PUBLIC
        /bigobj
        $<$<CONFIG:Debug>:/Zi;/Od>
        $<$<CONFIG:Release>:/O2>
    )
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
    target_compile_options(ToolKit PUBLIC
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O3>
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(ToolKit PUBLIC
            $<$<CONFIG:Debug>:-Wno-unused-parameter>
            $<$<CONFIG:Debug>:-Wno-unused-function>
        )
    endif()
endif()

set_common_output_dir(ToolKit)

#----------------------------- Source files -----------------------------
# # install(TARGETS ToolKit
# install(TARGETS ToolKit
#     ARCHIVE DESTINATION lib
#     LIBRARY DESTINATION lib
#     RUNTIME DESTINATION bin
# )
#
# # install headers`n# install(DIRECTORY src/ DESTINATION include/ToolKit
#     FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
# )
#
# # install(TARGETS ToolKit
# configure_file(
#     "${CMAKE_SOURCE_DIR}/ToolKit.pc.in"
#     "${CMAKE_BINARY_DIR}/ToolKit.pc"
#     @ONLY
# )

message(STATUS "CXX compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "CXX compiler version: ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "CXX flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "CXX flags (Debug): ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "CXX flags (Release): ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
