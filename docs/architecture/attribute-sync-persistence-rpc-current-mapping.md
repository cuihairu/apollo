---
title: 属性同步、持久化与 RPC 现状映射
icon: diagram-project
order: 24
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - AttributeSync
  - Persistence
  - RPC
  - Mapping
---

# 属性同步、持久化与 RPC 现状映射

这篇文档承接：

- [属性同步、持久化与 RPC 边界设计](./attribute-sync-persistence-rpc-boundary.md)

它解决的是另一个更落地的问题：

`如果按新的边界来理解 Apollo，当前代码分别落在哪些层，哪些部分已经对了，哪些部分还没有拆开。`

这篇文档不重新讨论原则，而是直接对照现有代码做映射。

## 一、先说结论

当前 Apollo 在这条链路上的状态可以概括成一句话：

`同步侧已经有雏形，持久化侧还停留在通用数据访问层，AOI 和复制链还没有打通，RPC 基本仍是独立交付层原型。`

更具体地说：

- `modules/game/attributes` 里已经有：
  - 属性值层
  - 属性变化监听
  - 属性同步规则与批处理
- `modules/data` 里目前主要是：
  - 连接
  - 缓存
  - ORM / Redis 包装
  - 还没有“属性持久化投影层”
- `modules/game/world` 里目前主要是：
  - AOI
  - scene / world session
  - 还没有“AOI -> 属性复制”桥接层
- `modules/net` 里目前主要是：
  - 传输
  - 编解码
  - RPC 原型
  - 和属性同步没有强耦合

这说明方向上是可收敛的，但当前还缺一个非常关键的中层：

- `PersistenceProjector`
- `ReplicationService`

## 二、目标边界回顾

按新的分层，Apollo 更合理的结构应是：

```text
L1 Attribute Value Layer
L2 Attribute Change Layer
L3 Attribute Sync Projection
L4 Attribute Persistence Projection
L5 Delivery Layer (Sync / RPC / Event)
```

下面按这个结构看当前代码。

## 三、L1：Attribute Value Layer

这一层当前在 Apollo 里已经基本存在。

核心文件：

- [attribute_value.h](/Users/cui/Workspaces/apollo/include/apollo/game/attributes/attribute_value.h)
- [attribute_value.cpp](/Users/cui/Workspaces/apollo/modules/game/attributes/src/attribute_value.cpp)

### 当前已经具备的能力

- `AttributeContainer` 负责保存对象属性值
- `AttributeValue` 里包含 `id / value / dirty`
- 支持：
  - `setAttribute`
  - `setAttributes`
  - `getDirtyValues`
  - `clearDirtyFlags`
  - `serialize / deserialize`
  - change listener

对应代码点：

- `AttributeContainer::setAttribute(...)` [attribute_value.cpp](/Users/cui/Workspaces/apollo/modules/game/attributes/src/attribute_value.cpp#L13)
- `AttributeContainer::getDirtyValues()` [attribute_value.cpp](/Users/cui/Workspaces/apollo/modules/game/attributes/src/attribute_value.cpp#L220)
- `AttributeContainer::addChangeListener(...)` [attribute_value.cpp](/Users/cui/Workspaces/apollo/modules/game/attributes/src/attribute_value.cpp#L249)

### 当前层级判断

这一层总体是合理的。

它已经比较接近：

- 状态容器
- 变化源头

### 当前问题

还存在两个小问题：

#### 1. `dirty` 仍然偏“同步脏标记”语义

`AttributeValue.dirty` 目前更像“需要回传/需要同步”的标志，而不是纯变化事实。[attribute_value.h](/Users/cui/Workspaces/apollo/include/apollo/game/attributes/attribute_value.h#L39)

长期更合理的做法应该是：

- `dirty` 留给容器内部增量收集
- 真正的分流靠 `AttributeChangeEvent + Projector`

#### 2. 容器和“属性定义元数据”仍然比较弱绑定

目前值容器很强，但还没有真正对应新的：

- `AttributeSyncSchema`
- `AttributePersistenceSchema`

## 四、L2：Attribute Change Layer

这一层当前也已经有雏形。

核心文件：

- [attribute_value.h](/Users/cui/Workspaces/apollo/include/apollo/game/attributes/attribute_value.h)

关键对象：

- `AttributeChangeEvent`

对应定义：

- [attribute_value.h](/Users/cui/Workspaces/apollo/include/apollo/game/attributes/attribute_value.h#L17)

### 当前已经具备的能力

- 变化事件里已经有：
  - `objectId`
  - `attributeId`
  - `oldValue`
  - `newValue`
  - `fromServer`

这说明 Apollo 已经把“属性变化”从纯 dirty flag 推进到“显式变化事件”了。

### 当前问题

这层还不够完整。

缺的主要是：

- `version`
- `changedAtMs`
- `source enum` 的完整扩展

也就是说，当前对象更像：

- `AttributeChangedNotification`

还不完全像：

- `AttributeChangeFact`

长期建议是把它补成真正可复用的事实对象。

## 五、L3：Attribute Sync Projection

这一层是当前 Apollo 在这条链路上完成度最高的一层。

核心文件：

- [attribute_sync.hpp](/Users/cui/Workspaces/apollo/include/apollo/game/attributes/attribute_sync.hpp)
- [attribute_sync.cpp](/Users/cui/Workspaces/apollo/modules/game/attributes/src/attribute_sync.cpp)

### 当前已经具备的能力

#### 1. 已经有独立的同步规则对象

- `AttributeSyncRule`
- `AttributeSyncSchema`

定义位置：

- [attribute_sync.hpp](/Users/cui/Workspaces/apollo/include/apollo/game/attributes/attribute_sync.hpp#L46)
- [attribute_sync.hpp](/Users/cui/Workspaces/apollo/include/apollo/game/attributes/attribute_sync.hpp#L57)

#### 2. 已经有明确的受众概念

- `Owner`
- `Aoi`
- `Team`
- `Guild`
- `Service`

定义位置：

- [attribute_sync.hpp](/Users/cui/Workspaces/apollo/include/apollo/game/attributes/attribute_sync.hpp#L13)

这说明同步层已经不再是“只给客户端”，而是在往通用状态传播器演进。

#### 3. 已经有 batch / version / ack / retry 雏形

当前同步器已经支持：

- `recordChange(...)`
- `collectDeltaBatches(...)`
- `collectRetryBatches(...)`
- `buildFullBatch(...)`
- `acknowledge(...)`

对应代码：

- [attribute_sync.cpp](/Users/cui/Workspaces/apollo/modules/game/attributes/src/attribute_sync.cpp#L86)
- [attribute_sync.cpp](/Users/cui/Workspaces/apollo/modules/game/attributes/src/attribute_sync.cpp#L131)
- [attribute_sync.cpp](/Users/cui/Workspaces/apollo/modules/game/attributes/src/attribute_sync.cpp#L205)
- [attribute_sync.hpp](/Users/cui/Workspaces/apollo/include/apollo/game/attributes/attribute_sync.hpp#L106)

### 当前层级判断

这层已经非常接近新的目标边界。

也就是说：

`AttributeSyncController` 当前最合理的重新命名语义确实是 SyncProjector / SyncBatcher。`

### 当前问题

#### 1. 它仍然直接监听 `AttributeContainer`

当前是：

- `bind(container)`
- 注册 change listener
- 直接收变化

这对早期实现是合理的，但长期更清晰的做法是：

- `AttributeContainer -> ChangeStream`
- `SyncProjector` 订阅 `ChangeStream`

而不是同步器直接绑定容器本体。

#### 2. `Aoi` 仍只是 audience 标签，不是真正 AOI 解析结果

目前：

- `AttributeSyncAudience::Aoi` 已存在

但还没有看到任何代码把：

- `AOIManager::GetVisibleEntities(...)`

真正接入到同步投影链路中。[aoi.cpp](/Users/cui/Workspaces/apollo/modules/game/world/src/aoi.cpp#L209)

所以当前的 `Aoi` 仍然是：

- 规则意图

不是：

- 已解析的视野受众

## 六、L4：Attribute Persistence Projection

这一层在当前 Apollo 里基本还没有正式出现。

这是目前最关键的缺口。

### 当前 `data/` 模块里有什么

`modules/data` 现在主要提供的是：

- cache manager
- primitive cache
- ORM connection / sql template
- Redis client / Redis template
- datasource / connection pool

例如：

- [cache_manager.cpp](/Users/cui/Workspaces/apollo/modules/data/cache/src/cache_manager.cpp)
- [connection_pool.cpp](/Users/cui/Workspaces/apollo/modules/data/orm/src/connection_pool.cpp)

### 当前 `data/` 模块里没有什么

没有看到明确的：

- `AttributePersistenceRule`
- `AttributePersistenceSchema`
- `PersistenceProjector`
- `PersistenceChangeSet`
- `SaveCoordinator` 的属性级实现

也就是说，当前 `data/` 更像：

- 数据访问基础设施层

还不是：

- 属性持久化投影层

### 当前层级判断

当前 Apollo 在这一层仍然停留在：

- “怎么连数据库”
- “怎么连 Redis”
- “怎么执行查询”

还没有进入：

- “属性变化如何被投影成持久化对象”

这是下一步最值得补的一层。

## 七、L5：Delivery Layer

这一层当前已经拆成了两块，但还没有和属性投影形成清晰对接。

## 八、L5.1：Sync Delivery

这一层当前只有零散原型，没有正式抽象闭环。

### 当前状态

- `AttributeSyncBatch` 已有
- 但没有统一的 `IAttributeSyncPublisher`
- 也没有正式的 `ReplicationService`

world 文档中已经明确提出需要：

- `ReplicationService`

见：

- [world-entry-transfer-design.md](/Users/cui/Workspaces/apollo/docs/architecture/world-entry-transfer-design.md#L118)

但当前 `modules/game/world` 实现里还没有这层对象。

### 当前层级判断

Apollo 目前已经有：

- 同步内容定义

但还没有：

- 同步发布执行层

## 九、L5.2：RPC Delivery

这一层当前相对清楚。

核心位置：

- [modules/net/CMakeLists.txt](/Users/cui/Workspaces/apollo/modules/net/CMakeLists.txt#L173)

从模块布局看：

- `net/tcp`
- `net/http`
- `net/websocket`
- `net/rpc`

已经和 `game/attributes`、`data/*` 分开。

### 当前层级判断

虽然实现还有原型性质，但边界方向是对的：

- RPC 在 `net/` 层
- 不在 `attributes/` 层
- 不在 `data/` 层

这意味着：

`RPC 基本已经天然是独立交付层，而不是属性同步器的一部分。`

### 当前问题

问题不在“有没有耦合”，而在“还没真正接好”。

也就是说：

- RPC 现在偏原型
- 但语义边界比同步层更干净

## 十、AOI 与复制链的当前状态

这一块是当前最容易产生误判的地方。

### 当前 AOI 做到了什么

`AOIManager` 当前负责：

- 更新实体位置
- 按 scene 建 AOIGrid
- 查询可见实体

核心代码：

- [aoi.cpp](/Users/cui/Workspaces/apollo/modules/game/world/src/aoi.cpp#L174)
- [aoi.cpp](/Users/cui/Workspaces/apollo/modules/game/world/src/aoi.cpp#L209)

### 当前 AOI 没做到什么

没有看到：

- `AOI -> AttributeSyncBatch`
- `AOI -> ReplicationPublisher`
- `enter/leave -> full sync / despawn sync`

也就是说：

AOI 当前只是：

- 可见性计算层

还不是：

- 复制执行层

### 这意味着什么

当前 Apollo 和 KBE 的差别正好卡在这里：

- KBE 的 `Witness` 把 AOI 和属性同步紧密接在一起
- Apollo 当前只把 AOI 和属性同步并列放着，还没中间桥接

这也正是为什么 Apollo 的 `Aoi` audience 目前只是一个标签，而不是完整复制链。

## 十一、当前代码按边界分类

可以先粗分成下面这张表。

### 已经比较符合边界的部分

- `modules/game/attributes/src/attribute_sync.cpp`
  - 已接近同步投影器
- `modules/net/*`
  - 基本仍是独立交付层
- `modules/game/world/src/aoi.cpp`
  - 目前仍是 AOI 本层，没有越界做存储或 RPC

### 还混杂但可收敛的部分

- `AttributeContainer`
  - 同时承担状态容器和 dirty 同步脏标记
- `AttributeChangeEvent`
  - 还不够完整，仍偏通知对象

### 明显缺失的部分

- `AttributePersistenceSchema`
- `PersistenceProjector`
- `PersistenceChangeSet`
- `IAttributeSyncPublisher`
- `ReplicationService`
- `VisibilityResolver -> SyncProjector` 桥接

## 十二、最推荐的下一步落点

如果按投入产出比排序，Apollo 后面最值得做的不是继续加重 `AttributeSyncController`，而是补两块桥接层。

### 1. 先补 Persistence Projection

原因：

- 当前 `data/` 只有访问层，没有属性持久化表达层
- 这是“属性同步器不要顺手写库”的关键分界点

建议新增：

- `include/apollo/game/attributes/attribute_persistence.hpp`
- `modules/game/attributes/src/attribute_persistence.cpp`

或按更强边界放到：

- `modules/data/core/include/apollo/data/persistence/...`

### 2. 再补 Replication Bridge

原因：

- 当前 `Aoi` audience 还只是标签
- world 侧还没有 replication 执行层

建议新增：

- `ReplicationService`
- `VisibilityResolver`
- `AttributeSyncPublisher`

让：

- `AOIManager`
- `AttributeSyncProjector`

真正通过中层桥接，而不是直接互相依赖。

## 十三、最终判断

当前 Apollo 在这条链路上的真实状态不是“完全没拆”，而是：

- 值层已经拆出来了
- 同步层也已经拆出来一半了
- 持久化投影层还没有成型
- AOI 到复制链的桥还没补上
- RPC 反而天生比同步更独立

所以后面应该坚持两条硬原则：

- 不要让 `AttributeSyncController` 继续吞持久化职责
- 不要让 `AOIManager` 继续吞属性打包职责

如果这两条守住，Apollo 这块边界就能很稳地收成：

- 属性变化是事实层
- 同步是同步
- 存储是存储
- RPC 是 RPC
