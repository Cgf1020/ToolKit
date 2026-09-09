# ToolKit

跨平台 C++17 基础工具库，封装日志、JSON、配置、定时器、事件循环、网络等基础设施，供上层业务直接链接使用。

## 提供什么

| 模块      | 路径（对外头文件）                 | 说明                                                               |
| --------- | ---------------------------------- | ------------------------------------------------------------------ |
| 事件循环  | `include/base/eventloop/`        | 基于 Boost.Asio 的事件循环、定时调度、信号等                       |
| 日志      | `include/base/log/`              | `Logger` 门面：按模块分文件、异步、滚动；默认 spdlog，可切 Boost.Log |
| JSON      | `include/base/json/`             | 自研 JSON 辅助（实践中可用）；也可选用`third_party_call/jsoncpp` |
| 配置      | `include/base/config/`           | INI 配置读写                                                       |
| 队列      | `include/base/queue/`            | 缓存队列、SPSC、同步队列模板等                                     |
| 线程池    | `include/threadpool/`            | 单线程/线程池异步调用接口                                          |
| 定时器    | `include/time/`                  | 标准定时器、事件循环定时器、libuv 定时器等                         |
| TCP / UDP | `include/network/tcp/`、`udp/` | 网络抽象（**自研 TCP 已有较多问题，新代码建议用 libhv**）    |
| WebSocket | `include/network/websocket/`     | 可选模块，默认关闭，依赖 Boost.Beast + OpenSSL                     |

仓库内 `third_party_call/` 还预置了 **libhv**、**jsoncpp**、**BS::thread_pool** 等第三方能力，可单独编译链接；网络相关新需求优先考虑 libhv（TCP/UDP/HTTP/WebSocket/定时器等）。

## 目录结构

```text
ToolKit/
├── include/           # 对外头文件
├── src/               # 库实现
├── example/           # 示例与测试程序
├── cmake/             # CMake 构建模块（选项、主库、工具链等）
├── docs/              # 构建与测试文档
├── third_party_call/  # 第三方库独立试验（不参与主工程编译）
├── CMakeLists.txt
├── CMakePresets.json
└── README.md
```

产物默认输出到构建目录下的 `bin/`（多配置生成器还有 `Debug` / `Release` 子目录）。构建目录已 `.gitignore`：

| 目录               | 说明                                                              |
| ------------------ | ----------------------------------------------------------------- |
| `build/`         | 仓库 ClangCL + Visual Studio preset；Linux Ninja；macOS Xcode     |
| `build-ninja/`   | 仓库 ClangCL / macOS LLVM 的 Ninja preset（`compile_commands.json`） |
| `build-msvc/`    | 文档约定：MSVC + Visual Studio 生成器                             |
| `build-msvc-ninja/` | 文档约定：MSVC + Ninja                                         |

## 依赖

- **CMake** ≥ 3.20，**C++17**
- **Boost**（system / thread / log 等；Windows 使用仓库旁 `third_party/boost_1_82_0`）
- **spdlog**（header-only，路径 `../third_party/spdlog`）
- **libuv**（Windows 使用 `../third_party/libuv`；Linux / macOS 需安装开发包）
- 启用 WebSocket 时额外需要 **OpenSSL**

Windows / 本机预编译库路径约定为相对本仓库的 `../third_party`（即与 `ToolKit` 同级的 `third_party` 目录）。

## 构建

默认生成**动态库**（`BUILD_SHARED_LIBS=ON`），WebSocket 默认关闭（`ENABLE_WEBSOCKET=OFF`）。Windows / macOS 也可用 `CMakePresets.json`。

### Windows（MSVC / Clang）

编译器二选一：**MSVC**（`cl`）或 **Clang**（ClangCL / `clang-cl`）。
CMake 负责生成构建文件；后端可用 **Visual Studio 生成器**（未指定 `-G` 时 Windows 上通常即此，产出 `.sln`，`cmake --build` 走 MSBuild）或 **Ninja**（`-G Ninja`，增量通常更快）。生成器与选哪个编译器无关。

- Visual Studio 生成器是**多配置**，编译时用 `--config Debug` / `--config Release`。
- Ninja 是**单配置**，配置时须设 `CMAKE_BUILD_TYPE`。
- Visual Studio 生成器**不会**产出 `compile_commands.json`；clangd 请用 Ninja。

换编译器或生成器时，请换一个构建目录，或删掉该目录下的 `CMakeCache.txt` 与 `CMakeFiles` 后再配置。

第三方依赖位于 `../third_party`（Boost 1.82、libuv、spdlog）。MSVC 与 ClangCL 都链接同一套预编译 Boost（`vc143`）。

#### MSVC

Visual Studio 生成器（可用 VS 打开 `.sln` 调试）：

```powershell
cmake -S . -B build-msvc -A x64 -G "Visual Studio 17 2022" -DENABLE_WEBSOCKET=OFF
cmake --build build-msvc --config Release
# 或 Debug
cmake --build build-msvc --config Debug
```

使用 Ninja（需已安装 `ninja`，并在 **x64 Native Tools Command Prompt for VS** 中执行，以便找到 `cl.exe`）：

```powershell
cmake -S . -B build-msvc-ninja `
  -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DENABLE_WEBSOCKET=OFF
cmake --build build-msvc-ninja
```

#### Clang（ClangCL）

Visual Studio 生成器 + ClangCL 工具集。仓库 preset `vs2022-clangcl` 产物在 `build/`：

```powershell
cmake --preset vs2022-clangcl
cmake --build build --config Release
# 或 Debug
cmake --build build --config Debug
```

等价手动配置：

```powershell
cmake -S . -B build -A x64 -G "Visual Studio 17 2022" -T ClangCL -DENABLE_WEBSOCKET=OFF
cmake --build build --config Release
```

使用 Ninja + `clang-cl`（产出 `compile_commands.json`）。仓库 preset `ninja-clangcl` 默认 **Debug**，产物在 `build-ninja/`：

```powershell
cmake --preset ninja-clangcl
cmake --build build-ninja --target ToolKit
```

等价手动配置（工具链见 `cmake/toolchains/clangcl-db.cmake`）：

```powershell
cmake -S . -B build-ninja `
  -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/clangcl-db.cmake `
  -DENABLE_WEBSOCKET=OFF
cmake --build build-ninja --target ToolKit
```

### Linux（GCC / Clang）

Linux 为单配置生成器，须设置 `CMAKE_BUILD_TYPE`（`Debug` 或 `Release`）。
编译器二选一：**GCC**（默认多为 `g++`）或 **Clang**（`clang++`）。
CMake 负责生成构建文件；后端可用 Ninja 或 Makefile（`-G Ninja` / 默认 Unix Makefiles），与选哪个编译器无关。

换编译器或 `CMAKE_BUILD_TYPE` 时，请换一个构建目录，或删掉该目录下的 `CMakeCache.txt` 与 `CMakeFiles` 后再配置。

系统依赖示例：`sudo apt-get install cmake ninja-build g++ clang libboost-all-dev libuv1-dev`。spdlog 取自 `../third_party/spdlog`。

#### GCC

```bash
cmake -S . -B build-gcc \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++ \
  -DENABLE_WEBSOCKET=OFF
cmake --build build-gcc -j$(nproc)
```

使用 Ninja（需已安装 `ninja`，增量通常更快）：

```bash
cmake -S . -B build-gcc-ninja \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++ \
  -DENABLE_WEBSOCKET=OFF
cmake --build build-gcc-ninja -j$(nproc)
```

#### Clang

默认使用 Unix Makefiles：

```bash
cmake -S . -B build-clang \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DENABLE_WEBSOCKET=OFF
cmake --build build-clang -j$(nproc)
```

使用 Ninja：

```bash
cmake -S . -B build-clang-ninja \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DENABLE_WEBSOCKET=OFF
cmake --build build-clang-ninja -j$(nproc)
```

产物在对应构建目录的 `bin/Release/` 或 `bin/Debug/`（随 `CMAKE_BUILD_TYPE`）。

也可用仓库 preset（默认系统编译器 + Ninja + **Debug**，产物在 `build/`）：

```bash
cmake --preset ninja-linux
cmake --build build --target ToolKit -j$(nproc)
```

### macOS（Ninja / Xcode）

推荐 Ninja + LLVM（产出 `compile_commands.json`）：

```bash
cmake --preset ninja-macos
cmake --build build-ninja --target ToolKit
```

完整 Xcode.app 可用：

```bash
cmake --preset xcode-macos
cmake --build build --config Debug
```

系统依赖示例：`brew install boost libuv`。spdlog 仍取自 `../third_party/spdlog`。

### 启用 WebSocket

Windows：

```powershell
cmake -S . -B build -DENABLE_WEBSOCKET=ON
cmake --build build --config Release
```

Linux：在 GCC / Clang 的配置命令中加 `-DENABLE_WEBSOCKET=ON` 即可。

若缓存里已有旧选项，需删除对应构建目录下的 `CMakeCache.txt` 后再配置，或用命令行 `-D` 覆盖。

### 只编某个示例

```powershell
# Windows（以 ClangCL + Visual Studio 生成器为例）
cmake --build build --target eventloop_test --config Debug
cmake --build build --target logger_test --config Debug
cmake --build build --target tcp_network_test --config Release

# MSVC + Visual Studio 生成器
cmake --build build-msvc --target eventloop_test --config Debug

# Ninja（MSVC 或 ClangCL）无需 --config
cmake --build build-msvc-ninja --target eventloop_test
cmake --build build-ninja --target logger_test
```

```bash
# Linux（以 GCC 构建目录为例）
cmake --build build-gcc --target eventloop_test
cmake --build build-gcc --target logger_test
```

## 使用（链接本库）

1. 包含头文件：把 `ToolKit/include` 加入 include 路径。
2. 链接目标库：`ToolKit`（动态库时注意运行时能找到同目录或 PATH / `LD_LIBRARY_PATH` 中的 DLL/so）。
3. CMake 侧若作为子工程 `add_subdirectory(ToolKit)`，可直接：

```cmake
target_link_libraries(your_app PRIVATE ToolKit)
```

日志请 `#include "base/log/logger.h"`，使用 `LOG_INFO` / `Logger::init`（`{}` 格式化）。示例程序已按此方式链接；可参考 `example/` 下各 `CMakeLists.txt`。

## 运行示例

编译后可执行文件一般在：

| 平台 / 配置                       | 路径                               |
| --------------------------------- | ---------------------------------- |
| Windows MSVC + VS Release         | `build-msvc/bin/Release/`        |
| Windows MSVC + Ninja Release      | `build-msvc-ninja/bin/Release/`  |
| Windows ClangCL + VS Debug        | `build/bin/Debug/`               |
| Windows ClangCL + VS Release      | `build/bin/Release/`             |
| Windows ClangCL + Ninja Debug     | `build-ninja/bin/Debug/`         |
| Linux GCC Release                 | `build-gcc/bin/Release/`         |
| Linux GCC + Ninja Release         | `build-gcc-ninja/bin/Release/`   |
| Linux Clang Release               | `build-clang/bin/Release/`       |
| Linux Clang + Ninja Release       | `build-clang-ninja/bin/Release/` |
| Linux preset `ninja-linux` Debug | `build/bin/Debug/`               |
| macOS Ninja（preset 为 Debug）      | `build-ninja/bin/`               |
| macOS Xcode Debug                 | `build/bin/Debug/`               |

示例：

```powershell
# Windows（ClangCL + Visual Studio 生成器）
.\build\bin\Debug\eventloop_test.exe
.\build\bin\Debug\logger_test.exe
.\build\bin\Release\tcp_network_test.exe

# MSVC
.\build-msvc\bin\Release\eventloop_test.exe
.\build-msvc-ninja\bin\Release\logger_test.exe
```

```bash
# Linux（CMAKE_BUILD_TYPE=Release 时）
./build-gcc/bin/Release/eventloop_test
./build-gcc/bin/Release/logger_test
./build-clang/bin/Release/tcp_network_test
./build-clang-ninja/bin/Release/eventloop_test
```

部分用例支持环境变量调参（如事件循环测试的延迟容差、是否跳过信号等），详见各示例旁的说明文档。

## 示例一览

当前 **CMake 默认会编译** 的目录（见 `example/CMakeLists.txt`）：

| 目标 / 程序                               | 目录                                  | 说明                                                                     |
| ----------------------------------------- | ------------------------------------- | ------------------------------------------------------------------------ |
| `eventloop_test`                        | `example/base_test/eventloop_test/` | 事件循环完整回归（Post/Dispatch、定时、信号、取消、生命周期等）          |
| `eventloop_simple_test`                 | 同上                                  | 更精简的事件循环演示                                                     |
| `logger_test`                           | `example/base_test/logger_test/`    | Logger 模块分流、滚动、异步刷盘；spdlog / Boost.Log 对比                 |
| `threadpool_invoke_full_test`           | `example/thread_test/`              | 线程池 `ThreadPoolInvoke` 全量测试（见同目录 README）                  |
| `event_loop_timer_example`              | `example/timer/`                    | 事件循环定时器示例                                                       |
| `tcp_network_test`                      | `example/network_test/`             | TCP 功能 / 并发 / 生命周期 / 压力等（详见同目录 `tcp_network_test.md`） |
| `udp_network_test`                      | 同上                                  | UDP 测试                                                                 |
| `tcp_client_test` / `tcp_server_test` | `example/network_test/tcp_test/`    | 独立 TCP 客户端 / 服务端小程序                                           |

仓库中还有、但 **默认未加入构建** 的示例（取消 `example/CMakeLists.txt` 中对应注释即可启用）：

| 目录                        | 说明                                              |
| --------------------------- | ------------------------------------------------- |
| `example/jsontest/`       | JSON 辅助测试                                     |
| `example/websocket_test/` | WebSocket 客户端示例（需 `ENABLE_WEBSOCKET=ON`） |
| `example/qttest/`         | Qt 相关试验工程（独立 vcxproj）                   |

`third_party_call/libhv/example/` 下另有 libhv 的 TCP、定时器、队列线程池等示例，需单独按其 CMake 编译。

## 模块成熟度（实践备注）

1. **自研 TCP server/client**：问题较多，后续网络事件循环相关能力优先使用 **libhv**。
2. **JSON**：自研封装在业务中可用；第三方 jsoncpp 也可链接，性能对比尚未系统评测。
3. **日志**：使用 `itflee::Logger` / `LOG_INFO`；旧流式宏 `LOG_I` 已移除。
4. **其余模块**：仍需在实际场景中继续验证。

更系统的测试关注点（生命周期、竞态、压力等）见 `docs/测试文档指导说明.md`。

## 相关文档

- [docs/BUILD.md](docs/BUILD.md) — 构建总览
- [docs/BUILD_VS2022_ClangCL.md](docs/BUILD_VS2022_ClangCL.md) — Windows 双目录详细说明
- [docs/BUILD_macos.md](docs/BUILD_macos.md) — macOS Xcode 构建
- [docs/BUILD_linux.md](docs/BUILD_linux.md) — Linux Ninja 构建
- [docs/测试文档指导说明.md](docs/测试文档指导说明.md) — 通用模块测试指导
- `example/base_test/eventloop_test/eventloop测试.md` — 事件循环用例说明
- `example/base_test/logger_test/logger_test_report.md` — Logger 功能与性能报告
- `example/network_test/tcp_network_test.md` — TCP 测试说明
- `example/thread_test/README_threadpool_invoke_test.md` — 线程池测试说明
- `src/base/log/spdlog/README.md` — spdlog 后端说明
- `third_party_call/readme.txt` — jsoncpp / libhv 使用备忘
