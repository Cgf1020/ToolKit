# For Ninja: LLVM clang-cl + lld-link + llvm-rc (uses MSVC STL / Windows SDK on Windows)
cmake_minimum_required(VERSION 3.20)

set(_llvm_root "")
foreach(_root IN ITEMS
    "$ENV{LLVM_INSTALL_DIR}"
    "D:/work_software/LLVM"
    "C:/Program Files/LLVM"
)
    if(_root AND EXISTS "${_root}/bin/clang-cl.exe")
        set(_llvm_root "${_root}")
        break()
    endif()
endforeach()

set(_clang_cl "")
set(_lld_link "")
set(_rc_compiler "")

if(_llvm_root)
    set(_clang_cl "${_llvm_root}/bin/clang-cl.exe")
    set(_lld_link "${_llvm_root}/bin/lld-link.exe")
    set(_rc_compiler "${_llvm_root}/bin/llvm-rc.exe")
else()
    file(GLOB _vs_clang
        "D:/Microsoft/Microsoft Visual Studio/2022/*/VC/Tools/Llvm/x64/bin/clang-cl.exe"
        "C:/Program Files/Microsoft Visual Studio/2022/*/VC/Tools/Llvm/x64/bin/clang-cl.exe"
    )
    foreach(_candidate IN LISTS _vs_clang)
        if(EXISTS "${_candidate}")
            get_filename_component(_vs_llvm_bin "${_candidate}" DIRECTORY)
            set(_clang_cl "${_candidate}")
            set(_lld_link "${_vs_llvm_bin}/lld-link.exe")
            break()
        endif()
    endforeach()
endif()

if(NOT _clang_cl)
    message(FATAL_ERROR "clang-cl not found (set LLVM_INSTALL_DIR or install LLVM)")
endif()

set(CMAKE_C_COMPILER "${_clang_cl}" CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER "${_clang_cl}" CACHE FILEPATH "" FORCE)

if(EXISTS "${_lld_link}")
    set(CMAKE_LINKER "${_lld_link}" CACHE FILEPATH "" FORCE)
endif()

if(NOT _rc_compiler)
    file(GLOB _sdk_rc
        "D:/Windows Kits/10/bin/*/x64/rc.exe"
        "C:/Program Files (x86)/Windows Kits/10/bin/*/x64/rc.exe"
    )
    list(SORT _sdk_rc COMPARE NATURAL ORDER DESCENDING)
    foreach(_rc IN LISTS _sdk_rc)
        if(EXISTS "${_rc}")
            set(_rc_compiler "${_rc}")
            break()
        endif()
    endforeach()
endif()

if(_rc_compiler)
    set(CMAKE_RC_COMPILER "${_rc_compiler}" CACHE FILEPATH "" FORCE)
endif()

message(STATUS "Ninja toolchain: CXX=${CMAKE_CXX_COMPILER}")
if(CMAKE_LINKER)
    message(STATUS "Ninja toolchain: LINKER=${CMAKE_LINKER}")
endif()
if(CMAKE_RC_COMPILER)
    message(STATUS "Ninja toolchain: RC=${CMAKE_RC_COMPILER}")
endif()
