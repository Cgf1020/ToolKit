# ThreadPoolInvokeInterface 测试说明

本文档说明 `example/thread_test/threadpool_invoke_full_test.cpp` 中各个测试用例的目的和覆盖范围，用于验证 `ThreadPoolInvokeInterface` 及其实现 `ThreadPoolInvokeImpl` 的行为是否正确、稳定。

## 测试列表

1. `basic_functionality_test`
2. `parallel_accumulate_test`
3. `invoke_sync_reentrancy_test`
4. `wait_and_clear_test`
5. `priority_order_test`
6. `pause_unpause_test`
7. `reset_test`
8. `deadlock_detection_test`
9. `deadlock_detection_stress_test`
10. `is_current_test`
11. `exception_propagation_test`
12. `stress_test`
13. `benchmark_test`

---

## 1. basic_functionality_test

**目的**：验证 `Invoke` 的基础功能。

**检查点**：

- 无参 / 有参任务都能正常执行。
- 有返回值的任务，其 `std::future<T>::get()` 能得到正确值。

---

## 2. parallel_accumulate_test

**目的**：验证线程池能正确并行处理一批计算任务。

**场景**：

- 将区间 `[1..N]` 切成多段，每段作为一个任务提交。
- 汇总各段的局部和，校验总和是否等于数学期望 \(N(N+1)/2\)。

---

## 3. invoke_sync_reentrancy_test

**目的**：验证 `InvokeSync` 的重入行为。

**场景**：

- 在 worker 线程内部再次调用 `InvokeSync`（递归结构）。
- 要求：在池线程中 `InvokeSync` 直接“就地执行”，不会再排队，从而避免自锁。
- 使用计数器验证递归次数是否符合预期。

---

## 4. wait_and_clear_test

**目的**：验证 `Wait()` 与 `ClearTask()` 的语义。

**场景**：

1. 提交一批任务 → `Wait()` → 所有任务执行完（计数达到期望）。  
2. 再提交一批任务 → `ClearTask()` → `Wait()`：  
   - 队列中尚未执行的任务被丢弃，不会再执行；  
   - 当前正在执行的任务完成后，`Wait()` 能正常返回且不死锁。

---

## 5. priority_order_test

**目的**：验证任务优先级（`InvokeAsync`）生效。

**策略**：

- 先调用 `Pause()` 暂停线程池，确保入队期间 worker 不消费任务。
- 先提交一批低优先级任务，再提交一批高优先级任务，然后 `Unpause()`。
- 执行完后查看任务执行顺序：要求在出现第一个低优先级任务之前，至少有一个高优先级任务执行过，用于证明优先级队列在整体上更倾向先调度高优先级任务。

---

## 6. pause_unpause_test

**目的**：验证 `Pause()` / `Unpause()` 以及它们与 `Wait()` 的交互。

**场景**：

1. 提交一批任务，调用 `Pause()` 后再 `Wait()`：  
   - 只等待“当前正在执行的任务”结束；  
   - 池保持暂停状态，`IsPaused()` 返回 true。
2. 调用 `Unpause()` 再次 `Wait()`：  
   - 所有任务（包括之前队列里未取出的）都应执行完。

---

## 7. reset_test

**目的**：验证线程数重置接口 `GetThreadCount()` / `Reset()`。

**场景**：

1. 记录原始线程数 `original`，调用 `Reset(new_count)`：  
   - `GetThreadCount()` 返回的线程数应更新为 `new_count`。
2. 在新线程数配置下提交一批任务，`Wait()` 后计数正确。
3. 再次 `Reset(original)` 还原线程数，并验证在还原配置下任务仍能正常执行。

---

## 8. deadlock_detection_test

**目的**：验证单次死锁检测逻辑。

**场景**：

- 调用 `EnableWaitDeadlockCheck(true)` 打开检测。  
- 在 worker 线程中调用 `Wait()`：应抛出 `WaitDeadlockError`。  
- 在测试中捕获该异常，并用标志位确认确实抛出了预期异常。

---

## 9. deadlock_detection_stress_test

**目的**：验证多轮死锁检测的稳定性。

**场景**：

- 多轮（如 50 次）循环，每轮都在 worker 线程里调用一次 `Wait()`。  
- 每次都应抛出 `WaitDeadlockError`，计入原子计数器。  
- 测试结束后要求捕获次数等于轮数：既没有漏检，也没有崩溃或挂死。

---

## 10. is_current_test

**目的**：验证 `IsCurrent()` 判断当前线程是否为池 worker 的正确性。

**场景**：

- 在主线程调用 `IsCurrent()`：应返回 false。  
- 在 worker 线程内部调用 `IsCurrent()`：应返回 true。  
- 使用一个原子标志验证至少有一个 worker 线程报告为“当前在线程池中”。

---

## 11. exception_propagation_test

**目的**：验证任务内部抛出的异常是否能通过 `future` 传递到调用方。

**场景**：

- 在任务体中抛出 `std::runtime_error`。  
- 在 `future.get()` 处捕获该异常，并确认类型和行为符合预期。

---

## 12. stress_test

**目的**：验证在线程池下大量小任务的稳定性和正确性。

**场景**：

- 提交大量（如 1000 个）简单任务，每个任务对全局计数器加一。  
- `Wait()` 返回后，计数器数值必须与任务数量严格一致。

---

## 13. benchmark_test

**目的**：在固定规模下粗略观测线程池调度性能和稳定性。

**场景**：

- 多轮（如 5 轮）循环，每轮提交固定数量（如 20000 个）任务。  
- 使用 `std::chrono::steady_clock` 统计每轮任务执行完毕的总耗时。  
- 每轮结束时检查计数器是否等于任务数，确保所有任务都执行过，并打印类似：  
  `round X/Y : 20000 tasks in Z ms`。  
- 该测试主要用于人工观察性能表现，不作为严格基准测试。

---

## 运行方式

在工程根目录执行：

```bash
cmake --build build --target threadpool_invoke_full_test
```

生成完成后运行可执行文件（Windows PowerShell 下示例）：

```powershell
.\build\bin\Debug\threadpool_invoke_full_test.exe
```

若所有测试通过，输出结尾应包含：

```text
[threadpool test] all tests passed.
Press Enter to continue...
```

表示上述所有测试项均已成功通过。 
