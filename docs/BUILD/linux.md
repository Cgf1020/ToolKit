# Linux 构建

Linux 为单配置生成器，须设置 `CMAKE_BUILD_TYPE`（`Debug` 或 `Release`）。
编译器：**GCC**（`g++`）或 **Clang**（`clang++`）。
生成器：**Ninja**（`-G Ninja`）或 **Unix Makefiles**（默认）。与选哪个编译器无关。

换编译器或 `CMAKE_BUILD_TYPE` 时请换构建目录。spdlog 取自仓库内 `3rdparts/spdlog`。

## 推荐：Ninja + 系统编译器（preset）

`ninja-linux` 为系统默认编译器 + Ninja + **Debug**，产物在 `build/`。

```bash
cd ToolKit
cmake --preset ninja-linux
cmake --build build --target ToolKit -j$(nproc)
```

或：`cmake --build --preset linux-debug`

## 前置依赖

```bash
sudo apt-get install cmake ninja-build g++ clang libboost-all-dev libuv1-dev
```

## GCC

### Makefile

```bash
cmake -S . -B build-gcc \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++ \
  -DENABLE_WEBSOCKET=OFF
cmake --build build-gcc -j$(nproc)
```

### Ninja

```bash
cmake -S . -B build-gcc-ninja \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++ \
  -DENABLE_WEBSOCKET=OFF
cmake --build build-gcc-ninja -j$(nproc)
```

## Clang

### Makefile

```bash
cmake -S . -B build-clang \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DENABLE_WEBSOCKET=OFF
cmake --build build-clang -j$(nproc)
```

### Ninja

```bash
cmake -S . -B build-clang-ninja \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DENABLE_WEBSOCKET=OFF
cmake --build build-clang-ninja -j$(nproc)
```

## 启用 WebSocket

在对应配置命令中加 `-DENABLE_WEBSOCKET=ON`。preset：

```bash
cmake --preset ninja-linux -DENABLE_WEBSOCKET=ON
```

## 只编某个示例

```bash
cmake --build build --target eventloop_test
cmake --build build-gcc --target eventloop_test
cmake --build build-gcc --target logger_test
```

## 产物路径

| 配置 | 路径 |
|------|------|
| preset `ninja-linux` Debug | `build/bin/Debug/` |
| GCC Makefile / Ninja Release | `build-gcc/bin/Release/`、`build-gcc-ninja/bin/Release/` |
| Clang Makefile / Ninja Release | `build-clang/bin/Release/`、`build-clang-ninja/bin/Release/` |

```bash
./build/bin/Debug/eventloop_test
./build-gcc/bin/Release/logger_test
./build-clang-ninja/bin/Release/eventloop_test
```

## 清理

```bash
rm -rf build build-gcc build-gcc-ninja build-clang build-clang-ninja compile_commands.json
```

其它平台：[Windows](windows.md)、[macOS](macos.md)。
