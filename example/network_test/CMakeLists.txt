add_executable(tcp_network_test
    tcp_network_test.cpp
)

target_link_libraries(tcp_network_test PRIVATE ToolKit)
if(NOT WIN32)
    target_link_libraries(tcp_network_test PRIVATE Threads::Threads)
endif()
target_include_directories(tcp_network_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_common_output_dir(tcp_network_test)
toolkit_set_origin_rpath(tcp_network_test)

add_executable(udp_network_test
    udp_network_test.cpp
)

target_link_libraries(udp_network_test PRIVATE ToolKit)
if(NOT WIN32)
    target_link_libraries(udp_network_test PRIVATE Threads::Threads)
endif()
target_include_directories(udp_network_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_common_output_dir(udp_network_test)
toolkit_set_origin_rpath(udp_network_test)

add_executable(tcp_client_test
    tcp_test/tcp_client_test.cpp
)

target_link_libraries(tcp_client_test PRIVATE ToolKit)
if(NOT WIN32)
    target_link_libraries(tcp_client_test PRIVATE Threads::Threads)
endif()
target_include_directories(tcp_client_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_common_output_dir(tcp_client_test)
toolkit_set_origin_rpath(tcp_client_test)

add_executable(tcp_server_test
    tcp_test/tcp_server_test.cpp
)

target_link_libraries(tcp_server_test PRIVATE ToolKit)
if(NOT WIN32)
    target_link_libraries(tcp_server_test PRIVATE Threads::Threads)
endif()
target_include_directories(tcp_server_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
set_common_output_dir(tcp_server_test)
toolkit_set_origin_rpath(tcp_server_test)

if(MSVC)
    target_compile_options(tcp_network_test PRIVATE
        /source-charset:utf-8
        /execution-charset:utf-8
    )
    target_compile_options(udp_network_test PRIVATE
        /source-charset:utf-8
        /execution-charset:utf-8
    )
    target_compile_options(tcp_client_test PRIVATE
        /source-charset:utf-8
        /execution-charset:utf-8
    )
    target_compile_options(tcp_server_test PRIVATE
        /source-charset:utf-8
        /execution-charset:utf-8
    )
endif()

