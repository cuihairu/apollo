---
title: BaseApp 演进设计
icon: server
order: 10
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - BaseApp
  - PlayerAnchor
  - 演进设计
---

# BaseApp 演进设计

这篇文档回答的是一个更落地的问题：

`Apollo 当前的 apps/base-app，接下来到底怎么改，才能逐步演进成真正的 PlayerAnchor Host。`

前面的文档已经明确了两件事：

- 当前 `apps/base-app` 的实现偏数据服务原型
- 未来 `BaseApp` 的目标语义应该是玩家锚点宿主

这篇文档不再重复讲概念，而是直接收敛成改造方案。

如果对 `BaseApp` 这个词在 Apollo 当前仓库里的错位还有疑问，先看：

- [进程语义重定义](./process-semantics-redefinition.md)

## 一、当前 `base-app` 的真实状态

从当前代码看：

- [base_server.hpp](/C:/Users/cui/Workspaces/apollo/apps/base-app/include/base/base_server.hpp)
  - `BaseServer` 只暴露 `handleDbLoadRequest`
  - `handleDbSaveRequest`
  - `handleDbQueryRequest`
- [base_server.cpp](/C:/Users/cui/Workspaces/apollo/apps/base-app/src/base_server.cpp)
  - 启动时初始化 `DatabaseService`
  - 启动 `SaveQueue`
  - RPC 分发只处理 DB 相关请求
  - 自动保存线程当前也还是围绕数据服务原型
- [database_service.hpp](/C:/Users/cui/Workspaces/apollo/apps/base-app/include/base/database_service.hpp)
  - 当前核心数据结构还是 `PlayerData`
  - 提供 `loadPlayer`、`savePlayer`、`queryPlayer`

这说明现在的 `base-app` 更接近：

- 一个玩家数据访问服务
- 一个带缓存和保存队列的 DB proxy 原型

它还不是：

- 玩家在线主状态宿主
- 登录归属协调器
- session/world assignment 中心

所以当前最合理的判断是：

`base-app 现在已经有未来 BaseApp 的一部分底子，但主体还没有切到玩家锚点语义。`

## 二、目标状态

未来的 `BaseApp` 应该成为 Apollo 在线系统里的中心进程之一。

建议它承接的核心职责是：

- `PlayerAnchor` 生命周期管理
- 登录激活
- 重复登录处理
- gateway/session 绑定
- world assignment
- 下线与重连恢复
- 脏数据收口与异步保存

其中最关键的一句话是：

`BaseApp 的中心对象不再是 PlayerData，而是 PlayerAnchor。`

## 三、哪些职责应该留在 `BaseApp`

### 应该保留

- 玩家在线主状态
- 玩家归属路由
- 玩家脏数据保存调度
- 登录后加载玩家快照
- 下线前触发保存

### 不应该继续扩大

- 纯公网连接处理
- world 内表现实体运行
- AOI
- 战斗逻辑主循环

所以 `BaseApp` 的合理定位是：

- 不是 world
- 不是 gateway
- 也不只是 DB

它是：

- `PlayerAnchor Host`

## 四、推荐内部对象结构

建议后续把 `BaseApp` 内部关系收成下面这组对象：

```text
BaseServer
    ├── AnchorManager
    ├── SessionLocator
    ├── WorldAssignmentService
    ├── PlayerRepository
    ├── SaveCoordinator
    └── DuplicateLoginPolicy
```

### `BaseServer`

职责：

- app 启动与停止
- RPC 接口装配
- 承接 `PlayerAnchor` 相关请求入口

### `AnchorManager`

职责：

- 创建和查找 `PlayerAnchor`
- 控制在线状态生命周期
- 管理重复登录和重连恢复

### `SessionLocator`

职责：

- 按 `playerId` 查 session
- 按 `sessionId` 查 player
- 给 `Gateway`、`Login`、`World` 提供统一查询

### `WorldAssignmentService`

职责：

- 给玩家分配目标 `world`
- 记录当前 map/instance/space
- 管理切图和跨实例迁移

### `PlayerRepository`

职责：

- 包装当前 `DatabaseService`
- 对外不再直接暴露“原始 DB 请求中心”
- 而是给 `PlayerAnchor` 提供快照加载与保存

### `SaveCoordinator`

职责：

- 接管当前 `SaveQueue`
- 承接脏数据合并
- 管理异步保存节奏

## 五、推荐分层落点

为了避免继续把所有逻辑塞进 `apps/base-app`，建议把未来代码拆成两层。

### app 壳层

保留：

- `apps/base-app/include/base/base_server.hpp`
- `apps/base-app/src/base_server.cpp`

这层只负责：

- 启动
- 装配
- 请求分发

### 领域层

建议新增：

- `modules/game/session/include/apollo/game/session/player_anchor.hpp`
- `modules/game/session/include/apollo/game/session/anchor_manager.hpp`
- `modules/game/session/include/apollo/game/session/session_locator.hpp`
- `modules/game/session/include/apollo/game/session/world_assignment.hpp`
- `modules/game/session/include/apollo/game/session/duplicate_login_policy.hpp`
- `modules/game/session/include/apollo/game/session/save_coordinator.hpp`

这样 `base-app` 本身就不会继续膨胀成无法维护的大杂烩。

## 六、当前 `PlayerData` 应该怎么处理

当前 `PlayerData` 不能直接删，因为它是现有数据访问的载体。

但它的定位要降级。

### 当前阶段的正确定位

`PlayerData` 更适合变成：

- 数据快照
- 持久化 DTO

而不是：

- 在线主状态对象

### 建议关系

```text
PlayerAnchor
    └── 持有 online state / session / world assignment / dirty flags

PlayerData
    └── 持久化快照
```

这一步非常关键。

如果后面继续让 `PlayerData` 代表玩家一切状态，那么 `BaseApp` 还是会滑回“数据服务中心”。

## 七、推荐迁移阶段

建议分 4 个阶段做，而不是一次性翻新。

### 阶段 1：先把数据服务包成仓储层

这一阶段先不改外部大语义，先做内部整理。

#### 要做什么

- 保留 `DatabaseService`
- 新增 `PlayerRepository`
- 把 `loadPlayer/savePlayer/queryPlayer` 包到 repository
- `BaseServer` 不再直接围绕 `PlayerData` 编排全部流程

#### 这一阶段的价值

- 先把“数据访问”从“在线流程”里隔出来
- 给后面接 `PlayerAnchor` 留接口

### 阶段 2：接入 `PlayerAnchor`

这是语义真正转向的起点。

#### 要做什么

- 新增 `PlayerAnchor`
- 新增 `AnchorManager`
- 新增 `SessionLocator`
- `BaseServer` 新增激活玩家、绑定 session、解绑 session、查询归属接口

#### 这一阶段结束后应该达到

- `BaseApp` 已经不只是 DB 请求入口
- `PlayerAnchor` 开始成为核心对象

### 阶段 3：接入 `WorldAssignment`

这一步把 `BaseApp` 真正接到在线主链上。

#### 要做什么

- 新增 `WorldAssignmentService`
- 玩家登录后由 `BaseApp` 分配 `world`
- world enter/leave/transfer 结果回写锚点

#### 这一阶段结束后应该达到

- `BaseApp` 能决定玩家该去哪个 world
- world 不再单独保存玩家长期归属

### 阶段 4：把保存系统升级成锚点驱动

#### 要做什么

- 新增 `SaveCoordinator`
- 让 `PlayerAnchor` 标记 dirty
- 定时保存从“扫描数据缓存”转成“扫描脏锚点”

#### 这一阶段结束后应该达到

- 保存系统围绕玩家在线主状态工作
- `BaseApp` 完成从数据服务原型到玩家锚点宿主的语义切换

## 八、兼容期怎么过渡

这里最容易出问题的是：

- 新旧接口并存期间，谁才是权威状态

答案应该很明确：

`兼容期内，玩家在线归属以 PlayerAnchor 为权威，PlayerData 只作为持久化快照。`

### 兼容策略

建议在一段时间内同时保留两类接口：

- 旧的 `DB_LOAD_REQUEST / DB_SAVE_REQUEST / DB_QUERY_REQUEST`
- 新的 `ACTIVATE_PLAYER / BIND_SESSION / ASSIGN_WORLD / LOGOUT_PLAYER`

但内部实现要逐步改成：

- 新接口直接走 `PlayerAnchor`
- 旧接口尽量转到 `PlayerRepository`

这样做可以保证：

- 现有 app 不会立刻全部重写
- 新语义已经开始收口

## 九、和 `LoginApp` 的边界

`LoginApp` 未来应该逐步退回到：

- 账号认证
- 风控
- 登录票据发放

而不再负责：

- 长期 session 存储
- gateway 归属权威
- world assignment

这些都应该让位给 `BaseApp`。

## 十、和 `GatewayApp` 的边界

`GatewayApp` 应该负责：

- 公网连接
- token/session 校验
- 断开通知

但不应该自己决定：

- 玩家是不是已经真正上线
- 玩家切图后该去哪
- 玩家断线后是否保留 world 归属

这些都应该回到 `BaseApp`。

## 十一、和 `WorldHost` 的边界

`WorldHost` 负责：

- world runtime
- 实体进入离开
- 实例运行

但不应该成为：

- 玩家长期在线主状态宿主

所以更合理的关系是：

- `BaseApp` 持有 `PlayerAnchor`
- `WorldHost` 只持有 `WorldSession` 和世界表现实体

## 十二、推荐接口方向

下面是推荐给 `BaseServer` 增加的接口方向。

```cpp
class BaseServer {
public:
    ActivatePlayerResponse handleActivatePlayer(const ActivatePlayerRequest&);
    BindSessionResponse handleBindSession(const BindSessionRequest&);
    UnbindSessionResponse handleUnbindSession(const UnbindSessionRequest&);
    AssignWorldResponse handleAssignWorld(const AssignWorldRequest&);
    LogoutPlayerResponse handleLogoutPlayer(const LogoutPlayerRequest&);
};
```

配套的内部调用方向：

```cpp
auto anchor = anchorManager_->activate(playerId);
auto snapshot = playerRepository_->load(playerId);
anchor->hydrate(snapshot);
anchor->bindSession(binding);
anchor->assignWorld(assignment);
saveCoordinator_->markDirty(anchor);
```

## 十三、近期最小可交付版本

为了避免改造面过大，建议先做一个最小版本。

### 最小范围

- `BaseApp` 新增 `AnchorManager`
- 能激活 `PlayerAnchor`
- 能绑定 `session`
- 能记录当前 `world assignment`
- 保存仍然复用当前 `DatabaseService + SaveQueue`

### 先不要做

- 复杂跨区目录
- 多副本大规模迁移策略
- 分布式 `BaseAppMgr`

### 验证标准

最少要能验证下面这条链：

1. 登录成功
2. `BaseApp` 激活锚点
3. `Gateway` 绑定 session
4. `BaseApp` 分配 world
5. 玩家进入 `WorldHost`
6. 断线后锚点保持短暂可恢复状态

只要这条链跑通，`BaseApp` 的语义就算真正开始转了。

## 十四、结论

当前 `apps/base-app` 不需要先改名，先改语义。

最正确的路径不是把它继续堆成更大的数据服务，而是：

1. 保留当前数据访问底座
2. 引入 `PlayerAnchor`
3. 让 `BaseApp` 接管玩家在线主状态
4. 让数据保存降级为锚点层的支撑能力

这样 Apollo 才会真正形成：

- `Login` 做认证入口
- `BaseApp` 做玩家锚点宿主
- `Gateway` 做公网连接入口
- `WorldHost` 做世界运行时

这才是一条能继续往 KBE/BigWorld 方向演进，同时又能服务普通 MMO 模式的稳定路径。

## 相关阅读

- [Base Cell Proxy 对象模型](./base-cell-proxy-model.md)
- [PlayerAnchor 设计稿](./player-anchor-design.md)
- [玩家在线主链设计](./player-online-flow.md)
- [WorldHost 设计稿](./world-host-design.md)
- [Apollo 重构路线图](./apollo-refactor-roadmap.md)
