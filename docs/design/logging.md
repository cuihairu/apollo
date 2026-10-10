# Apollo 运行日志与崩溃取证设计（logging）

> 状态：设计稿（评审中）。目标：回答「关键错误信息如何在一切正常 / 日志系统自身故障 / 进程崩溃三种状态下都留存」——每进程本地日志（**真相源**）、集中收集进程（**优化**）、崩溃取证链（**最后防线**）。关联：`docs/analysis/architecture-review.md`（C-25/C-26 日志现状审计、G-5 观测通道、C-32 信号处理反面教材）、`net-abstraction.md`（G-5 两截落位）、`scripting-lua.md`（§6 脚本错误审计分界）、`attribute-sync.md`（§8.2 BI 分流、§10.2 停机序）。

---

## 执行摘要

1. **本地文件是真相源，收集是优化**。每进程本地日志文件永远先写、必写；集中收集进程（apps/logger，P3）挂了只丢「汇聚检索」，不丢留证——各进程降级为只写本地，零业务影响。先例：KBE `error_msg` 双出口（远端 logger 推送与本地 console **并行**，debug_helper.cpp:973-986）。
2. **分级持久化保证（日志的 QoS）**：FATAL = 格式化后立即 `write`+`fsync`，**返回前不得继续崩溃路径**（先落盘再 abort）；ERROR = **旁路直写通道**（不进共享队列——队列被低级别塞满时也挤不出去）+ 即时 flush；WARN 攒批 100ms；INFO/DEBUG 攒批可丢。与 attribute-sync 带宽预算同型思维：预算给谁、谁不可丢，是声明出来的。
3. **启动序纪律：日志起不来 = 进程不起**。logger 是第一个 start 的 hosted service，失败即退出（先例 skynet：logger 是第一个创建的服务，失败 `fprintf(stderr)+exit(1)`，skynet_start.c:287-291）。日志不可用的进程不允许跑——这是「保证留下关键信息」的第一条：不能出现「带病运行、事后无证」的窗口。
4. **崩溃路径不依赖任何活着的子系统**。崩溃时刻恰好是堆/锁最可能已被同一故障炸坏的时候——留证设施必须是「死设施」：`sigaltstack` 备用栈 + 预分配静态缓冲 + `backtrace()` + 裸 `write(2)` 到独立 crash 文件（不经 logger 管线、不 malloc、不取锁），然后恢复默认处理器重新 raise 保证 core 产出。两家同款先例：KBE signal_handler（SIGABRT/SIGBUS/SIGSEGV 名单 signal_handler.cpp:22-27 + 注册 :112，backtrace debug_helper.cpp:1177-1180）、BW signal_processor（:29-34 名单）+ callstack_linux.cpp:228。
5. **core dump 是最后防线**。RLIMIT_CORE=unlimited（main 早期 setrlimit）+ core_pattern 独立分区/管道 + 磁盘配额轮转 + **CI 验证**（测试进程主动 raise(SIGSEGV)，断言 crash 文件与 core 产出）——崩溃路径是被测路径，不是「出事才知道配错了」。
6. **现状收敛**：C-25 四套日志并存（①utils::logging ②core/log 内建 ③modules/core/log ④内存版）+ 同路径双头文件 ODR 陷阱，收敛为 **modules/core/log 一套**；C-26 孤儿 TU（global_log_manager 无编译归属，默认构建链接必败推演）是第一刀。**自行开发薄层（数百行），不引 spdlog/log4cxx**——分级旁路与 signal-safe 崩溃面是自有语义，第三方日志库不替你保证（KBE 本地层引 log4cxx，debug_helper.cpp:978-980 条件编译可见，apollo 不走这条）。
7. **降级链每级只依赖更原始的设施**：logger 线程死/队列满 → ERROR 旁路直写；文件系统失败/磁盘满 → stderr（重定向管道也能接）+ 停低级别保高级别；进程崩溃 → crash handler 四件套 + core；整机死 → 本地盘的日志与 core 由 machined 式守护上报死亡事件（G-1 接线）后运维拉取。
8. **告警出口与 webhook 默认实现（§5.2，2026-09-30 补）**：双层——层 A 标准栈（Prometheus alerting rules + Alertmanager + 社区 adapter，仓库零代码）为生产推荐；层 B exporter 内置阈值 notifier 为小部署兜底（规则表驱动 + 四家机器人模板：企业微信/钉钉/飞书/Slack = JSON 模板 + 一次 POST，复用 net-abstraction §5.10 curl）。发送纪律六条：退避重试失败回写本地、静默窗聚合、全局速率上限、密钥 env、出站目标白名单（SSRF）、模板变量封闭集；**游戏进程永不直发**（三禁延伸，HTTP 客户端只在 exporter）。

---

## 1. 问题定义：三种故障深度

| 深度 | 状态 | 关键信息的保证手段 |
|---|---|---|
| 0 正常 | 全子系统活 | 分级缓冲 + 本地文件 + （P3）collector 汇聚 |
| 1 日志子系统故障 | 队列满 / logger 线程卡死 / 磁盘满 / collector 挂 | ERROR 旁路直写（不过队列）；磁盘水位停低级别；collector 挂 → 只写本地 |
| 2 进程崩溃 | 堆损坏 / 锁死 / 栈溢出——**最需要留证的时刻，恰恰一切设施最不可信** | crash handler（静态缓冲 + 裸 write）+ core dump——两者都不依赖堆、锁、logger 线程 |

设计原则从上表直接读出：**深度每加一层，留证手段依赖的设施就必须更少**。

## 2. 三家先例（源码证据）

| 机制 | BigWorld | KBEngine | skynet |
|---|---|---|---|
| 集中收集 | 独立 logger 进程 + 各进程 `logger_endpoint` 推送（architecture-review §16.2）；**推送端自带降级语义**：reconnect 连续失败上限 3 次（logger_endpoint.cpp:672-703）+ send 缓冲超 maxBufferedSize 即丢弃并报错——「收集通道不可用时保本地、不拖死进程」的 BW 原版形态；server/tools/message_logger 归档工具（§16.8.2 表） | logger 独立组件进程（kbe/src/server/tools/logger/：logger.cpp + logger_interface.{cpp,h} 接收端）；调用侧 `ERROR_MSG` 宏 → DebugHelper（debug_helper.h:217-218） | logger 是节点内第一个服务（service_logger.c，写文件 + FILE* 句柄 :9-27） |
| 本地兜底 | console/文件（log_msg 体系） | **双出口**：`error_msg` = onMessage 推 logger **并行** printf 本地 console（debug_helper.cpp:973-986）——远端挂本地在 | logger 服务本身就是本地的（节点内）；启动失败即 exit(1)（skynet_start.c:287-291） |
| 崩溃处理 | signal_processor（SIGABRT/SIGBUS/SIGSEGV 等，:29-34）+ callstack_linux backtrace（:228） | signal_handler.cpp:22-27 名单 + :112 注册；`backtrace_msg()`（debug_helper.cpp:1177-1180，glibc backtrace） | —（崩溃交 OS + monitor 线程版本号报警，skynet_monitor.c:31-45，architecture-review §16.2） |
| 共同形态 | **检测/留证在进程内，汇聚呈现在外挂进程**——与 G-5 两截落位同一条线 | 同 | 同 |

apollo 的反面教材在自己仓里：gateway main.cpp:14-19 信号处理器直接调 `stop()`（join 线程 + 关 socket + 锁）——非 async-signal-safe（architecture-review C-32 已录），本设计把这条列为崩溃面红线。

## 3. 每进程日志子系统（正常路径）

### 3.1 分级与持久化保证

| 级别 | 缓冲与落盘 | 丢失容忍 | 备注 |
|---|---|---|---|
| FATAL | 格式化 → `write`+`fsync` **同步完成** → 才允许走 abort/exit 路径 | **零** | 「先落盘再死」是硬序，不是 best-effort |
| ERROR | **旁路直写**：不进共享环形队列，直接 O_APPEND 追加 + 即时 flush | **零**（进程活着的全部时间） | 旁路是本设计与「全级别共队列」方案的分界：队列被 INFO 洪水塞满时，ERROR 依然出得去 |
| WARN | 攒批（100ms 或 4KB 触发） | 崩溃窗口内可容忍 | |
| INFO/DEBUG | 攒批 + 环形覆盖（满了丢最旧） | 可丢 | 低级别让位是显式策略（磁盘水位触发时先停 DEBUG/INFO） |

### 3.2 线程模型

```
业务线程（含场景线程）：无锁写 MPSC 环（复用 utils/loop_buffer.h——与 net 发送环同型）
                        ERROR：旁路直写通道（业务线程自己 write(2)，O_APPEND 原子追加）
logger 线程（1 个）：批量出环 → 格式化 → 本地文件；（P3）双出口 push collector
崩溃路径：不经过以上任何一环（§4）
```

- 无锁纪律与 net-abstraction §3 同构：业务线程是生产者、logger 线程是唯一消费者；日志格式化（时间戳/级别/线程名）发生在 logger 线程——**业务线程只拷贝字节**，热路径零格式化成本。
- ERROR 旁路的成本模型：低频（错误本来就是异常态），每条一次系统调用可接受；换来的是「队列状态与 ERROR 可达性解耦」。

### 3.3 启动序与停机序

- **启动序**：logger 是第一个 start 的 hosted service（IHostedService 序列首位）；初始化失败（文件开不了/权限/磁盘满）→ 进程退出（skynet_start.c:287-291 先例）。crash handler 与 RLIMIT_CORE 装配在更早的 main 入口处（线程创建之前）。
- **停机序**：attribute-sync §10.2 阶段 ⑤ 的模块逆序停止中，logger 是**最后一站**——drain 完缓冲区再关文件（停机路径的最后一批日志恰恰最关键：各模块 stop 的错误要能落盘）。

## 4. 崩溃取证（最后防线）

### 4.1 降级链总表

| 故障场景 | 行为 | 依赖的设施 |
|---|---|---|
| collector 挂（P3） | 断连检测 → 只写本地 → 重连恢复 | 本地文件 |
| logger 线程卡死/队列满 | WARN 以下丢最旧；ERROR 走旁路直写不受影响 | O_APPEND fd |
| 磁盘满/文件系统错误 | ERROR 旁路转写 stderr；水位告警；DEBUG/INFO 全停 | stderr fd（预开的） |
| 进程崩溃（信号） | crash handler 写 crash 文件 → 重新 raise → core dump | 静态缓冲 + 裸 write |
| 整机死 | 本地盘日志与 core 留存；machined 式守护上报死亡事件（G-1，net-abstraction §7 P3 前置设计）→ 运维拉取 | 本地盘 + 外部守护 |

### 4.2 crash handler 四件套

1. **sigaltstack**：handler 跑在预分配备用栈上——栈溢出型 SIGSEGV 也有栈可用（per-thread 装配，落点在 modules/base 的线程创建封装处，一处统一装）。
2. **预分配静态缓冲 + 预格式化**：进程名/版本/build 时间/信号/地址/线程名 + `backtrace()` 十几帧原始地址——**不做在线符号化**（离线 addr2line；符号不剥离纪律已在 scripting-lua §7.3 立过）；backtrace 在信号上下文的可用性是 glibc 实践标准（KBE/BW 两家先例同款用法）。
3. **裸 `write(2)` 到独立 crash 文件**（`crash-<pid>-<time>.log`）：不经 logger 管线、不 malloc、不取任何锁、不碰 stdio 缓冲——与主日志文件物理分开（主日志 fd 可能已处于坏状态）。
4. **恢复默认处理器 + 重新 raise**：handler 写完后 `signal(sig, SIG_DFL)` + `raise(sig)`——保证 core dump 正常产出（handler 里吞掉信号 = 丢 core，是最常见的自伤）。

红线（评审级）：崩溃路径禁用 malloc/锁/stdio/线程 join——C-32（gateway 信号处理器调 stop()）为既录反例，不得复发。

### 4.3 core dump 运维面

- **RLIMIT_CORE=unlimited**：main 早期 setrlimit（部署环境的 ulimit 不可假设）。
- **core_pattern**：指向独立分区的目录（或 systemd-coredump 管道）——不与日志同盘更不与数据盘同盘；磁盘水位检查 + 按数量/年龄轮转（core 上传归档后清理）。
- **CI 验证**：测试目标主动 `raise(SIGSEGV)`，断言 crash 文件内容（含 backtrace 帧）与 core 产出——崩溃路径是被测路径。
- core 上传/归档归 apps/ 运维工具（与 machined 死亡上报同批，G-1 接线）。

## 5. 集中收集进程（apps/logger，P3）与 BI 分界

> 2026-10-10：ADR-013 立项切分的落地前置设计已立 [logger-app](./logger-app.md)——apps 接线零实况（七 app 全 std::cout）A 级实读 + v1 采集形态三岔 + 五拍板点（候选倾向 = tail 拉 v1）。

- **形态**：独立收集进程（apps/logger），各进程 logger 线程双出口——本地文件（真相源）+ push collector（经 net-abstraction §5.7 InterServerLink——对上层开放的进程间稳定连接设施；BW logger_endpoint / KBE logger 组件 / skynet logger 服务同型）。**不占游戏会话四通道**（net-abstraction §4.1 的 movement/attributes/events/control 是客户端会话面；InterServerLink 是进程间域，天然分离）。
- **collector 职责**：汇聚归档、按进程/级别/时间检索、磁盘滚动；BI 分流在 collector 侧接（attribute-sync §8.2 的 BI 旁路——业务进程不必同时喂两套出口，脚本错误审计 scripting-lua §6 与运行日志在此汇合）。
- **失败语义**：collector 挂 → 各进程只写本地（断连检测 + 缓冲上限 + 重连），零业务影响、零进程退出——**收集永远不构成进程的运行依赖**。
- **每进程文件布局**：`log/<app>-<instance>.log`（主日志）+ `log/crash-<pid>-<time>.log`（崩溃摘要）+ core 分区目录；实例号区分同机多进程（用户要求：每个进程有自己的日志）。

### 5.1 外采边界：单一 exporter、tail 采集与 trace 子集（2026-09-30 补，design-gap-inventory #10）

G-5 与本文件此前只定了**内部**形态（MetricRegistry/collector/两截落位）；「内部 → 外部标准栈（Prometheus/Kafka/ES）」的接出契约此前零设计，本节定型（方向 2026-09-30 定调，细则随 P3 批）。

**三禁先行**（评审红线）：游戏进程**不直连 Kafka**（消息队列是采集侧组件，不是游戏进程依赖——外采器 tail 文件即解耦）、**不引 OTel SDK 全家桶**（OTLP exporter 后台线程/每 span 分配与 tick 纪律冲突——借数据模型不借实现）、**不开 per-process HTTP 端口**（每进程一个暴露面 = 攻击面 + 端口管理双负担）。

- **单一 exporter（admin 进程，吐 /metrics）**：admin 是既有规划的运维单入口（scripting-lua §7）；它消费 G-5 control 通道上行的各进程 MetricRegistry 聚合，以 Prometheus exposition 文本格式吐 /metrics——游戏进程零新增端口。**不引 prometheus-cpp**（文本格式自拼，几十行；外部标准栈的兼容点在文本协议，不在客户端库）。
- **日志外采 = 结构化行 + 稳定目录 + 外挂 tail**：行格式定型为结构化键值（`ts=wall_ms level= proc= tick= cat= msg=` + kv 扩展段——键集稳定、引号规则进 P3 批）；目录即上节布局（`log/<app>-<instance>.log`）；轮转 rename 原子（外采器 inode 跟随语义）；filebeat/fluent-bit 类采集器 tail 上送 Kafka/ES——**Kafka 在采集器之后**，游戏进程不知其存在。collector（apps/logger）与外采 tail 的关系：collector 管进程间汇聚检索（内部），tail 管标准栈上送（外部）——两出口并行不互斥，本地文件同为真相源。**落地（2026-10-07，P3-3 批 C）**：六键固定序 + 引号规则 + LogContext（proc/tick 注入）+ FileAppender `structuredOutput` 开关（默认关，apps/LoggerApp 接线批打开）——键集与转义细则以 `modules/core/log/include/apollo/core/log/structured.h` 头注为契约源（logfmt 族可解析，单行保证：值内换行转义）。
- **trace 子集（借模型不引 SDK）**：字段 = trace_id/span_id + W3C traceparent 语义（含采样标志）；**生成在进程入口**（客户端会话首触点/跨进程消息首生成处，`InternalMessageEnvelope.trace_id` 已是信封标准字段——net-abstraction §7）；**span 界定 = 消息边界**（一条消息的处理跨度为一段，跨进程两 span 同 trace_id）；**头部采样极低率**（默认 1/10⁴，配置常量，入口进程决定）；采样命中的 span 以结构化行进日志流（随 tail 上送）——不引 OTLP exporter。用途限定：慢链路定位（「登录 800ms 慢在哪一跳」），不做全链路追踪系统。

### 5.2 告警出口与 webhook 默认实现族（2026-09-30 补，architecture-review §19.5.2 / P-5）

§5.1 定了「内部 → 标准栈」的**数据**接出（/metrics + tail + trace 子集）；本节补最后一跳：**告警判定与通知发送**落在哪里、默认给什么。

**双层边界（先裁落点）**：

- **层 A（标准栈，生产推荐）**：Prometheus scrape /metrics + alerting rules + **Alertmanager**——告警规则、静默、分组、路由是 Alertmanager 的本职，apollo 仓库零代码；企业微信/钉钉等接收器用社区 adapter（prometheus-webhook 族）承接，同样不进仓库。有标准栈的部署**不走层 B**。
- **层 B（内置兜底，小部署可用）**：exporter（admin 进程）内自带**阈值 notifier**——无 Prometheus 的单机/开发部署零外部依赖即得告警。两层共用同一事件源（MetricRegistry 聚合值），不共用代码路径（层 B = 规则表 + 模板渲染 + 出站 POST，几十行量级）。

**层 B 形态（本节主体）**：

- **规则表驱动**：`{metric, op, window, threshold, period, silence_min, severity}`——如 `script_fault_total{module=*} rate 5min > 10 → severity=crit, silence 30min`。规则进配置非代码；scripting-lua §6.1 的三级计数器（`script_fault_total`/`script_budget_warn_total`）即典型输入。
- **出站复用 net-abstraction §5.10 curl 设施**（HTTP 客户端只在 exporter 进程——三禁的自然延伸：**游戏进程永不直发 webhook**；Lua 层不开 HTTP 出站能力：攻击面 + 同步 30s 超时违 tick 纪律双否决）。
- **四家默认模板**（机器人 webhook 本质 = JSON 模板 + 一次 POST；字段以各家开放平台现行文档为准，模板进配置非硬编码）：

| 目标 | 出站与鉴权 | 消息体 | 备注 |
|---|---|---|---|
| 企业微信 | POST `…?key=<token>`（key 即凭证，无签名） | `{"msgtype":"markdown","markdown":{"content":…}}` | 机器人限频 20 条/min |
| 钉钉 | `…?access_token=…&timestamp=&sign=`；sign = HMAC-SHA256(secret, `ts\n`+ts) base64 | `{"msgtype":"markdown","markdown":{"title":…,"text":…}}` | 加签机器人；限频 20 条/min |
| 飞书 | POST webhook URL；自签机器人附 `timestamp + sign`（HMAC-SHA256 base64） | `{"msg_type":"text"/"interactive",…}` | 卡片消息可选 |
| Slack | POST incoming webhook URL（URL 即凭证，无签名） | `{"text":…}` | — |

- **发送纪律（评审红线级）**：① 重试指数退避 ≤3 次（5s/15s/60s），仍失败**回写本地日志**——告警不丢，回到 §1「本地文件是真相源」原则；② 同规则静默窗内聚合计数、窗末合并一条（防风暴）；③ 全局速率上限（默认 10 条/min，超限合并为 digest 一条——对接各家 20 条/min 限频留余量）；④ 密钥走配置/env（`APOLLO_WEBHOOK_*`），不进 git、不进日志；⑤ 出站目标进**白名单**（域名/网段进配置、运行期不可加——net-abstraction §5.10 SSRF 同纪律，exporter 也不开放任意拨号）；⑥ 模板变量 = 事件字段封闭集（§5.1 结构化行键集 + scripting-lua §6.1 ScriptFaultEvent 字段），不做自由表达式（防注入）。

**与 §5.1 的关系**：三禁不动——webhook 是 exporter 进程的出站动作，不是游戏进程的新依赖；Kafka 仍在采集器之后（告警事件若要走 Kafka，由 tail 流承接，不经 notifier）；告警事件本身作为结构化行照常落本地日志（notifier 发送失败时它就是唯一留证）。

## 6. 现状收敛（C-25/C-26）与归属

- **现状**（architecture-review §11.5，行号见彼处）：四套并存——①遗留 `apollo::utils::logging`（消费方仅 examples）②顶层 `include/apollo/core/log` 内建栈 ③`modules/core/log`（vcpkg 无 spdlog，实际回落内建）④内存版 LogManager；外加同路径双头文件 ODR 陷阱与 C-26 孤儿 TU（`modules/core/src/log/log_manager.cpp:30-33` 无任何目标编译，默认构建按推演链接失败）。
- **收敛方向**：**modules/core/log 单套**——删 ①，④降级为测试专用或删除；C-26 处置（编入或删）是第一刀。本设计的分级/旁路/crash 面在该模块内落地。
- **收口交付**（2026-10-07，P3-3 批 A）：套 ① 删除（头树 include/apollo/utils/logging/ + 源 + legacy_compat CMake 行，零消费）；套 ② 顶层死树删除（291 行死版 log_manager.h 不迁，同名双头 ODR 陷阱消除；8 头 git mv 入 modules/core/log/include，.h/.hpp 同目录并存、零 include 串改动）；套 ③ 即收口单套（root/modules CMake 死源行清除）；套 ④ 已随 P0-6 先行删除。test_log.cpp 在活版 API 直接过 = 方法面兼容实证；门禁 ON/OFF 34/34、Examples 32/32。本设计的分级/旁路/crash 面仍未落地——见 §7 P1 余项。
- **归属**（architecture-review §16.8.3 判据套用）：log API/缓冲/线程/分级/crash handler → **modules/core/log**（「被运行期链接 + 认进程留证语义」）；sigaltstack 装配点在 modules/base 线程封装（一处）；collector 进程 → **apps/**（「只有运维跑」；BW server/tools、KBE tools/logger 先例——16.8.2 表「聚合呈现在外挂工具」）；依赖方向铁律照旧 apps→modules 单向。
- **与 G-5 的关系**：logger 是 skynet 三板斧之一（architecture-review §16.2/G-5 已引）；错误率/级别计数进 MetricRegistry（趋势），本设计的文件与条目是现状留证——「metrics=趋势、watcher/日志=现状」口径不变。

## 7. 分期

- **P1（单进程就该有）**：四套收敛为 modules/core/log 一套（含 C-26 处置）→ 分级缓冲 + ERROR 旁路 + FATAL 同步落盘 → crash handler 四件套 + RLIMIT_CORE/core_pattern 运维配置 + CI 崩溃测试。验收：`raise(SIGSEGV)` 的测试进程留下「崩溃摘要文件 + core + 主日志最后一条 FATAL」三件全。
- **P3（fleet）**：apps/logger collector + push 通道 + 断连降级 + BI 分流接线 + core 上传归档（与 G-1 machined 死亡上报同批）+ 告警 notifier（§5.2 层 B：规则表 + 四家 webhook 模板，随 exporter 同批）。

## 8. 与其余设计的交集

| 关联设计 | 落点 |
|---|---|
| architecture-review.md | C-25/C-26 现状审计与收敛清单；C-32 信号处理反面教材（崩溃面红线）；G-5 三板斧中的 logger 项；§16.8.3 归属判据 |
| net-abstraction.md | G-5 两截落位（检测原语在模块内/聚合工具在 apps/——collector 即后者）；collector push = §5.7 InterServerLink 的上层消费者（进程间稳定连接，独立于客户端会话四通道）；Link 有界排队与 §4.1「collector 挂 → 只写本地」同源语义 |
| scripting-lua.md | §6 脚本错误审计走 BI 旁路（与运行日志分界，在 collector 汇合）；§7.3 错误率进 metrics；符号不剥离纪律共享 |
| attribute-sync.md | §8.2 BI 分流（collector 侧接线）；§10.2 停机序（logger 最后停、drain 后关文件）；磁盘水位与低级别停写的同型预算思维 |
| ssengine-reference.md | 「头文件注释撒谎」教训（§5）同样适用：崩溃路径的宣称必须被 CI 测试证实，不接受注释自述 |

---

*基线：apollo 现状引 architecture-review §11.5（C-25/C-26，源码基线 35a9c528 时实读）与 C-32（§12）；三家框架行号对应各自工作副本本轮实读（skynet skynet-src/skynet_start.c:287-291、service-src/service_logger.c:9-27；KBEngine kbe/src/lib/helper/debug_helper.{h,cpp}:217-218/:973-986/:1177-1180、kbe/src/lib/server/signal_handler.cpp:22-27/:112、kbe/src/server/tools/logger/；BigWorld lib/server/signal_processor.cpp:29-34、lib/cstdmf/callstack_linux.cpp:228）。2026-09-29 首次落盘（设计语料的日志空白——G-5 只接了观测通道，运行日志与崩溃取证此前零落点）。2026-09-30 增补 §5.1 外采边界（design-gap-inventory #10——三禁/单一 exporter/tail 采集/trace 子集；net-abstraction §7 P3 观测行与本节互引）。2026-09-30 增补 §5.2 告警出口与 webhook 默认实现族（architecture-review §19.4 P-5——双层边界（层 A Alertmanager 零代码/层 B exporter 内置 notifier）/规则表驱动/四家模板（企业微信/钉钉/飞书/Slack）/发送纪律六条；§19.5.2 候选方向的正式落盘）——摘要 8、§7 P3 行联动。*
