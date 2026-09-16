add_executable(logger_example
    logger_example.cpp
)

target_link_libraries(logger_example PRIVATE ToolKit)
if(NOT WIN32)
    target_link_libraries(logger_example PRIVATE Threads::Threads)
endif()
target_include_directories(logger_example PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

set_common_output_dir(logger_example)

if(MSVC)
    target_compile_options(logger_example PRIVATE
        /source-charset:utf-8
        /execution-charset:utf-8
    )
endif()
