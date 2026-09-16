#
# 功能与性能测试：查 bug、回归、吞吐对比。用法演示见 example/。
#

# if(EXISTS "${PROJECT_SOURCE_DIR}/test/jsontest/CMakeLists.txt")
#     add_subdirectory(jsontest)
# endif()

# if(EXISTS "${PROJECT_SOURCE_DIR}/test/tcp_simple_test/CMakeLists.txt")
#     add_subdirectory(tcp_simple_test)
# endif()

# if(ENABLE_WEBSOCKET AND EXISTS "${PROJECT_SOURCE_DIR}/test/websocket_test/CMakeLists.txt")
#     add_subdirectory(websocket_test)
# endif()

if(EXISTS "${PROJECT_SOURCE_DIR}/test/thread_test/CMakeLists.txt")
    add_subdirectory(thread_test)
endif()

if(EXISTS "${PROJECT_SOURCE_DIR}/test/base_test/eventloop_test/CMakeLists.txt")
    add_subdirectory(base_test/eventloop_test)
endif()

if(EXISTS "${PROJECT_SOURCE_DIR}/test/base_test/logger_test/CMakeLists.txt")
    add_subdirectory(base_test/logger_test)
endif()

if(EXISTS "${PROJECT_SOURCE_DIR}/test/timer/CMakeLists.txt")
    add_subdirectory(timer)
endif()

if(EXISTS "${PROJECT_SOURCE_DIR}/test/network_test/CMakeLists.txt")
    add_subdirectory(network_test)
endif()

