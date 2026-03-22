---
title: 持久化进程与 DBMgr 设计
icon: database
order: 8
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - DBMgr
  - PersistenceService
  - Repository
  - BaseApp
---

# 持久化进程与 DBMgr 设计

这篇文档只解决一个问题：

`Apollo 里的持久化执行层到底应该怎么定义，才能不再和 BaseApp(PlayerAnchor Host) 混淆。`

如果这个边界不单独定清楚，后面很容易再次把：

- 玩家在线主状态
- 数据库存取
- 缓存写回
- 断线恢复

全部揉进同一个进程名里。

## 一、先说结论

Apollo 后续应把持久化层统一理解为：

- `DBMgr`
- 或 `PersistenceService`
- 或最终独立部署的 `db-app / persistence-app`

它的语义是：

- 数据访问执行层
- Repository 与 UnitOfWork 宿主
- 异步写回、读快照、事务与幂等协调者

它不是：

- 玩家在线主状态宿主
- 玩家 session 宿主
- 玩家 world 路由宿主

这些职责应属于：

- `BaseApp(PlayerAnchor Host)`

## 二、为什么必须单独拆出来

### 1. 在线主状态和数据库执行是两种完全不同的负载

`BaseApp(PlayerAnchor Host)` 关注的是：

- 玩家登录归属
- session 绑定
- world assignment
- 顶号与重连恢复
- 在线主链路的一致性

而 `DBMgr / PersistenceService` 关注的是：

- SQL / Redis 访问
- 读写批处理
- 落盘重试
- 幂等和事务边界
- 冷热数据切分

两者的吞吐模型、故障模型、扩容方式都不同。

### 2. 把两者混在一起会把进程语义彻底做脏

一旦混在一起，后面就会不断出现这些问题：

- `BaseApp` 名字在，但代码中心对象其实是 `PlayerData`
- 在线问题和数据库问题互相影响
- 任何 SQL 抖动都可能拖慢玩家主链
- 后续想引入真正的 `PlayerAnchor` 会越来越难

### 3. KBE / BigWorld 本身就没有这么定义

从本地 KBEngine 源码可以直接验证：

- `BaseApp` 负责的是 `Proxy / login / relogin / giveClientTo`
- 真正的数据角色是 `DBMgr`

所以 Apollo 如果继续把 `BaseApp` 解释成数据库服务，不是“实现不同”，而是语义已经错了。

## 三、职责边界

### 1. `BaseApp(PlayerAnchor Host)` 负责

- `PlayerAnchor`
- `SessionLocator`
- `WorldAssignment`
- 重连恢复
- 在线主状态一致性
- 存档决策与保存协调

注意：

`BaseApp` 可以决定“什么时候需要保存”，但不应亲自承担“所有数据如何执行写入”。

### 2. `DBMgr / PersistenceService` 负责

- `Repository`
- `UnitOfWork`
- `SaveCoordinator`
- SQL / Redis Adapter
- 批量读写
- 写回队列
- 快照读取
- 事务与幂等控制

注意：

`DBMgr / PersistenceService` 可以执行持久化，但不应直接持有玩家在线主语义。

### 3. `WorldHost` 负责

- 世界内实时状态
- 地图实体表现态
- 副本、AOI、战斗内临时态

它只把需要持久化的领域状态提交给：

- `BaseApp`
- 或由 `BaseApp` 协调提交给 `DBMgr / PersistenceService`

## 四、推荐调用链

更合理的链路应当是：

```text
Client
  -> GatewayApp
  -> BaseApp(PlayerAnchor Host)
  -> WorldHost
```

持久化链单独是：

```text
WorldHost / Domain Service
  -> BaseApp(PlayerAnchor Host)
  -> SaveCoordinator
  -> DBMgr / PersistenceService
  -> MySQL / PostgreSQL / Redis / SQLite
```

这里有两个关键点：

- `BaseApp` 是在线主链节点
- `DBMgr` 是持久化执行节点

## 五、内部对象建议

建议把持久化层至少拆成下面几类对象。

### 1. `Repository`

职责：

- 聚合根读写接口
- 屏蔽底层 SQL / KV 实现细节

### 2. `UnitOfWork`

职责：

- 同一业务事务中的变更收集
- flush / commit / rollback 边界

### 3. `SaveCoordinator`

职责：

- 合并保存请求
- 决定立即写、延迟写、批量写
- 处理保存去重与顺序约束

### 4. `CacheFacade`

职责：

- Redis 缓存访问
- 快照缓存
- 排行榜、分布式锁等平台能力接入

### 5. `PersistenceWorker`

职责：

- 后台写回执行
- 重试
- 超时与失败隔离

## 六、部署形态建议

Apollo 可以分三档演进。

### 档位 1：内嵌持久化

适合早期原型。

做法：

- `Repository / SaveCoordinator` 暂时和 `BaseApp` 同进程

约束：

- 语义上仍然明确这是“内嵌的 PersistenceService”
- 不能把它重新命名理解成 `BaseApp` 的核心定义

### 档位 2：独立持久化服务

适合普通 MMO。

做法：

- 独立 `db-app / persistence-app`
- `BaseApp` 通过内部协议提交读写请求

优点：

- 在线主链和数据库波动隔离
- 可独立扩容

### 档位 3：分层持久化

适合更复杂项目。

做法：

- 热数据缓存层
- 异步写回层
- SQL 持久化层
- 分析/审计旁路

优点：

- 适合高并发和复杂平台能力

代价：

- 链路更长
- 一致性设计更复杂

## 七、为什么不把它继续放进当前 `apps/base-app`

短期代码上可以先共存，但语义上不能再继续混。

原因很直接：

- 当前 `apps/base-app` 已经被错误理解过一次
- 如果继续把持久化设计写在 `BaseApp` 名字下面，后面还会再次回到老问题

更稳的写法应该始终是：

- 当前 `apps/base-app` 里可能暂住了一部分持久化原型
- 但目标上它们属于 `PersistenceService`

## 八、与平台基础层的关系

`DBMgr / PersistenceService` 不是底层驱动本身，而是对平台基础层的装配使用者。

依赖关系更合理的表达是：

```text
RelationalStore / KvStore / Queue / Lock
    -> Repository / UnitOfWork / SaveCoordinator
    -> PersistenceService
    -> BaseApp / Domain Services
```

所以：

- Redis 排行榜
- 分布式锁
- MySQL / PostgreSQL / SQLite 访问

这些属于 `Platform Foundation`

而：

- 存档调度
- 持久化执行
- 读写协调

这些属于 `PersistenceService`

## 九、实施建议

建议按下面顺序落地。

### 阶段 1

- 先在文档层统一 `BaseApp != DBMgr`
- 为当前仓库所有相关文档加上语义说明

### 阶段 2

- 在模块层补 `Repository / UnitOfWork / SaveCoordinator`
- 把数据库适配统一沉到平台层

### 阶段 3

- 让 `base-app` 开始围绕 `PlayerAnchor` 收口
- 把原有偏数据职责逐步迁往 `PersistenceService`

### 阶段 4

- 再决定是否独立目录和独立进程
- 例如 `apps/db-app` 或 `apps/persistence-app`

## 十、结论

Apollo 后续要稳定演进，必须坚持下面这个判断：

- `BaseApp(PlayerAnchor Host)` 负责在线主状态
- `DBMgr / PersistenceService` 负责持久化执行
- `WorldHost` 负责世界内实时态

这三者可以协作，但不能再被写成同一个概念。

## 相关阅读

- [进程语义重定义](./process-semantics-redefinition.md)
- [进程重组与实施路线](./process-reorganization-and-rollout-plan.md)
- [Apollo 实施清单](./apollo-implementation-checklist.md)
- [Platform Foundation 设计](./platform-foundation-design.md)
- [Persistence、Repository 与 Unit of Work 设计](./persistence-repository-unitofwork-design.md)
