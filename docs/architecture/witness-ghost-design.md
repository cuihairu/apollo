---
title: Witness 与 Ghost 设计
icon: eye
order: 16
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Witness
  - Ghost
  - Replication
---

# Witness 与 Ghost 设计

本页按 Apollo 体系解释 BigWorld 原词，完整对照见[BigWorld 与 Apollo](../design/bigworld-apollo-mapping.md)。当前本地 AOI 不等于完整 Witness，也没有 Ghost 复制实现。

## 概念与现有对应

Witness 是观察者可见集、更新上下文和同步进度；Apollo 的 SceneAoi、ViewerState 与属性同步承担部分职责。Ghost 是其他 Cell 中实体的非权威投影，用于邻接空间查询和可见性；不能独立扣血、发奖、执行玩家输入或保存长期资料。

BigWorld 的 [Witness](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellapp/witness.hpp) 与 [Entity](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellapp/entity.hpp) 是术语依据。Apollo 不因此增加另一个玩家资料对象或新的业务进程。

## 默认路径

每 Space 一个 Cell，由 cell 执行本地 AOI，worldServer 管理分配。跨服采用显式准入与模拟交接，不依赖 Ghost；玩家长期对象仍在 Base，经 database 保存。

迁移快照用于未激活准备和恢复，不是 Ghost。提交之前目标不能模拟，提交之后源端清理；存在两份快照不等于允许两个权威实体。

## 无缝地图预留

未来同 Space 多 Cell 时，需邻接投影、版本与失效控制、跨 Cell 可见集合并及去重、持续观察和 Real 权威交接。拓扑变更与投影更新不能授予 Ghost 写权。现在保留这些接口边界，不实施切分与复制算法。

完整 Instance 连续更新仍需共享玩法状态交接，Ghost 不替代结算或恢复记录。任务见根目录 todo.md。
