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
├── docs/              # 文档
│   └── BUILD/         # 分平台构建说明
├── third_party_call/  # 第三方库独立试验（不参与主工程编译）
├── CMakeLists.txt
├── CMakePresets.json
└── README.md
```

构建产物目录已 `.gitignore`（`build/`、`build-ninja/` 及文档中的 `build-msvc`、`build-gcc` 等）。说明见 [docs/BUILD/](docs/BUILD/README.md)。

## 依赖

- **CMake** ≥ 3.20，**C++17**
- **Boost**（system / thread / log 等；Windows 使用仓库旁 `third_party/boost_1_82_0`）
- **spdlog**（header-only，路径 `../third_party/spdlog`）
- **libuv**（Windows 使用 `../third_party/libuv`；Linux / macOS 需安装开发包）
- 启用 WebSocket 时额外需要 **OpenSSL**

Windows / 本机预编译库路径约定为相对本仓库的 `../third_party`（即与 `ToolKit` 同级的 `third_party` 目录）。

## 构建

默认生成**动态库**（`BUILD_SHARED_LIBS=ON`），WebSocket 默认关闭。具体命令按平台写在 `docs/BUILD/`：

| 平台 | 文档 |
|------|------|
| 总览 / 推荐 preset | [docs/BUILD/README.md](docs/BUILD/README.md) |
| Windows（MSVC / ClangCL，Visual Studio 或 Ninja） | [docs/BUILD/windows.md](docs/BUILD/windows.md) |
| Linux（GCC / Clang，Makefile 或 Ninja） | [docs/BUILD/linux.md](docs/BUILD/linux.md) |
| macOS（Ninja + LLVM 或 Xcode） | [docs/BUILD/macos.md](docs/BUILD/macos.md) |

## 使用（链接本库）

1. 包含头文件：把 `ToolKit/include` 加入 include 路径。
2. 链接目标库：`ToolKit`（动态库时注意运行时能找到同目录或 PATH / `LD_LIBRARY_PATH` 中的 DLL/so）。
3. CMake 侧若作为子工程 `add_subdirectory(ToolKit)`，可直接：

```cmake
target_link_libraries(your_app PRIVATE ToolKit)
```

日志请 `#include "base/log/logger.h"`，使用 `LOG_INFO` / `Logger::init`（`{}` 格式化）。示例程序已按此方式链接；可参考 `example/` 下各 `CMakeLists.txt`。

## 运行示例

可执行文件在各构建目录的 `bin/` 下（多配置还有 `Debug` / `Release` 子目录）。路径与示例命令见：

- [docs/BUILD/windows.md](docs/BUILD/windows.md#产物路径)
- [docs/BUILD/linux.md](docs/BUILD/linux.md#产物路径)
- [docs/BUILD/macos.md](docs/BUILD/macos.md)

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

- [docs/BUILD/README.md](docs/BUILD/README.md) — 构建总览
- [docs/BUILD/windows.md](docs/BUILD/windows.md) — Windows
- [docs/BUILD/linux.md](docs/BUILD/linux.md) — Linux
- [docs/BUILD/macos.md](docs/BUILD/macos.md) — macOS
- [docs/测试文档指导说明.md](docs/测试文档指导说明.md) — 通用模块测试指导
- `example/base_test/eventloop_test/eventloop测试.md` — 事件循环用例说明
- `example/base_test/logger_test/logger_test_report.md` — Logger 功能与性能报告
- `example/network_test/tcp_network_test.md` — TCP 测试说明
- `example/thread_test/README_threadpool_invoke_test.md` — 线程池测试说明
- `src/base/log/spdlog/README.md` — spdlog 后端说明
- `third_party_call/readme.txt` — jsoncpp / libhv 使用备忘
