---
title: 玩家在线主链设计
icon: flow
order: 9
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - 在线链路
  - PlayerAnchor
  - Gateway
---

# 玩家在线主链设计

这篇文档回答的是另外一个关键问题：

`PlayerAnchor 有了之后，玩家从登录到进世界，这条链具体怎么跑。`

如果这个问题不写清楚，Apollo 很容易继续退回到：

- login 自己管 session
- gateway 自己管在线状态
- world 自己管玩家进入

最后还是没有统一主链。

## 一、目标

Apollo 需要形成一条稳定的玩家在线主链：

```text
LoginApp
    -> 认证
BaseApp(PlayerAnchor Host)
    -> 激活玩家锚点
GatewayApp
    -> 建立公网连接
WorldHost
    -> 玩家进入世界
```

这条链里最关键的原则是：

`在线主状态只收口到 PlayerAnchor，不散落到 Login/Gateway/World 三处。`

## 二、为什么要单独写这条链

当前 Apollo 的代码更接近“几个原型 app 并排存在”：

- `LoginServer` 里有自己的 `SessionManager`
- `GatewayServer` 里有自己的连接和转发
- `CellServer` 里有自己的 world loop
- `BaseServer` 里主要是数据请求

这意味着当前还没有一个统一答案，说明：

- 谁决定玩家是否真正上线
- 谁决定玩家该进哪个 world
- 谁负责断线重连恢复
- 谁负责踢掉旧连接

这几个问题如果没有统一中心，后面一定会出现状态打架。

## 三、主链中的对象分工

### `LoginApp`

只负责：

- 账号认证
- 登录风控
- 返回登录票据或激活请求

不负责：

- 长期在线状态
- 玩家 world 归属
- 玩家断线恢复

### `BaseApp`

未来应作为 `PlayerAnchor` 宿主，负责：

- 激活玩家在线主对象
- 处理重复登录
- 绑定 `gateway/session`
- 分配 `world`
- 保存玩家在线期关键状态

这里要和持久化层区分开：

- `BaseApp(PlayerAnchor Host)` 负责在线主状态
- `DBMgr / PersistenceService` 负责持久化执行

### `GatewayApp`

只负责：

- 公网连接
- 心跳
- 协议转发
- 会话与锚点绑定

不负责：

- 决定玩家最终归属哪个世界
- 长期保存玩家在线状态

### `WorldHost`

只负责：

- 世界接入
- 实体创建与回收
- 地图实例
- world tick

不负责：

- 决定玩家是否已经完成登录
- 负责公网重连恢复

## 四、推荐主流程

建议把玩家在线主链收成 6 个阶段。

### 阶段 1：账号认证

1. 客户端请求 `LoginApp`
2. `LoginApp` 校验账号密码
3. 认证成功后，不直接在 login 里长期保留 session
4. `LoginApp` 向 `BaseApp` 发起“激活玩家锚点”请求

这一步的关键点是：

- `LoginApp` 只是入口
- 真正的在线状态从这里开始转交给锚点层

### 阶段 2：激活玩家锚点

1. `BaseApp` 按 `playerId` 查询 `PlayerAnchor`
2. 如果玩家已在线，执行重复登录策略
3. 如果玩家未在线，创建或恢复锚点
4. 加载玩家必要数据
5. 生成本次登录上下文

重复登录策略至少要明确三种之一：

- 拒绝新登录
- 踢掉旧连接，接受新登录
- 进入接管流程

Apollo 更推荐：

- `PlayerAnchor` 统一执行“踢旧接新”

这样不会让 login 或 gateway 各自处理一半。

### 阶段 3：分配接入入口

在 `Standard MMO` 模式下：

1. `BaseApp` 根据负载选择 `Gateway`
2. 生成 `SessionBinding`
3. 将 `sessionId/playerId/gatewayId` 记录进 `PlayerAnchor`
4. 返回客户端可连接的网关地址

在 `Distributed World` 模式下：

1. `BaseApp` 直接暴露 `Proxy` 接入点
2. 生成 `ClientBinding`
3. 将 `sessionId/playerId/baseAppId` 记录进 `PlayerAnchor`
4. 返回客户端可连接的 `BaseApp` 地址

这一步说明：

- 接入入口由锚点层确认
- 而不是 login 或客户端自己决定

### 阶段 4：建立公网连接

1. 客户端连入 `GatewayApp`
2. 提交登录票据或 session 凭证
3. `GatewayApp` 向 `BaseApp` / `SessionLocator` 校验归属
4. 校验成功后建立公网 session
5. `PlayerAnchor` 完成 session 绑定

这时玩家才算真正进入在线态。

### 阶段 5：分配世界并进入

1. `PlayerAnchor` 查询或生成 `WorldAssignment`
2. 选择目标 `WorldHost`
3. 向目标 world 发起 enter 请求
4. `WorldHost` 创建 `WorldSession`
5. `WorldHost` 创建或恢复世界表现实体
6. enter 完成后，锚点记录当前 world 归属

这一步之后，主链变成：

- `PlayerAnchor` 持有长期在线状态
- `Gateway` 挂公网连接
- `WorldHost` 挂世界表现

### 阶段 6：在线运行与状态维护

运行期主要由三类事件驱动：

- 来自 gateway 的连接状态变化
- 来自 world 的进入/离开/切图结果
- 来自锚点层的保存与下线流程

这里最重要的是：

- 所有关键状态变化都要先回到 `PlayerAnchor`

## 五、断线重连流程

这是判断架构是否成熟的关键链路。

### 推荐流程

1. `Gateway` 检测到连接断开
2. 通知 `PlayerAnchor` 会话解绑
3. `PlayerAnchor` 状态切到 `Disconnected`
4. 保留一段重连窗口，不立刻销毁 world 归属
5. 客户端重新登录或重连
6. `PlayerAnchor` 校验并重新绑定新 session
7. 若 world 归属仍有效，则恢复到原 world

### 为什么 world 归属不能马上删

因为很多 MMO 断线重连时希望保留：

- 原地图
- 原副本
- 原战斗上下文

如果 `Gateway` 一断就直接把 world 状态全删掉，那么重连恢复就会很差。

### 为什么 `Gateway` 不能单独做恢复

因为 `Gateway` 只知道连接，不知道：

- 玩家当前世界归属
- 切图是否进行中
- 是否还有待保存数据

所以恢复中心必须是 `PlayerAnchor`。

## 六、切图与换实例流程

切图不是 world 自己内部的纯局部行为，必须经过锚点层确认。

### 推荐流程

1. 当前 `WorldHost` 发起离开请求
2. `PlayerAnchor` 状态切到 `Transferring`
3. 更新目标 `WorldAssignment`
4. 新 `WorldHost` 执行进入
5. 成功后更新锚点当前 world 归属
6. 旧 world 表现实体回收
7. 状态恢复 `Online`

### 为什么切图要走锚点层

因为只有锚点层同时知道：

- 玩家当前公网 session
- 玩家旧 world
- 玩家新 world
- 是否允许重连恢复

如果切图只在 world 内部做，重连和跨 world 路由会很混乱。

## 七、下线流程

推荐下线主链如下：

1. `PlayerAnchor` 收到主动下线或超时下线事件
2. 通知 `Gateway` 断开公网连接
3. 通知 `WorldHost` 玩家离开
4. 标记脏数据并触发保存
5. 清理 session 绑定
6. 清理 world assignment
7. 锚点进入 `Offline`

### 关键点

- 最终的“这个玩家已经彻底离线”判断必须由锚点层完成
- 不能让 gateway 和 world 各自判断一半

## 八、推荐时序图

文档里先用文本版，避免过早引入复杂图语法。

```text
Client
  -> LoginApp: login(username, password)
LoginApp
  -> BaseApp: activate_player(playerId)
BaseApp
  -> DB: load player snapshot
BaseApp
  -> GatewayApp: reserve session binding
BaseApp
  -> Client: gateway endpoint + token
Client
  -> GatewayApp: connect(token)
GatewayApp
  -> BaseApp: validate binding
BaseApp
  -> WorldHost: enter_world(playerId, assignment)
WorldHost
  -> BaseApp: enter_ok(worldId, instanceId)
GatewayApp
  -> Client: login_complete
```

## 九、Apollo 中建议新增的接口

建议至少补下面几类接口。

### `BaseApp` 侧

- `activate_player(player_id)`
- `bind_session(player_id, session_binding)`
- `unbind_session(player_id, session_id)`
- `assign_world(player_id, world_assignment)`
- `begin_transfer(player_id, target_assignment)`
- `complete_logout(player_id)`

### `Gateway` 侧

- `validate_login_token(...)`
- `notify_disconnect(session_id, reason)`
- `notify_heartbeat_timeout(session_id)`

### `WorldHost` 侧

- `enter_world(player_id, assignment)`
- `leave_world(player_id, reason)`
- `prepare_transfer(player_id, target_assignment)`
- `restore_player(player_id, assignment)`

## 十、当前 Apollo 的改造顺序建议

不要一步到位改完所有 app。

建议顺序是：

1. 先完成 `WorldHost`
2. 再完成 `PlayerAnchor`
3. 让 `BaseApp` 接住激活、session 绑定、world 分配
4. 让 `LoginApp` 从“本地 session 管理”退回到“认证入口”
5. 让 `GatewayApp` 从“在线主状态持有者”退回到“连接入口”

## 十一、结论

Apollo 要真正变成一套可演进的 MMO 架构，关键不是 app 名字多，而是玩家在线主链要先收住。

这条链收住后，系统边界就会很清楚：

- `Login` 负责认证入口
- `BaseApp(PlayerAnchor Host)` 负责玩家在线主状态
- `Gateway` 负责公网连接
- `WorldHost` 负责世界表现

只要这条链清晰了，后面无论是普通 MMO 模式，还是再往 BigWorld 模式演进，都会稳很多。

## 相关阅读

- [LoginApp 收口设计](./login-app-design.md)
- [Gateway 会话设计](./gateway-session-design.md)
- [BaseApp 演进设计](./base-app-evolution.md)
- [持久化进程与 DBMgr 设计](./persistence-process-and-db-manager-design.md)
- [World 进入与切图设计](./world-entry-transfer-design.md)
- [PlayerAnchor 设计稿](./player-anchor-design.md)
- [WorldHost 设计稿](./world-host-design.md)
- [Apollo 重构路线图](./apollo-refactor-roadmap.md)
