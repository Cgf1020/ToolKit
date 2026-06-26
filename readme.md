# ToolKit

跨平台 C++ 工具库，提供日志、JSON、配置、定时器、事件循环、TCP/UDP 网络等模块。

## 目录结构

```
ToolKit/
├── src/                  # 库实现源码（.cpp / 私有头文件）
├── include/              # 对外公开头文件（API）
├── example/              # 示例与测试程序（链接 ToolKit 库）
├── cmake/                # CMake 构建模块（选项、主库、工具链等）
├── docs/                 # 构建与测试文档
├── third_party_call/     # 第三方库独立试验（不参与主工程编译）
├── beak/                 # 历史/备份代码（旧实现与接口草稿，仅供参考）
├── CMakeLists.txt        # 根 CMake 入口
├── CMakePresets.json     # 跨平台 CMake Preset（Windows / macOS / Linux）
└── ToolKit.pc.in         # pkg-config 模板（安装流程尚未启用）
```

构建产物目录（已 `.gitignore` 忽略）：

| 目录 | 说明 |
|------|------|
| `build/` | VS 2022 + ClangCL 或 Linux/macOS 单配置构建目录 |
| `build-ninja/` | Windows Ninja + clang-cl 构建目录（产出 `compile_commands.json`） |

### `src/` — 实现源码

各功能模块的实现代码，按子目录划分：

| 子目录 | 内容 |
|--------|------|
| `base/` | 日志、JSON、事件循环、工具函数 |
| `config/` | INI 配置（含 SimpleIni） |
| `time/` | 定时器（标准定时器、libuv 定时器、事件循环定时器） |
| `threadpool/` | 线程池 |
| `network/` | TCP / UDP / WebSocket（可选） |

私有头文件通常放在对应 `src/` 子目录内；对外 API 在 `include/`。

### `include/` — 公开头文件

供外部或 `example/` 引用的头文件，目录结构与 `src/` 大致对应：

- `base/` — 日志、JSON、队列、配置、事件循环
- `time/` — 定时器相关
- `threadpool/` — 线程池接口
- `network/` — TCP / UDP / WebSocket 接口

### `example/` — 示例程序

通过根 `CMakeLists.txt` 的 `add_subdirectory(example)` 接入主构建。当前常用子项：

| 子目录 | 说明 |
|--------|------|
| `base_test/eventloop_test/` | EventLoop 测试 |
| `timer/` | 事件循环定时器示例 |
| `network_test/` | TCP / UDP 网络测试 |
| `jsontest/`、`thread_test/`、`websocket_test/` 等 | 存在但未默认接入 CMake |

编译后输出在 `build/bin/Debug`（或 `Release`）下。

### `cmake/` — 构建脚本模块

| 文件 | 作用 |
|------|------|
| `ToolKitOptions.cmake` | 全局选项：构建类型、WebSocket 开关、编译警告 |
| `ToolKitCompileCommands.cmake` | 生成并同步 `compile_commands.json`（clangd） |
| `ToolKitTarget.cmake` | 输出目录、RPATH 等辅助函数 |
| `ToolKitLibrary.cmake` | 定义 `ToolKit` 主库：源文件、依赖、链接 |
| `toolchains/clangcl-db.cmake` | Windows Ninja 专用 clang-cl 工具链 |

### `docs/` — 文档

| 文件 | 内容 |
|------|------|
| `BUILD.md` | 构建总览 |
| `BUILD_VS2022_ClangCL.md` | Windows 双目录详细说明 |
| `BUILD_macos.md` | macOS Xcode 构建 |
| `BUILD_linux.md` | Linux Ninja 构建 |

### `third_party_call/` — 第三方库试验

**独立工程**，不在主 `CMakeLists.txt` 中编译。用于单独验证 jsoncpp、libhv、thread-pool 等第三方库的集成方式，需在该目录下自行 `cmake` 构建。

### `beak/` — 历史备份

旧版接口草稿（`api-b/`）与早期实现（`-----/`），不参与当前主库构建，仅供查阅对比。

### 其他根目录文件

| 文件 | 说明 |
|------|------|
| `AGENTS.md` / `CLAUDE.md` | GitNexus 为 AI 助手自动生成的代码分析规范 |
| `compile_commands.json` | Ninja 构建后生成，供 clangd 使用（可忽略提交） |
| `.vscode/` | VS Code / Cursor 编辑器配置 |

## 依赖

- C++17
- CMake 3.20+
- Windows：预编译第三方库位于 `../third_party`（Boost 1.82、libuv 等）
- Linux / macOS：系统包管理器安装 Boost、libuv

## 快速开始

### Windows

```powershell
# VS 工程（编译 / 调试）
cmake --preset vs2022-clangcl
cmake --build build --config Debug --target ToolKit

# Ninja + clangd（compile_commands.json）
cmake --preset ninja-clangcl
cmake --build build-ninja --target ToolKit
```

### macOS

```bash
cmake --preset xcode-macos
cmake --build build --config Debug
```

### Linux

```bash
cmake --preset ninja-linux
cmake --build build --target ToolKit -j$(nproc)
```

## 详细构建文档

见 [docs/BUILD.md](docs/BUILD.md)。

| 平台 | 文档 |
|------|------|
| Windows | [docs/BUILD_VS2022_ClangCL.md](docs/BUILD_VS2022_ClangCL.md) |
| macOS | [docs/BUILD_macos.md](docs/BUILD_macos.md) |
| Linux | [docs/BUILD_linux.md](docs/BUILD_linux.md) |

## 开发备注

- `src/network/tcp` 旧版 TCP 实现不再维护，后续使用 libhv 事件循环方案
- JSON、日志模块在项目中可正常使用；其余模块仍在实践中验证