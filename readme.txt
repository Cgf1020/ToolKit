构建说明见 BUILD.md（Windows 双目录 / macOS / Linux）。

生成 release 版本（单配置生成器）：
cd build
rm -rf CMakeCache.txt CMakeFiles
cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_WEBSOCKET=OFF ..
cmake --build . -j$(nproc)

# Windows / Linux 跨平台
cmake -S . -B build -DBUILD_SHARED_LIBS=ON -DENABLE_WEBSOCKET=OFF
cmake --build build --config Release

src/ 目录备注：

1. TCP server 和 client 不再使用，有很多 bug，以后使用 libhv 中的（事件循环）
2. json 可以使用自己的（使用中没有出现 bug），也可以使用第三方库，未测试性能
3. 日志在代码中使用没有问题
4. 其他源码在实践中待验证