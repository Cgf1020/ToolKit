cmake_minimum_required(VERSION 3.16)
project(ToolKitWebSocketTest VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 仅在启用 WebSocket 时编译该用例
if(NOT ENABLE_WEBSOCKET)
    message(STATUS "WebSocket tests are disabled (ENABLE_WEBSOCKET=OFF)")
    return()
endif()

file(GLOB_RECURSE TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
)

if(NOT TEST_SOURCES)
    message(FATAL_ERROR "No websocket test source files found")
endif()

message(STATUS "WebSocket test sources:")
foreach(FILE ${TEST_SOURCES})
    message(STATUS "  ${FILE}")
endforeach()

add_executable(ToolKitWebSocketTest ${TEST_SOURCES})

target_include_directories(ToolKitWebSocketTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(ToolKitWebSocketTest PRIVATE ToolKit)

if(NOT WIN32)
    target_link_libraries(ToolKitWebSocketTest PRIVATE pthread)
endif()

set_common_output_dir(ToolKitWebSocketTest)
