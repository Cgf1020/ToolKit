cmake_minimum_required(VERSION 3.16)
project(ToolKitTest VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


#-----------------------------添加编译的源文件----------------------------------#
# 收集源文件
file(GLOB_RECURSE TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
    # ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

# 检查是否找到源文件
if(NOT TEST_SOURCES)
    message(FATAL_ERROR "No test source files found")
endif()

# 输出源文件路径
message(STATUS "Test source files:")
foreach(FILE ${TEST_SOURCES})
    message(STATUS "  ${FILE}")
endforeach()

# 创建测试可执行文件
add_executable(ToolKitTest ${TEST_SOURCES} ${TEST_HEADERS})

# 添加 .h文件搜索路径，不作用于子项目
target_include_directories(ToolKitTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

#-----------------------------添加第三方库配置---------------------------------------#
#添加ToolKit库
target_include_directories(ToolKitTest PRIVATE ${CMAKE_SOURCE_DIR}/include)
# 简化链接配置，直接链接到 ToolKit 目标
target_link_libraries(ToolKitTest PRIVATE ToolKit)


if(WIN32)
else()
    target_link_libraries(ToolKitTest PRIVATE
        pthread
    )
endif()





#-----------------------------设置编译选项---------------------------------------#
# 继承ToolKit的编译选项
# target_compile_options(ToolKitTest PRIVATE ${CMAKE_CXX_FLAGS})


#-----------------------------其他设置---------------------------------------#
set_common_output_dir(ToolKitTest)  # 复用相同的输出目录配置


