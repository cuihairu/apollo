---
title: Distributed World Topology 实施设计
icon: diagram-project
order: 32
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Distributed World
  - Topology
  - AppMgr
---

# Distributed World Topology 实施设计

这篇文档是从 `Standard MMO` 走向 `Distributed MMO` 的实施桥接页。

它不替代 `Shard / Zone / Instance / Match` 这些语义文档，而是回答一个更实际的问题：

`什么时候可以上分布式世界，应该按什么顺序落地。`

## 前置条件

先把下面这条主链收住，再考虑分布式世界：

```text
LoginApp -> GatewayApp -> BaseApp(PlayerAnchor) -> WorldHost
```

如果这条链还不稳定，就不该先做 `Witness / Ghost / AuthorityTransfer`。

## 实施顺序

1. 稳定 `Standard MMO`
2. 收口 `BaseApp` 的玩家主状态语义
3. 引入 `Proxy` 和 `CellRuntime`
4. 补 `Witness / Ghost`
5. 再做 `AuthorityTransfer`
6. 最后补 `AppMgr`

## 对应文档

- [MMO 组件装配目录](./mmo-component-assembly-catalog.md)
- [MMO 模块落地清单](./mmo-module-rollout-plan.md)
- [Standard MMO 任务清单](./standard-mmo-task-checklist.md)
- [Shard、Zone、Instance 与 Match Topology 设计](./shard-zone-instance-match-topology-design.md)
- [World 进入与切图设计](./world-entry-transfer-design.md)
- [Witness 与 Ghost 设计](./witness-ghost-design.md)
- [Authority Transfer 设计](./authority-transfer-design.md)
- [AppMgr 设计](./app-manager-design.md)
- [BigWorld 架构深度解析](./bigworld.md)
