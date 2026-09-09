# macOS 构建

与 Windows 的 ClangCL + Ninja 同思路：推荐 **Ninja + LLVM**，产出 `compile_commands.json`。
生成器：**Ninja** 或 **Xcode**。

## 前置依赖

```bash
brew install cmake ninja llvm lld boost libuv
```

spdlog 取自 `../third_party/spdlog`。若 LLVM 不在默认路径：

```bash
export LLVM_INSTALL_DIR="$(brew --prefix llvm)"
```

## LLVM clang++

### Ninja（推荐）

preset `ninja-macos` 为 **Debug**，产物在 `build-ninja/`。工具链见 `cmake/toolchains/llvm-macos.cmake`。

| 组件 | 工具 |
|------|------|
| 生成器 | Ninja |
| 编译器 | Homebrew LLVM `clang++` |
| 链接器 | `ld64.lld`（`brew install lld`，可选） |

```bash
cd ToolKit
cmake --preset ninja-macos
cmake --build build-ninja --target ToolKit
```

或：

```bash
cmake --preset ninja-macos
cmake --build --preset macos-debug
```

产物：`build-ninja/bin/Debug/`。

```bash
./build-ninja/bin/Debug/eventloop_test
```

### 启用 WebSocket

```bash
cmake --preset ninja-macos -DENABLE_WEBSOCKET=ON
```

## Xcode

须安装完整 Xcode.app（不能只用 Command Line Tools）。preset `xcode-macos`，产物在 `build/`。

```bash
cmake --preset xcode-macos
open build/ToolKit.xcodeproj
cmake --build build --config Debug
```

产物：`build/bin/Debug/`。

## 清理

```bash
rm -rf build-ninja build compile_commands.json
```

其它平台：[Windows](windows.md)、[Linux](linux.md)。
