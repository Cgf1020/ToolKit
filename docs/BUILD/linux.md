# Linux 构建

Linux 为单配置生成器，须设置 `CMAKE_BUILD_TYPE`（`Debug` 或 `Release`）。
编译器：**GCC**（`g++`）或 **Clang**（`clang++`）。
生成器：**Ninja**（`-G Ninja`）或 **Unix Makefiles**（默认）。与选哪个编译器无关。

推荐组合由仓库根目录 `CMakePresets.json` 定义。换编译器、生成器或 `CMAKE_BUILD_TYPE` 时请换构建目录；**不能**在 Debug 目录上加 `--config Release`（那是 Visual Studio / Xcode 的用法）。spdlog 取自仓库内 `3rdparts/spdlog`。

## 推荐：Ninja + 系统编译器

### Debug（ninja-linux）

preset `ninja-linux`，产物在 `build/`。

```bash
cd ToolKit
cmake --preset ninja-linux
cmake --build build --target ToolKit --parallel
```

或：`cmake --build --preset linux-debug`

### Release（ninja-linux-release）

preset `ninja-linux-release`，产物在 `build-release/`。

```bash
cmake --preset ninja-linux-release
cmake --build build-release --target ToolKit --parallel
```

或：`cmake --build --preset linux-release`

等价手动配置：

```bash
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_WEBSOCKET=OFF
cmake --build build-release --target ToolKit --parallel
```



## 前置依赖

`cmake --preset` 需要 **CMake 3.25 或更高**（本仓库 `CMakePresets.json` 为 version 6）。Ubuntu 20.04 的 `apt` 只有 3.16，会把 `ninja-clang` 等 preset 名误当成源码目录。

用户目录安装（无需 sudo，且 `~/.local/bin` 通常已在 `PATH` 最前）：

```bash
pip3 install --user 'cmake>=3.25' ninja
hash -r
unset CMAKE_ROOT
cmake --version
```

`cmake --version` 应为 3.25 或更高，`which cmake` 应指向 `~/.local/bin/cmake`。

若出现 `Could not find CMAKE_ROOT` 或提示找不到 `~/.local/share/cmake-3.16`：说明仍在跑系统 3.16，或旧 `CMAKE_ROOT` 指到了空目录。新开终端后再执行上面的 `hash -r` 与 `unset CMAKE_ROOT`。

系统库仍用 apt：

```bash
sudo apt-get install g++ clang libboost-all-dev libuv1-dev
```

也可从 [Kitware APT](https://apt.kitware.com/) 安装系统级 CMake，不要用 20.04 自带的 3.16。

## GCC



### Makefile（GCC）



#### Debug（makefile-gcc）

preset `makefile-gcc`，产物在 `build-gcc/`。

```bash
cmake --preset makefile-gcc
cmake --build build-gcc --target ToolKit --parallel
```

或：`cmake --build --preset linux-gcc`

等价手动配置：

```bash
cmake -S . -B build-gcc -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++ -DENABLE_WEBSOCKET=OFF
cmake --build build-gcc --parallel
```



#### Release（makefile-gcc-release）

preset `makefile-gcc-release`，产物在 `build-gcc-release/`。

```bash
cmake --preset makefile-gcc-release
cmake --build build-gcc-release --target ToolKit --parallel
```

或：`cmake --build --preset linux-gcc-release`

等价手动配置：

```bash
cmake -S . -B build-gcc-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DENABLE_WEBSOCKET=OFF
cmake --build build-gcc-release --parallel
```



### Ninja（GCC）



#### Debug（ninja-gcc）

preset `ninja-gcc`，产物在 `build-gcc-ninja/`。

```bash
cmake --preset ninja-gcc
cmake --build build-gcc-ninja --target ToolKit --parallel
```

或：`cmake --build --preset linux-gcc-ninja`

等价手动配置：

```bash
cmake -S . -B build-gcc-ninja -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++ -DENABLE_WEBSOCKET=OFF
cmake --build build-gcc-ninja --parallel
```



#### Release（ninja-gcc-release）

preset `ninja-gcc-release`，产物在 `build-gcc-ninja-release/`。

```bash
cmake --preset ninja-gcc-release
cmake --build build-gcc-ninja-release --target ToolKit --parallel
```

或：`cmake --build --preset linux-gcc-ninja-release`

等价手动配置：

```bash
cmake -S . -B build-gcc-ninja-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DENABLE_WEBSOCKET=OFF
cmake --build build-gcc-ninja-release --parallel
```



## Clang



### Makefile（Clang）



#### Debug（makefile-clang）

preset `makefile-clang`，产物在 `build-clang/`。

```bash
cmake --preset makefile-clang
cmake --build build-clang --target ToolKit --parallel
```

或：`cmake --build --preset linux-clang`

等价手动配置：

```bash
cmake -S . -B build-clang -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ -DENABLE_WEBSOCKET=OFF
cmake --build build-clang --parallel
```



#### Release（makefile-clang-release）

preset `makefile-clang-release`，产物在 `build-clang-release/`。

```bash
cmake --preset makefile-clang-release
cmake --build build-clang-release --target ToolKit --parallel
```

或：`cmake --build --preset linux-clang-release`

等价手动配置：

```bash
cmake -S . -B build-clang-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DENABLE_WEBSOCKET=OFF
cmake --build build-clang-release --parallel
```



### Ninja（Clang）



#### Debug（ninja-clang）

preset `ninja-clang`，产物在 `build-clang-ninja/`。

```bash
cmake --preset ninja-clang
cmake --build build-clang-ninja --target ToolKit --parallel
```

或：`cmake --build --preset linux-clang-ninja`

等价手动配置：

```bash
cmake -S . -B build-clang-ninja -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ -DENABLE_WEBSOCKET=OFF
cmake --build build-clang-ninja --parallel
```



#### Release（ninja-clang-release）

preset `ninja-clang-release`，产物在 `build-clang-ninja-release/`。

```bash
cmake --preset ninja-clang-release
cmake --build build-clang-ninja-release --target ToolKit --parallel
```

或：`cmake --build --preset linux-clang-ninja-release`

等价手动配置：

```bash
cmake -S . -B build-clang-ninja-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++DENABLE_WEBSOCKET=OFF
cmake --build build-clang-ninja-release --parallel
```



## 启用 WebSocket

```bash
cmake --preset ninja-linux -DENABLE_WEBSOCKET=ON
cmake --preset ninja-linux-release -DENABLE_WEBSOCKET=ON
cmake --preset makefile-gcc -DENABLE_WEBSOCKET=ON
cmake --preset makefile-gcc-release -DENABLE_WEBSOCKET=ON
cmake --preset ninja-gcc -DENABLE_WEBSOCKET=ON
cmake --preset ninja-gcc-release -DENABLE_WEBSOCKET=ON
cmake --preset makefile-clang -DENABLE_WEBSOCKET=ON
cmake --preset makefile-clang-release -DENABLE_WEBSOCKET=ON
cmake --preset ninja-clang -DENABLE_WEBSOCKET=ON
cmake --preset ninja-clang-release -DENABLE_WEBSOCKET=ON
```

其它组合在对应配置命令上加 `-DENABLE_WEBSOCKET=ON`。缓存里已有旧值时，删掉该目录的 `CMakeCache.txt` 再配。

## 只编某个示例

```bash
cmake --build build --target eventloop_test
cmake --build build-release --target eventloop_test
cmake --build build --target logger_test
cmake --build build-gcc --target eventloop_test
cmake --build build-gcc-release --target logger_test
cmake --build build-gcc-ninja --target logger_test
cmake --build build-clang --target eventloop_test
cmake --build build-clang-ninja --target eventloop_test
cmake --build build-clang-ninja-release --target eventloop_test
```



## 产物路径


| 配置             | Debug                          | Release                                  |
| -------------- | ------------------------------ | ---------------------------------------- |
| 系统编译器 + Ninja  | `build/bin/Debug/`             | `build-release/bin/Release/`             |
| GCC Makefile   | `build-gcc/bin/Debug/`         | `build-gcc-release/bin/Release/`         |
| GCC Ninja      | `build-gcc-ninja/bin/Debug/`   | `build-gcc-ninja-release/bin/Release/`   |
| Clang Makefile | `build-clang/bin/Debug/`       | `build-clang-release/bin/Release/`       |
| Clang Ninja    | `build-clang-ninja/bin/Debug/` | `build-clang-ninja-release/bin/Release/` |


```bash
./build/bin/Debug/eventloop_test
./build-release/bin/Release/eventloop_test
./build-gcc/bin/Debug/logger_test
./build-clang-ninja-release/bin/Release/eventloop_test
```



## 清理

```bash
rm -rf build build-release build-gcc build-gcc-release build-gcc-ninja build-gcc-ninja-release build-clang build-clang-release build-clang-ninja build-clang-ninja-release compile_commands.json
```

其它平台：[Windows](windows.md)、[macOS](macos.md)。