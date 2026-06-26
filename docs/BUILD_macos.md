# ToolKit 构建说明 — macOS (Xcode)

```bash
cd ToolKit
cmake --preset xcode-macos
open build/ToolKit.xcodeproj
cmake --build build --config Debug
```

## 前置依赖

```bash
brew install boost libuv
```

## 清理

```bash
rm -rf build
```

Windows 见 [BUILD_VS2022_ClangCL.md](BUILD_VS2022_ClangCL.md)。