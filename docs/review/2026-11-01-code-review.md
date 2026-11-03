# 代码深审报告 · apollo（2026-11-01）

- 范围：本仓全量（apps/**、modules/**、tests/**）安全静态扫描 + 隐藏 bug 猎捕 + 质量门
- 纪律：每条发现先以 file:line 取证核验（VERIFIED 才算发现）；「空 grep ≠ 不存在」；已知/挂账项与真发现分开；未确认候选单独列，不混入确认数
- 基线：主分支 `49e385ff`；审计范围 49e385ff..HEAD 增量（4 文件）+ 全量安全面抽查

## 一、发现计数

| 严重度 | 确认 | 已修 | 剩余（记 todo） |
|---|---|---|---|
| P0 | 0 | 0 | 0 |
| P1 | 2 | 2 | 0 |
| P2 | 4 | 0 | 4 |
| P3 / 低置信 | 4 | 0 | 4 |

P1 两项：① 二进制解码面短体 OOB 读（codec 6 处裸 memcpy）；② WebSocket 分片聚合无界（frame 重组超限）。均已修。

## 二、P1 发现与修复

### CR-01 [P1] 二进制 decodeBody 短体越界读（OOB read）
- **位置**：`modules/protocol/src/codec.cpp` `CellCreateEntity:140`、`CellDestroyEntity:149`、`CellCrossBorder:155`、`CellEntityMove:183`、`CombatSkillCast:208`、`CombatDamage:227`——`memcpy` 定长字段前无 `data.size()` 检查；短体（合法帧头 + 截断载荷）直接越界读。
- **对照**：同文件 `GatewayClientDisconnect:108`、`Ping:270`、`Pong:281`、`PartyJoinRequest:575`、`SquadLaunch:587`、`SquadEnter:599`、`SquadStart:610`、`SquadKill:622`、`PartyResult:637`、`SquadResult:657` 均已带 `data.size()<kBody` 守卫；JSON 型（LoginRequest 等）经 `json::parse` 抛异常可被 dispatch 面 catch 转「Malformed」。仅上述 6 处遗漏。
- **暴露面**：调用方（`login_server.cpp:787/846/876`、`online_directory.cpp:269/295/337`、`gateway_server.cpp:780`、`m1_egress.cpp:339`、`fleetctl/main.cpp:425/455`）在 `parseHeader` 后不校验载荷长度即 `decodeBody<T>`；`login_server.cpp:763` 仅查 `request.size()>=sizeof(MessageHeader)`，短体直达。
- **修复**：6 处补 `constexpr size_t kBody=<字段和>; if (data.size()<kBody) throw std::runtime_error("Invalid data size");`，与既有 Ping/Party* 守卫同型。全树增量重建（codec.cpp 直接编入 world app 与多测试目标）exit 0；build-off 全量 ctest 86 套 85 绿（1 挂为并发新增 DashboardE2eSmoke，隔离重跑绿——环境噪声，不在 codec 编译面；另 WorldDrainTests 高载下泵超时假红已复跑绿）。
- **commit**：`185709cf`。

### CR-02 [P1] WebSocket 分片消息聚合无界（资源耗尽）
- **位置**：`modules/net/ws/src/frame.cpp:158` `fragment_.insert(...)`。逐帧上限有（`:104` `declared_len > max_message_` → fatal），但**分片聚合**无上限——对端发多个 ≤ max_message_ 的 continuation 帧，`fragment_` 无界增长（内存耗尽/DoS）。
- **语义**：`frame.hpp:41` `kDefaultMaxMessage=2MiB` 注明「分片容忍」——上限应约束单条重组消息，而非仅逐帧。
- **修复**：`frame.cpp` 分片分支加 `if (fragment_.size()+payload_size > max_message_) { out.fatal=true; break; }`；测试 `tests/test_net_ws.cpp` 加聚合超限 fatal 与边界内 100B 合法两例。ws_tests 全绿。
- **commit**：`d2fe1f2b`（已推）。

## 三、P2 发现（已记 todo，未修）

| ID | 位置 | 发现 |
|---|---|---|
| CR-03 | `modules/net/contract_wire/src/contract_wire.cpp:190-201` | StringList 解码 `resize(count)` 无界（count 来自 wire，最大 4B）；Int64List 同型已带 `(size-off)/8<count` 守卫（`:180-187`）。**注**：当前无契约消息使用 StringList（仅 Int64List 6 处），故为潜在/防御项——已补同型守卫（`(size-off)/4<count`），net_wire_tests 绿 |
| CR-04 | `apps/login/src/login_server.cpp:25-43,65` | 硬编码明文测试用户 `player1/pass1`、`player2/pass2` 无条件装载（构造 `:22` 调 `initTestUsers`），`authenticate` 明文比较；字段名 `passwordHash` 实持明文，注释自承「简化」。修法：哈希 + 测试用户配置门控（先记 todo） |
| CR-05 | `apps/world/src/main.cpp:174-175,283` | `--tick-rate` 用 `std::atoi`（`"0"`/`"abc"`→0）无校验，`:283` `1000/config.tickRateMs` 整除零 SIGFPE（本地启动崩溃，非远端）。修法：校验 `>0` 显式报错（同 cellServer `--world-id` 先例） |
| CR-06 | `modules/net/http/src/event_loop.cpp:325-333,541,637` | `processTimers`/`onCheckTimer`/`onReconnectTimer` 持锁调用回调（`timer->fire()`/`heartbeatCallback`/`callback()`），回调若回调本组件（addTimer/registerHeartbeat/…）即自锁死锁。live（bigworld runtime + tcp reactor 使用）；callback 依赖，非 wire 直触发 |

## 四、P3 / 低置信（记 todo）

| ID | 位置 | 发现 | 置信 |
|---|---|---|---|
| CR-07 | `modules/net/rpc/src/session_manager.cpp:191-200` | `getSessionByPlayerId` 持 `mutex_` 调 `getSession`（二次上锁）自锁死锁；**模块为遗留脚手架，live apps 零引用**（rpc_legacy.cpp 同名面）→ 死代码地雷 | 高（死代码） |
| CR-08 | `apps/world/src/m1_link_server.cpp:638,652-656,745-758` | 944 debug_eval 与 960 cell 注册裸帧受理无凭据校验；**已知/挂账**：`m1_link_server.hpp:76-78` 自述「认证凭据面按 world-cell-management 差距表另行挂账」——非新发现，随凭据面批 | 高（已知挂账） |
| CR-09 | `modules/net/rpc/src/session_manager.cpp:338-352`；`modules/game/attributes/src/attribute.cpp:78` | broadcast / SetAttribute 持锁（或监听器路径）调回调，回调重入风险；`online_directory.cpp:238,96` 无明确缺陷 | 中 |
| CR-10 | `modules/script/src/module_rollout.cpp:317`；`script_runtime.cpp:646` | rollback 遍历 `prepared_` 全集与版本清单读改写非原子，需随 rollout/版本面深审 | 低 |

## 五、commit 清单（本深审批次）

| commit | 内容 |
|---|---|
| `9b8a125d` | docs(readme)：README 双 docs 徽章 href 回归修复（en→/apollo/en/、zh→/apollo/；线上 /apollo/zh/=404 证实回归） |
| `d2fe1f2b` | fix：ws 分片聚合上限（CR-02）+ 测试 |
| `e4fcbe68` | fix：contract_wire StringList 解码上限（CR-03 防御），已推 |
| `185709cf` | fix：codec 6 处短体守卫（CR-01） |
| 早期批次 `a235a57d`（属主会话） | rollout ok-flag/驱动名登记修复 + 生产 cellServer 测试 |

## 六、结论

- **P0：0**；**P1：2（均修）**；P2：4（记 todo）；P3/低置信：4（记 todo）。
- 全量安全面未见 SQL 拼接/命令注入/eval 逃逸（eval 面为已知挂账的凭据缺口，非本批引入）；硬编码密钥无（login 测试凭据为开发桩，已记 P2）。
- 已知挂账项（CR-08）与未确认候选（CR-09/10）不混入确认计数，报告如实。
