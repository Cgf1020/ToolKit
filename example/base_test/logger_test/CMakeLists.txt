add_executable(logger_test
    logger_test.cpp
)

target_link_libraries(logger_test PRIVATE ToolKit)
if(NOT WIN32)
    target_link_libraries(logger_test PRIVATE Threads::Threads)
endif()
target_include_directories(logger_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

set_common_output_dir(logger_test)

file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/logger_test_report.md" LOGGER_TEST_REPORT_PATH)
target_compile_definitions(logger_test PRIVATE "LOGGER_TEST_REPORT_PATH=\"${LOGGER_TEST_REPORT_PATH}\"")

if(MSVC)
    target_compile_options(logger_test PRIVATE
        /source-charset:utf-8
        /execution-charset:utf-8
    )
endif()
