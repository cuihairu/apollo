---
title: EntitySchema 设计
icon: table-columns
order: 20
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - EntitySchema
  - RemoteEntityRef
  - ReplicationSchema
---

# EntitySchema 设计

这篇文档解决的是 Apollo 如果继续参考 KBE，最深的一层基础设施问题：

`实体定义到底由什么统一描述。`

如果这层不立住，Apollo 后面会出现同一个实体的信息散落在多处：

- C++ 类型
- 网络协议
- 持久化字段
- 客户端同步规则
- 远程方法表

前期还能忍，后期一定会越来越乱。

## 一、先说结论

Apollo 要真正吸收 KBE 的强项，必须尽早补出统一实体定义层。

也就是一组明确的 schema：

- `EntitySchema`
- `PropertySchema`
- `MethodSchema`
- `ReplicationSchema`
- `PersistenceSchema`
- `RemoteEntityRef`

这层不是工具层小优化，而是整个运行时的一致性底座。

## 二、KBE 里的直接证据

从本地源码看：

- [entitydef.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/lib/entitydef/entitydef.h)
  - `EntityDef` 负责加载实体定义系统
  - 维护脚本模块、property、method、component、md5
- [scriptdef_module.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/lib/entitydef/scriptdef_module.h)
  - 持有 `PROPERTYDESCRIPTION_MAP`
  - 持有 `METHODDESCRIPTION_MAP`
  - 持有 component 描述
  - 持有 detail level
  - 持有 volatile info

这说明 KBE 的核心不是“有 BaseApp 和 CellApp”，而是：

- 这些运行时对象都依赖一份统一的实体定义系统

## 三、为什么 Apollo 也必须有这一层

Apollo 现在如果继续往下长，而没有 schema 层，很容易出现这些问题：

### 1. 属性定义散落

- world 里一份
- DB 里一份
- 协议里一份

### 2. 远程方法边界散落

- 哪些方法可以跨进程调
- 哪些方法可以面向客户端

没有统一表。

### 3. 客户端同步策略散落

- 哪些属性进 AOI 增量
- 哪些属性只在 own client
- 哪些属性只在 detail level 变化时发

没有统一描述。

这三类问题一旦叠起来，后面 BigWorld 模式基本就没法稳。

## 四、推荐 schema 分层

建议 Apollo 至少形成下面这组 schema：

```text
EntitySchema
    ├── PropertySchema
    ├── MethodSchema
    ├── ComponentSchema
    ├── ReplicationSchema
    └── PersistenceSchema
```

### `EntitySchema`

职责：

- 描述一个实体整体定义
- 唯一类型 ID
- 名称
- 组件集合
- 属性集合
- 方法集合

### `PropertySchema`

职责：

- 属性名
- 数据类型
- 所属域
- 是否持久化
- 是否对客户端可见
- 是否 volatile

### `MethodSchema`

职责：

- 方法名
- 参数签名
- 调用域
- 暴露方向

### `ReplicationSchema`

职责：

- 哪些属性面向 own client
- 哪些属性面向 AOI viewer
- detail level 策略
- alias / 压缩策略

### `PersistenceSchema`

职责：

- 哪些属性入库
- 哪些属性只运行期存在
- 哪些属性可做脏数据跟踪

## 五、推荐域模型

建议每个属性和方法至少有一个明确域。

### 属性域

- `Anchor`
- `World`
- `ClientOwn`
- `ClientView`
- `Persistent`

### 方法域

- `BaseOnly`
- `WorldOnly`
- `ClientCallable`
- `RemoteCallable`

### 为什么域必须显式化

因为 Apollo 后面会面对：

- `PlayerAnchor`
- `AvatarEntity`
- `GhostReplica`
- `Witness`

这些对象并不是全都该看见全部属性。

## 六、推荐 `RemoteEntityRef`

如果 Apollo 后续要做 KBE 风格远程实体调用，必须有显式远程引用对象。

建议至少包含：

```text
RemoteEntityRef
    entity_id
    entity_type
    component_type
    component_id
    authority_role
    route_version
```

### 为什么不能只传 `(entityId, serviceId)`

因为到了 BigWorld 模式，远程调用还要知道：

- 调的是 base 还是 world
- 当前 entity 是 real 还是 ghost
- route 是否过期

## 七、推荐 alias 和压缩策略

KBE 的一个很实际的优点是：

- property / method / entity 在同步时可压缩成 alias

Apollo 不一定要照搬实现，但应该尽早把抽象留出来。

### 建议至少有

- `entity_alias`
- `property_alias`
- `method_alias`

### 为什么要提前留口

因为一旦协议完全定死，再做压缩会很痛苦。

## 八、推荐 detail level 模型

客户端同步不能只靠“全量 / 增量”。

建议提前支持：

- `Near`
- `Mid`
- `Far`

不同 detail level 下，不同属性和方法是否可见由 `ReplicationSchema` 决定。

这一步对：

- `Witness`
- `Ghost`
- 带宽控制

都很重要。

## 九、推荐 volatile 模型

不是所有属性都需要稳定持久化同步。

建议显式支持：

- 位置
- 朝向
- 速度
- 动作状态

这类高频 volatile 字段。

### 为什么要单独建模

因为 volatile 数据通常：

- 高频
- 可丢部分帧
- 不适合和长期属性走同一套策略

## 十、Apollo 中建议新增的抽象

建议新增：

- `modules/game/schema/include/apollo/game/schema/entity_schema.hpp`
- `modules/game/schema/include/apollo/game/schema/property_schema.hpp`
- `modules/game/schema/include/apollo/game/schema/method_schema.hpp`
- `modules/game/schema/include/apollo/game/schema/component_schema.hpp`
- `modules/game/schema/include/apollo/game/schema/replication_schema.hpp`
- `modules/game/schema/include/apollo/game/schema/persistence_schema.hpp`
- `modules/game/schema/include/apollo/game/schema/schema_registry.hpp`
- `modules/game/core/include/apollo/game/core/remote_entity_ref.hpp`

## 十一、和当前 Apollo 的关系

Apollo 当前还没到要把这层一次性全代码实现出来的阶段，

但至少要先把方向定住。

### 当前阶段

- 先定义 schema 抽象
- 先收敛实体定义来源

### 下一阶段

- `WorldHost` / `PlayerAnchor` / `AvatarEntity` 开始引用 schema
- 协议层开始引用 `ReplicationSchema`
- 数据层开始引用 `PersistenceSchema`

### 再下一阶段

- 进入 `RemoteEntityRef`
- 进入 distributed schema / alias / detail level

## 十二、近期最小可交付版本

### 最小范围

- `EntitySchema`
- `PropertySchema`
- `MethodSchema`
- `SchemaRegistry`
- `RemoteEntityRef`

### 暂时不做

- 完整 DSL
- 脚本生成器
- 全量客户端 SDK 自动生成

### 验证标准

至少要能验证：

1. 一个实体类型可由统一 schema 描述
2. world 和 anchor 都能读到同一份属性定义
3. 远程调用不再只靠散落的 ad-hoc 消息
4. replication / persistence 边界能从 schema 上读出来

## 十三、结论

Apollo 如果要真正从 KBE 里学到最值钱的东西，不能只学 `BaseApp / CellApp / Ghost / Witness` 这些运行时名词。

更深的一层是：

- 它们都依赖统一实体定义系统

Apollo 对应要补的就是：

- `EntitySchema`
- `RemoteEntityRef`
- `ReplicationSchema`
- `PersistenceSchema`

只有这层立住，前面所有运行时对象才不会各自长出一套局部规则。

## 相关阅读

- [KBEngine 源码分析](./kbe-source-analysis.md)
- [Base Cell Proxy 对象模型](./base-cell-proxy-model.md)
- [PlayerAnchor 设计稿](./player-anchor-design.md)
- [Distributed Space 设计](./distributed-space-design.md)
