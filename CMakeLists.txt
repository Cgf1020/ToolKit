cmake_minimum_required(VERSION 3.20)
project(ToolKit VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/ToolKitOptions.cmake)
include(cmake/ToolKitCompileCommands.cmake)
include(cmake/ToolKitTarget.cmake)
include(cmake/ToolKitLibrary.cmake)

if(EXISTS "${PROJECT_SOURCE_DIR}/example/CMakeLists.txt")
    add_subdirectory(example)
endif()
