# ToolKit 构建说明 — VS 2022 + ClangCL

Windows 采用 **双目录** 方案：

| 目录 | 生成器 | 用途 |
|------|--------|------|
| `build/` | Visual Studio 17 2022 + ClangCL | 生成 `.sln`，在 VS 中编译、调试 |
| `build-ninja/` | Ninja + clang-cl | 配置 + 编译 + `compile_commands.json`（Cursor 全套） |

> Visual Studio 生成器**不会**产出 `compile_commands.json`（CMake 限制），因此需要单独的 `build-ninja/` 目录。

## 工具链

| 组件 | 工具 |
|------|------|
| 生成器 | Visual Studio 17 2022 |
| 工具集 | ClangCL |
| 编译器 | `clang-cl` |
| 链接器 | `lld-link` |

## 前置要求

- CMake 3.20+
- Visual Studio 2022，勾选：
  - **使用 C++ 的桌面开发**
  - **适用于 Windows 的 C++ Clang 工具**（ClangCL 工具集）
  - **C++ CMake tools for Windows**（提供 Ninja，供 `build-ninja` 使用）
- 第三方依赖位于 `../third_party`（Boost 1.82、libuv 等）

## 一、生成 VS 解决方案（编译用）

```powershell
cd ToolKit
cmake --preset vs2022-clangcl
```

等价手动命令：

```powershell
cmake -S . -B build -A x64 -G "Visual Studio 17 2022" -T ClangCL
```

生成 `build/ToolKit.sln`。

### 编译

```powershell
cmake --build build --config Debug --target ToolKit
cmake --build build --config Release --target ToolKit
cmake --build build --config Debug --target event_loop_timer_example
```

产出路径示例：

| 产物 | 路径 |
|------|------|
| `ToolKit.dll` | `build/bin/Debug/ToolKit.dll` |
| `event_loop_timer_example.exe` | `build/bin/Debug/event_loop_timer_example.exe` |

## 二、生成 compile_commands.json（clangd 用）

```powershell
cmake --preset ninja-clangcl
cmake --build build-ninja --target ToolKit
```

或使用 build preset：

```powershell
cmake --build --preset ninja-toolkit
```

## 三、Ninja + clang-cl 日常命令

```powershell
cmake --preset ninja-clangcl
cmake --build build-ninja --target ToolKit
```

## 构建目标

| CMake Target | 说明 |
|--------------|------|
| `ToolKit` | 主库（动态库） |
| `eventloop_test` | EventLoop 测试 |
| `event_loop_timer_example` | 定时器示例 |
| `tcp_network_test` | TCP 网络测试 |

## 清理

```powershell
Remove-Item -Recurse -Force build, build-ninja -ErrorAction SilentlyContinue
Remove-Item -Force compile_commands.json -ErrorAction SilentlyContinue
```

macOS / Linux 见 [BUILD_macos.md](BUILD_macos.md)、[BUILD_linux.md](BUILD_linux.md)。