# Apollo 设计缺口清单(design-gap-inventory)

> 状态:登记性文档(滚动更新,状态列随设计批回落填)。定位:回答「六份设计文档(attribute-sync / logging / net-abstraction / scripting-lua / sdk-contract / xml-generation)+ docs/analysis 评审链(architecture-review / ssengine-reference / mmo-mechanism-deep-dive)之后,**还有哪些细节没有设计**」——本清单是后续设计批次的排程索引与防重漏底账。口径日期 2026-09-30;每条缺口带现状证据(负空间检索或存量实读,行号为本会话实测),§1 先列已覆盖项防误报。与 architecture-review §16.10.2 登记簿互为引用:本文件管「设计面缺什么」,登记簿管「审计链登记了什么」。

---

## 0. 口径与方法

- **「已设计」判据**:上述九份文档中有权威载体——有节、有决策、有分期落点;仅提及一词不算。
- **「缺口」判据**三选一:(a) 负空间——关键词全仓/全文档零命中;(b) 存量冒充——代码存在但形态与设计目标冲突且无处置设计;(c) 占位——设计文本仅方括号占位符(如 sdk-contract.md:206 `[加密 P3]`)。
- **来源**:2026-09-30 gap 分析会话(全量负空间检索 + 逐条核实);同日用户六问(logger/kafka、OTel、Prometheus、BI、openssl、http/curl)全部并入——#4 扩充加密库选型、新增 #10(观测接出)/#11(HTTP 出站)两行,BI 经核实为已覆盖出口 + 边界声明(§4.3),不立缺口行。
- 状态词汇:OPEN(未设计)/ IN-PROGRESS / CLOSED(设计落盘后回填,附落点)。

## 1. 已覆盖项(防误报清单——先核后报)

gap 分析中曾被怀疑、经实读核实**已有权威载体**的主题,列出以防后续批次误报:

| 主题 | 权威载体 | 核实要点 |
|---|---|---|
| 优雅停机(G-3) | attribute-sync §10.2 | 六阶段停机序,logger 最后停 |
| 调度范式论证(G-7) | attribute-sync §10.1 | tick vs skynet 消息驱动的正面论证 |
| 进程编队/服务发现(G-1)、备份容灾(G-2)、负载均衡与异常恢复、进程间共享模型 | net-abstraction §7「P3 前置设计」 | machined+UDP 双层 / backup-hash 链+reviver / 排他恢复相位 / owner-订阅四通道 |
| 定时器轮归属(G-4) | ssengine-reference §4.3 + architecture-review 16.10.1 | modules/base 新组件,驱动权 game loop |
| 实体契约继承(G-6) | sdk-contract §2.3(entities.xml)+ architecture-review §16.7.2 | 生成期展开、环检测、provenance |
| 运维观测通道两截(G-5 载体) | net-abstraction §7 P3 行 | 检测原语进模块 / 聚合工具进 apps/ |
| L1 帧头与 filter 链 | net-abstraction §3 / §5.5 | 16B 帧头(:62/:68)、FrameFilter 双族蓝本 |
| 进程间连接设施 | net-abstraction §5.7 InterServerLink | 连接器/重连/in-flight 语义/背压分界 |
| L0 传输模型 | net-abstraction §5.8 | reactor 基线、发送环提交语义、IOCP 不进路线图 |
| 运行日志与崩溃取证 | logging.md 全文 | 本地文件真相源 / ERROR 旁路 / crash 四件套 / collector P3 |
| 异步任务模型 | scripting-lua §8 | C++ 回调+request_id / Lua 协程 / 三纪律 |
| 热更协议与字节码缓存 | scripting-lua §3.2/§3.5 | 原子换表 / 版本指针持久化 / manifest 三元组 |
| 在线调试与状态内省 | scripting-lua §7(语义层 = architecture/observability 文档) | admin 单入口 / tick 边界 eval / 权限分级 |
| 契约版本与兼容窗口 | sdk-contract §6/§12 | schema_hash、N/N-1 窗口、compat_window_check(P3 job) |
| **BI 出口(2026-09-30 核实)** | attribute-sync §8.2(:288) | 属性变更事件收集阶段旁路采样导出,预算外;脚本错误审计(scripting-lua §6)与 collector(logging §5)汇合——**业务侧管道(数仓/报表/看板)不在 apollo 设计面,见 §4.3 边界声明** |
| **Redis 层定位** | attribute-sync §8.2(:289) | 只做跨进程共享热数据,不做实体属性缓存、不做二级缓存(§15.5 同判);部署细节缺 = 本清单 #9 |

## 2. 真空白清单(#1–#11)

### #1 时间与时钟模型 — **CLOSED**(docs/design/clock-and-time.md,2026-09-30 落盘)

- **缺什么**:三钟分工(steady 单调钟驱动 tick / 逻辑 tick 号 / wall 钟仅运维与持久化时间戳)、tick 号而非时间戳作为世界状态判定序、客户端校时只用于显示与回放对齐(不进判定)、跨进程时钟不互信(authority_epoch/route_version 已有,时间值本身的边界没有)。
- **证据**:design 六份零「时钟/单调钟/steady_clock/校时」命中;`GameClockService` 名存实亡——apps/game-server/src/main.cpp:20 实为 `struct GameClockService { std::string name = "clock"; }` 纯演示壳(实测复核)。深潜先例:BW/KBE 均 10Hz 固定 tick(mmo-mechanism-deep-dive §8),时钟作为独立主题两家也无成文设计——**apollo 补此件即超先例,不是抄先例**。
- **落点**:docs/design/clock-and-time.md——三钟分工决策表 + 三口诀红线(测时长只 steady/做判定只 tick/给人看只 wall)、TickSource 进程级单源/有界追赶(catch_up_max 2 拍,超则跳拍记 dropped_ticks)/三类合法暂停点、持久化 (tick, wall_ms) 双写、定时器轮 deadline=tick 号、跨进程比较只用 seq/位点/epoch、GameClockService 重写登记代码批。

### #2 战斗确定性与回放细则 — OPEN(随战斗玩法批次)

- **缺什么**:浮点跨端一致性策略(定点/fixed64/限 float 且禁双端差异运算)、容器迭代序确定性(同 tick 内实体/属性遍历序的稳定序源)、随机数(种子派生链/服务端权威流/客户端表现流分离)、回放 = seed + 有序输入序列的录制格式与重放验证。
- **证据**:「确定性/replay/lockstep/随机种子」design 六份仅 attribute-sync §10.1 tick 确定性一节(G-7 范畴,不覆盖浮点/迭代序/种子);lockstep 三仓负空间检索零命中(deep-dive §18)——回放无行业先例可抄,需自定;docs/25(战斗文档)已随 B 级清理删除,原承载的浮点/seed 讨论无权威载体。
- **落点计划**:新建 docs/design/battle-determinism.md(36 号 #16/#18 的缝在此收口)。

### #3 存储 DDL 生命周期 — OPEN(优先)

- **缺什么**:首次建表、随契约/存储演化的 ALTER(加列/加索引)、迁移的版本化与回滚边界、forward-only 还是允许 down-migration、禁 DROP 的数据安全线。
- **证据**:storage.xml 只定了「语句即数据」的运行语句面(architecture-review §15.5 三语句:快照/journal/列提升)——**DDL 语句面零设计**;「CREATE TABLE/ALTER/migration」design 全零命中。先例:KBE entity_table_mysql.cpp:113 ALTER TABLE ADD INDEX(启动期自改表——无版本化,反面参照)、BW server/tools/sync_db(独立工具族,契约邻接)。
- **落点计划**:attribute-sync §8.3(DDL 生命周期:版本化迁移目录 + 启动期 runner + forward-only + 禁 DROP)。

### #4 客户端通道安全语义 + 加密库选型 — OPEN(随 P3)

- **缺什么**:握手鉴权(证书/token 怎么发、与 login-app 的关系)、ECDH 密钥协商 + 对称加密算法(AES-GCM/ChaCha20-Poly1305)、seq 防重放、防 MITM/防重连劫持;**加密库选型**(2026-09-30 增问并入)。
- **证据**:sdk-contract.md:206 帧层图 `[加密 P3]` 方括号占位,全 design 无展开。加密库存量核实:openssl **已是 vcpkg.json 直接依赖**(:6,dependencies 第 1 项);websocket.cpp:38 用 `<openssl/sha.h>`(WS 握手);modules/net/CMakeLists.txt:135-160 built-in WebSocket 分支 `find_package(OpenSSL REQUIRED)`;contract_hash 刻意零加密库依赖(决策 #4 链接图最小化);libsodium 全仓零命中。**方向定调:OpenSSL 单一 crypto 源,不引第二套**——选型论证与用法纪律(算法分级/版本锁定/禁 MD5 等)待 P3 批落文。
- **落点计划**:net-abstraction 增补(客户端通道安全节,引用 login-app;filter 链位次已定——sdk-contract §10.5:压缩之后)。

### #5 容量模型与基准设施 — OPEN(随 P3)

- **缺什么**:单进程 CCU/实体数/属性变更率/带宽的预算表(设计文档各处散落数值:5000 CCU、25k msg/s、movement <100B——无一处汇总与推导);基准 harness(契约回放/压测拓扑)归属与形态;「M2+ 每项先有基准再动手」(net-abstraction §5.6)的基准从哪来。
- **证据**:「基准/benchmark/压测」design 零成文;attribute-sync §11 有度量验收表(单件级)无系统级容量模型。
- **落点计划**:新建 docs/design/capacity-and-benchmark.md。

### #6 GM/运营命令面 — OPEN(随 P3)

- **缺什么**:玩法 GM 指令表(发道具/封禁/踢人/改属性——白名单与契约 predict 位的关系)、GM 角色权限分级(与 observability 文档 AccessController 的关系:框架侧已定 Passive/Controlled,玩法侧未接)、GM 命令审计存储(谁何时对谁执行了什么)。
- **证据**:scripting-lua §7 在线调试是**框架侧**(eval/状态面);「GM」在设计文档中仅作为 admin 消息来源出现,指令语义面零设计。GM 校验在 scripting-lua §1 职责表有一行(GM 校验进 Lua),细则无。
- **落点计划**:scripting-lua §7 增补 GM 命令面子节(或独立小节)。

### #7 C++ 内存与对象池预算 — OPEN(随 P3)

- **缺什么**:实体/属性容器/消息对象的对象池策略、每进程内存预算表、Lua 状态上限已有(scripting-lua §6)但 C++ 侧无对应物、jemalloc/tcmalloc 选型立场。
- **证据**:「对象池/内存池/pmr」design 零命中;16.8.3 归属论证触及 modules/base(线程/内存/ID)但只定位置不定策略。
- **落点计划**:并入 capacity-and-benchmark.md(#5 同批)。

### #8 上行 intent/消息限流参数面 — OPEN(随 P3)

- **缺什么**:上行四通道(movement/attributes/events/control)各自的 per-session 速率上限、超限处置(丢弃/断开/警告)、与 schema 校验失败计数的联动、参数默认值表。
- **证据**:下行有完整预算体系(attribute-sync §5 token bucket + net-abstraction §4.2 水位);**上行只有「intent only,服务端定夺」一句**(sdk-contract §2.3 msg 注释)——恶意客户端侧的限流与预算零设计。BW LoginConditions 准入闸门(net-abstraction §7)是登录时点,不管持续上行。
- **落点计划**:net-abstraction 增补(§4 上行侧小节)。

### #9 Redis 部署与运维细节 — OPEN(随 P3)

- **缺什么**:部署拓扑(单实例/哨兵/集群)、连接池参数、键空间设计与 TTL 纪律、与 attribute-sync §8.2 定位的边界(只做跨进程共享热数据)配套的容量与淘汰策略。
- **证据**:attribute-sync:289 已定**定位**;部署/键空间/TTL 零展开。存量 redis 四套并存(C-45)是审计问题不是设计缺口,收敛方向已定(§15.2 纪律)。
- **落点计划**:attribute-sync §8.3 随 #3 同批补一小节(或 §8.4)。

### #10 观测接出形态(Prometheus/日志外采/trace)— OPEN(随 P3;2026-09-30 增问)

- **缺什么**:三件事——① **Prometheus expose 落点**:36 号 #14 已定「外部标准栈」方向,但 exporter 形态未定;② **日志外采边界**:游戏进程不直连 Kafka/filebeat 类采集器的接口契约(结构化行格式 + 稳定目录 + 轮转纪律);③ **trace 子集**:采样策略、透传字段、汇聚点。
- **证据**:prometheus/grafana/kafka/otel 全仓零命中(负空间实测);MetricRegistry/logging collector/G-5 两截已定**内部**形态,「内部→外部系统」的接出契约零设计。**trace 元缺口并轨于此**:trace_id 已是 InternalMessageEnvelope 信封标准字段(net-abstraction:351,remote-entity-call 四件套),但采样率/透传链(信封→日志行→collector)/跨度界定(消息边界为 span)无设计。
- **方向定调(增问的答案,细节随 P3 批落文)**:游戏进程不直连 Kafka、不引 OTel SDK 全家桶(OTLP exporter 后台线程/每 span 分配与 tick 纪律冲突)、不开 per-process HTTP 端口——出口收敛为:① 单一 exporter(admin 进程,读 MetricRegistry/collector 聚合吐 /metrics,游戏进程零新增端口);② 日志 = 结构化行格式 + 稳定目录供外挂采集器(filebeat/fluent-bit)tail(collector 挂了只写本地已定,logging §5);③ trace 借数据模型子集(trace_id/span_id + W3C traceparent 语义),span 界定 = 消息边界,极低采样,汇聚走 collector,P3。
- **落点计划**:net-abstraction §7 P3 行扩充或独立小节 + logging.md 增补。

### #11 出站 HTTP 客户端与第三方接出 — OPEN(随 P3;2026-09-30 增问)

- **缺什么**:服务端出站 HTTP(第三方登录/支付/推送回调)的依赖收口与线程模型接线——同步 30s 超时 API 不得在场景线程直调(须走 scripting-lua §8 异步交接同型);Drogon 备选分支的收口(§15.2「禁止并存」纪律的对象)。
- **证据(存量实读,2026-09-30)**:modules/net/http **已存在 2541 行**(rest_client.h 343 + rest_client.cpp 734 + http.cpp 816 + event_loop.cpp 648),namespace `apollo::net::http`,RestTemplate 是 Spring RestTemplate 风格(HttpMethod 枚举/HttpResponse/HttpEntity/RequestOptions{timeoutMs=30000, connectTimeoutMs=10000, verifyPeer, proxy});rest_client.cpp:13/#ifdef `APOLLO_HAS_CURL`、:16 `#define APOLLO_CURL_STUB 1`——**vcpkg.json 无 curl,默认构建全桩**(与 C-45 宏门 MySQL 同族);modules/net/CMakeLists.txt 另有 Drogon 备选分支(:81-91 http、:124-133 websocket)与 built-in 分支并存;**零生产消费方**(仅 examples/http_demo、tests/test_rest_template、docs/api/net.md);net-abstraction.md:29 现状盘点行将 HTTP/WebSocket 判为「与本设计正交,另行处理」——本行即「另行处理」的登记。
- **落点计划**:net-abstraction 增补出站 HTTP 小节(依赖收口 = curl 进 vcpkg 或删桩、线程模型 = L0 执行层挂 §8.2 异步交接、Drogon 分支裁决);**modules/net/{http,websocket} 登记为下轮审计候选**(见 §4.2 与登记簿——C-29 四栈盘点与 16.8.1 版图普查均未覆盖此两树)。

## 3. 已登记推迟项(登记簿管辖,不重复立项)

以下属「已设计/已判定、等代码阶段」而非设计缺口,状态以 architecture-review §16.10.2 登记簿为准:R-17a…R-17g(DI 域七项)、config 桩清理/FrameFilter 管线/继承生成器/定时器轮组件(代码影响项)、ipc 树与 bw/bigworld 兼容层(下轮审计候选)、36 号 #15/#17 表述修正、sdks/contract 旧名同步、origin 五笔源码提交回退与 backup-apollo-src(用户裁决项)。

## 4. 结构性元缺口

### 4.1 设计资产状态表 v1(docs/architecture/ 70 份与 design 六份的权威关系)

- **问题**:docs/architecture/ 存量 70 份,与 docs/design 六份权威稿的关系无登记——哪份仍有效、哪份被取代、哪份是占位,靠各人记忆。跨文档引用一旦指向已废稿即产生漂移(C-3 文档纪律的盲区)。
- **v1 口径(证据驱动,不做全量分类)**:以 design/analysis → architecture/ 的**实引关系**为锚点,分三档——
  - **实引有效(5 份)**:observability-watcher-and-runtime-introspection-design.md(scripting-lua §7 语义层)、remote-entity-call-design.md(net-abstraction §7 语义层)、starter-and-module-assembly-design.md(评审 §1/§4 引,历史定位=「不做 Spring 克隆」的正面表述源)、harbor-cluster.md 与 hot-reload.md(仅被 mmo-mechanism-deep-dive 引作「占位文档」证据——**列为待评审**:内容与 design 六份的关系未定);
  - **其余 ~65 份**:未被判废也未被判活——登记为「待盘点桶」,逐批消化(每设计批次顺手核与其主题相关的 architecture 稿);
  - 已删除 37 份(docs 根 B 级清理,登记簿有行)不在本表。
- **落点**:本节即状态表 v1;后续批次回填状态列。不另建新文件。

### 4.2 新审计候选:modules/net/{http,websocket}

16.8.1 版图普查与 C-29 四栈盘点(protocol/tcp/rpc + modules/protocol + include 双树)的**边界外存量**:modules/net/http(2541 行,证据见 #11)与 modules/net/websocket(built-in + Drogon 双分支)。二者与 §15.2「禁止第五套网络栈」纪律的关系未经审计——WebsocketFrameFilter 化(net-abstraction §5.5 判定:WS 是流 filter 不新起栈)意味着 built-in WS 栈的处置方向已有,但代码面审计未做。**已登记 architecture-review §16.10.2(本批补行)。**

### 4.3 BI 边界声明(2026-09-30 增问的核实结论)

BI 相关的**服务器侧出口已覆盖**:attribute-sync §8.2 属性变更事件旁路采样(预算外,不允许 BI 需求污染属性表结构)+ logging §5 collector 汇合脚本错误审计。**BI 的业务侧管道(数仓建模/报表/看板/离线分析)不在 apollo 设计面**——那是数据团队消费上述出口的自有系统;apollo 的义务止于「出口稳定、格式自描述、不丢关键事件」。此声明防止两件事:BI 需求倒灌进属性表结构(§8.2 已禁)与在 apollo 内重复设计数仓。

## 5. 优先级与批次计划

| 批 | 内容 | 缺口 | 依据 |
|---|---|---|---|
| B1 | 本清单落盘 + 登记簿补行 | 元缺口 4.1/4.2 | 用户指令「落盘吧」 |
| B2 | 新建 clock-and-time.md | #1 | 用户点名最先;**已完成**(b8ebed98 后续设计批) |
| B3 | attribute-sync §8.3 DDL(+§8.4 Redis 细则) | #3/#9 | 用户点名最先 |
| B4 | 新建 battle-determinism.md | #2 | 随战斗玩法 |
| B5 | net-abstraction 增补:通道安全+加密库 / 观测接出 / 上行限流 / 出站 HTTP | #4/#10/#8/#11 | 随 P3 |
| B6 | 新建 capacity-and-benchmark.md(含内存/对象池) | #5/#7 | 随 P3 |
| B7 | scripting-lua GM 命令面增补 | #6 | 随 P3 |
| 收尾 | 各批落盘后回填本表状态列;登记簿同步 | — | 滚动 |

每批独立提交(analysis/design 拆分照旧),完成即 fetch --rebase + push(推送纪律)。

---

*基线:apollo main @ 05919db9(文档态);源码证据为只读实读(vcpkg.json / modules/net/http 全文件行数 / rest_client.cpp 宏门 / main.cpp GameClockService / 负空间检索 kafka|otel|prometheus|grafana|libsodium 零命中——均 2026-09-30 本会话实测)。B 级文档清理后的引用基线按登记簿行(1e37073d 可溯)。*
