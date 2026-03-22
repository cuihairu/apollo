---
title: Gateway 接入 Facade 设计
icon: right-left
order: 34
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Gateway
  - Ingress
  - Facade
---

# Gateway 接入 Facade 设计

这篇文档解决的是技术收敛阶段里最先要落地的代码边界问题：

`Gateway 对外接入到底应该怎样抽象，才能既不继续手写 transport 细节，又不把 Apollo 的会话和路由语义绑死在具体网络库上。`

如果这个边界不先定下来，后面很容易出现两种坏结果：

- `gateway-app` 持续膨胀为 socket、协议、会话、路由、认证的大杂烩
- 后续即使接入 `Drogon` 或 `Boost.Asio`，也只是把第三方库直接塞进业务主链

## 一、先说结论

`Gateway` 对外接入应该收成一层明确的 ingress facade。

合理边界应该是：

- Apollo 保留接入语义
- 第三方库承接网络与 IO 细节
- `GatewayServer` 只编排流程，不再长期持有 socket 细节

建议对象关系如下：

```text
GatewayServer
    ├── ClientIngressServer
    ├── GatewayConnectionRegistry
    ├── GatewaySessionManager
    ├── SessionAdmissionService
    ├── UpstreamRouter
    └── ClientPacketDispatcher
```

其中最关键的原则是：

- `ClientIngressServer` 负责连接和 IO
- `GatewaySessionManager` 负责会话态
- `SessionAdmissionService` 负责票据接入
- `UpstreamRouter` 负责上游路由
- `GatewayServer` 只负责把这些流程装起来

## 二、为什么要单独抽这一层

### 1. 当前 `gateway_server.cpp` 持有过多底层细节

当前代码已经直接涉及：

- socket create
- bind
- listen
- accept
- recv / send
- 心跳线程
- 路由拉取
- admission 校验

这说明当前 `GatewayServer` 同时承担了：

- transport host
- protocol ingress
- session host
- route executor

这四层职责。

### 2. 这会阻碍技术收敛

如果继续按这个结构推进：

- 即使未来换成 `Drogon`
- 或换成 `Boost.Asio`

也只是把网络实现换掉，但 `GatewayServer` 仍然会继续承载错误层次。

### 3. Apollo 需要稳定的接入语义，而不是稳定的 socket 实现

Apollo 真正需要长期稳定的是：

- pending session
- login ticket admission
- gateway session binding
- route snapshot refresh
- disconnect notification

这些都是 Gateway 的语义层，不是 socket API 层。

## 三、设计目标

这层 façade 应至少解决 5 个目标：

### 1. 屏蔽具体网络库

支持后续在不改会话主链的前提下切换：

- `Drogon`
- `Boost.Asio`
- 其他接入实现

### 2. 明确连接态与会话态分离

避免继续把：

- socket
- playerId
- route snapshot

放在一个对象里。

### 3. 明确 admission 是独立服务

票据校验不应散落在连接回调里。

### 4. 明确路由层不碰公网连接细节

`UpstreamRouter` 应只接受会话和消息，不直接碰 socket。

### 5. 支持后续重连、接管、踢号

这也是为什么接入 façade 必须先抽出来。

## 四、推荐对象模型

## 五、`ClientIngressServer`

这是对外接入 façade 的核心入口。

职责：

- 启动监听
- 接入客户端连接
- 收发原始数据帧
- 上报连接生命周期事件

它不负责：

- 玩家认证
- playerId 归属
- world route 决策
- 后端转发语义

推荐接口：

```cpp
class ClientIngressServer {
public:
    virtual ~ClientIngressServer() = default;

    virtual void start() = 0;
    virtual void stop() = 0;

    virtual void send(ConnectionId connectionId, std::span<const std::uint8_t> payload) = 0;
    virtual void close(ConnectionId connectionId, DisconnectReason reason) = 0;

    virtual void setObserver(IClientIngressObserver* observer) = 0;
};
```

### 为什么这样设计

这样设计的关键好处是：

- `GatewayServer` 不再自己 accept
- 第三方网络库只需要适配这一层
- 上层代码根本不关心 `socket fd`、`epoll`、`asio::socket`

### 可选实现

后续可提供：

- `DrogonIngressServer`
- `AsioIngressServer`
- 仅测试用 `LoopbackIngressServer`

## 六、`IClientIngressObserver`

为了让 `ClientIngressServer` 变成纯接入层，需要反向上报事件。

推荐接口：

```cpp
class IClientIngressObserver {
public:
    virtual ~IClientIngressObserver() = default;

    virtual void onConnectionOpened(ConnectionId connectionId, const ClientEndpoint& endpoint) = 0;
    virtual void onPacketReceived(ConnectionId connectionId, std::span<const std::uint8_t> payload) = 0;
    virtual void onConnectionClosed(ConnectionId connectionId, DisconnectReason reason) = 0;
    virtual void onConnectionIdle(ConnectionId connectionId) = 0;
};
```

### 为什么不用直接把逻辑塞进回调 lambda

因为后面还要继续承接：

- admission
- heartbeat policy
- reconnect takeover
- disconnect notification

如果继续全塞 lambda，结构会再次发散。

## 七、`GatewayConnectionRegistry`

这层负责的不是“玩家”，而是“连接”。

职责：

- 记录 `ConnectionId -> ClientEndpoint`
- 记录连接状态
- 记录连接建立时间
- 记录最近一次心跳或收包时间

它不负责：

- `playerId`
- `sessionId`
- `route snapshot`

推荐对象模型：

```text
GatewayConnection
    ├── connectionId
    ├── remoteEndpoint
    ├── openedAt
    ├── lastSeenAt
    └── connectionState
```

### 为什么必须单独拆出来

因为 Apollo 后续会遇到：

- 连接还在，但未认证
- 会话已认证，但连接被替换
- 老连接被踢，新连接接管

这些都要求连接和会话分层。

## 八、`GatewaySessionManager`

这层才负责 Gateway 侧的会话态。

职责：

- 创建 pending session
- 绑定 authenticated session
- 保存 `sessionId -> connectionId`
- 保存 `sessionId -> playerId`
- 保存路由快照缓存

它不负责：

- 权威 world assignment
- 玩家长期在线状态

推荐对象模型：

```text
GatewaySession
    ├── sessionId
    ├── playerId
    ├── connectionId
    ├── admissionState
    ├── routeSnapshot
    └── lastHeartbeatAt
```

### `GatewaySession != PlayerAnchor`

这是这里最重要的边界之一。

`GatewaySession` 只代表：

- 这个公网接入点当前承认了一个会话

它不代表：

- 玩家长期归属
- 跨网关稳定身份
- world 权威状态

这些都仍然应该留在 `BaseApp(PlayerAnchor Host)`。

## 九、`SessionAdmissionService`

这个对象专门处理接入校验。

职责：

- 校验 `loginTicket`
- 校验 `sessionId`
- 校验 `playerId`
- 校验 gateway 接入是否允许
- 获取初始 route snapshot

推荐接口：

```cpp
struct AdmissionResult {
    bool accepted{false};
    SessionId sessionId{0};
    PlayerId playerId{0};
    RouteSnapshot routeSnapshot;
    std::string rejectReason;
};

class SessionAdmissionService {
public:
    virtual ~SessionAdmissionService() = default;

    virtual AdmissionResult admit(const AdmissionRequest& request) = 0;
};
```

### 为什么要单独成层

因为 admission 本质上是一段业务流程编排：

- `Gateway`
  - 收客户端接入凭证
- `LoginApp`
  - 校验票据是否有效或可一次性消费
- `BaseApp`
  - 确认该玩家在线主状态是否允许绑定此 gateway

这不是连接层自己应该处理的事情。

## 十、`ClientPacketDispatcher`

这个对象负责 ingress 的客户端消息分派。

职责：

- 判断是否为认证前消息
- 判断是否需要 heartbeat 处理
- 判断是否需要走 world/base/chat 上游
- 做协议层解包和分类

它不负责：

- 底层发送
- 路由权威决策

推荐分派结果：

```text
ClientPacket
    ├── bootstrap / admission
    ├── heartbeat
    ├── world message
    ├── base message
    ├── chat message
    └── invalid message
```

### 为什么要单独拆

如果不拆，后面所有客户端消息逻辑都会继续堆进 `GatewayServer::handleClientMessage()`。

## 十一、`UpstreamRouter`

这一层负责“往后端发”，不负责“这个玩家长期应该去哪”。

职责：

- 根据 `RouteSnapshot` 把 world 消息转发到目标 world host
- 把 base/chat 等消息转发到对应后端
- 按需要刷新 route snapshot

它不负责：

- 生成权威 world assignment
- 长期保存玩家在线主状态

### 路由权威边界

这里必须明确：

- `UpstreamRouter` 可以缓存 route snapshot
- 但权威 route 应来自 `BaseApp`

## 十二、推荐流程

## 十三、连接建立流程

1. `ClientIngressServer` 接到新连接
2. 生成 `ConnectionId`
3. `GatewayConnectionRegistry` 记录连接
4. 系统为该连接创建 pending session
5. 等待客户端提交 admission 包

### 关键点

此时连接已存在，但：

- 还没有 `playerId`
- 还没有 authenticated session
- 还没有可用 route snapshot

## 十四、admission 流程

1. 客户端发送 `sessionId / playerId / loginTicket`
2. `ClientPacketDispatcher` 把消息交给 `SessionAdmissionService`
3. `SessionAdmissionService` 调用 `LoginApp` / `BaseApp`
4. 返回 `AdmissionResult`
5. `GatewaySessionManager` 把 pending session 升级为 authenticated session
6. 绑定 `connectionId <-> sessionId <-> playerId`
7. 记录初始 `RouteSnapshot`

## 十五、正常转发流程

1. `ClientPacketDispatcher` 收到业务消息
2. 校验连接是否已完成 admission
3. 查当前 `GatewaySession`
4. world 消息交给 `UpstreamRouter`
5. `UpstreamRouter` 按当前 route snapshot 转发
6. 如果 route 过期，向 `BaseApp` 刷新

## 十六、断线流程

1. `ClientIngressServer` 发现连接关闭
2. `GatewayConnectionRegistry` 标记连接断开
3. `GatewaySessionManager` 标记 session 失去连接
4. 触发断线通知
5. `BaseApp` 决定是否保留重连窗口
6. 必要时通知当前 world host

### 关键点

这里 `Gateway` 只负责：

- 上报
- 释放连接
- 清理本地接入态

不负责最终裁决：

- 玩家是否离线
- world 是否立即踢出
- 是否保留恢复窗口

## 十七、和第三方库的边界

后续如果接 `Drogon` 或 `Boost.Asio`，建议只放在下面这层：

- `ClientIngressServer` 实现
- 可选的编码/解码适配
- 可选的 executor 绑定

不要直接让：

- `GatewaySession`
- `RouteSnapshot`
- `SessionAdmissionService`

变成某个第三方库 API 的附属物。

## 十八、推荐代码落点

建议新建独立模块或目录，例如：

```text
apps/gateway-app/include/gateway/ingress/
    client_ingress_server.hpp
    client_ingress_observer.hpp
    gateway_connection.hpp
    gateway_connection_registry.hpp
    session_admission_service.hpp
    client_packet_dispatcher.hpp
```

对应实现例如：

```text
apps/gateway-app/src/ingress/
    client_ingress_server_drogon.cpp
    gateway_connection_registry.cpp
    session_admission_service.cpp
    client_packet_dispatcher.cpp
```

### 为什么先放在 `gateway-app`

因为当前阶段目标是先收口主链，而不是立刻抽成全仓库公共模块。

等 Gateway 主线稳定后，再判断是否上移到：

- `modules/transport`
- `modules/runtime`

## 十九、近期重构顺序建议

建议按下面顺序推进：

1. 先把 `GatewayServer` 里的 accept / send / disconnect 细节抽到 `ClientIngressServer`
2. 把连接记录从 `SessionManager` 中拆到 `GatewayConnectionRegistry`
3. 把 admission 校验从 `GatewayServer` 中拆到 `SessionAdmissionService`
4. 把客户端消息分类从 `handleClientMessage()` 中拆到 `ClientPacketDispatcher`
5. 最后再把底层实现替换成 `Drogon` 或 `Boost.Asio`

这个顺序的关键点是：

- 先拆语义
- 再换底层

而不是反过来。

## 二十、结论

`Gateway` 的对外接入不应该继续写成“一个类里同时做 socket、会话、认证、路由”的结构。

更合理的方式是：

- 用 `ClientIngressServer` 承接网络接入
- 用 `GatewayConnectionRegistry` 承接连接态
- 用 `GatewaySessionManager` 承接 Gateway 会话态
- 用 `SessionAdmissionService` 承接票据接入
- 用 `UpstreamRouter` 承接后端转发

这样 Apollo 才能：

- 继续保留正确的接入语义
- 同时平滑接入成熟网络库
- 避免把后续主链继续绑死在历史自研 transport 上

## 相关阅读

- [Gateway 会话设计](./gateway-session-design.md)
- [LoginApp 收口设计](./login-app-design.md)
- [玩家在线主链设计](./player-online-flow.md)
- [技术收敛与替换清单](./technology-convergence-and-replacement-plan.md)
- [技术收敛实施计划](./technology-convergence-execution-plan.md)
