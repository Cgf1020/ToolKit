# Default build type for single-config generators (e.g. Ninja, Unix Makefiles)
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Multi-config generators (e.g. Visual Studio): put Release first
if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Release;Debug;RelWithDebInfo;MinSizeRel" CACHE STRING "Available build types" FORCE)
endif()

# Default to shared library
set(BUILD_SHARED_LIBS ON)

# WebSocket module toggle
# To override a cached value: delete build/CMakeCache.txt or pass -DENABLE_WEBSOCKET=OFF
option(ENABLE_WEBSOCKET "Enable WebSocket module (requires Boost Beast and OpenSSL)" OFF)
if(ENABLE_WEBSOCKET)
    message(STATUS "WebSocket module: ENABLED")
else()
    message(STATUS "WebSocket module: DISABLED")
endif()

if(MSVC)
    add_compile_options(/W3 /utf-8 /sdl)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra)
else()
    add_compile_options(-Wall -Wextra)
endif()