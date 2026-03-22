---
title: Authority Transfer 设计
icon: right-left
order: 17
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - AuthorityTransfer
  - CellApp
  - BigWorld
---

# Authority Transfer 设计

这篇文档专门解决 BigWorld 模式里最危险的一段链路：

`一个实体从当前权威 cell 切到另一个权威 cell 时，系统如何不断链。`

如果这层没设计清楚，分布式空间很快就会出现几类典型故障：

- 实体瞬间消失
- 消息打到旧 cell 丢失
- ghost 和 real 同时写状态
- 客户端视图短暂断裂
- 迁移失败后无法回滚

所以 authority transfer 必须单独作为一个运行时对象来设计，而不是塞进普通 `teleport` 接口里。

## 一、先说结论

`Authority Transfer` 不是普通切图，也不是简单的“删旧建新”。`

它本质上是：

- 一个带状态机的权威切换协议

至少要同时保证：

- 旧权威在切换窗口内仍可兜底
- 新权威能先建立接管态
- 迁移中的消息有 route 可走
- 成功后只有一个 real authority
- 失败后可以回滚

## 二、KBE 里的直接证据

从本地源码看：

- `cellapp/entity.h`
  - 有 `realCell()`
  - 有 `ghostCell()`
  - 有 `changeToGhost()`
  - 有 `changeToReal()`
  - 有 `teleport*()` 一组接口
- `cellapp/cellapp.h`
  - 有 `reqTeleportToCellApp`
  - 有 `reqTeleportToCellAppCB`
  - 有 `reqTeleportToCellAppOver`
- `cellapp/ghost_manager.h`
  - 有 `ghost_route_`
  - 有 route 和消息转发逻辑

这已经说明在 KBE 里，authority transfer 至少包含：

- 跨 cell 请求
- 回调确认
- 完成确认
- route 兜底
- real/ghost 角色切换

Apollo 后续如果要落大世界，这套语义必须单独立出来。

这里也要明确：

- 当前仓库里的 `apps/cell-app` 还不是这里说的真正 `CellApp`
- 当前阶段它仍应先按 `WorldHost` 装配壳理解

## 三、为什么普通 world transfer 不够

前面的 [World 进入与切图设计](./world-entry-transfer-design.md) 里讲的 `transfer out / transfer in`，解决的是：

- world 之间的进入离开

但 distributed space 里的 authority transfer 更细。

### 普通 world transfer

默认假设：

- 一个 world 里只有一个权威节点

### authority transfer

默认假设：

- 同一个大空间分布在多个 cell
- 同一个实体会在不同 cell 间切换实时权威

所以它面对的问题是：

- 仍在同一 space 内
- 但 authority 变了

这比“换地图”更细、更危险。

## 四、authority transfer 要解决什么问题

### 1. 权威唯一性

任意时刻最终只能有一个 real authority。

### 2. 边界连续性

迁移窗口内客户端不能看到实体直接消失。

### 3. 消息不断链

延迟消息不能因为切换到旧 cell 就直接丢。

### 4. 失败可回滚

目标 cell 接管失败时，旧权威不能已经不可恢复。

## 五、推荐核心对象

建议 Apollo 至少形成下面这些对象：

```text
AuthorityTransferCoordinator
    ├── AuthorityTransferContext
    ├── TransferStateMachine
    ├── MigrationRoute
    ├── TransferLease
    └── TransferJournal
```

### `AuthorityTransferCoordinator`

职责：

- 驱动整个迁移状态机
- 发起 prepare / commit / finalize
- 处理超时和回滚

### `AuthorityTransferContext`

职责：

- 保存本次迁移的上下文快照
- 保存源 cell、目标 cell、分片、版本号

### `MigrationRoute`

职责：

- 迁移窗口里的消息转发
- 旧权威到新权威的临时路由

### `TransferLease`

职责：

- 给迁移窗口设置租约
- 避免双边无限等待

### `TransferJournal`

职责：

- 记录迁移事件
- 便于故障恢复和调试

## 六、推荐状态机

建议 authority transfer 至少有下面这些状态：

- `Preparing`
- `Prepared`
- `Bridging`
- `Committing`
- `Committed`
- `RollingBack`
- `Closed`

### `Preparing`

- 源 cell 检测到需要迁移
- 创建 transfer context
- 请求目标 cell 准备接管

### `Prepared`

- 目标 cell 已接受上下文
- 但还没拿到最终 authority

### `Bridging`

- 双态窗口
- 旧 real 仍保底
- 新侧准备接管
- migration route 生效

### `Committing`

- 开始执行 real/ghost 角色切换

### `Committed`

- 目标 cell 成为新 real
- 源 cell 降级为 ghost 或清理

### `RollingBack`

- 接管失败或超时
- 撤销目标准备态
- 恢复旧权威

### `Closed`

- 迁移流程彻底结束

## 七、推荐四段式迁移协议

建议 authority transfer 按下面四步执行。

### 第一步：Prepare

1. 源 cell 发现实体接近迁移边界
2. 创建 `AuthorityTransferContext`
3. 把上下文发给目标 cell
4. 目标 cell 进入 `Prepared`

这一阶段不能切 real。

### 第二步：Bridge

1. 建立 `MigrationRoute`
2. 源 cell 继续保留 real
3. 目标 cell 创建预备接管实体视图
4. ghost / witness 进入迁移窗口模式

这一阶段的目标是不断链，不是完成切换。

### 第三步：Commit

1. 目标 cell 升级为 real
2. 源 cell 降级为 ghost
3. `CellAppMgr` 更新 partition authority
4. route version 增长

### 第四步：Finalize

1. 清理旧 route
2. 更新 witness 可见集
3. 清理 transfer journal 中的活动态
4. 迁移进入 `Closed`

## 八、推荐 `AuthorityTransferContext`

建议至少包含：

```text
AuthorityTransferContext
    transfer_id
    transfer_epoch
    entity_id
    entity_type
    source_cell_id
    target_cell_id
    source_partition_id
    target_partition_id
    source_space_id
    target_space_id
    position
    direction
    avatar_snapshot
    volatile_snapshot
    route_version
    lease_expire_at
```

### 为什么既要 `transfer_id` 又要 `transfer_epoch`

因为：

- `transfer_id` 标识本次迁移实例
- `transfer_epoch` 用来处理实体连续多次快速迁移时的乱序确认

## 九、real / ghost 切换规则

建议在协议层明确下面这条硬规则：

`只有 commit 成功后，目标 cell 才能对外宣告自己是 real。`

也就是说：

- prepare 阶段不能提前写死 target = real
- bridge 阶段也不能双 real

### 切换前

- source = real
- target = prepared/ghost-like shadow

### 切换后

- source = ghost 或 closed
- target = real

## 十、消息不断链策略

authority transfer 最大的坑，往往不在状态切换，而在延迟消息。

### 推荐策略

1. 旧 cell 在迁移窗口安装 `MigrationRoute`
2. 到达旧 cell 的延迟消息先查 route
3. 若 entity 已不再本地 real，则转发到新 real
4. route 在 finalize 后延迟一小段时间再清除

### 哪些消息必须走 route

- client 操作消息
- cross-cell entity message
- volatile sync follow-up

### 哪些消息不应继续转发

- 已经过期的旧 epoch 消息
- 明确针对旧 route version 的失效包

## 十一、回滚策略

如果目标 cell 在 prepare 或 bridge 阶段失败：

- 不能让实体直接消失

### 推荐回滚流程

1. 目标 cell 发送 prepare failed / lease expired
2. 源 cell 保持 real 不变
3. 目标 cell 回收预备状态
4. `CellAppMgr` 记录本次失败
5. 可按策略重试或延后迁移

### commit 后失败怎么办

commit 后已经切 real，这种失败代价更高。

建议：

- commit 之前尽量把可失败动作做完
- commit 后只允许 finalize 失败
- finalize 失败通过后台清理和 route 延时兜底处理

## 十二、和 `Witness / Ghost` 的关系

authority transfer 不能脱离：

- [Witness 与 Ghost 设计](./witness-ghost-design.md)

来理解。

### 对 `Ghost`

- transfer 期间 ghost 是桥，不是终点

### 对 `Witness`

- transfer 完成后必须刷新客户端视图来源
- 不能让客户端同时长期订阅旧 real 和新 real

## 十三、和 `CellAppMgr` 的关系

`CellAppMgr` 不应该直接操作实体内部状态，

但应该负责：

- 协调迁移申请
- 更新 partition authority 目录
- 记录迁移结果
- 对失败迁移做调度决策

所以关系应该是：

- `CellAppMgr` 负责控制面
- `AuthorityTransferCoordinator` 负责数据面协议执行

## 十四、Apollo 中建议新增的抽象

建议新增：

- `modules/game/world/include/apollo/game/world/authority_transfer_context.hpp`
- `modules/game/world/include/apollo/game/world/authority_transfer_state.hpp`
- `modules/game/world/include/apollo/game/world/authority_transfer_coordinator.hpp`
- `modules/game/world/include/apollo/game/world/migration_route.hpp`
- `modules/game/world/include/apollo/game/world/transfer_lease.hpp`
- `modules/game/world/include/apollo/game/world/transfer_journal.hpp`

## 十五、近期最小可交付版本

不要一开始就做完整跨进程大迁移。

建议先做非常窄的一版：

### 最小范围

- 明确 `AuthorityTransferContext`
- 明确状态机
- 明确 `MigrationRoute`
- 明确 prepare / commit / finalize 三段协议

### 暂时不做

- 自动负载驱动迁移
- 多实体批量迁移
- 复杂链式回滚

### 验证标准

至少要能验证：

1. 一个实体从 cell A 迁到 cell B
2. 迁移窗口里延迟消息不会直接丢
3. commit 后只有一个 real
4. prepare 失败时能回滚

## 十六、结论

Apollo 如果要做真正的大世界，authority transfer 必须被当成一级运行时问题，而不是普通接口细节。

它真正要保证的是：

- 权威唯一
- 消息不断链
- 迁移可回滚
- 客户端视图连续

只有这层单独立住，`CellApp` 才不会退化成“多个地图服加一点跨服转发”。

## 相关阅读

- [进程语义重定义](./process-semantics-redefinition.md)
- [Base Cell Proxy 对象模型](./base-cell-proxy-model.md)
- [Distributed Space 设计](./distributed-space-design.md)
- [Witness 与 Ghost 设计](./witness-ghost-design.md)
- [AppMgr 设计](./app-manager-design.md)
- [KBEngine 源码分析](./kbe-source-analysis.md)
