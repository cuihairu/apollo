---
title: Replication Pipeline 设计
icon: share-nodes
order: 22
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Replication
  - Witness
  - Ghost
---

# Replication Pipeline 设计

这篇文档解决的是 Apollo 继续参考 KBE 时，另一条必须落地的主线：

`实体状态如何从权威宿主稳定地复制到客户端视图。`

如果只写到 `AOI`、`Witness`、`Ghost` 这一层就停住，后面一定会遇到这些问题：

- 到底谁产生快照
- 谁判断 enter/leave
- 谁做 alias
- 谁做 delta
- 谁把包发给 gateway

所以复制链路必须单独收成一条完整 pipeline。

## 一、先说结论

Apollo 应该把复制链路统一成下面这条管线：

```text
Authoritative Entity
    -> Replication Source
    -> WitnessContext
    -> Delta Builder
    -> Packet Composer
    -> Gateway Dispatch
```

如果进入 BigWorld 模式，则在 source 层同时包含：

- `RealEntity`
- `GhostReplica`

## 二、KBE 里的直接证据

从本地源码看：

- [witness.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/server/cellapp/witness.h)
  - 管理 view 关系
  - 维护进入和离开视图
  - 负责给客户端组装更新
- [ghost_manager.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/server/cellapp/ghost_manager.h)
  - 管理 ghost route 和复制消息
- [scriptdef_module.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/lib/entitydef/scriptdef_module.h)
  - 持有 detail level
  - 持有 property/method 描述

这说明 KBE 的复制系统不是简单“找到附近实体然后广播”，而是：

- schema 决定哪些内容可复制
- witness 决定客户端当前能看见什么
- ghost 参与跨节点近场复制

## 三、Apollo 当前的缺口

Apollo 当前已经有这些方向：

- [Witness 与 Ghost 设计](./witness-ghost-design.md)
- [EntitySchema 设计](./entity-schema-design.md)
- [Space Partition 与 Topology 设计](./space-partition-topology-design.md)

但还缺一条显式的发包主链。

当前缺的是：

### 1. 缺复制源抽象

没有统一对象回答：

- 谁是当前权威快照来源
- ghost 是否可以作为临时复制源

### 2. 缺 witness 视图状态机

现在文档已经定义了 `WitnessContext`，但还没有把它展开成：

- 首次进入视图
- 稳态增量
- 离开视图
- 重同步

### 3. 缺 delta/alias/detail level 组包层

这块如果没有统一中间层，业务最后会退化成：

- 每个系统自己拼同步包

## 四、推荐目标

Apollo 应该显式形成下面这组对象：

```text
ReplicationService
    ├── ReplicationSourceRegistry
    ├── WitnessContext
    ├── VisibleSetTracker
    ├── DeltaBuilder
    ├── AliasAllocator
    ├── DetailLevelPolicy
    └── GatewayDispatcher
```

## 五、核心对象设计

### `ReplicationSource`

这是复制源抽象。

建议统一接口：

```text
ReplicationSource
    entity_id
    source_kind
    authority_epoch
    snapshot_revision
    collectSnapshot()
```

`source_kind` 建议至少支持：

- `Real`
- `Ghost`
- `AnchorMirror`

### `WitnessContext`

职责：

- 维护某个客户端当前可见集
- 记录每个实体上次已发送 revision
- 管理 enter/leave/dirty 状态

建议至少持有：

```text
WitnessContext
    witness_id
    player_id
    route_version
    visible_entities
    alias_table
    detail_level_state
    pending_packets
```

### `VisibleSetTracker`

职责：

- 根据 AOI、订阅关系、脚本可见性规则产出候选可见集
- 对比上一帧可见集
- 输出：
  - `entered`
  - `stayed`
  - `left`

### `DeltaBuilder`

职责：

- 根据 `snapshot_revision` 构造增量
- 区分首次全量和稳态增量
- 处理属性脏位图

### `AliasAllocator`

职责：

- 给 witness 内部的实体、属性、方法分配短 alias
- 降低包体大小

### `DetailLevelPolicy`

职责：

- 按距离、重要度、观察模式决定：
  - `Near`
  - `Mid`
  - `Far`

不同 detail level 下，复制字段和频率不同。

## 六、推荐复制包结构

建议 Apollo 的复制包至少包含：

```text
ReplicationPacket
    tick_no
    witness_id
    route_version
    packet_seq
    entered_entities[]
    updated_entities[]
    left_entities[]
    ack_hint
```

其中单个实体块建议分成：

```text
EntityReplicationChunk
    entity_alias
    entity_type_alias
    source_kind
    detail_level
    baseline_revision
    current_revision
    property_delta
    volatile_state
```

## 七、推荐标准流程

### 每 tick 复制流程

1. `WorldHost` 更新 world
2. `ReplicationSourceRegistry` 收集 dirty entity
3. `VisibleSetTracker` 计算每个 witness 的 enter/stay/leave
4. `DeltaBuilder` 为 `stay` 实体构造增量
5. `AliasAllocator` 分配或复用 alias
6. `PacketComposer` 输出复制包
7. `GatewayDispatcher` 投递给 gateway
8. gateway 下发给客户端

### 首次进入视图

1. 分配 `entity_alias`
2. 发送 spawn/full snapshot
3. 初始化 witness baseline

### 离开视图

1. 标记 leave
2. 发送 despawn/remove
3. 释放 alias 或延迟回收

## 八、复制源优先级

Apollo 进入 BigWorld 模式后，复制源必须有优先级。

建议规则：

### 1. 优先 real

如果实体当前权威就在本节点，默认用 `RealEntity` 作为复制源。

### 2. 边界窗口可退回 ghost

如果 witness 所在节点没有 real，但已有近场 `GhostReplica`，允许 ghost 作为临时复制源。

### 3. 不允许长期用 ghost 覆盖权威

ghost 只负责：

- 连续视图
- 迁移窗口
- 边界近场

不负责长期权威状态。

## 九、和 schema 的关系

复制链路必须依赖 [EntitySchema 设计](./entity-schema-design.md)。

### 复制时真正要查的是 schema

而不是业务代码临时 if/else。

schema 至少决定：

- 哪些属性进 spawn
- 哪些属性进 delta
- 哪些字段只给 own client
- 哪些字段进入 `Near/Mid/Far`

## 十、和 `Proxy / PlayerAnchor / AvatarEntity` 的关系

### `AvatarEntity`

通常是 world 内主要复制源。

### `PlayerAnchor`

不适合直接承接高频空间复制，但适合承接：

- own client 私有状态
- 非空间长期状态的低频同步

### `Proxy`

不负责决定复制内容，只负责把最终结果送到客户端。

这和 [Base Cell Proxy 对象模型](./base-cell-proxy-model.md) 的分层是一致的。

## 十一、普通 MMO 模式怎么简化

Apollo 不是默认就要上完整 BigWorld。

在普通 MMO 模式下，可以这样落地：

- `ReplicationSource` 只有 `Real`
- 不启用跨节点 `GhostReplica`
- `WitnessContext` 仍然保留
- `DeltaBuilder`、`AliasAllocator` 仍然保留

这样即使先做单 world，也不会把复制管线写死。

## 十二、当前落地建议

建议按 4 步推进。

### 第一步：先补 `ReplicationService`

把 world 内所有对客户端同步的出口先统一起来。

### 第二步：补 `WitnessContext` baseline

显式记录：

- 当前 visible set
- 每实体上次发送 revision
- 当前 alias

### 第三步：补 delta builder

不要继续让各系统自己决定怎么拼增量。

### 第四步：补 gateway dispatch 契约

让 world 只产出复制包，不直接操心公网协议细节。

## 十三、建议接口草图

```cpp
class ReplicationService {
public:
    void collectDirty(EntityId entityId);
    void updateWitness(WitnessId witnessId, TickContext tick);
    std::vector<ReplicationPacket> flushPackets(TickContext tick);
};
```

重点不是接口形式，而是这条责任链必须明确：

- world 产出复制数据
- gateway 负责对外发送
- witness 负责客户端视图

## 十四、结论

Apollo 后续如果只写 `AOI`、`Ghost`、`Witness` 这些概念，而不把复制路径做成真正的 pipeline，架构仍然是不完整的。

真正需要补齐的是：

- 复制源
- 可见集状态机
- delta 构建
- alias/detail level
- gateway 投递

只有这条链打通，Apollo 才算真正从“世界逻辑原型”进入“可运行的大型在线世界架构”。

## 相关阅读

- [Witness 与 Ghost 设计](./witness-ghost-design.md)
- [Space Partition 与 Topology 设计](./space-partition-topology-design.md)
- [EntitySchema 设计](./entity-schema-design.md)
- [Base Cell Proxy 对象模型](./base-cell-proxy-model.md)
- [RemoteEntityCall 设计](./remote-entity-call-design.md)
