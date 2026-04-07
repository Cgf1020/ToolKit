cmake_minimum_required(VERSION 3.16)

project(third_party_call LANGUAGES CXX)

# 线程池示例子项目
add_subdirectory(thread-pool-master)

# libhv 示例子项目（只包含 example 目录，不去改动原库本身）
add_subdirectory(libhv/example)

