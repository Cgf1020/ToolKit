# ToolKit 构建说明 — Linux (Ninja)

```bash
cd ToolKit
cmake --preset ninja-linux
cmake --build build --target ToolKit -j$(nproc)
```

## 前置依赖

```bash
sudo apt-get install cmake ninja-build libboost-all-dev libuv1-dev
```

## 清理

```bash
rm -rf build compile_commands.json
```