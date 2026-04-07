---
title: 属性同步、持久化与 RPC 边界设计
icon: arrows-split-up-and-left
order: 23
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - AttributeSync
  - Persistence
  - RPC
  - Boundary
---

# 属性同步、持久化与 RPC 边界设计

这篇文档只解决一个问题：

`Apollo 里和“属性变化”相关的能力，到底应该怎么拆，才能不再把同步、存储、调用揉成一个混合层。`

如果这层不提前收住，后面很容易继续出现：

- 属性同步器顺手写库
- 数据库存储层顺手决定客户端广播
- RPC 调用直接承载状态同步
- AOI 可见性逻辑和属性打包逻辑混在一起
- 一个“属性变化”同时带着业务语义、传输语义、存储语义

最终结果就是：

- 模块边界持续变脏
- 后续做重连恢复、切图、回档、观测都会越来越混

## 一、先说结论

Apollo 里更合理的拆法应该是：

```text
Attribute Value Layer
    -> Attribute Change Layer
    -> Attribute Sync Projection
    -> Attribute Persistence Projection
    -> Delivery Layer (Sync / RPC / Event)
```

其中最关键的原则只有一句：

- 同一份属性变化事实，可以被投影到同步、持久化、审计、事件
- 但这份事实本身不属于这些执行层

换句话说：

- 属性同步只负责同步
- 数据库存储只负责存储
- RPC 只负责调用
- AOI 只负责“谁看得见谁”

## 二、为什么必须这样拆

Apollo 现在已经逐步收清了几个重要边界：

- `Gateway` 不是在线主状态宿主
- `BaseApp(PlayerAnchor Host)` 不是数据库服务
- `DBMgr / PersistenceService` 不是玩家在线主状态宿主
- `WorldHost` 不是归属裁决层

但和属性变化相关的链路还缺最后一刀：

- “属性变化”到底是同步问题、存储问题，还是调用问题？

答案应该是：

- 它首先是一个领域事实
- 然后才被不同执行层消费

如果不这样拆，后面会反复出现几类问题。

### 1. 会把状态传播和动作调用混成一类

例如：

- “请扣 25 点血”是调用
- “当前血量变成 75”是状态传播

这两者不能用同一种对象表达。

### 2. 会把客户端同步和数据库落地混成一类

例如：

- 有些属性需要同步但不需要立即写库
- 有些属性需要写库但不需要同步给客户端
- 有些属性只需要同步给自己，不需要广播给 AOI

这说明“同步规则”和“持久化规则”天然不是一回事。

### 3. 会把 AOI 和属性同步打成一个黑箱

AOI 负责的是：

- 谁当前可见
- 谁进入视野
- 谁离开视野

它不应该直接变成：

- 具体属性如何打包
- 是否全量同步
- 是否需要重试

### 4. 会让基础层越来越难复用

如果属性同步器里直接写：

- SQL 落地
- Redis 写回
- RPC route
- 客户端协议拼装

那这层就不再是通用同步器，而会变成一个越来越重的耦合中心。

## 三、推荐分层

## 四、L1：Attribute Value Layer

这一层只负责：

- 保存属性值
- 提供读写接口
- 提供 dirty / version / snapshot

推荐对象：

- `AttributeContainer`
- `AttributeValue`
- `AttributeSnapshot`

### 这一层回答什么

- 当前属性值是什么
- 哪些属性发生了变化
- 当前对象的属性版本是多少

### 这一层不回答什么

- 该不该发给客户端
- 该不该写库
- 该不该发起 RPC
- 该不该进入 AOI 广播

也就是说：

`AttributeContainer` 只能是状态容器，不能是协调中心。

## 五、L2：Attribute Change Layer

这一层把“属性变化”表达成统一事实对象。

推荐对象：

- `AttributeChangeEvent`
- `AttributeChangeSource`

建议对象模型：

```cpp
enum class AttributeChangeSource : uint8_t {
    LocalLogic,
    RemoteSync,
    PersistenceReplay,
    Script,
    GMCommand,
};

struct AttributeChangeEvent {
    uint64_t objectId = 0;
    uint32_t attributeId = 0;
    ComVal oldValue;
    ComVal newValue;
    uint64_t version = 0;
    uint64_t changedAtMs = 0;
    AttributeChangeSource source = AttributeChangeSource::LocalLogic;
};
```

### 这一层的原则

- 它表达的是“发生了什么”
- 不是“要怎么发”
- 也不是“要怎么存”

### 为什么必须有这层

因为只有先把“事实”和“执行策略”拆开，后面才能同时支持：

- 状态同步
- 持久化写回
- 审计日志
- 领域事件
- 回放与恢复

## 六、L3：Attribute Sync Projection

这一层只负责：

- 读取 `AttributeChangeEvent`
- 根据同步规则投影为 `AttributeSyncBatch`

推荐对象：

- `AttributeSyncSchema`
- `AttributeSyncRule`
- `AttributeSyncProjector`
- `AttributeSyncBatch`

Apollo 当前已有的 `AttributeSyncController` 更合理的长期定位就是：

- 同步投影器 / 批处理器

而不是：

- 属性总协调器

### 推荐同步规则

```cpp
enum class AttributeSyncAudience : uint32_t {
    None = 0,
    Owner = 1u << 0,
    Aoi = 1u << 1,
    Team = 1u << 2,
    Guild = 1u << 3,
    Service = 1u << 4,
};

struct AttributeSyncRule {
    uint32_t attributeId = 0;
    AttributeSyncAudience audiences = AttributeSyncAudience::Owner;
    AttributeSyncPriority priority = AttributeSyncPriority::Normal;
    AttributeSyncReliability reliability = AttributeSyncReliability::Reliable;
    uint32_t minIntervalMs = 0;
    bool allowDelta = true;
    bool includeInFullSync = true;
    bool coalesce = true;
};
```

### 这一层负责什么

- owner / aoi / team / guild / service 的受众标签
- delta / full
- priority / reliability
- batch 合并
- ack / retry / resync

### 这一层不负责什么

- 数据库存储
- SQL / Redis 路由
- RPC handler
- AOI 查询算法

也就是说：

`SyncProjector` 只决定“同步表达和投递策略”。`

## 七、L4：Attribute Persistence Projection

这一层只负责：

- 读取 `AttributeChangeEvent`
- 根据持久化规则投影为 `PersistenceChangeSet`

推荐对象：

- `AttributePersistenceSchema`
- `AttributePersistenceRule`
- `PersistenceProjector`
- `PersistenceChangeSet`
- `PersistenceSnapshot`

建议对象模型：

```cpp
enum class PersistenceMode : uint8_t {
    None,
    Patch,
    Snapshot,
};

enum class FlushPolicy : uint8_t {
    Immediate,
    Debounced,
    Periodic,
    OnLogout,
};

struct AttributePersistenceRule {
    uint32_t attributeId = 0;
    bool persistent = false;
    PersistenceMode mode = PersistenceMode::Patch;
    FlushPolicy flushPolicy = FlushPolicy::Debounced;
};

struct PersistenceChangeSet {
    uint64_t objectId = 0;
    uint64_t version = 0;
    std::unordered_map<uint32_t, ComVal> values;
};
```

### 这一层负责什么

- 哪些属性属于持久化白名单
- patch 还是 snapshot
- 立即写、延迟写、周期写还是下线写
- 合并保存请求
- 保存顺序和去重

### 这一层不负责什么

- 客户端广播
- AOI 可见性
- RPC 响应
- world 路由

所以：

`PersistenceProjector` 只决定“持久化表达和落地策略”。`

## 八、L5：Delivery Layer

这一层分成两种完全不同的交付能力：

### 1. `Sync Delivery`

职责：

- 发送 `AttributeSyncBatch`
- 向客户端、Gateway、World replication、服务镜像通道投递状态

本质：

- 状态传播

### 2. `RPC Delivery`

职责：

- 发送 `RpcCommand`
- 返回 `RpcReply`
- 发布 `RpcEvent` 或 one-way command

本质：

- 动作调用

## 九、同步与 RPC 的区别

Apollo 后续必须明确：

- `RPC != 属性同步`
- `属性同步 != RPC`

### 判断规则

如果表达的是：

- “请做一件事”

那它更像：

- `RpcCommand`

如果表达的是：

- “现在状态是什么”

那它更像：

- `AttributeSyncBatch`

如果表达的是：

- “某件事已经发生”

那它更像：

- `DomainEvent`

### 例子

`ApplyDamage(playerId=1001, amount=25)`

这是：

- 调用

`HP=75`

这是：

- 状态同步

`PlayerDamaged(playerId=1001, amount=25)`

这是：

- 领域事件

这三者绝不能混成一个对象。

## 十、AOI 的准确边界

AOI 不属于属性同步器。

AOI 应该只负责：

- 当前可见集计算
- enter / leave 检测
- visibility delta

也就是说，AOI 提供的是：

- `VisibilitySet`
- `VisibilityDelta`
- `AudienceResolver`

而不是：

- 属性打包
- 序列化格式
- ack / retry
- full sync 逻辑

更合理的关系应该是：

```text
AOIService
    -> VisibilityResolver
    -> SyncProjector
    -> ReplicationPublisher
```

这里：

- AOI 负责“谁该收到”
- SyncProjector 负责“发什么”
- Publisher 负责“怎么发”

## 十一、推荐依赖方向

Apollo 更合理的依赖关系应该是：

```text
AttributeContainer
  -> emits AttributeChangeEvent
      -> AttributeSyncProjector
          -> AttributeSyncBatch
          -> SyncPublisher
      -> AttributePersistenceProjector
          -> PersistenceChangeSet
          -> PersistenceWriter
      -> DomainEventProjector (optional)
          -> DomainEvent
          -> EventBus
```

RPC 则应当是旁路：

```text
RpcCommand
  -> DomainService
      -> mutate AttributeContainer
          -> emits AttributeChangeEvent
```

也就是：

- RPC 驱动业务动作
- 业务动作修改属性
- 属性变化再分流到同步、持久化、事件

而不是：

- RPC 包直接充当同步包
- 同步器顺手写库
- 写库结果顺手拼客户端广播

## 十二、推荐接口边界

建议 Apollo 后续至少统一成下面这些接口。

```cpp
class IAttributeChangeStream {
public:
    virtual ~IAttributeChangeStream() = default;
    virtual void publish(const AttributeChangeEvent& event) = 0;
};

class IAttributeSyncProjector {
public:
    virtual ~IAttributeSyncProjector() = default;
    virtual std::vector<AttributeSyncBatch> project(
        const AttributeChangeEvent& event) = 0;
};

class IAttributePersistenceProjector {
public:
    virtual ~IAttributePersistenceProjector() = default;
    virtual std::optional<PersistenceChangeSet> project(
        const AttributeChangeEvent& event) = 0;
};

class IAttributeSyncPublisher {
public:
    virtual ~IAttributeSyncPublisher() = default;
    virtual void publish(const AttributeSyncBatch& batch) = 0;
};

class IPersistenceWriter {
public:
    virtual ~IPersistenceWriter() = default;
    virtual void write(const PersistenceChangeSet& changeSet) = 0;
};

class IRpcBus {
public:
    virtual ~IRpcBus() = default;
    virtual void sendCommand(const RpcCommand& command) = 0;
};
```

这样之后：

- `modules/game/attributes` 只负责状态容器、变化事件、同步投影
- `modules/data` 只负责持久化投影与持久化执行
- `modules/net` / `modules/protocol` 只负责传输与调用
- `modules/game/world` 只负责 AOI / world session / visibility

## 十三、和现有 Apollo 边界的关系

这套拆法应和现有边界保持一致。

### 1. 和 `BaseApp(PlayerAnchor Host)` 的关系

`BaseApp` 可以负责：

- 什么时候需要保存
- 哪些保存请求需要协调

但不应该亲自成为：

- 持久化规则定义层
- 数据库存储执行层
- 属性同步器本体

### 2. 和 `DBMgr / PersistenceService` 的关系

`DBMgr / PersistenceService` 应只消费：

- `PersistenceChangeSet`
- `PersistenceSnapshot`

它不应该消费：

- `AttributeSyncBatch`
- `RpcCommand`

### 3. 和 `Gateway` 的关系

`Gateway` 应只负责：

- 同步消息下行
- 上行状态消息接入

它不应拥有：

- 属性持久化语义
- 属性白名单语义

### 4. 和 `WorldHost` 的关系

`WorldHost` 应负责：

- 产生世界内属性变化
- 提供 AOI / visibility context

它不应独自决定：

- 数据库存储策略

## 十四、一个完整例子

玩家受到伤害，`HP` 从 `100` 变成 `75`。

### 第一步：产生事实

```text
AttributeChangeEvent(
  objectId=1001,
  attributeId=HP,
  oldValue=100,
  newValue=75,
  version=42
)
```

### 第二步：同步投影

```text
AttributeSyncBatch(
  audience=Owner|Aoi,
  entries=[HP=75]
)
```

### 第三步：持久化投影

```text
PersistenceChangeSet(
  objectId=1001,
  version=42,
  values=[HP=75]
)
```

### 第四步：可选领域事件

```text
PlayerDamaged(
  playerId=1001,
  amount=25
)
```

这里最重要的是：

- 这 3 份结果来自同一个事实
- 但它们不是同一个对象

## 十五、当前代码的演进建议

Apollo 当前更合理的演进方式应该是：

### 1. 保留 `AttributeSyncController`

但重新定位为：

- `AttributeSyncProjector / Batcher`

而不是：

- 属性总协调器

### 2. 新增 `AttributePersistenceSchema`

把“哪些属性要写库、何时写库”从同步层剥离出来。

### 3. 新增 `PersistenceProjector`

让数据库层消费：

- `PersistenceChangeSet`

而不是直接消费属性同步批次。

### 4. 严禁把同步包当 RPC 包

`AttributeSyncBatch` 只能表达状态同步，不能冒充：

- `Command`
- `Request`
- `Reply`

### 5. AOI 只输出 visibility context

不要把 AOI 继续发展成：

- 属性包生成器
- 持久化触发器

## 十六、最终边界总结

Apollo 后续应把这几句话当成硬边界。

- `AttributeChangeEvent` 是事实层，不带执行语义。
- `AttributeSyncBatch` 是状态传播对象，不带数据库和 RPC 语义。
- `PersistenceChangeSet` 是持久化对象，不带客户端广播语义。
- `RpcCommand / RpcReply` 是调用对象，不带属性同步语义。
- `AOI` 只回答“谁可见谁”，不回答“属性如何打包发送”。

如果后面任何一个模块同时开始承担：

- 属性变化表达
- 客户端同步
- 数据库存储
- RPC 调用

四者中的两种及以上语义，

就说明边界又开始重新变脏了。
