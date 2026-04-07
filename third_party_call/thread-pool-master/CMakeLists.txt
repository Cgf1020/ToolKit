cmake_minimum_required(VERSION 3.16)
project(BS_thread_pool_test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(BS_thread_pool_test BS_thread_pool_test.cpp)
target_include_directories(BS_thread_pool_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(BS_thread_pool_test PRIVATE Threads::Threads)

add_executable(BS_thread_pool_custom_test BS_thread_pool_custom_test.cpp)
target_include_directories(BS_thread_pool_custom_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(BS_thread_pool_custom_test PRIVATE Threads::Threads)

if(MSVC)
    target_compile_definitions(BS_thread_pool_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(BS_thread_pool_custom_test PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Optional: enable native extensions if the header was built with them
# target_compile_definitions(BS_thread_pool_test PRIVATE BS_THREAD_POOL_NATIVE_EXTENSIONS)
