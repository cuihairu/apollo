---
title: 进程重组与实施路线
icon: shuffle
order: 7
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - 进程重组
  - 实施路线
  - BaseApp
  - WorldHost
  - DBMgr
---

# 进程重组与实施路线

这篇文档回答的是一个更直接的问题：

`既然 Apollo 当前的进程语义已经错位，那么后面到底怎么重组，才不会一边修文档一边继续把错误实现固化。`

它不是重复讲理想架构，而是明确：

- 当前进程先怎么理解
- 中期怎么过渡
- 长期怎么收成稳定拓扑

如果还没看过术语基线，先看：

- [进程语义重定义](./process-semantics-redefinition.md)

## 一、先说结论

Apollo 更合理的进程演进路线，不是直接“把当前代码硬解释成 KBE 已落地”，而是分三步：

1. `cell-app` 先继续作为 `WorldHost` 装配壳推进
2. `base-app` 先停止被当成正确 `BaseApp`，再逐步转向 `PlayerAnchor Host`
3. 数据持久化最终要单独抽成 `DBMgr / PersistenceService / db-app`

也就是说，最终稳定形态更接近：

```text
LoginApp
GatewayApp
BaseApp(PlayerAnchor Host)
WorldApp / WorldHost
DBMgr / PersistenceService
```

然后只有在真正需要时，再继续进入：

```text
BaseAppMgr
CellApp
CellAppMgr
Distributed Space
```

## 二、为什么不能继续沿当前错位硬推

当前仓库里：

- `apps/base-app`
  - 行为上偏数据服务
- `apps/cell-app`
  - 行为上偏 world runtime 原型

如果继续按下面的错误前提推进：

- `base-app` 已经是 `BaseApp`
- `cell-app` 已经是 `CellApp`

就会出现两个后果：

### 1. 错误职责继续膨胀

当前 `base-app` 会继续长出更多数据相关职责，
最后变成“名字叫 BaseApp，但骨子里仍然是数据服务中心”。

### 2. World 层继续被误判成熟度

当前 `cell-app` 如果继续被当成真正 `CellApp`，后面会过早讨论：

- ghost
- witness
- authority transfer
- cellappmgr

但 world runtime 自己还没收口。

## 三、推荐的稳定目标拓扑

Apollo 中长期更稳的进程拓扑，建议按下面理解。

### 1. `LoginApp`

职责：

- 账号认证
- 登录风控
- 票据签发

不持有：

- 长期在线主状态
- world assignment

### 2. `GatewayApp`

职责：

- 公网连接
- 心跳
- 协议适配
- 路由转发

不持有：

- 玩家长期归属
- world 主状态

### 3. `BaseApp(PlayerAnchor Host)`

职责：

- `PlayerAnchor`
- `SessionLocator`
- `WorldAssignment`
- 重连恢复
- 重复登录策略
- 持久化协调

### 4. `WorldApp / WorldHost`

职责：

- `WorldHost`
- `MapInstance`
- `WorldSession`
- 世界内实体表现态
- AOI / 地图 / 副本 / 进入离开 / 切图

### 5. `DBMgr / PersistenceService`

职责：

- SQL / Redis / 持久化执行
- Repository / UnitOfWork
- 异步写回与读快照

不持有：

- 玩家在线主状态

## 四、对当前进程的过渡判断

### 1. 当前 `apps/cell-app`

短期统一理解为：

- `WorldHost` 装配壳

这意味着：

- 代码可以继续推进
- 但语义上不再说它已经是完整 `CellApp`

### 2. 当前 `apps/base-app`

短期统一理解为：

- 偏数据服务原型

这意味着：

- 不能继续把它当成正确 `BaseApp` 已经落地
- 也不应该继续给它追加更多“纯 DB 服务”语义

### 3. 当前是否立刻改目录名

不建议现在立刻大改目录名。

原因很简单：

- 当前主矛盾是语义和实现错位
- 不是目录名本身

所以短期更合理的是：

- 先改语义
- 再改实现
- 最后才决定是否改名

## 五、阶段化重组路线

建议分 4 个阶段推进。

## 六、阶段 1：先收口 `WorldHost`

### 目标

让当前 `cell-app` 完全退回到 world runtime 装配壳。

### 要做什么

- 把 world loop 收到 `WorldHost`
- 补 `WorldSpace`
- 补 `MapInstance`
- 补 `WorldSession`
- 统一 world entry / leave / transfer 接口

### 这一阶段完成后

系统语义变成：

- `cell-app` 当前本质上是 `world-app`
- 即使目录名不改，架构理解已经改对

### 这一阶段不要做什么

- 不做 `Ghost`
- 不做 `Witness`
- 不做 `CellAppMgr`

## 七、阶段 2：让 `base-app` 停止继续当“纯数据服务名字壳”

### 目标

不要求这一步立刻变成完整 `BaseApp`，但至少要开始转向 `PlayerAnchor Host`。

### 要做什么

- 新增 `PlayerAnchor`
- 新增 `AnchorManager`
- 新增 `SessionLocator`
- 新增 `WorldAssignment`
- 让 `base-app` 开始承接：
  - 激活玩家
  - 绑定 session
  - world 路由
  - 重连恢复

### 这一步的关键判断

从这一步开始：

- `base-app` 的中心对象不再是 `PlayerData`
- 而是 `PlayerAnchor`

## 八、阶段 3：把数据层正式降级成支撑层

这是最关键的一步，因为它会决定是否需要独立 `db-app`。

### 路线 A：短中期继续保留在 `base-app` 内部

适合当前仓库状态。

做法：

- 保留 `DatabaseService`
- 保留 `SaveQueue`
- 但把它们降级为 `PlayerAnchor` 的支撑能力
- 通过 `PlayerRepository / SaveCoordinator` 对外提供能力

优点：

- 迁移成本低
- 对当前代码冲击小

缺点：

- 过渡期职责仍然会比较挤

### 路线 B：中后期拆独立 `db-app`

更接近长期正确形态。

做法：

- 把当前 `DatabaseService`
- `SaveQueue`
- DB 请求协议

逐步抽到独立进程或独立持久化服务中。

此时结构变成：

```text
BaseApp(PlayerAnchor Host)
  -> PlayerRepository
  -> Persistence RPC / DBMgr
```

优点：

- 语义最清晰
- 更接近长期稳定分层

缺点：

- 当前阶段改造成本更高

### 当前建议

当前最现实的建议是：

- 阶段 2 先走路线 A
- 阶段 3 再视复杂度决定是否进入路线 B

也就是：

- 先把 `BaseApp` 做对
- 再决定是否拆 `db-app`

## 九、阶段 4：按需进入真正的大世界进程拓扑

只有当前三步都稳定后，才建议继续进入：

- `BaseAppMgr`
- `CellApp`
- `CellAppMgr`

### 进入条件

至少满足下面几个条件再开始：

- `WorldHost` 已稳定
- `PlayerAnchor` 已稳定
- `BaseApp` 和持久化层边界已稳定
- world entry / transfer / recovery 已经跑顺

### 否则会怎样

如果前面没稳就提前上：

- `CellAppMgr`
- `Ghost`
- `Witness`
- authority transfer

那么复杂度会直接爆炸。

## 十、是否应该引入 `world-app`

这是一个现实问题。

### 方案 A：目录名暂时不改

做法：

- 继续保留 `apps/cell-app`
- 但文档、设计、实现上全部按 `WorldHost` 装配壳理解

优点：

- 改动最小

缺点：

- 名称仍然会持续误导

### 方案 B：中期引入 `world-app`

做法：

- 新增 `apps/world-app`
- 当前 `apps/cell-app` 逐步退役或后续用于真正 distributed cell

优点：

- 语义最清楚

缺点：

- 需要迁移构建和部署语义

### 当前建议

当前先不急着新增 `world-app` 目录。

更合理的顺序是：

1. 先把当前 `cell-app` 的实现收成真正 `WorldHost` 壳
2. 如果后面真的要做 distributed `CellApp`
3. 再决定：
   - 新增 `world-app`
   - 或恢复 `cell-app` 到真正 distributed 语义

## 十一、是否应该引入 `db-app`

这个问题和 `world-app` 类似，但优先级稍低。

### 短期建议

不急着新增 `db-app`。

先做：

- `PlayerRepository`
- `SaveCoordinator`
- `PersistenceService`

把数据能力先从 `BaseApp` 语义中心降级成支撑层。

### 中期判断标准

当出现下面情况时，再考虑独立 `db-app`：

- `base-app` 内部同时维护大量锚点逻辑和数据库执行逻辑
- 保存队列、仓储、缓存、事务协调明显膨胀
- 多个进程都开始依赖统一持久化 RPC

这时拆 `db-app` 就合理了。

## 十二、推荐的阶段里程碑

### 里程碑 A

当前 `cell-app` 已完全按 `WorldHost` 装配壳运行。

达到后说明：

- world runtime 已收口

### 里程碑 B

当前 `base-app` 已开始持有 `PlayerAnchor`。

达到后说明：

- 在线主状态开始收口到正确语义

### 里程碑 C

持久化层与 `BaseApp` 形成稳定边界。

达到后说明：

- 可以决定是否独立 `db-app`

### 里程碑 D

才开始真正进入 distributed world。

达到后说明：

- `CellApp / AppMgr / Ghost / Witness` 才值得推进

## 十三、近期执行建议

如果只看最近几轮最应该落地的动作，我建议优先顺序是：

1. 继续把 `WorldHost` 收实
2. 新增 `PlayerAnchor` 和 `AnchorManager`
3. 让 `base-app` 开始承接玩家激活与路由
4. 把 `DatabaseService` 包进 `PlayerRepository`
5. 再决定是否拆独立 `db-app`

而不是：

1. 继续修当前 `base-app` 的 DB 原型
2. 一边叫它 `BaseApp`
3. 一边又把语义往 `DBApp` 方向越做越深

## 十四、结论

Apollo 的进程重组，不应该理解成一次性目录改名。

更准确的理解应该是：

- 先把错误语义停住
- 再把正确语义做出来
- 最后再决定目录名和最终进程拓扑

压缩成一句话就是：

`先把 WorldHost 和 PlayerAnchor 做对，再决定 world-app / db-app / true CellApp 怎么拆。`

## 相关阅读

- [进程语义重定义](./process-semantics-redefinition.md)
- [Apollo 重构路线图](./apollo-refactor-roadmap.md)
- [Apollo 实施清单](./apollo-implementation-checklist.md)
- [BaseApp 演进设计](./base-app-evolution.md)
- [玩家在线主链设计](./player-online-flow.md)
- [WorldHost 设计稿](./world-host-design.md)
