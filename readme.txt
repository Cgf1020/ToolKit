生成release版本，这个版本的只有单配置器是可以的
cd build
rm -rf CMakeCache.txt CMakeFiles  # 清除旧的配置
cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_WEBSOCKET=OFF ..   // Debug  Release
cmake --build . -j$(nproc)

#windows、linux   跨平台可以的
cmake -S . -B build -DBUILD_SHARED_LIBS=ON -DENABLE_WEBSOCKET=OFF
cmake -S . -B build -DENABLE_WEBSOCKET=OFF
cmake --build build --config Release/Debug


src/目录下的

1.TCP server 和 client 不再使用，有很多bug，以后使用 libhv中的(事件循环)
2.json 可以使用自己的(使用中没有出现bug)，也可以使用第三方库中的，未测试 他们的性能咋样
3.日志在代码中使用是没有问题的
3.其他的源码，在实践中有待验证