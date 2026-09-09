# spdlog

Header-only C++ 日志库，版本 **1.17.0**。

- 官方仓库：https://github.com/gabime/spdlog
- 本目录只保留官方 `include/spdlog`（含捆绑的 fmt），不编译、不放 `.lib` / `.dll`
- 许可证：MIT（见 `LICENSE`）

## 业务工程引用

CMake：

```cmake
target_include_directories(your_target PRIVATE
  ${PROJECT_SOURCE_DIR}/3rdparts/spdlog/include)
```

Visual Studio：把 `3rdparts/spdlog/include` 加到「附加包含目录」。

使用：

```cpp
#include <spdlog/spdlog.h>

spdlog::info("hello {}", 42);
```

需要 C++11 或更高。不要定义 `SPDLOG_COMPILED_LIB`（当前没有预编译库）。
