cmake_minimum_required(VERSION 3.16)
project(JsonUsageExample LANGUAGES CXX)

# 复用父工程的 C++ 标准设置（父 CMakeLists 已经设为 C++17）

add_executable(JsonUsageExample
    main.cpp
)

# 头文件搜索路径
target_include_directories(JsonUsageExample PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# 链接主库
target_link_libraries(JsonUsageExample PRIVATE ToolKit)

# 输出目录与主工程保持一致
# 直接调用函数（函数在父 CMakeLists.txt 中已定义）
set_common_output_dir(JsonUsageExample)


