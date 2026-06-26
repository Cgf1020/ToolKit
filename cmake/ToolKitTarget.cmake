# Unified output directory for executables and libraries
function(set_common_output_dir target)
    set(_BIN_DIR ${CMAKE_BINARY_DIR}/bin)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${_BIN_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${_BIN_DIR}
        ARCHIVE_OUTPUT_DIRECTORY ${_BIN_DIR}
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${_BIN_DIR}/Debug
        LIBRARY_OUTPUT_DIRECTORY_DEBUG ${_BIN_DIR}/Debug
        ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${_BIN_DIR}/Debug
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${_BIN_DIR}/Release
        LIBRARY_OUTPUT_DIRECTORY_RELEASE ${_BIN_DIR}/Release
        ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${_BIN_DIR}/Release
    )
endfunction()

# Prefer loading libToolKit.so from the executable directory ($ORIGIN / @loader_path)
function(toolkit_set_origin_rpath target)
    if(APPLE)
        set_target_properties(${target} PROPERTIES
            MACOSX_RPATH ON
            BUILD_RPATH "@loader_path"
            INSTALL_RPATH "@loader_path")
    elseif(UNIX)
        target_link_options(${target} PRIVATE "-Wl,--disable-new-dtags")
        set_target_properties(${target} PROPERTIES
            BUILD_RPATH "$ORIGIN"
            INSTALL_RPATH "$ORIGIN")
    endif()
endfunction()