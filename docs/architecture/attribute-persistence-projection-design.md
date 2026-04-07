---
title: 属性持久化投影层设计
icon: hard-drive
order: 25
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Attribute
  - Persistence
  - Projection
  - DBMgr
---

# 属性持久化投影层设计

这篇文档只解决一个问题：

`当 Apollo 里的属性发生变化时，应该如何被投影成“可持久化对象”，并交给 DBMgr / PersistenceService 执行。`

这篇文档是：

- [属性同步、持久化与 RPC 边界设计](./attribute-sync-persistence-rpc-boundary.md)
- [属性同步、持久化与 RPC 现状映射](./attribute-sync-persistence-rpc-current-mapping.md)

的直接续篇。

它的目标不是讨论数据库细节，而是把下面这层单独定清楚：

- `AttributeChangeEvent`
  到
- `PersistenceChangeSet / PersistenceSnapshot`

之间的投影边界。

## 一、为什么必须单独定义这层

如果这层不单独存在，Apollo 后面很容易继续走向两种错误：

### 1. 属性同步器顺手写库

这会导致：

- 同步和持久化重新耦合
- 一旦数据库抖动，同步链也会被拖住
- 同一属性是否写库，变成同步层的隐式逻辑

### 2. DBMgr 直接监听属性容器

这会导致：

- 持久化层反向侵入 game 侧对象
- 在线态、world 态、存储态重新混成一层
- 后续做缓存、快照、回放时边界继续变脏

更合理的方式应该是：

- 属性变化先变成统一事实
- 再由持久化投影层生成持久化对象
- 最后交给 `DBMgr / PersistenceService` 执行

## 二、先说结论

Apollo 更合理的持久化链应该是：

```text
AttributeContainer
  -> AttributeChangeEvent
  -> AttributePersistenceProjector
  -> PersistenceChangeSet / PersistenceSnapshot
  -> SaveCoordinator
  -> Repository / UnitOfWork
  -> DBMgr / PersistenceService
  -> MySQL / Redis / Blob Store
```

其中：

- `AttributeContainer` 只负责值
- `Projector` 只负责投影
- `SaveCoordinator` 只负责保存调度
- `Repository` 只负责存取
- `DBMgr / PersistenceService` 只负责执行

## 三、这层到底回答什么

属性持久化投影层只回答 4 个问题：

### 1. 哪些属性需要持久化

不是所有属性都需要落库。

例如：

- `HP` 可能需要阶段性保存
- `position` 可能只在离线或切图时保存
- `temporary combat state` 可能完全不保存

### 2. 这些属性应该形成 patch 还是 snapshot

有的适合：

- 增量 patch

有的适合：

- 聚合快照

### 3. 什么时候需要 flush

例如：

- 立即写
- 防抖合并后写
- 周期写
- 下线写
- 切图前写

### 4. 持久化对象应该长什么样

例如：

- `PersistenceChangeSet`
- `PersistenceSnapshot`

而不是直接把：

- `AttributeChangeEvent`

拿去写库。

## 四、这层不应该回答什么

### 1. 不负责客户端同步

它不产生：

- `AttributeSyncBatch`

### 2. 不负责 RPC

它不产生：

- `RpcCommand`
- `RpcReply`

### 3. 不负责 AOI

它不关心：

- 谁可见谁
- 当前在哪个 view range

### 4. 不直接决定数据库实现细节

它可以决定：

- patch 还是 snapshot

但不应该直接决定：

- SQL 文本怎么拼
- Redis key 怎么组织
- MySQL 表连接怎么取

这些应该交给：

- `Repository`
- `CacheFacade`
- `PersistenceWorker`

## 五、推荐对象模型

## 六、Persistence Rule

建议至少定义一份独立的持久化规则对象：

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
    OnTransfer,
};

struct AttributePersistenceRule {
    uint32_t attributeId = 0;
    bool persistent = false;
    PersistenceMode mode = PersistenceMode::Patch;
    FlushPolicy flushPolicy = FlushPolicy::Debounced;
    bool includeInSnapshot = true;
    bool cacheBacked = true;
    bool requiresOrderedFlush = false;
};
```

这份规则只定义：

- 是否持久化
- 以什么模式持久化
- 什么时候进入 flush

它不定义：

- 同步 audience
- RPC route

## 七、Persistence Schema

所有规则应收在一份独立 schema 里：

```cpp
class AttributePersistenceSchema {
public:
    void setDefaultRule(AttributePersistenceRule rule);
    void registerRule(AttributePersistenceRule rule);
    const AttributePersistenceRule& resolve(uint32_t attributeId) const;

private:
    AttributePersistenceRule defaultRule_{};
    std::unordered_map<uint32_t, AttributePersistenceRule> rules_;
};
```

这层和 `AttributeSyncSchema` 并列存在，不应互相嵌套。

原因很简单：

- “需要同步”
- 和
- “需要持久化”

不是同一个维度。

## 八、Persistence ChangeSet

建议 Apollo 把增量持久化对象统一成：

```cpp
struct PersistenceChangeEntry {
    uint32_t attributeId = 0;
    ComVal value;
    uint64_t version = 0;
    uint64_t changedAtMs = 0;
    AttributePersistenceRule rule{};
};

struct PersistenceChangeSet {
    uint64_t objectId = 0;
    uint64_t objectVersion = 0;
    FlushPolicy flushPolicy = FlushPolicy::Debounced;
    uint64_t emittedAtMs = 0;
    std::vector<PersistenceChangeEntry> entries;
};
```

它代表的是：

- 一批可执行的持久化增量

它不代表：

- 已经落库完成
- 网络投递对象

## 九、Persistence Snapshot

对于需要全量保存的对象，建议独立定义：

```cpp
struct PersistenceSnapshot {
    uint64_t objectId = 0;
    uint64_t objectVersion = 0;
    uint64_t emittedAtMs = 0;
    std::unordered_map<uint32_t, ComVal> values;
};
```

使用场景：

- 下线保存
- 切图/迁移前保存
- 周期 checkpoint
- 崩溃恢复基线

## 十、Projector 的职责

建议引入：

```cpp
class IAttributePersistenceProjector {
public:
    virtual ~IAttributePersistenceProjector() = default;

    virtual std::optional<PersistenceChangeSet> projectChange(
        const AttributeChangeEvent& event) = 0;

    virtual std::optional<PersistenceSnapshot> buildSnapshot(
        uint64_t objectId,
        const AttributeContainer& container) = 0;
};
```

### 它做什么

- 根据规则过滤属性
- 决定进入 patch 还是 snapshot 流
- 合并同对象的多个属性变化
- 生成统一持久化对象

### 它不做什么

- 不直接写 MySQL
- 不直接发 Redis
- 不直接调 RPC

## 十一、SaveCoordinator 的职责

这层不是 Projector，但和它紧挨着。

推荐职责：

- 收集 `PersistenceChangeSet`
- 按 `FlushPolicy` 合并
- 去重
- 保序
- 决定何时交给 writer

建议对象：

```text
SaveCoordinator
    ├── ImmediateQueue
    ├── DebouncedQueue
    ├── PeriodicFlushWorker
    └── OrderedObjectTracker
```

### 为什么要把这层单独拉出来

因为：

- “怎么投影”
- 和
- “什么时候真正执行保存”

不是一回事。

`Projector` 负责表达。

`SaveCoordinator` 负责调度。

## 十二、Repository / UnitOfWork 的职责

Projector 之后不应该直接进入 SQL 字符串。

中间还应保留：

- `Repository`
- `UnitOfWork`

### `Repository`

职责：

- 将 `PersistenceChangeSet / Snapshot` 映射到聚合根或存储模型
- 屏蔽底层数据库细节

### `UnitOfWork`

职责：

- 合并一次业务事务中的多个持久化动作
- commit / rollback
- 幂等控制

## 十三、和 BaseApp / DBMgr 的关系

这层的边界必须和现有进程边界保持一致。

### 1. `BaseApp(PlayerAnchor Host)` 做什么

它可以：

- 决定何时需要保存
- 提交保存请求
- 协调登出、切图、重连恢复时的保存时机

但它不应直接承担：

- 属性持久化规则定义
- 数据库存储执行细节

### 2. `DBMgr / PersistenceService` 做什么

它只消费：

- `PersistenceChangeSet`
- `PersistenceSnapshot`

并负责：

- 执行写库
- 缓存写回
- 事务与重试

它不应该回头去理解：

- `AttributeSyncBatch`
- `AOI audience`

## 十四、典型属性应该怎么分类

### 1. 强持久化属性

例如：

- 等级
- 货币
- 经验
- 背包容量

建议：

- `persistent = true`
- `mode = Patch`
- `flush = Immediate 或 Debounced`

### 2. 弱持久化属性

例如：

- 当前 HP
- 当前 MP
- 上次地图位置

建议：

- `persistent = true`
- `mode = Patch 或 Snapshot`
- `flush = Periodic / OnLogout / OnTransfer`

### 3. 非持久化属性

例如：

- temporary battle state
- 当前技能前摇
- 临时 AOI 表现态

建议：

- `persistent = false`

### 4. 只快照不增量的属性

例如：

- 大型属性集合
- 派生缓存态

建议：

- `mode = Snapshot`

## 十五、一个完整例子

玩家 `HP` 从 `100` 变为 `75`，`Gold` 从 `200` 变为 `250`。

### 第一步：属性变化事实

```text
AttributeChangeEvent(player=1001, attr=HP, old=100, new=75)
AttributeChangeEvent(player=1001, attr=Gold, old=200, new=250)
```

### 第二步：Persistence Projector 处理

假设规则是：

- `HP`: 弱持久化，`Periodic`
- `Gold`: 强持久化，`Immediate`

则投影结果可能是：

```text
PersistenceChangeSet(
  objectId=1001,
  flushPolicy=Periodic,
  entries=[HP=75]
)

PersistenceChangeSet(
  objectId=1001,
  flushPolicy=Immediate,
  entries=[Gold=250]
)
```

### 第三步：SaveCoordinator 分流

- `Gold` 立即提交给 writer
- `HP` 进入周期队列

### 第四步：Writer 执行

- `Repository` 生成更新动作
- `PersistenceWorker` 执行落库

这里最重要的是：

- 这条链完全不需要借助同步器
- 也不需要借助 RPC

## 十六、和当前代码的落点建议

Apollo 当前最合理的落点有两种。

### 方案 A：先放在 `modules/game/attributes`

优点：

- 离 `AttributeChangeEvent` 最近
- 先把边界立起来最快

建议文件：

- `include/apollo/game/attributes/attribute_persistence.hpp`
- `modules/game/attributes/src/attribute_persistence.cpp`

适合：

- 先做投影对象与规则层

### 方案 B：直接放在 `modules/data/core`

优点：

- 语义上更贴近 persistence 层
- 长期更干净

建议文件：

- `modules/data/core/include/apollo/data/persistence/attribute_persistence.hpp`
- `modules/data/core/src/attribute_persistence.cpp`

适合：

- 已经准备好同步调整 `modules/data` 目录语义

### 当前更推荐

短期建议：

- 先做方案 A

原因：

- 当前 Apollo 还没有成熟的 data-persistence 核心层
- 先把投影边界做出来，比先争目录更重要

## 十七、和同步器的关系

必须再强调一次：

- `AttributeSyncSchema`
- `AttributePersistenceSchema`

是平行关系，不是包含关系。

不能出现这种设计：

- 同步规则里顺便加 `persistent=true`

因为这样很快又会退回：

- “同步器顺手管存储”

Apollo 后续必须把这两份规则分开维护。

## 十八、最终总结

Apollo 后续应把属性持久化投影层理解成：

- 属性变化事实到持久化表达对象之间的独立中层

它的职责只有三个：

- 定义哪些属性应持久化
- 定义它们应该以 patch 还是 snapshot 形式表达
- 定义它们应该进入哪种 flush 策略

它不负责：

- 客户端同步
- AOI 可见性
- RPC 调用
- 数据库执行细节

如果这层单独立住，Apollo 这条链路就能真正形成稳定结构：

- 属性变化是事实层
- 同步是同步
- 存储是存储
- RPC 是 RPC

而这正是后面把 `BaseApp / DBMgr / WorldHost / Gateway` 边界继续做干净的前提。
