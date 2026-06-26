# compile_commands.json support
#
# Visual Studio generator cannot export compile_commands.json (CMake limitation).
# Use a separate build-ninja directory (Ninja + clang-cl); syncs to project root on build.

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

get_property(_tk_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

if(NOT _tk_multi_config)
    add_custom_target(update_compile_commands
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/compile_commands.json"
            "${CMAKE_SOURCE_DIR}/compile_commands.json"
        COMMENT "Sync compile_commands.json to project root for clangd"
        VERBATIM
    )
endif()