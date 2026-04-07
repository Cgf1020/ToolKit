🛠 `eventloop_test.cpp` 逐条测试说明（EventLoopInterface / EventLoopBoost）

本文档用于解释 `ToolKit/example/base_test/eventloop_test/eventloop_test.cpp` 中每一个 `*_test` 用例的目的、执行步骤与判定逻辑。该可执行文件会依次运行所有测试；任何断言失败都会抛异常并导致进程返回 `1`。

## 0. 运行方式

Windows（MSVC）示例：

```powershell
cmake --build build --target eventloop_test
.\build\bin\Debug\eventloop_test.exe
```

默认不等待回车退出，避免脚本/CI 卡住：

- 若希望最后暂停：设置 `ITFLEE_EVENTLOOP_WAIT_ENTER=1`

## 1. 公共约定

- 内部使用 `expect(cond, "message")` 做断言；失败会抛出 `std::runtime_error`。
- 测试使用 `LoopRunner` 在独立线程启动 `loop->Run()`，并通过 `loop->Stop()` 终止循环。
- `ScheduleAfter / ScheduleEvery / OnSignal` 返回的 `CancelToken` 在用例期间需要保持存活；否则回调会被提前取消。

## 2. 测试用例清单（函数名与代码一致）

### 2.1 `post_dispatch_order_test`

**覆盖点**：验证 `Post()` 与 `Dispatch()` 的执行时序。

**断言逻辑**：
- `Dispatch` 在 loop 线程内应尽可能“立即执行”，因此场景1要求顺序严格为 `P_start -> Dispatch_inline -> P_end`。
- `Post` 在 loop 线程内再次调用时应排队到下一轮，因此场景2要求顺序严格为 `After_Post_call -> Post_from_loop_thread`。

### 2.2 `schedule_after_precision_test`

**覆盖点**：`ScheduleAfter` 延迟误差验证。

**参数**：
- `ITFLEE_EVENTLOOP_DELAY_AFTER_MS`（默认 `100`）
- `ITFLEE_EVENTLOOP_SCHEDULE_AFTER_ERROR_TOLERANCE_MS`（默认按平台给出）

**判定**：计算实际触发时间与目标触发时间的误差（ms），要求 `|error_ms| <= tolerance_ms`。

### 2.3 `schedule_every_drift_test`

**覆盖点**：周期定时器 `ScheduleEvery` 的稳定性。

**参数**：
- `ITFLEE_EVENTLOOP_EVERY_PERIOD_MS`（默认 `10`）
- `ITFLEE_EVENTLOOP_EVERY_COUNT`（默认 `100`）
- `ITFLEE_EVENTLOOP_EVERY_INTERVAL_AVG_TOLERANCE_MS`（默认 Windows=10ms，其它=5ms）
- `ITFLEE_EVENTLOOP_EVERY_INTERVAL_MAX_TOLERANCE_MS`（默认 Windows=25ms，其它=15ms）

**判定策略**：改用“相邻触发间隔偏差”而不是“首尾累计误差”，以避免系统调度开销导致误判。

### 2.4 `signal_handling_test`

**覆盖点**：注册并触发 OS 信号回调（通过 `raise()`）。

**跳过开关**：
- `ITFLEE_EVENTLOOP_SKIP_SIGNALS=1` 会跳过本用例。

**判定**：要求 `SIGINT`（以及非 Windows 下的 `SIGTERM`）回调被触发；并使用定时器超时兜底避免卡死。

### 2.5 `stop_logic_test`

**覆盖点**：`Stop()` 让 `Run()` 阻塞退出、`IsRunning()` 状态正确。

**判定**：停止后要求 `IsRunning()` 为 `false`。

### 2.6 `cancel_token_invalidation_test`

**覆盖点**：`CancelToken` reset 能否取消定时器回调。

**参数**：
- `ITFLEE_EVENTLOOP_TOKEN_INVALIDATE_SECONDS`（默认 `5`）

**判定**：取消后等待结束，断言回调绝不执行（`fired==false`）。

### 2.7 `object_lifetime_uaf_prevention_test`

**覆盖点**：回调捕获 `shared_ptr` 的生命周期正确性（避免 UAF 预防思路）。

**步骤摘要**：
1. 外层创建 `shared_ptr<int>` 和 `weak_ptr`。
2. 在 `ScheduleAfter` 回调中捕获 `shared_ptr`，回调中验证内容、`done.set_value()` 并 `Stop()`。
3. 在 `done` 完成后 `r.Join()`，确保 loop 线程退出后再断言对象释放关系。

**判定**：回调结束后 `weak.lock()!=nullptr`；`sp.reset()` 后 `weak.lock()==nullptr`。

### 2.8 `signal_unregister_test`

**覆盖点**：频繁注册/注销信号处理器后，被注销的 handler 不应触发。

**跳过开关**：
- `ITFLEE_EVENTLOOP_SKIP_SIGNALS=1`

**参数**：
- `ITFLEE_EVENTLOOP_SIGNAL_UNREGISTER_ROUNDS`（默认 `200`）
- `ITFLEE_EVENTLOOP_SIGNAL_UNREGISTER_WAIT_MS`（默认 `5`）

**判定**：最终 `raise(SIGINT)` 后要求触发次数为 0。

### 2.9 `cross_thread_post_test`

**覆盖点**：跨线程并发 `Post()` 的正确性。

**参数**：
- `ITFLEE_EVENTLOOP_CROSS_POST_THREADS`（默认 `10`）
- `ITFLEE_EVENTLOOP_CROSS_POST_PER_THREAD`（默认 `10000`）

**判定**：`counter == threads * per_thread`。

### 2.10 `reentrancy_post_callback_test`

**覆盖点**：Post 回调内部再次调用 `Post / ScheduleAfter` 不应死锁或崩溃。

**判定**：`step` 最终必须走到 3（表示两个嵌套操作都成功调度）。

### 2.11 `reentrancy_stop_from_post_callback_test`

**覆盖点**：Post 回调内部调用 `Stop()` 不应异常。

**判定**：`Stop()` 后 `IsRunning()==false`。

### 2.12 `cancel_token_race_test`

**覆盖点**：定时器触发附近，另一个线程竞态 reset `CancelToken` 时程序不应崩溃。

**参数**：
- `ITFLEE_EVENTLOOP_CANCEL_RACE_ITER`（默认 `20`）
- `ITFLEE_EVENTLOOP_CANCEL_RACE_DELAY_MS`（默认 `80`）
- `ITFLEE_EVENTLOOP_CANCEL_RACE_JITTER_MS`（默认 `3`）

**判定策略**：只要不崩溃即通过；同时打印触发次数便于观察。

### 2.13 `task_backlog_test`

**覆盖点**：1,000,000 级任务瞬时入队稳定性。

**跳过开关**：
- `ITFLEE_EVENTLOOP_SKIP_HEAVY=1`

**参数**：
- `ITFLEE_EVENTLOOP_BACKLOG_TASKS`（默认 `1000000`）
- `ITFLEE_EVENTLOOP_BACKLOG_TIMEOUT_MS`（默认 `30000`）

**判定**：所有任务都执行完成（不超时）。

### 2.14 `signal_storm_test`

**覆盖点**：高频信号风暴下回调调度稳定性。

**跳过开关**：
- `ITFLEE_EVENTLOOP_SKIP_HEAVY=1`（压力用例）
- `ITFLEE_EVENTLOOP_SKIP_SIGNALS=1`（信号用例）

**参数**：
- `ITFLEE_EVENTLOOP_SIGNAL_STORM_COUNT`（默认 `500`）
- `ITFLEE_EVENTLOOP_SIGNAL_STORM_DURATION_MS`（默认 `1000`）
- `ITFLEE_EVENTLOOP_SIGNAL_STORM_MIN_EXPECTED`（默认 `1`）

**判定**：回调次数 `>= min_expected`。

### 2.15 `empty_turn_power_test`

**覆盖点**：空转是否存在明显忙等（CPU 负载近似）。

**跳过开关**：
- `ITFLEE_EVENTLOOP_SKIP_HEAVY=1`

**参数**：
- `ITFLEE_EVENTLOOP_EMPTY_RUN_SECONDS`（默认 `60`）

**说明**：用进程 CPU 时间差做近似测量并输出 `load_ratio` 供人工观察。

### 2.16 `fd_exhaustion_tcp_listen_test`

**覆盖点**：FD 耗尽附近的异常处理（针对网络扩展场景）。

**平台说明**：Windows 下直接跳过（不可靠）。

**跳过开关**：
- `ITFLEE_EVENTLOOP_SKIP_HEAVY=1`

**判定策略**：不强行打满 FD；验证异常不会 escape 导致 abort，并输出错误捕获情况。

### 2.17 `latency_post_average_test`

**覆盖点**：`Post` 投递到实际开始执行的平均延迟（us）。

**跳过开关**：
- `ITFLEE_EVENTLOOP_SKIP_PERF=1`

**参数**：
- `ITFLEE_EVENTLOOP_LATENCY_TASKS`（默认 `50000`）

**判定**：所有任务执行完；输出平均延迟供观察。

### 2.18 `throughput_empty_tasks_test`

**覆盖点**：空任务吞吐量（tasks/s）。

**跳过开关**：
- `ITFLEE_EVENTLOOP_SKIP_PERF=1`

**参数**：
- `ITFLEE_EVENTLOOP_THROUGHPUT_TASKS`（默认 `200000`）

**判定**：所有任务执行完；输出吞吐量供观察。

### 2.19 `high_freq_timer_context_switch_test`

**覆盖点**：高频 `ScheduleEvery` 下 CPU 负载近似评估。

**跳过开关**：
- `ITFLEE_EVENTLOOP_SKIP_PERF=1`

**参数**：
- `ITFLEE_EVENTLOOP_HIGH_FREQ_PERIOD_MS`（默认 `1`）
- `ITFLEE_EVENTLOOP_HIGH_FREQ_DURATION_SECONDS`（默认 `2`）

**判定策略**：不设置强阈值，只输出触发次数与负载比率供观察。

## 3. 一键快速通过（开发/CI 常用）

快速跳过重压/性能并跳过信号测试：

```powershell
$env:ITFLEE_EVENTLOOP_SKIP_HEAVY="1"
$env:ITFLEE_EVENTLOOP_SKIP_PERF="1"
$env:ITFLEE_EVENTLOOP_SKIP_SIGNALS="1"
$env:ITFLEE_EVENTLOOP_TOKEN_INVALIDATE_SECONDS="1"
$env:ITFLEE_EVENTLOOP_EVERY_COUNT="100"
$env:ITFLEE_EVENTLOOP_EVERY_PERIOD_MS="10"
.\build\bin\Debug\eventloop_test.exe
```