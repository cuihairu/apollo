---
title: Base Cell Proxy 对象模型
icon: project-diagram
order: 19
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - BaseApp
  - CellApp
  - Proxy
---

# Base Cell Proxy 对象模型

这篇文档解决的是 KBE 体系里一个最容易被误解、但又最关键的对象模型问题：

`为什么玩家不能只用一个对象来表示，而要拆成 Proxy / Base / Cell 三层。`

如果这层没有拆开，Apollo 后面不管走普通 MMO 模式还是 BigWorld 模式，都会反复撞到同一类问题：

- session 和实体耦合
- 玩家断线后逻辑状态不好保留
- world 切换时对象边界混乱
- 大世界模式里 authority 根本没法拆

## 一、先说结论

Apollo 要参考 KBE，最关键的一条对象模型原则是：

`Session/Proxy`、`PlayerAnchor/Base`、`AvatarEntity/Cell` 必须分层。`

更准确地说：

- `Proxy` 解决连接与客户端承载
- `Base` 解决玩家长期逻辑归属
- `Cell` 解决空间内实时权威表现
- `DBMgr / PersistenceService` 负责持久化执行，不属于这三层对象模型

这三层是协作关系，不是继承替代关系。

## 二、KBE 里的直接证据

从本地源码看：

- [proxy.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/server/baseapp/proxy.h)
  - `sendToClient`
  - `hasClient`
  - `giveClientTo`
  - `onGetWitness`
  - `kick`
- [entity.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/server/baseapp/entity.h)
  - `cellEntityCall`
  - `clientEntityCall`
  - `onTeleport*`
  - `onMigrationCellapp*`
  - `reqBackupCellData`
- [entity.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/server/cellapp/entity.h)
  - `realCell`
  - `ghostCell`
  - `baseEntityCall`
  - `clientEntityCall`
  - `controlledBy`
  - `teleport`
  - `changeToGhost`
  - `changeToReal`
  - `onGetWitness`

这说明在 KBE 里：

- `Proxy` 不是 `CellEntity`
- `BaseEntity` 也不是 `CellEntity`
- 三者处在不同语义层

## 三、为什么不能只用一个“Player”对象

很多项目一开始都会这么写：

- 一个 `Player`
- 里面既有 socket/session
- 又有背包/任务/邮件
- 还有位置、移动、AOI、战斗

前期简单，后期会越来越难改。

### 问题 1：连接和长期状态生命周期不同

玩家断线时：

- 连接会马上没
- 但玩家长期逻辑归属不一定要立刻没

### 问题 2：长期状态和空间表现生命周期不同

玩家切图时：

- world 里的表现实体可能销毁重建
- 但玩家长期逻辑对象应该连续

### 问题 3：大世界模式里空间权威会迁移

在 BigWorld 模式下：

- cell 内实时权威可能变更
- 但玩家长期逻辑对象不能跟着随意漂

所以必须拆层。

## 四、推荐三层模型

建议 Apollo 明确成下面这组对象：

```text
ClientConnection / GatewaySession
    -> Proxy
    -> PlayerAnchor
    -> AvatarEntity
```

### 1. `Proxy`

语义：

- 客户端代理
- 网络附着点
- 会话承载点

### 2. `PlayerAnchor`

语义：

- 玩家长期逻辑归属
- 登录后在线主状态
- world 分配与重连恢复中心

### 3. `AvatarEntity`

语义：

- 玩家在 world/cell 中的空间表现
- 位置、移动、AOI、战斗等实时状态

## 五、`Proxy` 应该负责什么

建议 Apollo 里的 `Proxy` 对应 KBE `Proxy` 的语义，至少负责：

- 绑定当前客户端连接
- 发送消息到客户端
- 记录客户端附着状态
- 支持断线、踢下线、重绑连接
- 和 `Witness` 对接

### `Proxy` 不应该负责什么

- 不应该直接持有完整背包/任务等长期状态
- 不应该成为 world 内权威实体
- 不应该单独决定玩家归属哪个 world

## 六、`PlayerAnchor` 应该负责什么

Apollo 里这一层已经在前面的文档里定义得比较清楚了。

它负责：

- 玩家长期在线状态
- gateway/session 绑定
- world assignment
- 脏数据收口
- 重连恢复

在 KBE 语义里，这一层最接近：

- `BaseEntity`

并和 `Proxy` 强关联。

## 七、`AvatarEntity` 应该负责什么

这一层对应 world/cell 中的实时实体。

它负责：

- 位置、朝向
- 移动
- AOI
- combat/skill 表现
- cell 侧实时 authority

它不应该持有：

- 全部长期账号逻辑
- 全部持久化流程

## 八、推荐对象关系

建议 Apollo 后续显式形成下面的引用关系：

```text
Proxy
    -> player_id
    -> session_id
    -> current_witness

PlayerAnchor
    -> player_id
    -> proxy_ref
    -> world_assignment
    -> avatar_ref

AvatarEntity
    -> entity_id
    -> player_id
    -> anchor_ref
    -> proxy_hint
```

### 为什么这里用 ref，而不是强塞完整对象

因为这三层经常处在不同宿主中：

- `Proxy` 更偏 gateway/base 协作
- `PlayerAnchor` 在 base
- `AvatarEntity` 在 world/cell

引用边界清楚后，迁移和恢复才容易做。

## 九、`Proxy` 和 `Witness` 的关系

这两层很容易被混。

### `Proxy`

负责：

- 我连着哪个客户端
- 我怎么把消息送给客户端

### `Witness`

负责：

- 这个客户端当前应该看见什么

所以更合理的关系是：

- `AvatarEntity` 挂 `Witness`
- `Witness` 通过 `Proxy` 发到客户端

## 十、`PlayerAnchor` 和 `AvatarEntity` 的关系

这两层也不能混成一个对象。

### `PlayerAnchor`

负责：

- 玩家长期归属
- world assignment
- online lifecycle

### `AvatarEntity`

负责：

- 进入 world 之后的实时表现

### 典型流程

1. `PlayerAnchor` 被激活
2. 分配 world
3. `WorldHost` 创建 `AvatarEntity`
4. `PlayerAnchor` 持有一个 `avatar_ref`
5. 切图后旧 `AvatarEntity` 可销毁，新 `AvatarEntity` 可重建

但：

- `PlayerAnchor` 本身保持连续

## 十一、BigWorld 模式下的额外关系

进入 BigWorld 模式后，还会进一步分裂成：

- base 侧长期逻辑
- cell 侧实时 authority
- ghost 侧投影

也就是说：

```text
Proxy
PlayerAnchor(Base)
AvatarEntity(Cell Real)
GhostReplica(Cell Ghost)
```

### 这里的关键点

- `GhostReplica` 不是第四层长期对象
- 它只是 `AvatarEntity` 在非权威节点上的投影

## 十二、Apollo 中建议新增的抽象

建议未来至少新增：

- `modules/game/session/include/apollo/game/session/player_proxy.hpp`
- `modules/game/session/include/apollo/game/session/player_anchor.hpp`
- `modules/game/world/include/apollo/game/world/avatar_entity.hpp`
- `modules/game/world/include/apollo/game/world/avatar_ref.hpp`
- `modules/game/session/include/apollo/game/session/proxy_ref.hpp`

如果 BigWorld 模式启动，再补：

- `modules/game/world/include/apollo/game/world/ghost_replica.hpp`

## 十三、当前 Apollo 的落地顺序

建议按下面顺序落：

1. 先定义 `PlayerAnchor`
2. 再定义 `Proxy`
3. 再定义 `AvatarEntity`
4. 最后再补 `GhostReplica`

### 为什么不是先补 `Proxy`

因为 Apollo 当前真正缺的是：

- 玩家长期逻辑归属

也就是 `PlayerAnchor`。

## 十四、近期最小可交付版本

### 最小范围

- `GatewaySession` 保持连接语义
- `PlayerAnchor` 保持长期在线语义
- `WorldSession + AvatarEntity` 保持 world 侧语义
- 这三层显式拆开

### 暂时不做

- 完整 KBE 风格 `Proxy` API
- `giveClientTo` 这类跨代理转移细节
- 完整 ghost 语义

### 验证标准

至少要能验证：

1. 玩家断线时 `GatewaySession` 消失
2. `PlayerAnchor` 仍可短时保留
3. `AvatarEntity` 可被销毁或恢复
4. 三层生命周期互不混淆

## 十五、结论

Apollo 如果要真正吸收 KBE 的对象模型，最关键的一点不是名字，而是把：

- `Proxy`
- `Base`
- `Cell`

三层语义拆开。

换成 Apollo 现在更合适的说法就是：

- `Proxy`
- `PlayerAnchor`
- `AvatarEntity`

这三层一旦不拆，后面的 world 切换、重连恢复、BigWorld authority 都会一直打架。

## 相关阅读

- [进程语义重定义](./process-semantics-redefinition.md)
- [PlayerAnchor 设计稿](./player-anchor-design.md)
- [持久化进程与 DBMgr 设计](./persistence-process-and-db-manager-design.md)
- [World 进入与切图设计](./world-entry-transfer-design.md)
- [Witness 与 Ghost 设计](./witness-ghost-design.md)
- [Authority Transfer 设计](./authority-transfer-design.md)
