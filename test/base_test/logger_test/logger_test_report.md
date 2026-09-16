# Logger V2 测试报告

- 时间：2026-09-08 18:09:03
- 平台：Windows
- 构建：Debug（MSVC 1944）
- 硬件并发：12
- 断言合计：通过 150，失败 0
- 功能覆盖：模块分流、error 汇聚、级别过滤、时间戳滚动、异步 flush、并发、shutdown 后再 LOG、小异步队列落盘、源位置、未知模块、异常宏、按模块控制台
- 性能口径：调用耗时 = 最后一条 LOG_* 返回；shutdown 耗时 = 刷盘并释放；入队吞吐按调用耗时，落盘吞吐按调用+shutdown

## 1. 功能测试

### spdlog（12/12 用例通过）

| 用例 | 结果 | 断言 | 耗时 |
| --- | --- | --- | --- |
| 未初始化写 stderr | 通过 | 1/1 | 0.1 ms |
| 模块分流与 error 汇聚 | 通过 | 16/16 | 12.3 ms |
| Info 级别过滤 Debug | 通过 | 4/4 | 4.1 ms |
| 按大小滚动与时间戳文件名 | 通过 | 13/13 | 30.3 ms |
| 异步 shutdown 刷空 | 通过 | 4/4 | 10.7 ms |
| 四线程并发写 | 通过 | 4/4 | 17.8 ms |
| 源文件名与线程号 | 通过 | 4/4 | 3.0 ms |
| 未知模块回退 | 通过 | 3/3 | 4.3 ms |
| LOG_EXCEPTION | 通过 | 3/3 | 4.8 ms |
| 按模块覆盖控制台 | 通过 | 10/10 | 6.1 ms |
| shutdown 后再打日志走 stderr | 通过 | 5/5 | 4.3 ms |
| 小异步队列仍完整落盘 | 通过 | 4/4 | 37.2 ms |

### Boost.Log（12/12 用例通过）

| 用例 | 结果 | 断言 | 耗时 |
| --- | --- | --- | --- |
| 未初始化写 stderr | 通过 | 1/1 | 0.0 ms |
| 模块分流与 error 汇聚 | 通过 | 16/16 | 13.5 ms |
| Info 级别过滤 Debug | 通过 | 4/4 | 3.5 ms |
| 按大小滚动与时间戳文件名 | 通过 | 13/13 | 21.7 ms |
| 异步 shutdown 刷空 | 通过 | 4/4 | 14.2 ms |
| 四线程并发写 | 通过 | 4/4 | 25.1 ms |
| 源文件名与线程号 | 通过 | 4/4 | 3.4 ms |
| 未知模块回退 | 通过 | 3/3 | 3.4 ms |
| LOG_EXCEPTION | 通过 | 3/3 | 4.3 ms |
| 按模块覆盖控制台 | 通过 | 10/10 | 3.9 ms |
| shutdown 后再打日志走 stderr | 通过 | 5/5 | 6.3 ms |
| 小异步队列仍完整落盘 | 通过 | 4/4 | 98.6 ms |

## 2. 性能

写盘场景正文为短 INFO；控制台关闭；`flush_level=Critical`，避免每条 WARN 同步刷盘。
Debug 构建下的绝对值会明显慢于 Release，**对比两个后端时请看同一行的相对倍数**。

| 场景 | 后端 | 条数 | 调用耗时 | shutdown | 入队吞吐 | 落盘吞吐 | 单次调用 | 落盘体积 | 数据完整 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| sync_20k | spdlog | 20000 | 127.0 ms | 0.2 ms | 157472 /s | 157267 /s | 6350 ns | 1664380 B | 是 |
| async_50k | spdlog | 50000 | 233.9 ms | 0.7 ms | 213783 /s | 213191 /s | 4678 ns | 4154380 B | 是 |
| async_4x10k | spdlog | 40000 | 414.5 ms | 0.8 ms | 96511 /s | 96331 /s | 10361 ns | 3211050 B | 是 |
| filtered_debug | spdlog | 200000 | 398.6 ms | 0.1 ms | 501790 /s | 501790 /s | 1993 ns | 0 B | 是 |
| sync_20k | Boost.Log | 20000 | 433.9 ms | 0.8 ms | 46096 /s | 46014 /s | 21694 ns | 1765380 B | 是 |
| async_50k | Boost.Log | 50000 | 2334.1 ms | 1.6 ms | 21421 /s | 21406 /s | 46682 ns | 4405380 B | 是 |
| async_4x10k | Boost.Log | 40000 | 1775.6 ms | 84.5 ms | 22528 /s | 21505 /s | 44389 ns | 3412050 B | 是 |
| filtered_debug | Boost.Log | 200000 | 1057.8 ms | 0.1 ms | 189077 /s | 189077 /s | 5289 ns | 0 B | 是 |

### spdlog 场景说明

- `sync_20k`：同步单线程 INFO，含格式化与写盘
- `async_50k`：异步单线程 INFO；调用耗时=入队，合计含 shutdown 刷盘
- `async_4x10k`：异步 4 线程各 1 万条
- `filtered_debug`：全局 Info，打 Debug；应跳过格式化与落盘

### Boost.Log 场景说明

- `sync_20k`：同步单线程 INFO，含格式化与写盘
- `async_50k`：异步单线程 INFO；调用耗时=入队，合计含 shutdown 刷盘
- `async_4x10k`：异步 4 线程各 1 万条
- `filtered_debug`：全局 Info，打 Debug；应跳过格式化与落盘

## 3. 双后端对比（入队吞吐）

| 场景 | spdlog | Boost.Log | 相对 |
| --- | --- | --- | --- |
| sync_20k | 157472 /s | 46096 /s | spdlog 约为 Boost.Log 的 3.42 倍 |
| async_50k | 213783 /s | 21421 /s | spdlog 约为 Boost.Log 的 9.98 倍 |
| async_4x10k | 96511 /s | 22528 /s | spdlog 约为 Boost.Log 的 4.28 倍 |
| filtered_debug | 501790 /s | 189077 /s | spdlog 约为 Boost.Log 的 2.65 倍 |

## 4. 结论

- 功能：spdlog 与 Boost.Log 均通过分流、error 汇聚、滚动文件名、异步刷盘与并发用例。
- 热路径：`filtered_debug` 表示级别不够时只做 shouldLog，不应产生文件内容。
- 异步：入队吞吐通常高于同步；真正落盘看「落盘吞吐」和 shutdown 耗时。
- Boost.Log 异步为每 sink 一条投递线程、队列 8192 满则阻塞；spdlog 为共享线程池。
