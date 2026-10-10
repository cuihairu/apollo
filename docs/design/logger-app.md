# LoggerApp 立项前置设计（进程壳与结构化行接线的落地形态）

> 状态：**已落地（2026-10-10，L1+L2+L3 三批同日交付；L4 挂 M1）**。定位：ADR-013「LoggerApp 立项」的落地形态展开——钉 apps 接线现状实证、钉 v1 采集形态三岔、钉批切分与拍板点。体裁仿 net-abstraction §7「P3 前置设计」/battle-instance-offload/cell-single-writer。术语以 term-contract §1.3 为准（日志收集进程 logger / 代码标识 `LoggerApp`）；权威口径 = ADR-013（立项切分：壳先行 + 开关打开 + push 留接口）+ logging.md §5（collector 形态母件）。批记见 ADR-013 落地注记（adr.md）。

---

## 1. 现状声明（A 级实读，2026-10-10 立项时点）

> **已过期**：本节钉的是立项时点实况。L1+L2 落地后七 app 输出面已全量转
> logger（console 人读 + 本地结构化文件并行），apps 接线现状 = 全量；
> 行号不再对应当前源码。立项时点证据保留不删（决策链可追溯），
> 现状以 ADR-013 批记为准。

**立项时点：模块面已交付（P3-3 批 C），apps 接线 = 零。**

- 结构化行面在库：`FileAppenderConfig::structuredOutput` 默认 false（file_appender.h:46）、写路径三元分支（file_appender.cpp:79-80 `structuredOutput ? to_structured_line(record) : format(record)`）、`to_structured_line`（structured.cpp:116）、六键固定序 + 引号规则契约以 structured.h 头注为源；`LogManagerConfig::processIdentity` 注入位（log_manager.h:46）；测试面 test_log_structured / test_core_log / test_log 在库。FileAppender 滚动（ByDate/BySize/ByBoth + 重启序号续接 + maxFiles 清理）已交付。
- **七 app 输出面全部 `std::cout` 直写**——machined（main.cpp:60-71 born/death/restart 行）、baseappmgr（:28-99）、base-app（:22-65）、cell-app（:14-53）、gateway-app（:16-61）、login-app（login_server.cpp:364-431）、game-server（main.cpp:123-126）。唯一 log 模块触点 = game-server main.cpp:73 `global_log_manager()`（崩溃遗留可见化行 cat=crash pending=N，经 console 出）。
- 推论一：**文件出口现状不存在**（fileEnabled 默认 false 且无人开）；`log/<app>-<instance>.log` 布局是 logging.md §5 设计口径非现实。
- 推论二：ADR-013「各 app FileAppender structuredOutput 开关打开」实含两层——**先接 log 模块（L1）、再翻开关（L2）**。todo P3-3 批 C 边界「apps 接线未做」即指 L1；本件钉此批切，防立项批把两层并成一句漏做 L1。
- 收集面空白：apps/ 七进程无 collector；machined 监督面（supervisor.cpp:121-124 退避参数 / :204-205 重启裁决）为 roster 接入点；dev_fleet fleet_entries 六成员（dev_fleet.sh:36-43）。
- 权威口径既有：logging.md §5 collector 形态（apps/logger + push 经 net-abstraction §5.7 InterServerLink + 失败语义「收集永不构成运行依赖」+ §4 文件布局）；§5.1 三禁（不直连 Kafka / 不引 OTel SDK / 不开 per-process HTTP 端口）+ 外挂 tail 分工；§7 P3 行全量面（push 通道/断连降级/BI 分流/core 归档/告警 notifier）。ADR-013 切分：**壳先行 + structuredOutput 打开 + push 留接口不实现（collector 面 M1 后）**。

## 2. 目标形态（权威口径）

v1 终态：logger 进程入编队；各 app 双出口第一步 = 本地结构化文件（真相源，人读 console 并行）；logger 只读汇聚检索（按进程/级别/时间/类别过滤）；push 通道留缝（接口位不实现），M1 后换 InterServerLink 底座——ADR-013 三句的字面展开。

失败语义 v1 天然满足：tail 形态下 logger 挂 = 各 app 文件照写，零依赖零影响——logging.md §5「收集永不构成运行依赖」的平凡解。

## 3. 方案三岔展开（v1 采集形态）

### (a) tail 拉——logger 读 `log/*.log` 结构化行

- **语义**：检索面建在文件真相源之上；结构化行（六键）使过滤退化为键值匹配；push 留缝后 M1 换底座不动检索面。
- **代价**：实时性受 tail 轮询间隔（检索型负载可接受；告警型负载归 §5.2 层 B 走 metrics 不走日志）；文件必须先存在——L1/L2 接线批是其前置。
- **与既有口径关系**：logging.md §5.1「本地文件同为真相源」与外挂 tail 同构——logger v1 就是内部版 tail。

### (b) push 直连 v1——各 app logger 线程直推 collector

需 InterServerLink（net-abstraction §5.7，M1 未落地；现役 nng 底座属换血对象）——**违 ADR-013「push 留接口不实现」，否**。

### (c) UDP 推（G-1 beacon 同型）

有损传输 vs 日志的审计/取证语义（crash-capture 取证链、脚本错误审计 scripting-lua §6 都要求不丢行）——**否**。

## 4. 诚实对比

| 维度 | (a) tail 拉 | (b) push 直连 | (c) UDP 推 |
|---|---|---|---|
| 新增传输依赖 | 零 | InterServerLink（M1） | UDP 面（新写） |
| 丢行风险 | 无（文件为准） | 无（有界队列+断连降级） | **有**（审计语义冲突） |
| 实时性 | 轮询间隔级 | 推送级 | 推送级 |
| M1 耦合 | 无（底座可后换） | **强耦合，违 ADR-013** | 无 |
| 三禁关系 | 合（零新端口） | 合（进程间域） | 合（进程间域） |
| 失败语义 | 平凡满足 | 需断连降级面 | 需容忍丢行 |
| 检索面复用 | L4 换底座不动 | 检索面同型 | 同型 |

## 5. 拍板点（2026-10-10 随 ADR-013 拍板裁定的落定口径）

1. **v1 采集形态 = (a) tail 拉**——(b) push 直连违 ADR-013「push 留接口不实现」（本批权威切分）、(c) UDP 推有损与审计/取证语义冲突（§3/§4 双否已裁）；L1+L2 文件真相源落定后 (a) 是唯一可行岔。
2. **接线批切分 = L1+L2 合并一批**——立项批指令字面即为合并（apps 接 log 模块 + structuredOutput 开关打开一并交付）；行为面回归未发生（kill -9 演练全链断言绿，消息正文子串契约全保）。
3. **检索面 v1 形态 = (a) logger 二进制子命令**——`logger scan`（一发检索）+ `logger follow`（跟随聚合，编队 roster 行消费面）；logger_tests 9 组入门禁，(b)/(c) 否决理由维持原记。
4. **归档/滚动职责 = v1 logger 只读**——滚动沿用 FileAppender 既有（ByDate/BySize/ByBoth + maxFiles + 重启序号续接），logger 不接管。
5. **目录/二进制名 = apps/logger + 二进制 logger + 类 `LoggerApp`**——term-contract §1.3 契约代码标识落类名，不随目录名变。

## 6. 拍板后路径

```text
批 L1+L2  ✅ 已交付（2026-10-10，7ad64938）：七 app main 配 LogManagerConfig
          （fileEnabled + processIdentity + structuredOutput，console 保人读）；
          验收全过 = 三树门禁 + 七 app 日志文件落盘断言 + 结构化行可解析断言
          + kill -9 演练断言回归（消息正文子串契约全保）
批 L3     ✅ 已交付（2026-10-10）：apps/logger 进程 + 二进制 logger + 类
          LoggerApp；读侧解析（structured.cpp 镜像）+ scan（proc/level/ts/cat）
          + follow（编队聚合）+ push 接缝 IPushSink/NullPushSink（不实现）；
          验收全过 = logger_tests 9 组入门禁 + dev_fleet roster logger 行
          + 编队冒烟（7 成员、聚合 103 行、scan 12 文件全解析）
批 L4     ⏳ M1 后：push 通道（InterServerLink，接缝已留）+ 断连降级
          （logging.md §5 全形态）；更后 BI 分流（attribute-sync §8.2）
          与告警 notifier（§5.2 层 B）同批
```

## 7. 交集

| 件 | 关系 |
|---|---|
| logging.md §5/§5.1/§7 | 母件：collector 形态、三禁、P3 全量面（本件为其 ADR-013 切分的落地展开） |
| ADR-013 | 立项切分源：壳先行 + 开关打开 + push 留接口 |
| net-abstraction §5.7 | L4 push 底座（InterServerLink，M1） |
| crash-capture | cat=crash 行 = 结构化检索首个消费先例；crashdumps pending 计数可见化 |
| G-1 machined | roster 接入点；死亡上报同批口径（logging.md §7 P3 行） |
| scripting-lua §6 / attribute-sync §8.2 | BI 旁路在 collector 汇合（L4 后） |
| term-contract §1.3 | `LoggerApp` 词条（命名真相源） |

## 8. 证据清单

- A 级（2026-10-10 本地源码实读）：modules/core/log/include/apollo/core/log/file_appender.h:46、modules/core/log/src/file_appender.cpp:79-80、modules/core/log/src/structured.cpp:116、modules/core/log/include/apollo/core/log/log_manager.h:46、apps/machined/src/main.cpp:60-71、apps/baseappmgr/src/main.cpp:28-99、apps/base-app/src/main.cpp:22-65、apps/cell-app/src/main.cpp:14-53、apps/gateway-app/src/main.cpp:16-61、apps/login-app/src/login_server.cpp:364-431、apps/game-server/src/main.cpp:73/:123-126、apps/machined/src/supervisor.cpp:121-124/:204-205、scripts/dev_fleet.sh:36-43、tests/test_log_structured.cpp
- B 级（本仓文档）：docs/design/logging.md §5/§5.1/§7、docs/architecture/adr.md ADR-013、docs/todo.md P3-3（批 C 边界「apps 接线未做」）

*基线：apollo main @ 331de147（写件时）；L1+L2 = 7ad64938、L3 批随同日落库。§1 行号为前置设计稿时点实读（2026-10-10），L1+L2 落地后 apps 输出面已全量转 logger，行号不再对应当前源码——现状实证以 ADR-013 批记为准。*
