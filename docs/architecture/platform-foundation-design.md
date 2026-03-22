---
title: Platform Foundation 设计
icon: database
order: 12
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Platform
  - Redis
  - SQL
---

# Platform Foundation 设计

这篇文档解决的是 Apollo 分层里另一块非常容易混乱的区域：

`Redis、MySQL、PostgreSQL、SQLite、排行榜、分布式锁、缓存封装这些能力，到底属于哪一层。`

如果不把它们单独抽出来，后面通常会出现两种坏结果：

- 原始存储细节直接渗进业务域
- 排行榜、分布式锁这种平台语义能力到处各写一套

所以 Apollo 需要单独定义：

- `Platform Foundation`

## 一、先说结论

Apollo 应该把“平台基础能力”单独作为一层：

- 位于 `Runtime Ops Host` 之上
- 位于 `Game Foundation` 和 `Domain Components` 之下

它负责的不是游戏对象语义，而是：

- 存储
- 缓存
- 中间件语义封装
- 通用平台能力

这里也要和进程语义分开理解：

- `Platform Foundation` 是模块层能力
- `DBMgr / PersistenceService` 是对这些能力的装配使用者
- `BaseApp(PlayerAnchor Host)` 不是这一层的同义词

## 二、为什么要单独成层

### 1. 原始驱动不是业务逻辑

例如：

- Redis client
- MySQL/PostgreSQL/SQLite connection
- transaction
- pool

这些都不属于：

- guild
- friend
- task
- battle

### 2. 平台语义能力也不该散在业务域

例如：

- leaderboard
- distributed lock
- rate limiter
- queue

这些已经不是原始驱动，但也不该由每个业务域自己造一遍。

### 3. 不同业务会复用同一平台能力

例如排行榜可以被：

- 战力排行
- 公会排行
- 活动排行
- 赛季排行

共同复用。

这说明它更像平台组件，而不是单一业务域。

## 三、这层的职责

建议至少承接下面 5 类能力。

### 1. 数据访问基础

- MySQL
- PostgreSQL
- SQLite
- Redis
- 连接池
- 事务
- 查询执行
- 序列化映射

### 2. 缓存与键值语义

- cache aside
- TTL
- namespace
- key codec

### 3. 平台通用组件

- leaderboard
- distributed lock
- rate limiter
- idempotency store
- delay queue
- simple queue

### 4. 数据访问抽象

- repository base
- unit of work
- shard route

### 5. 平台级服务能力

- config storage
- feature flag storage
- audit storage
- operation state storage

## 四、推荐分层子结构

`Platform Foundation` 内部建议再分 3 档：

### 1. Driver Layer

最底层原始驱动能力。

例如：

- Redis executor
- SQL connection
- connection pool

### 2. Adapter Layer

把驱动统一成 Apollo 风格接口。

例如：

- `KvStore`
- `RelationalStore`
- `CacheStore`
- `LockProvider`

### 3. Platform Components

更高层的通用平台语义。

例如：

- `LeaderboardService`
- `DistributedLockService`
- `RateLimiter`
- `QueueService`

## 五、推荐对象模型

```text
PlatformFoundation
    ├── RelationalStore
    ├── KvStore
    ├── CacheStore
    ├── LockProvider
    ├── LeaderboardService
    ├── QueueService
    └── RepositoryBase
```

### `RelationalStore`

职责：

- MySQL/PostgreSQL/SQLite 抽象
- connection / transaction / query

### `KvStore`

职责：

- Redis 等键值访问
- zset/hash/stream 等统一入口

### `CacheStore`

职责：

- TTL
- 命名空间
- codec

### `LockProvider`

职责：

- 分布式锁
- lease
- renew
- fencing token

### `LeaderboardService`

职责：

- 分数更新
- 排行查询
- 排行截断
- 赛季切换

### `RepositoryBase`

职责：

- 给业务域提供统一 repository 基类
- 隔离底层存储细节

## 六、典型能力归属

### 应该放在 `Platform Foundation` 的

- Redis 封装
- MySQL/PostgreSQL/SQLite 抽象
- repository base
- leaderboard
- distributed lock
- queue
- rate limiter

### 不应该放在 `Platform Foundation` 的

- 玩家任务逻辑
- 公会业务规则
- 战斗伤害计算
- world runtime

### 业务域应该怎么依赖它

业务域通过：

- repository
- platform service
- cache service

来依赖平台层，而不是直接散写 Redis/SQL 操作。

## 七、和其他层的边界

### 和 `Runtime Ops Host`

`Runtime Ops Host` 负责：

- 进程治理
- 调试
- 观测

`Platform Foundation` 负责：

- 中间件和平台语义封装

### 和 `Game Foundation`

`Game Foundation` 负责：

- schema
- entity
- remote call
- replication
- script ABI

`Platform Foundation` 不承接这些游戏对象语义。

### 和 `Domain Components`

业务域依赖平台层，但不应自己重造平台层。

例如：

- `GuildRepository` 依赖 `RelationalStore`
- `SeasonRankDomain` 依赖 `LeaderboardService`

## 八、推荐目录

建议未来显式形成：

- `modules/platform`

推荐结构：

```text
modules/platform
    ├── data
    ├── cache
    ├── lock
    ├── leaderboard
    ├── queue
    └── repository
```

## 九、对当前 Apollo 的直接含义

Apollo 后续如果继续推进整体框架，`modules/data` 不应只停留在数据库访问层。

更合理的演进是：

- 保留底层 `data` 能力
- 再往上补一个明确的 `platform` 层

这样下面这些能力才有稳定落点：

- Redis 简单封装
- 排行榜
- 分布式锁
- MySQL/PostgreSQL/SQLite 统一访问
- repository base

## 十、结论

Apollo 的整体框架如果要继续做清楚，平台基础能力必须单独成层。

这层既不是纯底层驱动，也不是具体游戏业务，而是：

- 存储和缓存的统一抽象层
- 通用平台语义组件层

只有这样，后面的业务域装配和进程装配才不会继续被 Redis/SQL 细节污染。

## 相关阅读

- [Apollo 分层设计](./apollo-layering-design.md)
- [持久化进程与 DBMgr 设计](./persistence-process-and-db-manager-design.md)
- [Runtime Ops Host 设计](./runtime-ops-host-design.md)
- [Apollo 渐进式游戏框架理论设计](./apollo-progressive-game-framework.md)
