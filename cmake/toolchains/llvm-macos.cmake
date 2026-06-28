# For Ninja on macOS: Homebrew/standalone LLVM clang++ (+ lld when available)
cmake_minimum_required(VERSION 3.20)

set(_llvm_root "")
foreach(_root IN ITEMS
    "$ENV{LLVM_INSTALL_DIR}"
    "/opt/homebrew/opt/llvm"
    "/usr/local/opt/llvm"
)
    if(_root AND EXISTS "${_root}/bin/clang++")
        set(_llvm_root "${_root}")
        break()
    endif()
endforeach()

if(NOT _llvm_root)
    message(FATAL_ERROR
        "LLVM clang++ not found. Install LLVM (brew install llvm) or set LLVM_INSTALL_DIR.")
endif()

set(_clang_c "${_llvm_root}/bin/clang")
set(_clang_cxx "${_llvm_root}/bin/clang++")

set(CMAKE_C_COMPILER "${_clang_c}" CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER "${_clang_cxx}" CACHE FILEPATH "" FORCE)

set(_lld "")
foreach(_lld_candidate IN ITEMS
    "${_llvm_root}/bin/ld64.lld"
    "${_llvm_root}/bin/ld.lld"
    "${_llvm_root}/bin/lld"
    "/opt/homebrew/opt/lld/bin/ld64.lld"
    "/usr/local/opt/lld/bin/ld64.lld"
)
    if(EXISTS "${_lld_candidate}")
        set(_lld "${_lld_candidate}")
        break()
    endif()
endforeach()

if(_lld)
    set(_lld_flag "-fuse-ld=${_lld}")
    set(CMAKE_EXE_LINKER_FLAGS_INIT "${_lld_flag}" CACHE STRING "" FORCE)
    set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_lld_flag}" CACHE STRING "" FORCE)
    set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_lld_flag}" CACHE STRING "" FORCE)
    message(STATUS "Ninja toolchain (macOS): LINKER=${_lld}")
else()
    message(STATUS "Ninja toolchain (macOS): lld not found, using default linker (brew install lld)")
endif()

message(STATUS "Ninja toolchain (macOS): CXX=${CMAKE_CXX_COMPILER}")
