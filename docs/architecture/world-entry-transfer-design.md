---
title: World 进入与切图设计
icon: map-location-dot
order: 13
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - WorldHost
  - 切图
  - 重连恢复
---

# World 进入与切图设计

这篇文档解决的是 world 侧最关键的一段链路：

`玩家怎么进入 world，怎么切图，断线后 world 侧又该怎么配合恢复。`

前面的文档已经把几个核心边界收住了：

- `LoginApp` 负责认证入口
- `BaseApp` 负责 `PlayerAnchor`
- `Gateway` 负责公网连接
- `WorldHost` 负责世界运行时
- `DBMgr / PersistenceService` 负责持久化执行

但 world 侧还缺一篇更细的流程稿。

这篇就是把这部分补出来。

## 一、先说结论

world 侧最合理的定位不是“自己决定玩家归属”，而是：

- 接收来自 `PlayerAnchor` 的 world assignment
- 承接玩家进入、离开、切图
- 管理世界内会话和表现实体
- 向 `BaseApp` 回报 world 侧结果

也就是说，world 侧是：

- 在线主链里的执行层

而不是：

- 归属裁决层

## 二、当前代码状态

从当前代码看：

- [cell_manager.hpp](/C:/Users/cui/Workspaces/apollo/apps/cell-app/include/cell/cell_manager.hpp)
  - 当前已经有 `EntityManager`
  - 有 `AOIManager`
  - 有 `PlayerEntity`
- [cell_server.cpp](/C:/Users/cui/Workspaces/apollo/apps/cell-app/src/cell_server.cpp)
  - 当前 `CellServer` 自己维护 `gameLoop()`
  - 自己接收创建实体、销毁实体、移动实体请求
  - `broadcastToViewers()` 里已经出现“实际应通过 Gateway 发送”的痕迹
- [scene.cpp](/C:/Users/cui/Workspaces/apollo/modules/game/world/src/scene.cpp)
  - 当前 `Scene` 只负责场景内实体生命周期

这说明 Apollo 当前 world 原型已经有了一些核心积木：

- tick
- scene
- entity
- AOI

但还缺下面这些关键层：

- world entry pipeline
- world session
- enter/leave/transfer 状态机
- reconnect recovery hook

## 三、world 侧应该负责什么

建议 `WorldHost` 和其下属 world 服务只负责下面 6 类事情。

### 1. 接收入场请求

- 接收来自 `BaseApp` 的 `enter_world`
- 校验目标 map/instance 是否存在
- 准备 world 侧会话上下文

### 2. 创建 world session

- 为玩家创建 `WorldSession`
- 记录其当前 space/map/instance
- 记录其 world 内运行状态

### 3. 创建或恢复表现实体

- 创建 `AvatarEntity`
- 恢复位置、朝向、基础属性快照
- 把实体放入 `Scene/WorldSpace`

### 4. 维护 world 内生命周期

- tick 更新
- AOI
- scene 切换
- map/instance 生命周期

### 5. 执行离开与切图

- world leave
- transfer out
- transfer in

### 6. 回报执行结果

- 进入成功
- 进入失败
- transfer 成功
- transfer 回滚
- 离开完成

## 四、world 侧不应该负责什么

### 1. 不应该判断玩家是不是“真正上线”

这个判断应该在：

- `BaseApp(PlayerAnchor Host)`

### 2. 不应该持有长期连接权威

world 不应该知道公网 socket。

它应该只知道：

- `playerId`
- `session binding` 摘要
- `route target`

### 3. 不应该单独决定切图最终归属

world 可以发起 transfer 请求，但最终归属更新应该经由：

- `PlayerAnchor`

### 4. 不应该长期持有玩家完整在线主状态

world 持有的是：

- world session
- 表现实体

不是：

- 玩家全部长期状态

## 五、推荐对象关系

建议 world 侧收成下面这组对象：

```text
WorldHost
    ├── MapInstanceManager
    ├── WorldSessionManager
    ├── AvatarFactory
    ├── TransferCoordinator
    ├── AOIService
    └── ReplicationService
```

### `WorldSessionManager`

职责：

- 创建 `WorldSession`
- 记录玩家当前 world 上下文
- 按 `playerId/sessionId` 查找 world 侧状态

### `WorldSession`

职责：

- 记录当前玩家在哪个 `world/map/instance/space`
- 记录当前表现实体 ID
- 记录当前 world 生命周期状态

### `AvatarFactory`

职责：

- 根据玩家快照创建或恢复 `AvatarEntity`
- 处理入场点、默认位置、出生点

### `TransferCoordinator`

职责：

- world 内发起切图
- 执行 transfer out / transfer in
- 处理失败回滚

## 六、为什么需要 `WorldSession`

当前很容易把 world 里的玩家直接理解为：

- `PlayerEntity`

但这层是不够的。

因为世界里的“表现实体”和“world 侧在线上下文”不是一回事。

### `WorldSession` 和 `AvatarEntity` 的区别

`WorldSession` 关注：

- 当前 world 归属
- 当前实例
- 当前实体 ID
- 当前 enter/transfer state

`AvatarEntity` 关注：

- 位置
- 朝向
- 属性快照
- AOI 可见性
- 战斗表现

这两层拆开后，切图和重连恢复会清楚很多。

## 七、推荐进入世界流程

### 标准 enter 流程

1. `BaseApp` 根据 `PlayerAnchor` 下发 `enter_world`
2. `WorldHost` 校验目标 `MapInstance`
3. `WorldSessionManager` 创建 `WorldSession`
4. `AvatarFactory` 生成或恢复 `AvatarEntity`
5. `WorldSpace` / `Scene` 挂载实体
6. `AOIService` 执行 enter
7. `WorldHost` 回报 enter success
8. `BaseApp` 更新锚点的当前 world 归属

### enter 失败时怎么处理

如果 world 侧无法进入，例如：

- map 不存在
- instance 满员
- 快照非法

应该：

1. 不创建正式 world session
2. 回报 `enter failed`
3. 由 `BaseApp` 决定是否换 world 或回退到默认地图

## 八、推荐离开世界流程

### 标准 leave 流程

1. `BaseApp` 或 `PlayerAnchor` 发起 `leave_world`
2. `WorldHost` 找到 `WorldSession`
3. 触发 `AOIService` 离场
4. `Scene/WorldSpace` 卸载表现实体
5. 回收 `WorldSession`
6. 回报 leave complete

### leave 和 logout 的关系

不是所有 leave 都是 logout。

leave 可能代表：

- 切图前离开旧 world
- 断线后暂时冻结
- 主动下线离场

所以 leave reason 应该显式区分。

## 九、推荐切图流程

切图建议收成标准两段式。

### 阶段 A：transfer out

1. 当前 `WorldHost` 发起 `prepare_transfer`
2. `WorldSession` 状态切到 `TransferringOut`
3. 冻结必要 world 内操作
4. 导出 transfer context
5. 通知 `BaseApp`

### 阶段 B：transfer in

1. `BaseApp` 更新 `WorldAssignment`
2. 目标 `WorldHost` 接收 `enter_world`
3. 使用 transfer context 恢复 `WorldSession`
4. 创建目标地图里的表现实体
5. 成功后旧 world 执行最终清理

### 为什么要两段式

因为如果只写成：

- 旧 world 直接删
- 新 world 直接建

那么一旦目标 world 进入失败，就很难回滚。

## 十、推荐 transfer context

建议至少包含下面这些字段：

```text
TransferContext
    player_id
    source_world_id
    target_world_id
    source_map_id
    target_map_id
    source_instance_id
    target_instance_id
    spawn_position
    avatar_snapshot
    route_version
```

### 为什么需要 `route_version`

因为切图后：

- `Gateway` 的旧路由快照可能失效

切图完成后要由 `BaseApp` 推新路由。

## 十一、推荐断线恢复流程

world 侧的断线恢复必须和 `PlayerAnchor` 配合，而不是自己独立决策。

### 推荐流程

1. `Gateway` 断线并通知 `BaseApp`
2. `PlayerAnchor` 进入 `Disconnected`
3. `BaseApp` 通知 `WorldHost` 进入“可恢复保留”状态
4. `WorldSession` 状态切到 `Suspended`
5. world 暂时保留玩家 world 归属和必要实体快照
6. 若客户端在窗口内重连成功，则恢复 `WorldSession`
7. 若超时未恢复，则执行正式离场

### world 侧需要保留什么

建议只保留最少量可恢复信息：

- 当前 map/instance/space
- 表现实体快照
- 必要战斗上下文摘要

不要把整个 world runtime 对象图都指望原样冻结。

## 十二、推荐 world session 状态机

建议 world 侧至少有下面这些状态：

- `Entering`
- `Active`
- `Suspended`
- `TransferringOut`
- `TransferringIn`
- `Leaving`
- `Closed`

### 状态意义

`Entering`

- 正在创建 world 会话和实体

`Active`

- 正常在线运行

`Suspended`

- 断线后等待恢复

`TransferringOut`

- 准备离开当前 world

`TransferringIn`

- 正在进入目标 world

`Leaving`

- 正在执行离场清理

`Closed`

- 已经彻底关闭

## 十三、推荐接口方向

建议 world 侧至少补下面几类接口。

### `BaseApp -> WorldHost`

- `enter_world`
- `leave_world`
- `prepare_transfer`
- `resume_world_session`
- `expire_world_session`

### `WorldHost -> BaseApp`

- `enter_world_result`
- `leave_world_result`
- `transfer_prepared`
- `transfer_completed`
- `transfer_failed`

### `WorldHost` 内部

- `create_world_session`
- `restore_avatar_entity`
- `suspend_world_session`
- `close_world_session`

## 十四、对当前 `cell-app` 的迁移建议

当前 `cell-app` 最稳的迁移方向是：

### 第一步：先把 `CellServer` 解释成 world app 原型

- 保留目录名
- 不急着重命名
- 先在语义上按 world 进程理解

### 第二步：把 `gameLoop()` 收进 `WorldHost`

- `CellServer` 退成 app 壳
- `EntityManager/AOIManager` 逐步变成 world service

### 第三步：补 `WorldSession`

- 不再直接把“玩家在 world 里”只理解成 `PlayerEntity`
- 增加 world 侧会话层

### 第四步：补 enter/leave/transfer/recovery 接口

- 让 world 真正接到在线主链上

## 十五、近期最小可交付版本

### 最小范围

- `WorldHost` 能承接 `enter_world`
- 新增 `WorldSession`
- world 能创建 `AvatarEntity`
- world 能执行 leave
- 断线后 world 能短时保留 `Suspended` 状态

### 验证标准

至少要能验证：

1. 玩家登录后进入目标地图
2. world 创建 `WorldSession + AvatarEntity`
3. 玩家切图时能完成 transfer out / in
4. 断线后在恢复窗口内可重新回到原 world
5. 超时后能执行正式离场

## 十六、结论

Apollo world 侧当前已经有 scene、entity、AOI、tick 这些积木，但还没有真正形成完整的“玩家进入世界管线”。

这篇设计稿补的就是这块缺口：

- 用 `WorldSession` 承接 world 侧在线上下文
- 用 `WorldHost` 统一执行 enter/leave/transfer/recovery
- 用 `BaseApp(PlayerAnchor Host)` 继续做归属权威

只要这条链跑通，Apollo 的普通 MMO world runtime 就会真正闭环。之后再往 distributed space 演进，成本才可控。

## 相关阅读

- [Distributed Space 设计](./distributed-space-design.md)
- [WorldHost 设计稿](./world-host-design.md)
- [玩家在线主链设计](./player-online-flow.md)
- [BaseApp 演进设计](./base-app-evolution.md)
- [持久化进程与 DBMgr 设计](./persistence-process-and-db-manager-design.md)
- [Gateway 会话设计](./gateway-session-design.md)
- [LoginApp 收口设计](./login-app-design.md)
