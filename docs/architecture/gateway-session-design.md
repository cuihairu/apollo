---
title: Gateway 会话设计
icon: network-wired
order: 11
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Gateway
  - Session
  - PlayerAnchor
---

# Gateway 会话设计

这篇文档解决的是 `Gateway` 这一层的边界问题：

`Apollo 的 Gateway 到底应该持有什么，不应该持有什么。`

当前如果不把这层收住，系统会很容易变成：

- `Login` 持有一部分 session
- `Gateway` 持有一部分玩家状态
- `BaseApp` 再持有一部分在线归属

这样后面做重连、切图、踢号时一定会打架。

## 一、先说结论

`Gateway` 应该是公网连接入口和会话接入层，不应该是玩家在线主状态宿主。

也就是说：

- `GatewaySession` 很重要
- 但 `GatewaySession != PlayerAnchor`

合理边界应该是：

- `Gateway` 持有连接态
- `BaseApp(PlayerAnchor Host)` 持有在线主状态
- `WorldHost` 持有世界表现态
- `DBMgr / PersistenceService` 持有持久化执行职责

## 二、当前代码状态

从当前代码看：

- [session_manager.hpp](/C:/Users/cui/Workspaces/apollo/apps/gateway-app/include/gateway/session_manager.hpp)
  - `SessionManager` 管理 `ClientConnection`
  - `ClientConnection` 里同时持有 `sessionId`
  - `playerId`
  - `assignedCellApp`
- [gateway_server.hpp](/C:/Users/cui/Workspaces/apollo/apps/gateway-app/include/gateway/gateway_server.hpp)
  - `GatewayServer` 持有 `SessionManager`
  - `MessageRouter` 负责转发给 `base/chat/cell`
- [gateway_server.cpp](/C:/Users/cui/Workspaces/apollo/apps/gateway-app/src/gateway_server.cpp)
  - 心跳超时由 `Gateway` 自己检查
  - 断线时通知 `CellApp` 和 `BaseApp`
  - `forwardToCellApp` 仍然有“按 cell 直接路由”的原型思路

这说明当前 `gateway-app` 已经有两种语义混在一起：

- 正常的连接入口语义
- 一部分玩家归属与 world 路由语义

后者需要逐步收回到 `BaseApp`。

## 三、`Gateway` 应该负责什么

建议 `Gateway` 只负责下面 5 类事情。

### 1. 公网连接

- accept client socket
- 建立连接
- 读写消息
- 关闭连接

### 2. 协议编解码

- 登录票据校验请求
- 客户端消息解包
- 下行消息封包

### 3. 会话状态

- 会话创建
- 心跳更新时间
- 连接超时检测
- 断开原因记录

### 4. 上下行转发

- 客户端到后端
- 后端到客户端

### 5. 连接事件通知

- 登录成功绑定
- 主动断开
- 超时断开
- 重连替换

## 四、`Gateway` 不应该负责什么

### 1. 不应该成为玩家权威归属中心

`Gateway` 不应该独自决定：

- 这个玩家是否真的已经上线
- 这个玩家当前属于哪个 world
- 这个玩家断线后是否保留 world 归属

### 2. 不应该直接决定 world 分配

当前 `ClientConnection` 里有 `assignedCellApp`，这更像原型期做法。

后续更合理的方式应该是：

- `Gateway` 只保存当前已绑定的路由快照
- 真正权威的 world assignment 在 `PlayerAnchor`

### 3. 不应该持有玩家长期业务状态

例如：

- 背包
- 属性
- 切图状态机
- 长期在线恢复状态

这些都不应该进 `GatewaySession`。

## 五、推荐对象关系

建议后续收成下面这组对象关系：

```text
GatewayServer
    ├── GatewayConnectionManager
    ├── GatewaySessionManager
    ├── SessionAuthenticator
    ├── UpstreamRouter
    └── DisconnectNotifier
```

### `GatewayConnectionManager`

职责：

- socket 生命周期
- 连接接入
- 读写缓冲

### `GatewaySessionManager`

职责：

- 会话创建和回收
- 心跳状态
- 将连接和 `sessionId` 绑定

### `SessionAuthenticator`

职责：

- 向 `BaseApp` 校验登录票据
- 校验 `sessionId -> playerId` 绑定是否合法

### `UpstreamRouter`

职责：

- 按照 `BaseApp` 下发的路由快照转发消息
- 不直接掌握长期归属权威

### `DisconnectNotifier`

职责：

- 把断线事件上报给 `BaseApp`
- 必要时通知当前 world

## 六、当前 `SessionManager` 应该怎么演进

当前 `SessionManager` 是可以保留的，但要降语义。

### 当前结构的问题

`ClientConnection` 现在混了三类信息：

- 连接信息
- 玩家信息
- world 路由信息

这会导致 `Gateway` 越写越像在线中心。

### 更合理的拆法

建议把当前 `ClientConnection` 的内容拆成：

```text
GatewayConnection
    └── socket / ip / port / heartbeat

GatewaySession
    └── sessionId / playerId / auth state

RouteSnapshot
    └── worldId / route target / version
```

这样职责更清楚：

- 连接态归连接层
- 会话态归会话层
- 路由快照归转发层

## 七、推荐登录接入流程

### 推荐流程

1. 客户端连入 `Gateway`
2. 提交登录票据或会话凭证
3. `Gateway` 调 `BaseApp` 校验会话归属
4. `BaseApp` 返回：
   - `playerId`
   - 会话是否合法
   - 当前 route snapshot
5. `Gateway` 创建 `GatewaySession`
6. `Gateway` 把连接和会话绑定
7. 后续所有上行消息按路由快照转发

### 关键点

这里 `Gateway` 不是“批准上线”的权威。

批准动作来自：

- `BaseApp(PlayerAnchor Host)`

## 八、推荐断线流程

### 主动断开或超时断开

1. `Gateway` 检测到连接关闭
2. 标记 `GatewaySession` 进入断开态
3. 上报 `BaseApp`
4. `BaseApp` 决定是否保留重连窗口
5. 如需要，通知当前 `WorldHost`
6. `Gateway` 释放本地连接资源

### 为什么 `Gateway` 只上报不裁决

因为 `Gateway` 不掌握：

- 当前玩家是否处于切图中
- 当前 world 是否允许短时重连恢复
- 当前是否有待保存状态

这些判断必须回到 `PlayerAnchor`。

## 九、推荐重连流程

### 推荐流程

1. 客户端重新连入 `Gateway`
2. 提交旧会话凭证或重连票据
3. `Gateway` 调 `BaseApp` 校验
4. `BaseApp` 判断该玩家是否仍处于可恢复窗口
5. 若可恢复，则绑定新连接
6. 更新 `GatewaySession`
7. 使用最新 route snapshot 恢复后续消息流

### 关键点

- 重连的决定权不在 `Gateway`
- `Gateway` 只是执行绑定替换

## 十、推荐路由策略

当前 `MessageRouter` 还是更像：

- 直接找一个 `CellApp`
- 直接把消息丢过去

这在原型期可以工作，但后续要改成“路由快照”模式。

### 推荐方向

`Gateway` 保存的是短期 `RouteSnapshot`，而不是长期归属权威。

例如：

```text
RouteSnapshot
    player_id
    world_id
    target_service
    target_instance
    route_version
```

### 为什么要有 `route_version`

因为玩家切图或迁移时：

- 旧 route 可能已经失效

有版本号后，`Gateway` 就可以在发现版本过旧时重新向 `BaseApp` 拉取路由。

## 十一、推荐 RPC 接口

建议后续让 `Gateway` 重点依赖下面几类接口。

### `Gateway -> BaseApp`

- `validate_gateway_session`
- `bind_gateway_session`
- `unbind_gateway_session`
- `refresh_route_snapshot`
- `notify_disconnect`

### `BaseApp -> Gateway`

- `kick_session`
- `replace_session`
- `push_route_snapshot`

### `WorldHost -> Gateway`

- 不建议让 `WorldHost` 直接主导 gateway
- 更合理的是经由 `BaseApp` 协调

## 十二、对当前代码的迁移建议

建议分 3 步做。

### 第一步：把 `Gateway` 从“玩家归属持有者”降为“连接入口”

- 保留 `SessionManager`
- 去掉它对长期 world 归属的权威语义
- `assignedCellApp` 逐步替换成 `route snapshot`

### 第二步：让所有登录校验都回到 `BaseApp`

- `Gateway` 不再只看本地 session
- 所有登录、重连、替换连接都走 `BaseApp`

### 第三步：让 world 路由从“直连 cell”变成“按 assignment 路由”

- `forwardToCellApp` 后续应演进成更通用的 `forwardToWorld`
- 世界路由目标由 `BaseApp` 下发

## 十三、近期最小可交付版本

### 最小范围

- `Gateway` 保留现有连接管理
- 新增和 `BaseApp` 的会话校验接口
- 断线后以上报 `BaseApp` 为准
- 不再把 `assignedCellApp` 当作长期权威状态

### 验证标准

至少要能验证：

1. 登录后 `Gateway` 通过 `BaseApp` 完成会话绑定
2. `Gateway` 能拿到 route snapshot
3. 断线后 `BaseApp` 保留短期恢复窗口
4. 重连后 `Gateway` 能替换连接并继续转发

## 十四、结论

`Gateway` 在 Apollo 里最合适的定位是：

- 公网连接入口
- 会话接入层
- 路由执行器

但不是：

- 玩家在线主状态中心
- world assignment 权威

只要这条边界收住，Apollo 的在线链路就会清楚很多：

- `Login` 负责认证入口
- `BaseApp` 负责玩家主状态
- `Gateway` 负责连接和转发
- `WorldHost` 负责世界运行

这也是后续继续往更完整的 MMO 架构演进时，最稳的一种拆法。

## 相关阅读

- [LoginApp 收口设计](./login-app-design.md)
- [BaseApp 演进设计](./base-app-evolution.md)
- [持久化进程与 DBMgr 设计](./persistence-process-and-db-manager-design.md)
- [PlayerAnchor 设计稿](./player-anchor-design.md)
- [玩家在线主链设计](./player-online-flow.md)
- [WorldHost 设计稿](./world-host-design.md)
