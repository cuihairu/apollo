---
title: LoginApp 收口设计
icon: right-to-bracket
order: 12
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Login
  - 认证
  - Ticket
---

# LoginApp 收口设计

这篇文档回答的是主链上的最后一个关键边界问题：

`Apollo 的 LoginApp 到底应该保留什么职责，哪些职责必须收回去。`

如果这层不收口，系统很容易长期停留在：

- `Login` 保存短期 session
- `Gateway` 保存连接 session
- `BaseApp` 再保存玩家锚点

最后就会出现三套状态。

## 一、先说结论

`LoginApp` 应该是认证入口和登录票据发放层，不应该是长期在线状态宿主。

也就是说，`LoginApp` 应该负责：

- 账号认证
- 风控
- 登录票据生成
- 登录入口限流

但不应该负责：

- 长期 session 存储
- gateway 归属权威
- world assignment
- 玩家重连恢复

这些职责都应该逐步让位给 `BaseApp(PlayerAnchor Host)`。

## 二、当前代码状态

从当前代码看：

- [login_server.hpp](/C:/Users/cui/Workspaces/apollo/apps/login-app/include/login/login_server.hpp)
  - `Authenticator` 负责账号密码校验
  - `GatewayAllocator` 负责挑选网关
  - `SessionManager` 负责创建和验证 `sessionId`
- [login_server.cpp](/C:/Users/cui/Workspaces/apollo/apps/login-app/src/login_server.cpp)
  - 登录成功后直接创建 `sessionId`
  - `LoginResponse` 里直接返回 `playerId/sessionId/gateway`
  - `LoginServer` 本地线程负责清理过期 session
  - `handleGatewayAssignRequest` 仍然依赖 login 里的 `SessionManager`

这说明当前 `LoginApp` 同时承担了三类事情：

- 认证入口
- 网关分配
- 短期会话中心

其中前两类可以保留一部分，但第三类应该逐步退出。

## 三、`LoginApp` 应该负责什么

建议未来 `LoginApp` 只保留下面 4 类职责。

### 1. 账号认证

- 用户名密码校验
- 第三方登录票据校验
- 设备信息校验

### 2. 登录风控

- 密码错误次数限制
- IP 级限流
- 账号临时锁定
- 异常登录检测

### 3. 登录入口编排

- 接收客户端登录请求
- 调用 `BaseApp` 激活玩家
- 返回客户端后续接入信息

### 4. 登录票据发放

- 生成短期登录票据
- 控制票据过期时间
- 控制票据一次性使用

## 四、`LoginApp` 不应该负责什么

### 1. 不应该长期保存 session

当前 `SessionManager` 维护的 `sessionId -> playerId -> gatewayUrl` 映射，只适合原型期。

后续更合理的方向应该是：

- `Login` 只发放短期 `login ticket`
- `BaseApp` 才是长期在线状态的权威

### 2. 不应该成为 gateway 分配的最终权威

`LoginApp` 可以参与挑选候选 gateway，但最终归属应该由：

- `BaseApp(PlayerAnchor Host)`

来确认。

因为只有 `BaseApp` 同时知道：

- 玩家是否已在线
- 玩家是否在重连窗口内
- 当前应该恢复到哪个 gateway/session

### 3. 不应该知道 world assignment

`Login` 不需要知道：

- 玩家当前 world
- 当前 map/instance
- world 是否允许重连恢复

这些都超出了认证入口的边界。

## 五、推荐主流程

未来推荐把 `LoginApp` 主流程收成下面 5 步。

### 阶段 1：认证

1. 客户端请求 `LoginApp`
2. `Authenticator` 校验账号密码
3. 登录风控通过后得到 `playerId`

也就是说，密码验证就从这里开始：

- 发生在 `LoginApp`
- 属于认证入口阶段
- 早于 `PlayerAnchor` 激活
- 早于 `Gateway` 建连

### 阶段 2：激活玩家

1. `LoginApp` 不再自己创建长期 session
2. 改为向 `BaseApp` 发起 `activate_player`
3. `BaseApp` 负责：
   - 重复登录策略
   - 激活 `PlayerAnchor`
   - 加载玩家快照

### 阶段 3：获取接入结果

1. `BaseApp` 返回：
   - `playerId`
   - 候选 `gateway`
   - 登录票据或绑定信息
   - 可选的 route snapshot 摘要

2. `LoginApp` 把这些结果包装给客户端

### 阶段 4：客户端接入网关

1. 客户端拿到短期票据后连接 `Gateway`
2. `Gateway` 再向 `BaseApp` 校验票据
3. 成功后正式建立公网 session

### 阶段 5：登录入口退出主流程

这一步之后，`LoginApp` 就应该退出玩家在线主链。

后续玩家在线、切图、断线、重连，都不应该再由 `LoginApp` 长期参与。

## 六、推荐对象关系

建议后续收成下面这组对象：

```text
LoginServer
    ├── Authenticator
    ├── LoginRateLimiter
    ├── LoginRiskController
    ├── LoginTicketIssuer
    └── BaseAppActivator
```

### `Authenticator`

职责：

- 校验账号凭据
- 返回 `playerId`

### `LoginRateLimiter`

职责：

- 登录频率限制
- IP/账号级节流

### `LoginRiskController`

职责：

- 错误次数锁定
- 异常登录检测

### `LoginTicketIssuer`

职责：

- 生成短期登录票据
- 控制 TTL
- 控制一次性使用

### `BaseAppActivator`

职责：

- 向 `BaseApp` 发起玩家激活
- 获取 gateway 绑定信息
- 获取登录后接入结果

## 七、当前 `SessionManager` 应该怎么处理

当前 `LoginApp` 里的 `SessionManager` 不建议继续扩展。

### 当前问题

它现在承担了：

- session 创建
- session 校验
- gateway 归属记录

这些职责一旦继续增长，就会和 `BaseApp` 形成双中心。

### 建议演进方向

分两步走：

#### 第一步：降级成 `LoginTicketStore`

把当前 `SessionManager` 先收缩成：

- 短期票据存储
- TTL 校验
- 一次性消费控制

这时候它不再表达：

- 玩家长期在线 session

#### 第二步：完全让票据校验回到 `BaseApp`

等 `BaseApp` 的锚点层成熟后，`LoginApp` 连本地票据存储都可以进一步变薄：

- 只生成票据
- 由共享存储或 `BaseApp` 完成最终消费校验

## 八、推荐票据模型

Apollo 这里更适合的不是“login 自己保存长期 session”，而是“短期 login ticket”。

### 推荐字段

```text
LoginTicket
    ticket_id
    player_id
    issue_time
    expire_time
    gateway_hint
    nonce
```

### 推荐特性

- TTL 很短
- 一次性使用
- 只能用于接入 `Gateway`
- 被消费后立即失效

### 为什么这样更合理

因为这能让 `LoginApp` 的定位很清楚：

- 它负责让玩家拿到“进入在线系统的通行证”
- 但不负责后续整个在线生命周期

## 九、和 `BaseApp` 的边界

`BaseApp` 应该负责：

- 玩家激活
- 玩家是否已在线
- 重复登录决策
- gateway 归属
- world assignment

`LoginApp` 不应该再尝试保存这些权威状态。

所以更合理的接口方向是：

- `LoginApp -> BaseApp: activate_player`
- `LoginApp -> BaseApp: issue_login_binding`

而不是：

- `LoginApp` 自己先建好 session，再让别的进程来适配

## 十、和 `Gateway` 的边界

`Gateway` 负责：

- 票据校验
- 建立公网 session
- 转发消息

`LoginApp` 只负责：

- 发票据

所以两者关系应该是：

- `Login` 发入口凭证
- `Gateway` 验入口凭证
- 权威状态仍然在 `BaseApp`

## 十一、推荐 RPC 接口

### `LoginApp -> BaseApp`

- `activate_player`
- `prepare_login_binding`
- `query_login_result`

### `Gateway -> BaseApp`

- `validate_login_ticket`
- `consume_login_ticket`
- `bind_gateway_session`

### `LoginApp` 自身保留接口

- `login_request`
- `ping`

后续像 `gateway_assign_request` 这种接口可以逐步弱化，最终被更直接的激活结果返回替代。

## 十二、对当前代码的迁移建议

建议分 3 步推进。

### 第一步：保留认证器，收缩会话管理器

- `Authenticator` 保留
- `GatewayAllocator` 可暂时保留
- `SessionManager` 改成短期票据存储

### 第二步：把登录成功后的中心动作改成 `activate_player`

- 不再由 `LoginApp` 自己创建长期 session
- 登录成功后改为调用 `BaseApp`
- 由 `BaseApp` 返回登录接入结果

### 第三步：把网关分配从“login 本地决策”改成“base 确认”

- `LoginApp` 可以给候选 gateway 建议
- 但最终绑定结果由 `BaseApp` 确认

## 十三、近期最小可交付版本

### 最小范围

- 保留当前 `Authenticator`
- 登录成功后新增 `BaseApp` 激活调用
- `LoginApp` 不再把本地 `sessionId` 当长期权威
- `Gateway` 接入时通过票据向 `BaseApp` 校验

### 验证标准

至少要能验证：

1. 账号密码仍可正常登录
2. 登录后先激活 `PlayerAnchor`
3. 客户端拿到的是短期登录票据
4. `Gateway` 通过 `BaseApp` 校验该票据
5. `LoginApp` 不再长期持有玩家在线状态

## 十四、结论

`LoginApp` 在 Apollo 里最合适的定位不是“在线 session 中心”，而是：

- 认证入口
- 风控入口
- 登录票据发放层

只要这条边界收住，Apollo 的玩家在线主链就会彻底清楚：

- `LoginApp` 负责把玩家送进系统
- `BaseApp` 负责接住玩家主状态
- `Gateway` 负责建立公网连接
- `WorldHost` 负责让玩家进入世界

这才是后续普通 MMO 模式和更复杂架构都能复用的稳定入口设计。

## 相关阅读

- [Gateway 会话设计](./gateway-session-design.md)
- [BaseApp 演进设计](./base-app-evolution.md)
- [持久化进程与 DBMgr 设计](./persistence-process-and-db-manager-design.md)
- [玩家在线主链设计](./player-online-flow.md)
- [PlayerAnchor 设计稿](./player-anchor-design.md)
