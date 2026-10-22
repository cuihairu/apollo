# 崩溃采集

Apollo 使用 Crashpad 进行进程级 minidump 采集。采集与监督恢复分开：生成 dump 不恢复活动副本，machined 重启也不证明玩家权威和路由已恢复。

## 执行摘要

应用入口调用 `apollo::runtime::init_crash_capture`，由独立 `crashpad_handler` 写报告。默认只保存在本地，不上传。日志初始化应先完成，避免崩溃模块触发默认日志配置。

## 1. 依赖与构建

### 1.1 采集与处理工具

采集客户端使用 Crashpad；符号处理可使用 Breakpad 的 `dump_syms` 和 `minidump_stackwalk` 工具，不引入另一套采集客户端。

### 1.2 当前配置

依赖通过 vcpkg manifest、override 和 overlay 端口管理。`modules/runtime/CMakeLists.txt` 检测 Crashpad 并设置 `APOLLO_HAS_CRASHPAD`；没有可用 SDK 时是 no-op 实现，必须通过初始化日志区分，不能把成功编译当成采集已启用。

## 2. 集成边界

### 2.1 初始化与生命周期

Login、Gateway、World 等应用入口初始化崩溃采集。采集初始化失败不阻断业务启动，失败原因进入日志。handler 独立于目标进程执行；采集模块不接管玩法或玩家状态恢复。

### 2.2 目录和 handler

默认目录为 `crashdumps/<process>/`，World 使用其实际程序身份。`--crash-dump-dir` 可覆盖目录，`--crash-handler-path` 可指定 handler。

handler 按显式路径、可执行文件同目录、构建期缓存路径定位。CMake 通过 `apollo_package_crash_handler` 将工具放到应用旁。部署须保留执行权限和对应架构。

### 2.3 符号管理

`APOLLO_ENABLE_DEBUG_SYMBOLS=ON` 启用符号构建。`scripts/split_symbols.sh <binary>` 分离 `.debug` 并关联发行件；符号与二进制必须使用同一 build-id。

`build_crash_tools.sh` 构建处理工具，产物位于 `tools/crash-bin/`。通过 `dump_syms` 生成符号文件，再用 `minidump_stackwalk` 还原函数、文件和行号。缺少工具或不匹配的符号时应明确记录，不能把无符号地址当成验收完成。

### 2.4 上传

默认以空上传 URL 和关闭的上传开关保持本地模式。外发目标、访问控制和报告保留策略需要独立部署配置；当前不以接口参数存在宣称上传链路已经接通。

### 2.5 日志和指标

初始化记录采集状态、目录和 handler。启动时检查待处理报告并输出日志和 `crash_reports_pending` 指标。该检查不改变报告状态。

machined 的死亡日志与 dump 是同一故障的不同证据。二者需按进程身份、运行代次和时间关联，不把死亡检测、dump 生成或自动重启中的一项替代完整恢复验收。

## 3. 验收流程

1. 使用当前程序和带符号构建，在隔离运行目录确认 handler 启动。
2. 通过独立测试驱动触发可控故障，确认目标进程异常退出且报告写入对应目录；库内 `crash_test` 是测试函数，不意味着每个应用都暴露同名 CLI。
3. 对同一构建生成符号并检查还原栈。
4. 重启后确认待处理报告可见。
5. 若使用监督器，对照死亡行与重启结果；另行验证玩家资料、目录和活动副本失败/恢复策略。

不能复用已删除进程的运行记录作为当前应用的验收证据。

## 4. 当前边界

已有本地采集、handler 打包、符号工具和启动可见性原语。上传、报告清理配额、具体部署保留策略及平台运行验证按实际需求补齐。Windows 编译通过不等于崩溃采集链路已在该平台验收。

## 5. 关联设计

- [日志](./logging.md)：结构化故障证据。
- [网络](./net-abstraction.md)：出站目标与异步边界。
- [容量与基准](./capacity-and-benchmark.md)：运行证据与指标口径。
