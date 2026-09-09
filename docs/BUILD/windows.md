# Windows 构建

编译器：**MSVC**（`cl`）或 **ClangCL**（`clang-cl`）。
生成器：**Visual Studio**（未指定 `-G` 时通常即此，产出 `.sln`）或 **Ninja**。

- Visual Studio 生成器是多配置，编译用 `--config Debug` / `Release`。
- Ninja 是单配置，配置时须设 `CMAKE_BUILD_TYPE`。
- Visual Studio 生成器不产出 `compile_commands.json`；clangd 用 Ninja。

第三方：Boost 1.82、libuv 在 `../third_party`；spdlog 在仓库内 `3rdparts/spdlog`。MSVC 与 ClangCL 链接同一套 `vc143` Boost。

## ClangCL（推荐）

前置：CMake 3.20+；VS 2022 勾选「使用 C++ 的桌面开发」「适用于 Windows 的 C++ Clang 工具」「C++ CMake tools for Windows」（含 Ninja）。

### Visual Studio 生成器

preset `vs2022-clangcl`，产物在 `build/`。

```powershell
cd ToolKit
cmake --preset vs2022-clangcl
cmake --build build --config Debug --target ToolKit
cmake --build build --config Release --target ToolKit
```

等价手动配置：

```powershell
cmake -S . -B build -A x64 -G "Visual Studio 17 2022" -T ClangCL -DENABLE_WEBSOCKET=OFF
cmake --build build --config Release
```

### Ninja

preset `ninja-clangcl` 为 **Debug**，产物在 `build-ninja/`，供 clangd。

```powershell
cmake --preset ninja-clangcl
cmake --build build-ninja --target ToolKit
```

或：`cmake --build --preset ninja-toolkit`

等价手动配置（须 Debug；工具链 `cmake/toolchains/clangcl-db.cmake`）：

```powershell
cmake -S . -B build-ninja `
  -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/clangcl-db.cmake `
  -DENABLE_WEBSOCKET=OFF
cmake --build build-ninja --target ToolKit
```

## MSVC

产物目录与 ClangCL 分开，避免共用 `CMakeCache.txt`。

### Visual Studio 生成器

preset `vs2022-msvc`，产物在 `build-msvc/`。

```powershell
cmake --preset vs2022-msvc
cmake --build build-msvc --config Debug --target ToolKit
cmake --build build-msvc --config Release --target ToolKit
```

或：`cmake --build --preset msvc-debug` / `msvc-release`

等价手动配置：

```powershell
cmake -S . -B build-msvc -A x64 -G "Visual Studio 17 2022" -DENABLE_WEBSOCKET=OFF
cmake --build build-msvc --config Release
cmake --build build-msvc --config Debug
```

### Ninja

preset `ninja-msvc` 为 **Debug**，产物在 `build-msvc-ninja/`。需已安装 `ninja`，在 **x64 Native Tools Command Prompt for VS** 中执行：

```powershell
cmake --preset ninja-msvc
cmake --build build-msvc-ninja --target ToolKit
```

或：`cmake --build --preset ninja-msvc-toolkit`

等价手动配置：

```powershell
cmake -S . -B build-msvc-ninja `
  -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DENABLE_WEBSOCKET=OFF
cmake --build build-msvc-ninja
```

## 启用 WebSocket

```powershell
cmake --preset vs2022-clangcl -DENABLE_WEBSOCKET=ON
cmake --build build --config Release
cmake --preset vs2022-msvc -DENABLE_WEBSOCKET=ON
cmake --build build-msvc --config Release
```

其它组合在对应配置命令上加 `-DENABLE_WEBSOCKET=ON`。缓存里已有旧值时，删掉该目录的 `CMakeCache.txt` 再配。

## 只编某个示例

```powershell
cmake --build build --target eventloop_test --config Debug
cmake --build build --target logger_test --config Debug
cmake --build build --target tcp_network_test --config Release
cmake --build build-msvc --target eventloop_test --config Debug
cmake --build build-msvc-ninja --target eventloop_test
cmake --build build-ninja --target logger_test
```

## 产物路径

| 配置                                    | 路径                                                   |
| --------------------------------------- | ------------------------------------------------------ |
| ClangCL + Visual Studio Debug / Release | `build/bin/Debug/`、`build/bin/Release/`           |
| ClangCL + Ninja Debug                   | `build-ninja/bin/Debug/`                             |
| MSVC + Visual Studio Debug / Release    | `build-msvc/bin/Debug/`、`build-msvc/bin/Release/` |
| MSVC + Ninja Debug                      | `build-msvc-ninja/bin/Debug/`                        |

```powershell
.\build\bin\Debug\eventloop_test.exe
.\build\bin\Debug\logger_test.exe
.\build-msvc\bin\Debug\eventloop_test.exe
.\build-msvc-ninja\bin\Debug\eventloop_test.exe
```

## 构建目标（节选）

| CMake Target                 | 说明           |
| ---------------------------- | -------------- |
| `ToolKit`                  | 主库           |
| `eventloop_test`           | EventLoop 测试 |
| `logger_test`              | Logger 测试    |
| `event_loop_timer_example` | 定时器示例     |
| `tcp_network_test`         | TCP 网络测试   |

## 清理

```powershell
Remove-Item -Recurse -Force build, build-ninja, build-msvc, build-msvc-ninja -ErrorAction SilentlyContinue
Remove-Item -Force compile_commands.json -ErrorAction SilentlyContinue
```

其它平台：[Linux](linux.md)、[macOS](macos.md)。
