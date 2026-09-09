# 构建

默认生成**动态库**（`BUILD_SHARED_LIBS=ON`），WebSocket 默认关闭（`ENABLE_WEBSOCKET=OFF`）。
推荐组合由仓库根目录 `CMakePresets.json` 定义。换编译器、生成器或 `CMAKE_BUILD_TYPE` 时请换构建目录。

| 平台 | 文档 | 推荐 preset | 产物目录 |
|------|------|-------------|----------|
| Windows | [windows.md](windows.md) | `vs2022-clangcl` / `ninja-clangcl`（ClangCL）；`vs2022-msvc` / `ninja-msvc`（MSVC） | `build/`、`build-ninja/`；`build-msvc/`、`build-msvc-ninja/` |
| Linux | [linux.md](linux.md) | `ninja-linux` | `build/` |
| macOS | [macos.md](macos.md) | `ninja-macos` | `build-ninja/` |

各平台文档内按**编译器**分节，其下再写 **Ninja** 与其它生成器（Visual Studio / Makefile / Xcode）。GCC 等无 preset 的组合须使用独立目录（如 `build-gcc`），不要与已有 preset 的产物目录混用。
