# ToolKit 构建说明 — macOS (Ninja + LLVM)

与 Windows 的 `ninja-clangcl` 同思路：**Ninja + LLVM 全套**，产出 `compile_commands.json` 供 clangd 使用。

| 目录 | 生成器 | 用途 |
|------|--------|------|
| `build-ninja/` | Ninja + LLVM clang++ | 配置 + 编译 + `compile_commands.json`（推荐） |
| `build/` | Xcode | 可选，需安装完整 Xcode.app |

## 工具链

| 组件 | 工具 |
|------|------|
| 生成器 | Ninja |
| 编译器 | Homebrew LLVM `clang++` |
| 链接器 | `ld64.lld`（`brew install lld`，可选） |

## 前置依赖

```bash
brew install cmake ninja llvm lld boost libuv
```

若 LLVM 不在默认路径，可设置：

```bash
export LLVM_INSTALL_DIR="$(brew --prefix llvm)"
```

## 配置与编译

```bash
cd ToolKit
cmake --preset ninja-macos
cmake --build build-ninja --target ToolKit
```

或使用 build preset：

```bash
cmake --preset ninja-macos
cmake --build --preset macos-debug
```

## 可选：Xcode 工程

需要 Xcode IDE 时（须安装完整 Xcode.app，不能只用 Command Line Tools）：

```bash
cmake --preset xcode-macos
open build/ToolKit.xcodeproj
cmake --build build --config Debug
```

## 清理

```bash
rm -rf build-ninja build compile_commands.json
```

Windows 见 [BUILD_VS2022_ClangCL.md](BUILD_VS2022_ClangCL.md)。
