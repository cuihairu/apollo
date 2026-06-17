---
title: RemoteEntityCall 设计
icon: arrows-turn-right
order: 21
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - RemoteEntityCall
  - Message
  - RPC
---

# RemoteEntityCall 设计

这篇文档解决的是 Apollo 后续做多进程 world、玩家锚点、跨节点实体协作时，一个绕不过去的问题：

`实体之间到底怎么做远程调用，内部消息又该长什么样。`

如果这层不补出来，Apollo 后面很容易继续停留在：

- 直接拿 `service name + channel` 发裸消息
- 消息内容靠调用方自己约定
- entity route、authority、版本信息散落在业务层

前期能跑，后期一定会乱。

## 一、先说结论

Apollo 需要在现有 `Channel` 之上，补一层明确的实体远程调用模型：

- `RemoteEntityRef`
- `RemoteMethodSchema`
- `InternalMessageEnvelope`
- `RouteResolver`
- `EntityCallEncoder`
- `RemoteEntityCall`

这层的目标不是再造一套重量级 RPC 框架，而是把 `PlayerAnchor`、`AvatarEntity`、`GhostReplica`、`Witness` 之间的调用边界收口。

## 二、KBE 里的直接证据

从本地源码看：

- [entity_call.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/lib/entitydef/entity_call.h)
  - `EntityCall` 是显式远程实体引用
  - `createRemoteMethod`
  - `newCall`
- [entitycallabstract.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/lib/entitydef/entitycallabstract.h)
  - 持有 `componentID`
  - 持有 `ENTITYCALL_TYPE`
  - 持有 `ENTITY_ID`
  - `sendCall`
  - `getChannel`
- [bundle.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/lib/network/bundle.h)
  - `Bundle` 负责按消息边界写包
  - `newMessage`
  - `finiMessage`
  - 支持多字段顺序编码

这说明 KBE 的关键点不是“能跨进程发数据”，而是：

- 远程实体引用是显式对象
- 调用方法和路由信息绑定在这个引用上
- 发包边界不是业务层临时拼字符串

## 三、Apollo 当前的缺口

从当前代码看：

- [channel.hpp](/C:/Users/cui/Workspaces/apollo/modules/net/protocol/include/apollo/net/protocol/channel.hpp)
  - `Channel` 只提供 `connect/send/receive`
  - 面向的是 endpoint，不是 entity route
- [endpoint.hpp](/C:/Users/cui/Workspaces/apollo/modules/net/protocol/include/apollo/net/protocol/endpoint.hpp)
  - `Endpoint` 更偏服务发现结果
  - 没有 entity 级路由语义
- [gateway_server.cpp](/C:/Users/cui/Workspaces/apollo/apps/gateway-app/src/gateway_server.cpp)
  - 当前转发更偏“按 app/service 路由”

这意味着 Apollo 现在缺的不是 socket，而是下面三层：

### 1. 缺显式远程实体引用

当前没有一个统一对象表达：

- 我想调哪个实体
- 调的是 anchor、avatar 还是 ghost
- 这条 route 当前版本是多少

### 2. 缺统一内部消息信封

当前消息如果继续靠调用方自己拼，会缺少：

- trace
- route version
- authority role
- method alias
- payload codec

### 3. 缺 schema 驱动的方法表

Apollo 已经在 [EntitySchema 设计](./entity-schema-design.md) 里定义了 `MethodSchema` 方向，但还没有把它落成“可执行的远程调用表”。

## 四、推荐目标

Apollo 应该把远程调用统一成下面这条链：

```text
业务对象
    -> RemoteEntityRef
    -> RouteResolver
    -> InternalMessageEnvelope
    -> EntityCallEncoder
    -> Channel
```

也就是说：

- 业务只声明“调谁、调什么方法、带什么参数”
- 路由、版本、编码、发包边界由统一层处理

## 五、核心对象设计

### `RemoteEntityRef`

这是远程实体引用对象。

建议至少包含：

```text
RemoteEntityRef
    entity_id
    entity_type
    target_domain
    target_app
    authority_role
    route_version
    shard_key
```

其中：

- `target_domain`
  - `Anchor`
  - `World`
  - `Proxy`
  - `Ghost`
- `authority_role`
  - `Authoritative`
  - `Replica`
  - `ForwardOnly`

### `RemoteMethodSchema`

建议在 `EntitySchema` 基础上补一层可执行方法定义：

```text
RemoteMethodSchema
    method_name
    method_alias
    invoke_mode
    target_domain
    arg_types
    codec
    timeout_ms
    idempotent
```

`invoke_mode` 建议至少支持：

- `OneWay`
- `RequestReply`
- `ReliableEvent`

### `InternalMessageEnvelope`

这层相当于 Apollo 版 `Bundle` 的统一消息头。

建议至少包含：

```text
InternalMessageEnvelope
    message_type
    trace_id
    request_id
    source_app
    target_app
    entity_id
    method_alias
    route_version
    authority_epoch
    flags
    payload
```

### `RouteResolver`

职责：

- 根据 `RemoteEntityRef` 找到当前真实宿主
- 校验 route version 是否过期
- 决定是否需要转发到 `PlayerAnchor`、`WorldHost`、`GhostRoute`

### `EntityCallEncoder`

职责：

- 根据 `RemoteMethodSchema` 编码参数
- 写入 envelope
- 做 alias 压缩
- 输出 `apollo::net::protocol::Message`

## 六、推荐消息分类

不要把所有内部消息都混成一种“泛 RPC”。

建议至少分成 4 类：

### 1. `EntityMethodCall`

用于：

- `PlayerAnchor` 调 `AvatarEntity`
- `AvatarEntity` 回调 `PlayerAnchor`
- `Proxy` 通知 `Witness`

### 2. `RouteControl`

用于：

- route 刷新
- authority epoch 更新
- ghost 转发切换

### 3. `LifecycleEvent`

用于：

- 实体创建
- 进入 world
- 离开 world
- migration 完成

### 4. `ReplicationCommand`

用于：

- 请求增量快照
- ack 某个复制窗口
- detail level 调整

这样 Apollo 后面在排查问题时，日志和监控都更容易收口。

## 七、推荐调用流程

### 标准 one-way 调用

1. 业务拿到 `RemoteEntityRef`
2. 通过 `MethodSchema` 找到目标方法
3. `RouteResolver` 定位目标 app 和 route version
4. `EntityCallEncoder` 编码 envelope
5. 通过 `Channel` 发送
6. 接收端按 `entity_id + method_alias` 分发

### request-reply 调用

1. 生成 `request_id`
2. 发送 `InternalMessageEnvelope`
3. 接收端执行后回包
4. 调用方按 `request_id` 收口 promise/future

### route 过期处理

1. 接收端发现 `route_version` 旧了
2. 返回 `RouteStale`
3. 调用方刷新 `RemoteEntityRef`
4. 重新发送

这一步必须显式做，不要靠业务层猜。

## 八、和对象模型的关系

这层要和 [Base Cell Proxy 对象模型](./base-cell-proxy-model.md) 对齐。

### `PlayerAnchor`

适合承接：

- 长期在线态
- world 分配
- 重连恢复

因此它更适合作为很多远程调用的稳定入口。

### `AvatarEntity`

适合承接：

- 空间内实时行为
- AOI
- 战斗表现

它的 route 更容易变化，所以更依赖 `route_version`。

### `Proxy`

适合承接：

- 向客户端发送
- session 绑定

但不应成为所有业务调用的默认入口。

## 九、Apollo 推荐接口草图

建议后续在模块层形成类似下面的接口：

```cpp
struct RemoteInvokeOptions {
    InvokeMode mode;
    std::chrono::milliseconds timeout;
    bool allowRouteRefresh;
};

class RemoteEntityCall {
public:
    explicit RemoteEntityCall(RemoteEntityRef ref);

    bool invokeOneWay(std::string_view method, const EncodedArgs& args);
    Expected<EncodedReply, InvokeError> invokeRequest(
        std::string_view method,
        const EncodedArgs& args,
        RemoteInvokeOptions options);
};
```

这里关键不是接口长什么样，而是：

- route
- schema
- envelope
- trace

必须被统一封住。

## 十、当前落地建议

Apollo 现在不需要一口气做成 KBE 全量脚本式 `EntityCall`，但至少应先补这 4 步：

### 第一步：补 envelope

先定义统一内部消息头，不再裸发字符串负载。

### 第二步：补 `RemoteEntityRef`

把 `PlayerAnchorRef`、`AvatarRef`、`ProxyRef` 的公共远程引用抽出来。

### 第三步：补 `RouteResolver`

让 route 刷新、迁移重定向、ghost forwarding 有统一入口。

### 第四步：补 schema 驱动的方法分发表

不要让方法名、参数格式继续散在业务代码里。

## 十一、和普通 MMO / BigWorld 两种模式的关系

### 普通 MMO 模式

这层仍然有价值，因为即使没有 distributed cell，也有：

- `Gateway -> BaseApp`
- `BaseApp -> WorldHost`
- `WorldHost -> Gateway`

这些内部调用。

### BigWorld 模式

这层会变成刚需，因为还会新增：

- `Cell -> Ghost`
- `Cell -> Witness`
- `Cell -> AuthorityTransfer`
- `AppMgr -> RouteControl`

## 十二、结论

Apollo 下一步如果要继续参考 KBE，不能只看进程划分，更要补“实体远程调用”这层中间件语义。

真正该收口的是：

- 远程实体引用
- 方法 schema
- 消息信封
- route 解析
- 版本与 authority 校验

只有这层立住，后面的 `PlayerAnchor`、`Distributed Space`、`Ghost/Witness` 才不会继续靠业务代码硬拼。

## 相关阅读

- [EntitySchema 设计](./entity-schema-design.md)
- [Base Cell Proxy 对象模型](./base-cell-proxy-model.md)
- [玩家在线主链设计](./player-online-flow.md)
- [Distributed Space 设计](./distributed-space-design.md)
- [Witness 与 Ghost 设计](./witness-ghost-design.md)
- [Authority Transfer 设计](./authority-transfer-design.md)
