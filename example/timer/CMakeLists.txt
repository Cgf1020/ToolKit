add_executable(event_loop_timer_example
    event_loop_timer_example.cpp
)

target_link_libraries(event_loop_timer_example PRIVATE ToolKit)
if(NOT WIN32)
    target_link_libraries(event_loop_timer_example PRIVATE Threads::Threads)
endif()
target_include_directories(event_loop_timer_example PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

set_common_output_dir(event_loop_timer_example)

if(MSVC)
    target_compile_options(event_loop_timer_example PRIVATE
        /source-charset:utf-8
        /execution-charset:utf-8
    )
endif()
