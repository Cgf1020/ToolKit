# TCP 网络测试文档

## 1. 文档目的

本文档说明 `example/network_test/tcp_network_test.cpp` 的测试范围、测试项与功能逻辑，便于开发、联调和回归验证。

测试对象：`ToolKit` 中的 TCP 抽象接口与实现（`TcpServerInterface` / `TcpClientInterface` / `TcpConnection`）。

---

## 2. 被测能力范围

- TCP 服务端监听与连接建立（`Listen`）
- TCP 客户端连接发起与关闭（`Connect` / `Close`）
- 连接状态回调（`kConnected` / `kDisconnected` / `kConnectFailed`）
- 单播发送与回显（`Send` + 服务器端 `conn->Write`）
- 服务端广播（`Broadcast`）
- 并发发送下的线程安全（多线程 `Send`）
- 生命周期安全示例（`weak_ptr` 防悬空访问）
- 创建/销毁压力稳定性（循环创建对象）
- 可选延迟统计（echo 回环平均延迟）

---

## 3. 测试程序结构

文件：`example/network_test/tcp_network_test.cpp`

主要组成：

- 公共辅助：
  - `expect(bool, msg)`：断言统计失败数
  - `wait_until(pred, timeout)`：轮询等待异步条件
  - `env_int(name, default)`：读取环境变量参数
- 回调桩：
  - `TcpServerCb`：统计连接数、接收次数，收到消息后执行回显 `echo:xxx`
  - `TcpClientCb`：统计状态变化、接收次数并缓存最后消息
- 测试项函数：
  - `test_tcp_functional_and_order`
  - `test_tcp_concurrency`
  - `test_lifecycle_weak_ptr_guard`
  - `test_stress_create_destroy`
  - `test_latency_optional`

---

## 4. 测试项说明

## 4.1 基础功能 + 接口顺序测试

函数：`test_tcp_functional_and_order()`

### 逻辑步骤

1. 创建 `TcpServerInterface` 与 `TcpClientInterface`
2. 顺序异常验证：
   - `Send` 在 `Connect` 前调用，应返回 `-1`
   - `Connect("", port)` 空 host，应返回 `-1`
3. 正常链路验证：
   - 服务端 `Listen(127.0.0.1:19091)`
   - 客户端 `Connect(127.0.0.1:19091)`
   - 等待服务端和客户端 `kConnected`
4. 消息与回显验证：
   - 客户端发送 `ping`
   - 服务端收到后回显 `echo:ping`
   - 客户端校验收到内容
5. 广播验证：
   - 服务端调用 `Broadcast("broadcast")`
   - 客户端校验收到广播
6. 关闭验证：
   - 客户端 `Close()`
   - 等待断连状态回调

### 通过判据

- 所有断言成立，且无崩溃、无超时。

---

## 4.2 并发发送测试

函数：`test_tcp_concurrency()`

### 逻辑步骤

1. 建立一组 TCP 连接（端口 `19092`）
2. 启动 4 个线程并发发送，每线程发送 25 条消息
3. 统计成功入队发送次数 `send_ok`
4. 断言：
   - 至少有一条发送成功
   - 服务端至少收到一条消息（说明并发路径可达）
5. 调用 `Close()` 收尾

### 说明

- 该测试重点是“并发调用稳定性与线程安全可用”，不是严格吞吐验收。
- 如需严格收包计数一致性，可后续扩展 ACK/序号协议。

---

## 4.3 生命周期安全示例（weak_ptr）

函数：`test_lifecycle_weak_ptr_guard()`

### 逻辑步骤

1. 创建 `Owner` 并保存 `weak_ptr`
2. 执行一次 `weak.lock()` 成功路径，计数 +1
3. 释放 `Owner`
4. 再执行一次 `weak.lock()`，应安全失败，不访问已释放对象

### 目的

- 给出异步回调里典型的 UAF 防护写法样例。

---

## 4.4 压力测试（创建/销毁）

函数：`test_stress_create_destroy()`

### 逻辑步骤

1. 从环境变量读取循环次数：`ITFLEE_NETWORK_STRESS`（默认 200）
2. 每次循环：
   - 创建 `TcpClientInterface`
   - 立即 `Close()`

### 目的

- 验证资源创建和释放路径的稳定性。
- 快速发现明显泄漏或析构时序问题。

---

## 4.5 可选延迟统计

函数：`test_latency_optional()`

### 触发条件

- 当 `ITFLEE_NETWORK_LATENCY_SAMPLES > 0` 时启用。

### 逻辑步骤

1. 建立 TCP 回环链路（端口 `19094`）
2. 连续发送 `samples` 次消息
3. 每次从发送到收到 echo 计算耗时（微秒）
4. 打印平均延迟

### 输出

- `avg tcp echo latency: <xx> us`

---

## 5. 端口与资源占用

- `19091`：基础功能测试
- `19092`：并发发送测试
- `19094`：延迟采样测试

若端口被占用，对应测试会失败。

---

## 6. 执行方式

在项目根目录执行：

```bash
cmake --build build --config Debug
./build/bin/Debug/tcp_network_test.exe
```

可选参数：

```bash
# 压力循环次数（默认 200）
set ITFLEE_NETWORK_STRESS=1000

# 延迟采样数（默认 0，表示关闭）
set ITFLEE_NETWORK_LATENCY_SAMPLES=200
```

---

## 7. 成功/失败判定

- 成功：进程退出码为 `0`，并打印 `[tcp_network_test] all checks passed.`
- 失败：任一断言失败，退出码为 `1`，并打印失败项 `[FAIL] ...`

---

## 8. 后续增强建议

- 增加“严格计数一致性”并发测试（消息序号 + ACK）
- 增加“连接失败重试策略”专项测试（配合 `SetReconnect`）
- 增加“服务端多客户端”并行广播与定向回包测试
- 结合 ASan/TSan 做内存与数据竞争动态检测

