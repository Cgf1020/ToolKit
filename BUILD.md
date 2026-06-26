# ToolKit 构建说明

| 平台 | 编译 | clangd | 文档 |
|------|------|--------|------|
| Windows | `build/`（VS 2022 + ClangCL） | `build-ninja/`（Ninja + clang-cl） | [BUILD_VS2022_ClangCL.md](BUILD_VS2022_ClangCL.md) |
| macOS | Xcode | — | [BUILD_macos.md](BUILD_macos.md) |
| Linux | Ninja | `compile_commands.json`（Ninja preset） | [BUILD_linux.md](BUILD_linux.md) |

## Windows 快速开始

```powershell
# 1. VS 工程（编译）
cmake --preset vs2022-clangcl
cmake --build build --config Debug --target ToolKit

# 2. Ninja 工程（clangd 编译数据库）
cmake --preset ninja-clangcl
cmake --build build-ninja --target ToolKit
# 产出：build-ninja/compile_commands.json + 根目录 compile_commands.json
```

## macOS 快速开始

```bash
cmake --preset xcode-macos
open build/ToolKit.xcodeproj
cmake --build build --config Debug
```

## Linux 快速开始

```bash
cmake --preset ninja-linux
cmake --build build --target ToolKit -j$(nproc)
```