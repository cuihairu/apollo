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

## 2. 真空白清单(#1–#17)

### #1 时间与时钟模型 — **CLOSED**(docs/design/clock-and-time.md,2026-09-30 落盘)

- **缺什么**:三钟分工(steady 单调钟驱动 tick / 逻辑 tick 号 / wall 钟仅运维与持久化时间戳)、tick 号而非时间戳作为世界状态判定序、客户端校时只用于显示与回放对齐(不进判定)、跨进程时钟不互信(authority_epoch/route_version 已有,时间值本身的边界没有)。
- **证据**:design 六份零「时钟/单调钟/steady_clock/校时」命中;`GameClockService` 名存实亡——apps/game-server/src/main.cpp:20 实为 `struct GameClockService { std::string name = "clock"; }` 纯演示壳(实测复核)。深潜先例:BW/KBE 均 10Hz 固定 tick(mmo-mechanism-deep-dive §8),时钟作为独立主题两家也无成文设计——**apollo 补此件即超先例,不是抄先例**。
- **落点**:docs/design/clock-and-time.md——三钟分工决策表 + 三口诀红线(测时长只 steady/做判定只 tick/给人看只 wall)、TickSource 进程级单源/有界追赶(catch_up_max 2 拍,超则跳拍记 dropped_ticks)/三类合法暂停点、持久化 (tick, wall_ms) 双写、定时器轮 deadline=tick 号、跨进程比较只用 seq/位点/epoch、GameClockService 重写登记代码批。

### #2 战斗确定性与回放细则 — **CLOSED**(docs/design/battle-determinism.md,2026-09-30 落盘)

- **缺什么**:浮点跨端一致性策略(定点/fixed64/限 float 且禁双端差异运算)、容器迭代序确定性(同 tick 内实体/属性遍历序的稳定序源)、随机数(种子派生链/服务端权威流/客户端表现流分离)、回放 = seed + 有序输入序列的录制格式与重放验证。
- **证据**:「确定性/replay/lockstep/随机种子」design 六份仅 attribute-sync §10.1 tick 确定性一节(G-7 范畴,不覆盖浮点/迭代序/种子);lockstep 三仓负空间检索零命中(deep-dive §18)——回放无行业先例可抄,需自定;docs/25(战斗文档)已随 B 级清理删除,原承载的浮点/seed 讨论无权威载体。
- **落点**:docs/design/battle-determinism.md——**先裁域后定约束**(服务器权威 → 确定性问题收缩为「同 binary 同平台重放一致」,消费方 = 回放复算/跨进程卸载/AI 重算三同构);**四约束**:判定域唯一(场景线程 tick 边界,异步不进判定)/浮点纪律(IEEE754+编译档锁 `-ffp-contract=off`/`-fno-fast-math`/禁 `-march=native`,**不做定点**)/迭代序(模拟段不要求序,结算段按 target entity_id 全序)/随机数(`(world_seed, tick, stream_id)` 子流派生 PCG32,stream 分域 combat_roll/drop/ai_decision/proc,AI 重算不录制);**回放四元组**(binary_id/world_seed/快照位点/输入序列)+ 事件流滚动 hash 链对照,观战=事件流重演与复算=重模拟两形态分开;Lua 侧禁 pairs 结算/os.time/math.random(沙盒裁+注入);前置 = C-34 ECS 收敛;36 号 #16/#18 缝收口(§7),docs/25 NNG 引用按 §15.2 退役改接 InterServerLink。

### #3 存储 DDL 生命周期 — **CLOSED**(attribute-sync §8.3,2026-09-30 落盘)

- **缺什么**:首次建表、随契约/存储演化的 ALTER(加列/加索引)、迁移的版本化与回滚边界、forward-only 还是允许 down-migration、禁 DROP 的数据安全线。
- **证据**:storage.xml 只定了「语句即数据」的运行语句面(architecture-review §15.5 三语句:快照/journal/列提升)——**DDL 语句面零设计**;「CREATE TABLE/ALTER/migration」design 全零命中。先例:KBE entity_table_mysql.cpp:113 ALTER TABLE ADD INDEX(启动期自改表——无版本化,反面参照)、BW server/tools/sync_db(独立工具族,契约邻接)。
- **落点**:attribute-sync §8.3——db/migrations 版本化目录 + schema_migrations 位点表(唯一真相源)+ forward-only 无 down + migration lint 禁破坏性语句(DROP/TRUNCATE/改语义 RENAME 拒载,加列必须 NULL-able/带默认值保 journal 重放兼容)+ 启动期 runner(fail-fast,执行期持锁,单进程文件锁/P3 编队锁)+ 大表 ALTER INPLACE/LOCK=NONE 声明 + 影子库预演归 apps/ db 工具;契约 column:true 变更只出 CI diff 提醒不自动产迁移;xml-generation §4「明确不生成」行同步交叉引用。

### #4 客户端通道安全语义 + 加密库选型 — **CLOSED**(net-abstraction §5.9,2026-09-30 落盘)

- **缺什么**:握手鉴权(证书/token 怎么发、与 login-app 的关系)、ECDH 密钥协商 + 对称加密算法(AES-GCM/ChaCha20-Poly1305)、seq 防重放、防 MITM/防重连劫持;**加密库选型**(2026-09-30 增问并入)。
- **证据**:sdk-contract.md:206 帧层图 `[加密 P3]` 方括号占位,全 design 无展开。加密库存量核实:openssl **已是 vcpkg.json 直接依赖**(:6,dependencies 第 1 项);websocket.cpp:38 用 `<openssl/sha.h>`(WS 握手);modules/net/CMakeLists.txt:135-160 built-in WebSocket 分支 `find_package(OpenSSL REQUIRED)`;contract_hash 刻意零加密库依赖(决策 #4 链接图最小化);libsodium 全仓零命中。**方向定调:OpenSSL 单一 crypto 源,不引第二套**——选型论证与用法纪律(算法分级/版本锁定/禁 MD5 等)待 P3 批落文。
- **落点**:net-abstraction §5.9——加密 = FrameFilter 族 CryptoFilter(§5.5 槽位,压缩后位次);威胁模型裁域(防窃听/重放/劫持/MITM,**非目标 = 客户端逆向**——加密防网络第三方不防持客户端的玩家);握手序列(login_token 归 login-app/X25519+HKDF-SHA256/per-frame nonce 由 seq 派生/resume 重走 ECDH 前向保密);算法分级禁用表(MD5/SHA-1 新代码/RC4/DES/裸 ECB/无 MAC CBC 禁);**OpenSSL 单一 crypto 源定案**(EVP 接口/CRYPTO_memcmp/RAND_bytes 纪律,版本锁 3.x LTS);WSS 兜底 P3 随 gateway。

### #5 容量模型与基准设施 — **CLOSED**(docs/design/capacity-and-benchmark.md,2026-09-30 落盘)

- **缺什么**:单进程 CCU/实体数/属性变更率/带宽的预算表(设计文档各处散落数值:5000 CCU、25k msg/s、movement <100B——无一处汇总与推导);基准 harness(契约回放/压测拓扑)归属与形态;「M2+ 每项先有基准再动手」(net-abstraction §5.6)的基准从哪来。
- **证据**:「基准/benchmark/压测」design 零成文;attribute-sync §11 有度量验收表(单件级)无系统级容量模型。
- **落点**:docs/design/capacity-and-benchmark.md——顶层目标(5000 CCU/100 人同屏)+ 推导链四步;**口径统一**(主循环 10Hz = clock-and-time 定案,sdk-contract §10.6「20Hz」估算旧口径修正);帧预算表(100ms → 消息段 20/tick 六阶段 70/富余 10ms,初始分配基准校准);基准**三形态**(微基准 ctest / 场景基准 apps/bench 合成 intent 流·确定性 RNG 可复现 / 录制回放 = battle-determinism §5 同格式);M2+ 准入基准设施化(before/after + 显著性);系统级验收指标集九项(跳拍率/水位触发率/violation_score 断开率/帧分位等——clock-and-time §10 引用的「容量批 §5」即此)。

### #6 GM/运营命令面 — **CLOSED**(scripting-lua §7.5,2026-09-30 落盘)

- **缺什么**:玩法 GM 指令表(发道具/封禁/踢人/改属性——白名单与契约 predict 位的关系)、GM 角色权限分级(与 observability 文档 AccessController 的关系:框架侧已定 Passive/Controlled,玩法侧未接)、GM 命令审计存储(谁何时对谁执行了什么)。
- **证据**:scripting-lua §7 在线调试是**框架侧**(eval/状态面);「GM」在设计文档中仅作为 admin 消息来源出现,指令语义面零设计。GM 校验在 scripting-lua §1 职责表有一行(GM 校验进 Lua),细则无。
- **落点**:scripting-lua §7.5——管道零新增(与 §7.1 attach 同路:admin 单入口 → control 通道 → 场景线程 tick 边界);**指令表注册表驱动**(`gm_commands` 模块声明 + 参数 schema 校验,GM 面无 eval 权限);**gm_write 独立白名单**(contract.lua 内与 `predict` 分列,GM 写走属性钩子同路不绕管线);**level 三级 + 高危双人复核**(框架 AccessController 之上的业务粒度,BW/KBE 无此层——加强项);**gm_audit 独立审计表**(wall+tick 双写,拒绝也落,observability `/gm` 分支查询);GM 输入 = admin 会话 intent 流(battle-determinism 复算自动含);踢人/封禁执行位归 net close/login-app 风控。

### #7 C++ 内存与对象池预算 — **CLOSED**(capacity-and-benchmark §4,2026-09-30 落盘)

- **缺什么**:实体/属性容器/消息对象的对象池策略、每进程内存预算表、Lua 状态上限已有(scripting-lua §6)但 C++ 侧无对应物、jemalloc/tcmalloc 选型立场。
- **证据**:「对象池/内存池/pmr」design 零命中;16.8.3 归属论证触及 modules/base(线程/内存/ID)但只定位置不定策略。
- **落点**:capacity-and-benchmark §4——每进程内存预算表(实体属性 ≤1.5GB/Lua 每状态 64MB 数值化/net 收发 640MB/journal ≤512MB,稳态 RSS ≤4GB + 告警熔断线);**对象池三类**(帧对象/属性收集缓冲/发送环切片——free-list 挂 owning 线程单写者无锁,池大小 = 容量参数启动期分配,耗尽走降级不现分配);**jemalloc/tcmalloc 不引入**(单写者+三类池已收敛分配热点,全局分配器替换 = 过早优化 + 第二行为面;列 M2+ 基准准入——分配占帧 >5% 才评估)。

### #8 上行 intent/消息限流参数面 — **CLOSED**(net-abstraction §4.3,2026-09-30 落盘)

- **缺什么**:上行四通道(movement/attributes/events/control)各自的 per-session 速率上限、超限处置(丢弃/断开/警告)、与 schema 校验失败计数的联动、参数默认值表。
- **证据**:下行有完整预算体系(attribute-sync §5 token bucket + net-abstraction §4.2 水位);**上行只有「intent only,服务端定夺」一句**(sdk-contract §2.3 msg 注释)——恶意客户端侧的限流与预算零设计。BW LoginConditions 准入闸门(net-abstraction §7)是登录时点,不管持续上行。
- **落点**:net-abstraction §4.3——IO 线程解帧后投递场景线程**前**判(超限帧不进场景线程,计数器零锁);四通道参数表(movement 60/attributes 120/events 30/control 20 msg/s,control 硬超限直接断;总 8KB/s + 64KB 突发桶);三级处置(soft 丢+计数/hard 丢+throttle_notice/abuse 断开+账号级风控供 login-app 准入闸门);**violation_score 单桶**(限流计数+schema 校验失败+attribute-sync §9 权威纠正同权——入口闸与语义闸共享出口)。

### #9 Redis 部署与运维细节 — **CLOSED**(attribute-sync §8.4,2026-09-30 落盘)

- **缺什么**:部署拓扑(单实例/哨兵/集群)、连接池参数、键空间设计与 TTL 纪律、与 attribute-sync §8.2 定位的边界(只做跨进程共享热数据)配套的容量与淘汰策略。
- **证据**:attribute-sync:289 已定**定位**;部署/键空间/TTL 零展开。存量 redis 四套并存(C-45)是审计问题不是设计缺口,收敛方向已定(§15.2 纪律)。
- **落点**:attribute-sync §8.4——单实例起步/多机 Sentinel/Cluster 不进路线图(键空间规模论证);hiredis 统一客户端 + 连接池,同步面禁场景线程直调(走 scripting-lua §8 异步交接);键空间 `apollo:{域}:{世界}:{键}` 分层 + 每键 TTL 或显式永驻;Redis = 共享工作内存非真相源(整库丢失 = DB 重建 + 降级,不构成数据丢失事故);跨进程互斥不依赖 Redis 锁(权利判定归 G-1 mgr 单点定序,Redis 原子性只用于数据面)。

### #10 观测接出形态(Prometheus/日志外采/trace)— **CLOSED**(logging §5.1 + net-abstraction §7 P3 观测行引用,2026-09-30 落盘)

- **缺什么**:三件事——① **Prometheus expose 落点**:36 号 #14 已定「外部标准栈」方向,但 exporter 形态未定;② **日志外采边界**:游戏进程不直连 Kafka/filebeat 类采集器的接口契约(结构化行格式 + 稳定目录 + 轮转纪律);③ **trace 子集**:采样策略、透传字段、汇聚点。
- **证据**:prometheus/grafana/kafka/otel 全仓零命中(负空间实测);MetricRegistry/logging collector/G-5 两截已定**内部**形态,「内部→外部系统」的接出契约零设计。**trace 元缺口并轨于此**:trace_id 已是 InternalMessageEnvelope 信封标准字段(net-abstraction:351,remote-entity-call 四件套),但采样率/透传链(信封→日志行→collector)/跨度界定(消息边界为 span)无设计。
- **方向定调(增问的答案,细节随 P3 批落文)**:游戏进程不直连 Kafka、不引 OTel SDK 全家桶(OTLP exporter 后台线程/每 span 分配与 tick 纪律冲突)、不开 per-process HTTP 端口——出口收敛为:① 单一 exporter(admin 进程,读 MetricRegistry/collector 聚合吐 /metrics,游戏进程零新增端口);② 日志 = 结构化行格式 + 稳定目录供外挂采集器(filebeat/fluent-bit)tail(collector 挂了只写本地已定,logging §5);③ trace 借数据模型子集(trace_id/span_id + W3C traceparent 语义),span 界定 = 消息边界,极低采样,汇聚走 collector,P3。
- **落点**:logging.md §5.1(+ net-abstraction §7 P3 观测行引用)——**三禁**(不直连 Kafka/不引 OTel SDK/不开 per-process HTTP 端口);单一 exporter = admin 吐 /metrics(消费 G-5 control 通道上行,不引 prometheus-cpp,文本自拼);日志 = 结构化键值行 + 稳定目录 + 外挂 tail(filebeat/fluent-bit 上送,**Kafka 在采集器后**);trace 子集 = trace_id/span_id + W3C traceparent 语义、span = 消息边界、入口生成(信封 trace_id 既有)、头部采样 1/10⁴、采样行进日志流;collector(内部汇聚)与 tail(标准栈上送)两出口并行不互斥。

### #11 出站 HTTP 客户端与第三方接出 — **CLOSED**(net-abstraction §5.10,2026-09-30 落盘)

- **缺什么**:服务端出站 HTTP(第三方登录/支付/推送回调)的依赖收口与线程模型接线——同步 30s 超时 API 不得在场景线程直调(须走 scripting-lua §8 异步交接同型);Drogon 备选分支的收口(§15.2「禁止并存」纪律的对象)。
- **证据(存量实读,2026-09-30;行数与覆盖同日勘误回填,architecture-review §19.2 P-2)**:modules/net/http **已存在 5048 行**(rest_client.h 343 + rest_client.cpp 734 + http.cpp 816 + event_loop.cpp 648 + **websocket.cpp 1193 + 三公共头 1314**——登记时漏计后两项),namespace `apollo::net::http`,RestTemplate 是 Spring RestTemplate 风格(HttpMethod 枚举/HttpResponse/HttpEntity/RequestOptions{timeoutMs=30000, connectTimeoutMs=10000, verifyPeer, proxy});rest_client.cpp:13/#ifdef `APOLLO_HAS_CURL`、:16 `#define APOLLO_CURL_STUB 1`——**vcpkg.json 无 curl,默认构建全桩**(与 C-45 宏门 MySQL 同族);modules/net/CMakeLists.txt 另有 Drogon 备选分支(:81-91 http、:124-133 websocket)与 built-in 分支并存;**零生产消费方**(仅 examples/http_demo、tests/test_rest_template、docs/api/net.md);net-abstraction.md:29 现状盘点行将 HTTP/WebSocket 判为「与本设计正交,另行处理」——本行即「另行处理」的登记。**测试覆盖为零**:tests/test_rest_template.cpp 与 modules/net/tests/net_comprehensive_tests.cpp 均引用幻影 API、无法编译(分别被 APOLLO_BUILD_GTESTS=OFF 与 BUILD_TESTING 恒假挡住)——architecture-review §18 C-59。
- **落点**:net-abstraction §5.10 三裁决——① **curl 进 vcpkg**,rest_client 转真实现(TLS 后端 OpenSSL,单一 crypto 源不破;APOLLO_CURL_STUB 删随代码批);② **Drogon 备选分支删除**(CMakeLists :81-91/:124-133——§15.2 禁并存对象;admin exporter 不需要 Drogon);③ 同步 API 禁场景线程直调(curl_multi 执行层**按新建计**——既有 event_loop.cpp 不构成执行层,见 architecture-review §19.1 勘误 P-1;scripting-lua §8 异步交接,回包不进当 tick 判定);边界 = 目标白名单(SSRF,与 §5.7 同纪律);代码面审计仍归下轮候选(§4.2)。

### #12 会话与在线目录域——玩家所在线/所在 Zone/在线状态/顶号 — **CLOSED**(docs/design/session-and-online-directory.md,2026-09-30 落盘)

- **缺什么**:玩家在线状态的权威登记与查询面——谁在线、在哪条**线**(同 map 并行 scene 实例——「线」为中文 MMO 圈通称,KBE 引擎源码/配置无 line 一级概念,本轮实测 kbe/src + kbengine_defaults.xml 零命中,同图多 Space 实例即多线、由脚本层 Spaces 管理)、在哪个 Zone/哪个副本;**重复登录与顶号裁决**;掉线保活窗口;跨进程玩家寻址的**数据源**(architecture/remote-entity-call-design.md RouteResolver 四件套的「宿主定位」职责无数据来源);好友在线查询/GM 在线查询/全服广播寻址。
- **证据(负空间+先例,2026-09-30 实测)**:design/ 九份 grep「顶号|重复登录|在线状态|online」零命中;KBE 侧引擎无在线目录(在线 = baseapp 实体在内存,分配归 baseappmgr;重复登录裁决在 assets 脚本层——assets 仓库本机无,sdk_templates spaces 目录仅 .gitignore 占位已核);BW 侧在线目录分散在 mgr(baseappmgr 持 base 分配表 + (addr,load) 上报 loginapp 分流,baseappmgr.cpp:588-599/:1117);apollo 已有机制件但无目录:manager 域最轻分配(net-abstraction §7)、sceneId 隔离(attribute-sync §4.3)、ServerID 分段(36号 #15)。
- **落点(2026-09-30 落盘)**:docs/design/session-and-online-directory.md——**manager 域进程内存权威**(不进 Redis 不落 DB 无 journal——「全局仲裁态集中不共享」落地件;顶号/准入单点串行);条目 = SessionBinding+WorldAssignment 的进程间扩展(account/entity/session/gateway/zone/world_assignment/state/anchor_epoch/deadline_tick——存量 modules/game/session 302 行定位为进程内锚点/上报源,唯一消费方 base-app);写路径 = 三事件源(Zone 增删/gateway 生死/manager 裁决)+ 周期对账快照重置;**顶号 = 新顶旧框架语义**(anchor_epoch 锚竞争裁决,resume=同会话恢复两事分开——KBE 脚本层自决的刻意加强);**掉线窗口与 resume token TTL 同源一值**(Suspended 态,计时依赖定时器轮组件 §23-④/C-80 同批);查询三消费方(RouteResolver 镜像+分段先验+epoch 兜底/GM 走 manager 集中/广播走镜像);崩溃恢复 = 全量重报重建(目录是索引非权威数据持有者);消息 = internal 域事件族(client 域零新增);P2 退化 = 进程内表,验收三指标(顶号并发零双权威/manager kill -9 收敛 <5s/镜像失配率);配套 glossary 四词条(线/在线目录/顶号/掉线保活窗口)。

### #13 登录链路整体设计 — **OPEN**(2026-09-30 增补)

- **缺什么**:login-app 职责全链——账号鉴权、login_token 签发/校验/TTL/一次性(net-abstraction §5.9 只有一行)、选服/排队/准入(BW LoginConditions 先例已引但无登录链设计)、客户端 SDK 下发时机(KBE clientsdk_downloader 先例)、断线重连/顶号的会话裁决衔接(#12)。
- **证据**:design/ 中 login 相关仅三处一句带过(§5.9 login_token、sdk-contract schema_hash 握手、§5.7 epoch);KBE loginapp + clientsdk_downloader.{h,cpp}(deep-dive §4 已核);BW loginapp 指派 baseapp(36号 §2.1)。
- **落点**:P2-P3 设计批,依赖 #12(目录)与 G-1;拓扑入口 = gateway-app(sdk-contract §10.6 网关透传)。

### #14 入站第三方对接面(interfaces 域) — **OPEN**(2026-09-30 增补)

- **缺什么**:第三方账号绑定/充值回调/运营后台的**入站** HTTP 面——#11 只裁了出站;入站归哪个进程承载(gateway-app / 独立 admin-app)、鉴权、回调与游戏内实体投递的接线(异步、不进场景线程)。
- **证据**:design/ grep「充值|回调入站|interfaces」零命中(2026-09-30);KBE interfaces 独立进程(kbe/src/server/tools/interfaces,deep-dive §4 目录清点);BW 由 db 层 billing 承接(lib/db_storage_mysql/mysql_billing_system.cpp,36号 问11 A 级)。
- **落点**:随 #13 同批(§5.10 出站三裁决的镜像面;目标白名单/鉴权同纪律)。

### #15 Bots/协议级压测客户端 — **OPEN**(2026-09-30 增补)

- **缺什么**:模拟客户端**协议层**的机器人进程(多客户端并发接入、移动/技能/登录脚本化)——capacity-and-benchmark §5 三形态(微基准/合成 intent/录制回放)覆盖服务端机制面,无「真实四通道会话 + 握手 + 重连」的端到端压力形态。
- **证据**:capacity-and-benchmark 全文无 bots;KBE tools/bots + BW server/tools/bots(deep-dive §4/§16 目录清点已核——两家都把 bots 当引擎一等工具进程)。
- **落点**:capacity-and-benchmark §5 增第四形态(协议级 bots)或 apps/bench 扩展;与 #13 登录链互为验收对象,建议同批。

### #16 地图与空间数据管线(导入/构建/加载) — **OPEN**(2026-09-30 增补,用户点名)

- **缺什么**:地图资产从编辑器到运行时的**管线**——格式选型(地形/碰撞/导航网格/出生点/AOI 网格基准)、离线构建工具、运行时加载与 scene 配置映射(map_id→资源)、与 AOI 网格(AOI 服务)、NavMesh(todo 批次 5)、副本实例的接线。todo.md 批次 5 只有一行「Recast/Detour 接入评估」,管线本体零设计。
- **证据(负空间+先例,2026-09-30 实测)**:design/ grep「navmesh|NavMesh|导航|寻路|地图」仅两处非设计性命中(concept-glossary 场景定义、xml-generation 列 KBE 产物);KBE 先例:cellapp/navigation 三件(navigate_handler.*、loadnavmesh_threadtasks.*——navmesh 线程任务加载)+ 地图资产在 assets 仓库 res/spaces/(本机无,sdk_templates 占位 .gitignore 已核);BW 先例:World Editor→chunk 体系 + `Space : GeometryMapper`(cellapp/space.hpp 本轮实读)+ cellappmgr space 的 geomappingPath(cellappmgr/space.h 本轮实读)——地图 = 几何映射目录配置,运行时按 chunk 加载。
- **落点**:建议 P2 设计批——「烘焙在离线工具、运行时只读」与「生成器不进运行时」(36号 #4)同纪律;AOI 网格基准与 NavMesh 同源同批(同一份地图资产两个投影)。

### #17 战斗验证服务(客户端权威战斗的复算对账)— **CLOSED**(docs/design/battle-verification-service.md,2026-09-30 立项即落盘)

- **缺什么**:客户端权威战斗(36号 #18 缝兑现的 lockstep 小房间/客户端演算)下的服务端复算验证服务——独立进程注册、RPC 提交/结果链、verdict 与权威结算、结算门/仲裁/风控接线。concept-glossary「战斗验证服务」词条原写「未立项」且无对应条目(悬空引用,本条即补)。
- **证据**:battle-determinism §5 已有复算 hash 链与四元组,但其消费方「反作弊对账」仅一笔带过——服务形态/消息族/结算门/verdict 分级/部署容量零展开(2026-09-30 实读);BW/KBE/skynet 无内建(三家皆服务端权威无需——行业 JS/C# 双端自研通型,glossary 词条考证)。
- **落点**:docs/design/battle-verification-service.md——§0 适用/不适用写死(判据一句:判定在客户端才需要)/G-1 VERIFIER 型 + sdk-contract §11 internal 域三消息族(invoke_mode 三分)/Lua 双端共享 + `combat_bundle_hash`+VM 版本线双锚/跨端增量三件(运算白名单、版本锚、漂移降级 WARN——对 §2「不做定点」口径的关系已写明)/verdict 四值 × 结算门两级/权威结算由复算产出/无状态 worker 池 + Compact 内嵌 verifier-kernel/M0-M5(M4 与 #15 bots 互为验收)。

## 3. 已登记推迟项(登记簿管辖,不重复立项)

以下属「已设计/已判定、等代码阶段」而非设计缺口,状态以 architecture-review §16.10.2 登记簿为准:R-17a…R-17g(DI 域七项)、config 桩清理/FrameFilter 管线/继承生成器/定时器轮组件(代码影响项)、ipc 树与 bw/bigworld 兼容层(下轮审计候选)、sdks/contract 旧名同步(随下一代码批次)。**已完成项**:36 号 #12/#15/#17/#19 与 deep-dive §12 表述修正已于 2026-09-30 执行(B8 批——sol2/Lua 5.4 → 弃 sol2 + 5.5 主线随 vcpkg(当前 5.5.x,同日两度修订);#15 BW 分段 ID / #17 KBE MySQL-only 两处证伪改写,含对比表/问9总结/存储表联动)。**已裁决项(2026-09-30 用户:「都是历史记录」)**:origin 五笔源码提交(cb78d4b0…18152898)保留为历史记录、不回退;backup-apollo-src(c349f850)留档维持——不合并不删除;处置记录见 architecture-review 附录 A。

## 4. 结构性元缺口

### 4.1 设计资产状态表 v1(docs/architecture/ 70 份与 design 六份的权威关系) — **CLOSED**(2026-09-30 文档重整理批:docs/architecture/README.md 全量状态表落盘)

- **问题**:docs/architecture/ 存量 70 份,与 docs/design 六份权威稿的关系无登记——哪份仍有效、哪份被取代、哪份是占位,靠各人记忆。跨文档引用一旦指向已废稿即产生漂移(C-3 文档纪律的盲区)。
- **v1 口径(证据驱动,不做全量分类)**:以 design/analysis → architecture/ 的**实引关系**为锚点,分三档——
  - **实引有效(5 份)**:observability-watcher-and-runtime-introspection-design.md(scripting-lua §7 语义层)、remote-entity-call-design.md(net-abstraction §7 语义层)、starter-and-module-assembly-design.md(评审 §1/§4 引,历史定位=「不做 Spring 克隆」的正面表述源)、harbor-cluster.md 与 hot-reload.md(仅被 mmo-mechanism-deep-dive 引作「占位文档」证据——**列为待评审**:内容与 design 六份的关系未定);
  - **其余 ~65 份**:未被判废也未被判活——登记为「待盘点桶」,逐批消化(每设计批次顺手核与其主题相关的 architecture 稿);
  - 已删除 37 份(docs 根 B 级清理,登记簿有行)不在本表。
- **落点**:本节即状态表 v1;后续批次回填状态列。不另建新文件。

### 4.2 新审计候选:modules/net/{http,websocket}

16.8.1 版图普查与 C-29 四栈盘点(protocol/tcp/rpc + modules/protocol + include 双树)的**边界外存量**:modules/net/http(5048 行,证据见 #11)与 modules/net/websocket(built-in + Drogon 双分支)。二者与 §15.2「禁止第五套网络栈」纪律的关系未经审计——WebsocketFrameFilter 化(net-abstraction §5.5 判定:WS 是流 filter 不新起栈)意味着 built-in WS 栈的处置方向已有,但代码面审计未做。**已登记 architecture-review §16.10.2(本批补行)。**

### 4.3 BI 边界声明(2026-09-30 增问的核实结论)

BI 相关的**服务器侧出口已覆盖**:attribute-sync §8.2 属性变更事件旁路采样(预算外,不允许 BI 需求污染属性表结构)+ logging §5 collector 汇合脚本错误审计。**BI 的业务侧管道(数仓建模/报表/看板/离线分析)不在 apollo 设计面**——那是数据团队消费上述出口的自有系统;apollo 的义务止于「出口稳定、格式自描述、不丢关键事件」。此声明防止两件事:BI 需求倒灌进属性表结构(§8.2 已禁)与在 apollo 内重复设计数仓。

## 5. 优先级与批次计划

| 批 | 内容 | 缺口 | 依据 |
|---|---|---|---|
| B1 | 本清单落盘 + 登记簿补行 | 元缺口 4.1/4.2 | 用户指令「落盘吧」 |
| B2 | 新建 clock-and-time.md | #1 | 用户点名最先;**已完成**(b8ebed98 后续设计批) |
| B3 | attribute-sync §8.3 DDL(+§8.4 Redis 细则) | #3/#9 | 用户点名最先;**已完成**(§8.3/§8.4 落盘 + xml-generation §4 交叉引用) |
| B4 | 新建 battle-determinism.md | #2 | 随战斗玩法;**已完成**(四约束+回放四元组落盘,36 号 #16/#18 收口) |
| B5 | net-abstraction 增补:通道安全+加密库 / 观测接出 / 上行限流 / 出站 HTTP | #4/#10/#8/#11 | 随 P3;**已完成**(§4.3/§5.9/§5.10 + logging §5.1 落盘;同批收 scripting-lua 弃 sol2 改原生 C API 绑定 + Lua 5.5 决策(版本策略后改随 vcpkg)——见 §3 与登记簿) |
| B6 | 新建 capacity-and-benchmark.md(含内存/对象池) | #5/#7 | 随 P3;**已完成**(容量模型+帧预算表+内存对象池+基准三形态+指标集九项) |
| B7 | scripting-lua GM 命令面增补 | #6 | 随 P3;**已完成**(§7.5 指令表/权限分级/审计存储) |
| B8 | 36 号/deep-dive 历史表述修正 + Lua 版本策略再修订 + 裁决落档 | —(登记簿待办收尾,非缺口) | 登记簿 #15/#17 行、sol2 决策尾注与用户裁决项的收尾;**已完成**(2026-09-30——36 号 #12/#15/#17/#19/:166/:167/:243/:267 + deep-dive §12 标题;Lua 5.5.1 → 5.5 主线随 vcpkg(当前 5.5.x);origin 五笔/backup-apollo-src 裁决入 architecture-review 附录 A) |
| B9 | 新建 battle-verification-service.md | #17 | 用户点名「战斗验证服务怎么设计」(glossary 词条展开);**已完成**(2026-09-30 立项即落盘,glossary 词条同步改「设计已立」) |
| B10 | 新建 session-and-online-directory.md | #12 | 巡检令「todo 下一个高优先级设计批次」——#12 为 #13/#14/#15 依赖根;**已完成**(2026-09-30 落盘,glossary 四词条/net-abstraction §7§8/index/architecture-README 登记簿同步) |
| 收尾 | 各批落盘后回填本表状态列;登记簿同步 | — | 滚动;**B2-B7 全批完成(2026-09-30),#1-#11 全部 CLOSED;#17 同日 B9、#12 同日 B10 CLOSED——OPEN 仅余 #13-#16** |

每批独立提交(analysis/design 拆分照旧),完成即 fetch --rebase + push(推送纪律)。

---

*基线:apollo main @ 05919db9(文档态);源码证据为只读实读(vcpkg.json / modules/net/http 全文件行数 / rest_client.cpp 宏门 / main.cpp GameClockService / 负空间检索 kafka|otel|prometheus|grafana|libsodium 零命中——均 2026-09-30 本会话实测)。B 级文档清理后的引用基线按登记簿行(1e37073d 可溯)。*
