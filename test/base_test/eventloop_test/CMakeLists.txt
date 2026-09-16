add_executable(eventloop_test
    eventloop_test.cpp
)

target_link_libraries(eventloop_test PRIVATE ToolKit)
if(NOT WIN32)
    target_link_libraries(eventloop_test PRIVATE Threads::Threads)
endif()
target_include_directories(eventloop_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

set_common_output_dir(eventloop_test)

add_executable(eventloop_simple_test
    eventloop_simple_test.cpp
)

target_link_libraries(eventloop_simple_test PRIVATE ToolKit)
if(NOT WIN32)
    target_link_libraries(eventloop_simple_test PRIVATE Threads::Threads)
endif()
target_include_directories(eventloop_simple_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

set_common_output_dir(eventloop_simple_test)

if(MSVC)
    # 源文件包含非 GBK 字符时，显式告诉 MSVC 用 UTF-8 解析，避免 C4819
    target_compile_options(eventloop_test PRIVATE
        /source-charset:utf-8
        /execution-charset:utf-8
    )
    target_compile_options(eventloop_simple_test PRIVATE
        /source-charset:utf-8
        /execution-charset:utf-8
    )
endif()

