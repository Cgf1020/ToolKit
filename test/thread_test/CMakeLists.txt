add_executable(threadpool_invoke_full_test
    threadpool_invoke_full_test.cpp
)

target_link_libraries(threadpool_invoke_full_test PRIVATE ToolKit)
target_include_directories(threadpool_invoke_full_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

set_common_output_dir(threadpool_invoke_full_test)

