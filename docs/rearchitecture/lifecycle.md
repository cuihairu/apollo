# Apollo 生命周期地图（lifecycle）

> 所属：第一阶段只读架构审查配套件之四。配套：audit.md、architecture.md、object-model.md（对象归属）、concurrency.md（线程/锁）、persistence.md、improvement-plan.md。
> 依据：任务书 §9（Instance 生命周期）、§11（Scene Transfer）、§18（Runtime/Application 生命周期、constructor 做太多/析构隐式释放/全局 singleton/跨模块隐式生命周期四检查）、§20（Recovery）。术语按 term-contract v1.0；差异清单见文末。

---

## 1. 生命周期层次总图（现状）

```text
进程层：
  ApplicationHost（Boot→ConfigLoaded→Initialized→Ready→Stopping→Stopped 阶段机，apps/game-server 用）
    └ ServiceHost（薄转发）[game-server 路径]
    └ WorldHost（默认 20Hz；注意：world_host.cpp 无编译目标——见 §1.4）
         └ cell-app：CellWorldService::tick（CellServer 私有，保证类）

对象层（现状状态机全集——只有两枚）：
  PlayerAnchor 六态：Loading → Online → Transferring → Disconnected → Saving → Offline
  WorldSession 七态：Entering → Active → Suspended → TransferringOut → TransferringIn → Leaving → Closed

无状态机对象（现状）：Scene / MapInstance / Entity / AOI / Avatar（无类）/ Connection / SaveQueue / DatabaseService
```

### 1.1 各对象的生命周期现状（详尽核对）

| 对象 | 现状生命周期 | 谁创建 | 谁销毁 | 缺陷 |
|---|---|---|---|---|
| ApplicationHost | 阶段机完整（start 失败走 StartupFailed→Stopping→Stopped） | main | main | 失败路径不调用已启动 service 的 stop（例外安全缺失）；`if (!is_running()) skip stop` 语义脆弱；run_once 恒返回 0 无错误传播；异常直接冒泡 |
| ServiceHost | 薄壳 | main | main | 无独立语义 |
| WorldHost | initialize/tick/shutdown + 20Hz 定帧 | cell_server.cpp:281 | — | **无编译目标**（runtime/CMakeLists 只编 application_host+runtime_manifest；compile_commands 88 条无 world_host.cpp）——engine 正确但从未进过构建 |
| PlayerAnchor | Loading（activate 时）→ Online（bindSession）→ Disconnected（unbindSession）→ Saving→Offline（deactivate 时） | base-app activatePlayer | base-app deactivate | Transferring 态永不到达（无跨进程转移）；Saving 态无保存动作（SaveQueue 假）；Offline 前无落库保证 |
| WorldSession | 七态簿记：attachPlayerWorldSession 置 Entering→resume Active；detach susupend 后 close | cell-app attach | cell-app detach | close_session 置 Leaving 后**立即**置 Closed 并擦除——Leaving 不可观察；resume/complete_transfer 不校验当前态（Closed 会话可被直接 resume）；begin_transfer 后 complete_transfer 同步立即完成——TransferringOut/In 永远是瞬时态，与「跨进程异步对账」的设计叙事不符 |
| Scene | 无：构造即存在，随 owner 析构 | MapInstance ctor 链 | 无显式 | 无 ready/running/stopped；「独立停止/恢复」不可能 |
| MapInstance | 五操作注册表条目 | create_instance | destroy（erase） | 无 Initialize/Waiting/Running/Finishing/Draining/Rejoining 语义（任务书 §9 八段全缺） |
| Entity（cell） | add→update→remove：on_spawn/on_despawn/on_update 钩子 | EntityManager::add（无生产调用） | 容器 | 组件 on_attach 双调（add_component 一次、spawn 一次）；EntityManager::update 持锁调用 on_update |
| Avatar | **不存在** | — | — | 无生/死/换幕规则 |
| Connection | 接受即存在至断开 | gateway 裸 TCP | 断开 | 无会话承载、无断线保活、无重连；HttpServer 的连接从不回收（http.cpp:506-549 无界增长） |
| SaveQueue | 常驻线程 workerLoop | base-app ctor | 进程结束 | worker 只 callback(true)；无停止协议（析构 join 未写） |
| DatabaseService | initialize() 恒 true；无关闭 | base-app ctor | — | 生命周期为空实现 |

### 1.2 任务书 §18 的四检查结论

1. **constructor 做太多事情**：总体否。DI `create_bean` 无重活约束（构造时执行依赖动作是调用方纪律问题）；但 FileAppender 字符串构造器先按默认配置开文件再改 config_ 且不重开（file_appender.cpp:36-50，实为 bug）；AnchorManager::activate 原地 make_shared 无加载（加载在 activatePlayer 里做——顺序正确）。
2. **析构隐式释放业务资源**：部分。ApplicationContext 析构调 shutdown 是显式逆序（正确）；但 cell EntityManager 裸指针不负责销毁；WorldSession/Anchor 的析构无业务动作（资源随 map 擦除）；legacy ecs.hpp `DestroyEntity→RemoveAllComponents` 不调组件析构（含 std::string 的组件内存泄漏）。
3. **全局 singleton**：5+ 个（AOIManager/AttributeManager/AttributeContainerManager/BattleManager 死件 + global_config/global_log_manager 活件 + 双 LogManager 类定义 ODR）。活件合理（进程级配置/日志），死件应删（improvement-plan P1/P3）。
4. **跨模块隐式生命周期**：是。双树并存产生「同一能力两套生命周期」横切（如 modules/game/session 的 anchor 与 base-app 的 anchor 消费、modules/net 实现旧头 API 却无统一生命周期契约）；ODR 使 log 生命周期完全未定义。

---

## 2. 关键时序（现状逐条，对照设计文档）

### 2.1 登录（login-flow 设计两阶段连接 → 代码）

```text
client → login-app（stub 恒成功）→ [token=ticket 时间 XOR hash, sessionSecret 未用]
      → base-app（9002）：PLAYER_ACTIVATE → activatePlayer → DatabaseService.loadPlayer（997 造假 "player_+id"）→
        AnchorManager.activate → bindSession（anchor→Online，SessionLocator 登记）→ 返回真
设计要求的落点链（login-flow §10）：manager 裁决 Home Zone → base 立锚 → zone 备 scene——代码无此链
```

### 2.2 进场景（attach 路径）

```text
cell-app CellServer::attachPlayerWorldSession（cell_server.cpp:532-550）：
  assign_map_instance(id=1 defaultMapInstanceId_) ；assign_space(同一 id) ；bind avatar EntityId；route_version+1
  world_session Entering → resume → Active
无：scene enter 校验、spawn Avatar 对象、AOI 登记、初始视野快照
```

### 2.3 换幕 / 跨边界（任务书 §11 焦点；对照 player-object-model §3）

```text
代码：handleCellCrossBorder → WorldSessionManager.transfer_session → 立即 complete_transfer（同步）
设计（契约口径）：准入（玩法层）→ 目标 scene 创建 → 旧 scene 销毁 Avatar → 新 scene 重建 Avatar（状态从 Anchor 投影）
                  → 易失态（buff/CD/坐标）不带走 → 结算类结果单向落回 Anchor
差距：
  1. 「准备/准入/驳回」无（transfer 无前置校验）
  2. 状态传递是字段搬运（world_id/map_id/space 拷进新记录），非对象投影（无 Avatar）
  3. 失败回滚路径无（complete 无失败分支）
  4. 断线发生在转移中：无（Transferring 态瞬时）
  5. AOI 重建：无（AOI 与 scene 无关）
  6. 「目标 scene 不存在」：无（默认单 scene）
```

### 2.4 断线与重连（任务书 §10/§12/§13 焦点）

```text
现状：断开 = 连接消失 = 一切消失（gateway 无会话；base-app 的 SessionBinding 无人解绑——disconnect 以纯文本发后端无 handler；reconnectWindowMs 零引用）
设计：断线 → Cell 场景内挂机 N 秒（Suspended 窗口）→ 窗口满 Cell 移除；Anchor 驻留保状态；Session 可 resume
      （resume_token 与窗口同源 TTL）
差距：全链路零实现
```

### 2.5 登出

```text
现状：无登出消息类型（全仓 grep 无 logout）；deactivate 直接擦除锚点（不存档）
设计：登出存档 → 落库 → deactivate
```

### 2.6 进程关闭（任务书 §18 收口）

```text
现状：ApplicationHost 逆序停 service（跳过 !is_running 的）；无最后一帧 flush 语义；
      cell-app 无 shutdown handler；base-app 自动保存线程仅打印
设计：Stop → 各 service 停止 → 脏数据刷落（持久化停-刷协议）→ 进程退出；恢复相位（recovery phase）拒新至收敛
```

---

## 3. 恢复（Recovery，任务书 §20）现状矩阵

| 恢复对象 | 必须持久化 | 可以重新计算 | 可以丢失 | 代码现状 |
|---|---|---|---|---|
| 玩家长期态（anchored） | 是（level/背包/进度） | — | 否 | load 造假数据、save 不落盘 → **全部丢失** |
| 场景运行时态 | — | 是（重开 scene） | 部分 | 无 scene 状态文件 |
| Instance 进度 | 结果单向落 Anchor | 中途可 abort | 否（结算） | 无 instance 生命周期 |
| 会话/连接 | — | 是（重连窗口内重建） | 窗口外 | 无 | 
| 服务拓扑（Zone 崩溃） | manager 目录可重建 | — | 临时丢失 | 无（baseappmgr 是单点内存，本身不持久） |
| 战斗中间态 | — | 重赛 | 结算前可丢 | 无 battle |

**结论**：恢复 = 全空白；改进计划 P1-5（Recovery）从「定义必须持久/可重算/可丢」三档清单开始（任务书 §20 原文要求），其承载机制 = persistence.md 的 write-behind 目标模型。

---

## 4. 术语契约差异清单（lifecycle 级）

1. **Suspended 语义缺口**：契约 §1.3「掉线保活窗口 = Suspended 态，窗口与 resume token TTL 同源」；代码 world_session.hpp 有 Suspended 枚举但零定时器（world_session.cpp:74-101）——枚举不承载窗口。差异登记。
2. **换幕 = transfer 字段搬运**：契约「换幕」= Cell 销毁重建（禁 migration 联想）；代码 begin_transfer/complete_transfer 是记录搬运且同步完成——方向对（无跨进程税）但无对象重建/状态投影（lifecycle §2.3 六差距）。差异登记。
3. **Leaving/Transferring 不可观察态**：代码状态机声称 7 态，实际 2 态瞬时、1 态不可观察（close_session 立即置 Closed）；契约只需 Suspended 一词对应的**可观察窗口**。差异登记。
4. **Anchor 六态的 Transferring/Saving**：契约 Anchor 永不迁移（Home Zone），代码 Transferring 态永不到达无妨；Saving 态无保存动作是**功能缺口**而非命名问题。登记。
5. **「断线挂机 N 秒」**：player-object-model §3 的 Cell 挂机窗口在代码无任何对应实现（全仓 grep 无挂机/超时逻辑）——P1 实现项，命名随契约。

---

（完）