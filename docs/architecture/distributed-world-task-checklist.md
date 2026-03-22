---
title: Distributed World 任务清单
icon: square-check
order: 44
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Distributed World
  - BigWorld
  - Checklist
---

# Distributed World 任务清单

这篇文档把 `Distributed World` 的实施计划继续下钻成任务清单。

目标主链：

```text
Client -> LoginApp -> BaseApp(Proxy + PlayerAnchor) -> CellApp
```

## 一、前置条件

只有在下面条件成立后，才建议启动这一阶段：

- `Standard MMO` 已稳定
- `PlayerAnchor` 已稳定
- `LoginApp` 已能返回可切换入口
- `World runtime` 已有成熟原型

## 二、阶段目标

最终要达成的是：

1. 客户端直接接到 `BaseApp Proxy`
2. `BaseApp` 绑定 `PlayerAnchor + Proxy`
3. `CellApp` 作为空间实时权威运行
4. 客户端通过 `Witness` 接收视野更新
5. 玩家跨 cell 时完成 authority transfer

## 三、任务清单

### A. Proxy 化 BaseApp

- [ ] `ClientAttachment` 独立出来
- [ ] `ClientBinding` 与 `SessionBinding` 区分
- [ ] `Proxy` 支持附着、切换、踢线
- [ ] `ReconnectTakeover` 支持连接接管

验收点：

- 分布式世界模式下客户端可直接接到 `BaseApp`

### B. Cell 侧运行时

- [ ] `CellRuntime` 独立出来
- [ ] `SpaceRuntime` 支持空间宿主
- [ ] `AvatarEntity` 支持基础玩家实体
- [ ] `CellRouting` 支持 cell 间消息路由

验收点：

- 空间内实时逻辑不再跑在 `BaseApp`

### C. Witness 与可见性

- [ ] `Witness` 表示客户端可见世界上下文
- [ ] `WitnessBridge` 负责 `Cell -> Proxy -> Client`
- [ ] `Ghost` 表示跨分区可见实体投影

验收点：

- 可见性同步不再靠普通 MMO 的单服 AOI 假设

### D. 权威迁移

- [ ] `AuthorityTransfer` 支持跨 cell 切换
- [ ] `SpacePartition` 支持空间切片
- [ ] `MigrationState` 记录迁移状态机

验收点：

- 跨边界移动不丢失控制权和客户端附着

### E. 控制面增强

- [ ] `BaseAppMgr`
- [ ] `CellAppMgr`
- [ ] `TopologyRegistry`
- [ ] `MigrationControl`

验收点：

- Base / Cell 的负载与迁移不再靠静态配置硬编码

## 四、建议顺序

推荐按下面顺序推进：

1. Proxy 化 BaseApp
2. Cell 侧运行时
3. Witness 与可见性
4. 权威迁移
5. 控制面增强

## 五、里程碑

### D1

- `LoginApp` 可返回 `BaseApp Proxy` 入口

### D2

- 客户端成功附着到 `BaseApp Proxy`

### D3

- `CellApp` 可稳定运行空间实时权威

### D4

- `Witness / Ghost` 可稳定下发视野

### D5

- authority transfer 跑通

## 六、结论

`Distributed World` 不是第二套默认起步架构，而是建立在 `Standard MMO` 之上的增强层。

只有在普通 MMO 主链稳定后，再进入：

- `Proxy`
- `CellRuntime`
- `Witness`
- `Ghost`
- `AuthorityTransfer`

才是合理顺序。

## 相关阅读

- [MMO 模块落地清单](./mmo-module-rollout-plan.md)
- [BigWorld 进程架构与玩家生命周期](./bigworld-lifecycle.md)
- [Witness 与 Ghost 设计](./witness-ghost-design.md)
- [Authority Transfer 设计](./authority-transfer-design.md)
