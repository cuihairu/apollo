---
title: Persistence、Repository 与 Unit of Work 设计
icon: hard-drive
order: 23
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Persistence
  - Repository
  - UnitOfWork
---

# Persistence、Repository 与 Unit of Work 设计

这篇文档解决的是 Apollo 整体框架继续往平台与业务交界处推进时，一个必须收住的问题：

`业务域到底如何正确地使用 Redis、SQL、缓存和持久化抽象，而不把平台层重新打穿。`

如果这层不清楚，后面很容易出现：

- world 里直接写 SQL
- social 里直接操作 Redis zset
- task、activity、guild 各写各的 repository 风格
- 事务边界、缓存边界、幂等边界完全失控

## 一、设计目标

这层设计要解决 6 个问题：

1. 业务域如何通过 repository 访问数据。
2. 哪些状态适合 SQL，哪些适合 Redis，哪些适合缓存。
3. Unit of Work 在 Apollo 里应承担什么责任。
4. 如何避免 repository 抽象过厚或过薄。
5. 如何和 `Platform Foundation`、`Domain Event`、`PlayerAnchor` 等设计对齐。
6. 如何给 BigWorld 增强层保留持久化扩展空间。

## 二、参考来源

### 1. 参考 repository / unit of work 思想

参考点：

- 业务域不直接依赖底层存储细节
- 一组状态变更有明确提交边界

### 2. 参考 MMO 实际数据形态

参考点：

- 玩家主状态和在线态不同
- 排行榜、锁、缓存、配置热数据不该全进同一个存储模型

### 3. 参考 KBE 的启示

参考点：

- 玩家长期逻辑和空间实时逻辑应该拆开
- 持久化不应直接绑在实时世界热路径里

不照搬点：

- 不把所有状态都做成重实体对象统一存取

## 三、为什么这样设计

Apollo 现在已经明确有：

- `Platform Foundation`
- `PlayerAnchor`
- `WorldHost`
- `Domain Components`

如果数据访问层没有统一方式，后面这些层都会被存储细节污染。

更合理的方式应该是：

- 平台层提供存储抽象和平台组件
- 业务域通过 repository 访问自己的数据边界
- Unit of Work 只负责有限范围的一致性提交

## 四、优点

- 业务域边界更稳定
- 存储替换更容易
- 更适合做缓存、幂等、事务边界
- 普通 MMO 和 BigWorld 模式都能共用

## 五、代价与风险

- repository 设计不好会过度抽象
- Unit of Work 设计太大，会把所有变更绑成重事务模型
- 需要严格定义哪些状态应该持久化、哪些不应该

## 六、为什么不选其他方案

### 不选“业务域直接写 SQL/Redis”

因为这会迅速把平台层打穿。

### 不选“所有状态都统一 ORM 化”

因为 MMO 很多状态并不适合单一存储模型。

### 不选“全系统统一大事务”

因为在线游戏和分布式系统本来就不适合这样设计。

Apollo 更适合：

- 业务域 repository
- 有限范围 unit of work
- outbox / event 协作

## 七、推荐状态分类

Apollo 建议至少把状态分成 4 类。

### 1. 长期主状态

例如：

- 账号
- 玩家主档
- 社交关系
- 任务长期状态

更适合：

- SQL / 关系型存储

### 2. 在线态 / 路由态

例如：

- 当前 session
- 当前 gateway
- 当前 world assignment

更适合：

- Redis / 内存索引 / 可恢复缓存

### 3. 高速热数据

例如：

- 排行榜
- 临时活动状态
- 限流计数

更适合：

- Redis / cache / specialized platform component

### 4. 世界实时态

例如：

- AOI
- 位置
- 当前战斗瞬时状态

更适合：

- `WorldHost` 内存态

而不是每次实时落库。

## 八、Repository 的职责

`Repository` 负责：

- 给业务域暴露稳定的数据访问接口
- 隐藏底层存储细节
- 负责聚合根边界内的数据读写

### 它不应该负责什么

- 不应该承接复杂业务编排
- 不应该直接承担跨多个业务域的大事务
- 不应该把所有底层存储能力一股脑往上暴露

## 九、推荐 repository 分类

### 1. Aggregate Repository

例如：

- `PlayerRepository`
- `GuildRepository`
- `TaskRepository`

### 2. Read Model Repository

例如：

- `LeaderboardReadRepository`
- `SocialProfileReadRepository`

### 3. Platform-backed Repository

例如：

- `SessionRouteRepository`
- `LockStateRepository`

## 十、Unit of Work 的定位

Apollo 里的 `UnitOfWork` 应该是：

- 有限范围的一次提交边界

而不是：

- 全系统一致性魔法

### 适合承担的

- 一个聚合根内的状态提交
- 一次命令处理过程中的相关写入
- 写模型 + outbox 的协同提交

### 不适合承担的

- 跨多个 app 的全局事务
- 跨多个宿主的强一致提交

## 十一、推荐对象模型

```text
PersistenceLayer
    ├── Repository
    ├── UnitOfWork
    ├── OutboxWriter
    ├── CachePolicy
    └── PersistenceMapper
```

### `UnitOfWork`

职责：

- 跟踪变更
- 提交写入
- 写出 outbox

### `OutboxWriter`

职责：

- 把领域事件安全写入待投递表或待投递队列

### `PersistenceMapper`

职责：

- 把 domain object 和存储模型做映射

## 十二、推荐提交流程

建议标准写流程接近：

1. command handler 加载 aggregate
2. 修改 domain state
3. 记录 domain events
4. `UnitOfWork` 提交 aggregate 状态
5. 同事务写出 outbox
6. 由消息层异步发布 integration event

### 为什么这样设计

因为这样可以把：

- 数据写入
- 事件发布

的可靠性衔接起来，而不是让二者互相脱节。

## 十三、缓存策略边界

缓存不应直接散在每个业务方法里。

Apollo 更合理的方式是：

- repository 配合 `CachePolicy`
- 平台层提供 cache store

### 建议策略

- cache aside
- read-through 读模型缓存
- leaderboard 走专用平台组件

### 不建议

- 在业务域里到处手写 `get/set/del redis key`

## 十四、和 Domain Event 的关系

这层必须和 `Domain Event / Message Bus` 对齐。

### 原则

- 领域状态变化先落 repository / unit of work
- 再通过 outbox 推出 integration event

这样能避免：

- 事件发出成功但数据没落地
- 数据落地成功但事件丢失

## 十五、和 PlayerAnchor / WorldHost 的关系

### `PlayerAnchor`

更适合：

- 主档状态
- 在线态快照
- world assignment

### `WorldHost`

更适合：

- 运行时热状态
- 周期性快照
- 必要的离场/切图保存

### 设计结论

Apollo 不应把 world 热路径强行做成 repository 驱动。

更合理的是：

- world 以内存态为主
- 关键边界点做持久化收口

## 十六、对当前 Apollo 的直接含义

Apollo 后续如果继续推进平台层和业务层，建议优先补：

- `RepositoryBase`
- `UnitOfWork`
- `Outbox`
- `CachePolicy`

优先试点业务域：

- player/session
- guild/social
- leaderboard

## 十七、结论

Apollo 的持久化设计如果要真正稳，关键不是“支持多少数据库”，而是：

- 平台层提供统一抽象
- 业务域通过 repository 访问
- Unit of Work 只做有限提交边界
- 事件通过 outbox 和消息层衔接

只有这样，平台能力和业务域边界才不会重新混掉。

## 相关阅读

- [Platform Foundation 设计](./platform-foundation-design.md)
- [Domain Event 与 Message Bus 设计](./domain-event-and-message-bus-design.md)
- [Configuration 与 Profile 设计](./configuration-and-profile-design.md)
