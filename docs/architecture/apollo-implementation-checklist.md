---
title: Apollo 实施清单
icon: list-check
order: 8
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - 实施
  - 清单
  - Roadmap
---

# Apollo 实施清单

这篇文档不再停留在原则层，而是直接回答：

`从 Apollo 当前仓库出发，接下来到底先做什么。`

它和路线图的区别是：

- 路线图回答阶段顺序
- 实施清单回答当前代码仓库的具体落点和执行顺序

如果需要先看进程重组策略，见：

- [进程重组与实施路线](./process-reorganization-and-rollout-plan.md)

## 一、设计目标

这份清单要完成 3 件事：

- 把目标架构拆成可以逐步落地的实施项
- 明确当前仓库哪些目录先动，哪些先不动
- 把“语义校正”和“代码实现”放到同一执行顺序里

## 二、参考来源

这份清单主要基于：

- Apollo 当前仓库已有 `apps/*` 与 `modules/*`
- 已完成的分层、世界、对象、脚本、平台、运维设计稿
- KBE 提供的分布式世界参考边界

## 三、为什么这样设计

Apollo 当前最需要的不是同时启动所有重构线，而是先建立一条不会反复推倒重来的主线。

所以这份实施清单遵循 4 条规则：

- 先收口稳定主链，再补增强能力
- 先补模块边界，再补进程装配
- 先校正对象语义，再决定是否改目录名
- 先补可验证的最小实现，再继续扩张设计

## 四、当前仓库的实施基线

当前仓库可以先按下面的现实状态理解：

- `apps/login-app` 是登录入口原型
- `apps/gateway-app` 是接入网关原型
- `apps/base-app` 当前实现偏数据服务
- `apps/cell-app` 当前更像 world runtime 原型

这意味着短期不能做的事情是：

- 不能把当前 `base-app` 直接当成真正意义上的 BaseApp 已完成
- 不能把当前 `cell-app` 直接当成 BigWorld 语义下完整 CellApp

## 五、阶段 A：先收口普通 MMO 主链

这是一切实施项的最高优先级。

### 目标

让 Apollo 先成为一套能工作的普通 MMO 框架。

### 必做项

- 为 `modules/runtime` 增加统一 `WorldHost`
- 为 `modules/game/world` 增加 `WorldSpace`
- 为 `modules/game/world` 增加 `MapInstance`
- 为 `modules/game/world` 增加 `WorldSession`
- 让当前 `apps/cell-app` 退回装配壳角色

### 目录落点

- `modules/runtime/include/apollo/runtime/world_host.hpp`
- `modules/runtime/src/world_host.cpp`
- `modules/game/world/include/apollo/game/world/world_space.hpp`
- `modules/game/world/include/apollo/game/world/map_instance.hpp`
- `modules/game/world/include/apollo/game/world/world_session.hpp`

### 验证标准

- 单个 world 进程可稳定启动
- 地图实例可创建和销毁
- 玩家可进入和离开地图
- 世界更新由统一 tick 驱动

## 六、阶段 B：补玩家锚点主链

### 目标

把 Apollo 从“只有 world 原型”升级为“有完整玩家在线主链”。

### 必做项

- 新增 `PlayerAnchor`
- 新增 `AnchorManager`
- 新增 `SessionLocator`
- 新增 `WorldAssignment`
- 让 `apps/base-app` 开始承接玩家归属和在线上下文

### 目录落点

- `modules/game/session/include/apollo/game/session/player_anchor.hpp`
- `modules/game/session/include/apollo/game/session/anchor_manager.hpp`
- `modules/game/session/include/apollo/game/session/session_locator.hpp`
- `modules/game/session/include/apollo/game/session/world_assignment.hpp`

### 语义要求

- `Session != PlayerAnchor != AvatarEntity`
- `base-app` 的目标语义是玩家锚点宿主
- 数据访问只是它的一部分，不是全部语义
- 持久化执行层最终应沉到 `DBMgr / PersistenceService`

### 验证标准

- 玩家登录后能建立稳定锚点
- gateway 重连后能找回玩家归属
- 玩家切图后 world 归属可更新

## 七、阶段 C：补运行治理与运维主链

### 目标

让 Apollo 具备“能看见自己在跑什么、能控制自己怎么跑”的能力。

### 必做项

- 落 watcher tree
- 落 runtime diagnostics
- 落 console host
- 落 pidfile / single instance guard
- 落 systemd / supervisor adapter
- 落 OTel tracing / metrics exporter

### 推荐目录

- `modules/runtime/ops/include/apollo/runtime/ops/console_host.hpp`
- `modules/runtime/ops/include/apollo/runtime/ops/process_guard.hpp`
- `modules/runtime/ops/include/apollo/runtime/ops/watcher_registry.hpp`
- `modules/runtime/ops/include/apollo/runtime/ops/telemetry_bridge.hpp`

### 验证标准

- 每个主要 app 都能暴露基础运行信息
- 可查询在线玩家数、连接数、实体数、地图数
- 能优雅停机并输出基础诊断信息

## 八、阶段 D：补平台基础能力

### 目标

把 Redis、SQL、排行榜、分布式锁这类平台能力从零散封装提升成统一基础层。

### 必做项

- 定义 `KvStore` / `RedisClient` 边界
- 定义 `RelationalStore` 边界
- 定义 `Repository / UnitOfWork`
- 定义 `Leaderboard`
- 定义 `DistributedLock`
- 定义 `Queue / RateLimiter`

### 推荐目录

- `modules/platform/kv/`
- `modules/platform/sql/`
- `modules/platform/repository/`
- `modules/platform/leaderboard/`
- `modules/platform/lock/`
- `modules/platform/queue/`

### 验证标准

- 不同 app 可以复用同一套数据抽象
- 业务域不再直接依赖裸 Redis/SQL 客户端
- `BaseApp(PlayerAnchor Host)` 与 `DBMgr / PersistenceService` 边界清晰

## 九、阶段 E：补 Game Foundation 主链

### 目标

把实体定义、远程调用、复制链路、脚本 ABI 收成稳定的游戏基础语义层。

### 必做项

- 收口 `EntitySchema`
- 收口 `RemoteEntityCall`
- 收口 `ReplicationPipeline`
- 收口 `Script ABI`
- 规范 `RemoteEntityRef`、`AuthorityRole`、`ReplicationTarget`

### 验证标准

- 属性、方法、复制、持久化元信息有统一模型
- 跨宿主调用不再由各模块私自拼协议
- 复制目标和复制内容有统一入口

## 十、阶段 F：落统一脚本层

### 目标

统一脚本宿主接口，但保持脚本后端可替换。

### 必做项

- 定义 `ScriptRuntime`
- 定义 `ScriptModuleLoader`
- 定义 `ScriptValue / ScriptObject / ScriptFunction`
- 定义 entity hook 与 domain hook
- 先完成 Lua 或 Python 其中一个后端

### 实施建议

- 项目级别二选一
- Apollo 框架级别保留 Lua / Python 两种后端入口
- 不把高频热路径交给脚本层

### 验证标准

- 脚本可注册实体钩子
- 脚本可承接业务规则与事件回调
- 脚本后端替换不影响上层业务接口

## 十一、阶段 G：补标准业务域组件

### 目标

把 Apollo 从“有框架”推进到“有可装配业务域”。

### 推荐优先级

1. 登录与会话
2. 世界与地图
3. 聊天与社交
4. 任务、活动、成就
5. 战斗、技能、buff
6. 匹配与实例编排
7. 敏感词过滤
8. 支付与平台业务接入

### 实施原则

- 先做组件，不强制先拆进程
- 先定义 domain service 和 aggregate 边界
- 通过 command / event / query 协作

## 相关阅读

- [进程语义重定义](./process-semantics-redefinition.md)
- [进程重组与实施路线](./process-reorganization-and-rollout-plan.md)
- [持久化进程与 DBMgr 设计](./persistence-process-and-db-manager-design.md)

## 十二、阶段 H：预留分布式世界升级口

### 目标

不是立即完成 BigWorld，而是确保当前实现未来能平滑升级。

### 必做项

- 定义 `AuthorityRole`
- 定义 `SpacePartitionId`
- 定义 `TeleportContext`
- 定义 `WitnessContext` 接口边界
- 定义 `GhostReplica` 需要的最小复制信息

### 当前不做

- 不急着完整实现 `AppMgr`
- 不急着完整实现多 partition world
- 不急着把所有 world 逻辑都改成分布式

## 十三、阶段 I：按需进入 BigWorld 模式

### 目标

只有项目真正需要连续大世界时，再进入这一阶段。

### 必做项

- 落 `DistributedSpace`
- 落 `Witness`
- 落 `GhostReplica`
- 落 `AuthorityTransfer`
- 落 `AppManager`
- 落真正的大世界拓扑装配

### 验证标准

- 实体能跨 partition 迁移
- 客户端可见世界在边界附近保持连续
- 多 world/cell 宿主能协同运行

## 十四、按模块的近期施工顺序

建议按下面顺序推进模块，而不是到处同时开工：

1. `modules/runtime`
2. `modules/game/world`
3. `modules/game/session`
4. `modules/runtime/ops`
5. `modules/platform`
6. `modules/game/core`
7. `modules/script`
8. 其他 domain modules
9. `modules/world/distributed` 或等价增强层目录

## 十五、按 app 的近期施工顺序

### `login-app`

近期目标：

- 收口认证入口
- 对接 `PlayerAnchor` 建立流程

### `gateway-app`

近期目标：

- 收口连接、心跳、重连、接管
- 对接 `Session` 与 `PlayerAnchor`

### `base-app`

近期目标：

- 从偏数据服务原型升级成玩家锚点宿主

### `cell-app`

近期目标：

- 按 `world-app` 原型理解
- 收口为统一世界宿主装配壳

## 十六、哪些事情现在先不要做

为了避免再次发散，当前阶段建议不要优先做下面这些事：

- 不急着改 app 目录名
- 不急着先拆一堆 manager 进程
- 不急着做完整 BigWorld 全家桶
- 不急着把全部业务域都拆微服务
- 不急着把热点系统全脚本化
- 不急着用目录重命名代替语义收口

## 十七、测试与验证清单

实施必须伴随验证，不然文档会再次和代码脱节。

### 第一批测试

- `WorldHost` 生命周期测试
- `MapInstance` 创建销毁测试
- `PlayerAnchor` 建立与恢复测试
- `Session -> Anchor -> World` 路由测试
- watcher 查询测试

### 第二批测试

- replication pipeline 测试
- remote entity call 测试
- repository / unit of work 测试
- Lua 或 Python 脚本宿主测试

### 第三批测试

- authority transfer 测试
- witness / ghost 边界测试
- world failover / recovery 测试

## 十八、优点

这份实施清单的优点是：

- 顺序明确，不会一开始就陷入大世界复杂度
- 先把普通 MMO 主线做实，收益最快
- 当前目录结构基本可复用，不需要先大规模改名
- 平台、运维、脚本、业务域都有明确落点

## 十九、代价与风险

### 1. 过渡期会有双重语义

部分目录名和目标语义一段时间内不会完全一致。

### 2. 多条线需要持续收口

如果只写文档不补最小实现，路线会再次失焦。

### 3. 平台层和业务层可能互相越界

这需要在实施时持续审查依赖边界。

## 二十、为什么不选其他推进方式

### 1. 为什么不先做 BigWorld 全量实现

因为 Apollo 当前连普通 MMO 主链都还没有收稳，先做全量大世界会直接失控。

### 2. 为什么不先大规模重命名目录

因为目录名不是当前主矛盾，先把语义和实现收口更重要。

### 3. 为什么不先补全部业务域

因为没有稳定世界主链和玩家主链，业务域越多，返工越大。

## 二十一、结论

Apollo 当前最现实的实施路径就是：

1. 先把 `cell-app` 收成真正的普通 MMO 世界宿主
2. 再把 `base-app` 收成真正的玩家锚点宿主
3. 再补运维、平台、脚本和统一基础语义
4. 最后按项目需要进入分布式世界增强

这条路最慢的地方不是编码，而是持续保持语义和边界不跑偏。

## 相关阅读

- [Apollo 重构路线图](./apollo-refactor-roadmap.md)
- [Apollo 最终蓝图](./apollo-final-blueprint.md)
- [Apollo 分层设计](./apollo-layering-design.md)
- [PlayerAnchor 设计稿](./player-anchor-design.md)
- [WorldHost 设计稿](./world-host-design.md)
- [Runtime Ops Host 设计](./runtime-ops-host-design.md)
- [Platform Foundation 设计](./platform-foundation-design.md)
- [统一脚本层设计](./script-layer-design.md)
