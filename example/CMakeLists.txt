#
# 统一管理 example 目录下的各个示例程序
#

# if(EXISTS "${PROJECT_SOURCE_DIR}/example/jsontest/CMakeLists.txt")
#     add_subdirectory(jsontest)
# endif()

# if(EXISTS "${PROJECT_SOURCE_DIR}/example/tcp_simple_test/CMakeLists.txt")
#     add_subdirectory(tcp_simple_test)
# endif()

# if(ENABLE_WEBSOCKET AND EXISTS "${PROJECT_SOURCE_DIR}/example/websocket_test/CMakeLists.txt")
#     add_subdirectory(websocket_test)
# endif()

if(EXISTS "${PROJECT_SOURCE_DIR}/example/thread_test/CMakeLists.txt")
    add_subdirectory(thread_test)
endif()

if(EXISTS "${PROJECT_SOURCE_DIR}/example/base_test/eventloop_test/CMakeLists.txt")
    add_subdirectory(base_test/eventloop_test)
endif()

if(EXISTS "${PROJECT_SOURCE_DIR}/example/timer/CMakeLists.txt")
    add_subdirectory(timer)
endif()

if(EXISTS "${PROJECT_SOURCE_DIR}/example/network_test/CMakeLists.txt")
    add_subdirectory(network_test)
endif()

