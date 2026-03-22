---
title: Internal Service Client 与 Envelope 设计
icon: envelope-open-text
order: 35
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - ServiceClient
  - Envelope
  - RPC
---

# Internal Service Client 与 Envelope 设计

这篇文档回答的是技术收敛阶段的另一个关键问题：

`Apollo 内部跨进程调用，到底应该依赖什么主线，而不是继续在 socket、channel、legacy rpc 之间并存。`

如果这层不收口，后面很容易继续出现：

- `Gateway` 直接拿 `RpcClient` 发请求
- `BaseApp` 直接拿 `RepSocket` 收请求
- 其他模块继续绕过统一入口自己拼消息

最终结果就是：

- 超时策略不统一
- route 解析不统一
- tracing 和 metadata 不统一
- 上层代码无法判断“内部主线”到底是哪一条

## 一、先说结论

Apollo 内部调用应收成一条明确主线：

```text
业务调用方
    -> ServiceClient
    -> InternalMessageEnvelope
    -> RouteResolver
    -> TransportAdapter
    -> ServiceEndpoint
```

其中：

- `ServiceClient` 负责调用语义
- `InternalMessageEnvelope` 负责统一消息头
- `RouteResolver` 负责找目标节点
- `TransportAdapter` 负责真正发出去

Apollo 不应该再让业务代码直接依赖：

- `ReqSocket`
- `RepSocket`
- `Channel`
- `rpc_legacy`

这些都应该被收进统一主线后面。

## 二、为什么要这样设计

### 1. 当前内部调用主线并不唯一

从当前仓库看，至少同时存在：

- `modules/protocol/include/apollo/protocol/socket.hpp`
- `modules/protocol/src/socket.cpp`
- `modules/net/rpc/src/rpc_legacy.cpp`
- `modules/net/protocol/include/apollo/net/protocol/channel.hpp`
- `apps/gateway-app/src/gateway_server.cpp`
- `apps/base-app/src/base_server.cpp`

这说明 Apollo 现在能“发消息”，但还没有统一定义：

- 内部调用边界
- 统一 metadata
- 统一 timeout / retry / reconnect 策略

### 2. Apollo 需要的是服务语义，不是 socket 语义

上层真正关心的是：

- 调哪个服务
- 调哪个动作
- 是否需要 response
- 当前 route version 是多少
- 当前 trace 是什么

这些都不应该让业务层自己拼。

### 3. 未来还要继续承接更多主链能力

例如：

- `PlayerActivateRequest`
- `PlayerBindSessionRequest`
- `PlayerAssignWorldRequest`
- `PlayerResolveRouteRequest`
- world transfer
- route refresh

这些都要求内部调用边界尽快固定。

## 三、设计目标

这层设计至少要满足 6 个目标：

### 1. 统一 request / response 语义

### 2. 统一 one-way event 语义

### 3. 统一 timeout / retry / reconnect 策略

### 4. 统一 route resolve 与 endpoint 选择

### 5. 统一 tracing / metadata / versioning

### 6. 屏蔽具体 transport 实现

## 四、推荐对象关系

```text
ServiceClient
    ├── RouteResolver
    ├── EnvelopeEncoder
    ├── TransportAdapter
    └── RetryPolicy

ServiceHost
    ├── EnvelopeDecoder
    ├── RequestDispatcher
    ├── HandlerRegistry
    └── ResponseWriter
```

这个结构最关键的意义是：

- 调用方看的是 `ServiceClient`
- 宿主侧看的是 `ServiceHost`
- transport 只是底层适配，不是业务入口

## 五、`ServiceClient`

这是 Apollo 内部调用主入口。

职责：

- 发起 request / response 调用
- 发起 one-way 调用
- 统一超时、重试、重连策略
- 写入统一 envelope
- 通过 `RouteResolver` 找目标 endpoint

推荐接口：

```cpp
struct ServiceRequest {
    std::string serviceName;
    std::string methodName;
    std::string routeKey;
    std::span<const std::uint8_t> payload;
    std::chrono::milliseconds timeout;
};

struct ServiceResponse {
    bool ok{false};
    std::vector<std::uint8_t> payload;
    std::string errorCode;
    std::string errorMessage;
};

class ServiceClient {
public:
    virtual ~ServiceClient() = default;

    virtual ServiceResponse call(const ServiceRequest& request) = 0;
    virtual void notify(const ServiceRequest& request) = 0;
};
```

### 为什么用 `ServiceClient` 而不是继续直接暴露 `RpcClient`

因为 Apollo 要保留的是服务调用语义，而不是某个现有实现类。

`RpcClient` 可以退化为一种 transport adapter 或 façade 内部实现，但不应继续成为业务主接口。

## 六、`InternalMessageEnvelope`

这是内部主线最关键的统一信封。

建议至少包含：

```text
InternalMessageEnvelope
    messageKind
    requestId
    traceId
    sourceService
    targetService
    methodName
    routeKey
    routeVersion
    timeoutMs
    flags
    payload
```

### 为什么必须有统一 envelope

否则后面每个调用方都会自己决定：

- trace 放不放
- requestId 放不放
- route version 放不放
- timeout 信息如何传

最后就无法统一做：

- tracing
- 重试去重
- route 失效检查
- 日志审计

## 七、`RouteResolver`

这层负责从服务语义走到目标地址。

职责：

- 根据 `serviceName`
- 根据 `routeKey`
- 根据可选的 affinity / shard 信息
- 解析出当前最合适的 `ServiceEndpoint`

推荐接口：

```cpp
struct ServiceEndpoint {
    std::string serviceName;
    std::string address;
    std::uint64_t version{0};
    std::string nodeId;
};

class RouteResolver {
public:
    virtual ~RouteResolver() = default;

    virtual std::optional<ServiceEndpoint> resolve(const ServiceRequest& request) = 0;
};
```

### 为什么这层不能让调用方自己拼地址

因为后续 Apollo 会继续叠加：

- 网关路由
- world route
- app manager 分配
- affinity 路由
- topology aware route

调用方直接拼地址，后面一定失控。

## 八、`TransportAdapter`

这层只负责真正发送，不对业务暴露。

职责：

- 建连
- 发包
- 收包
- 断线重连

可选实现例如：

- `NngTransportAdapter`
- `AsioTransportAdapter`
- 未来其他实现

### 为什么 transport 应下沉

因为 transport 本身不是 Apollo 的领域能力，只是内部调用的承载介质。

Apollo 不应把上层服务调用直接写死成：

- NNG 语义
- socket 语义
- 某个 channel 类语义

## 九、宿主侧：`ServiceHost`

除了客户端，也要统一服务侧入口。

职责：

- 收到 envelope
- 解码 metadata
- 通过 `HandlerRegistry` 找 handler
- 执行业务方法
- 写回 response

推荐对象关系：

```text
ServiceHost
    ├── HandlerRegistry
    ├── EnvelopeDecoder
    ├── RequestDispatcher
    └── ResponseWriter
```

这样 `BaseApp`、`LoginApp`、`WorldHost` 都可以统一看成：

- 一组注册了内部服务 handler 的宿主

而不是：

- 每个 app 自己手搓一套 request handler

## 十、推荐消息分类

不要把所有内部消息都混成一种“泛 RPC”。

建议至少区分：

### 1. `RequestResponse`

用于：

- `PLAYER_RESOLVE_ROUTE`
- `PLAYER_ACTIVATE`
- `PLAYER_BIND_SESSION`

### 2. `OneWayNotify`

用于：

- disconnect notify
- session invalidated
- world route changed

### 3. `ControlMessage`

用于：

- heartbeat
- transport reconnect
- route invalidation

### 4. `StreamMessage`

如果后续需要批量复制或大消息流，再单独扩。

但不要一开始就让所有类型共用同一套松散接口。

## 十一、和 `RemoteEntityCall` 的关系

这层和 [RemoteEntityCall 设计](./remote-entity-call-design.md) 不是冲突关系，而是上下层关系。

更合理的理解是：

- `ServiceClient`
  - 负责服务级调用主线
- `RemoteEntityCall`
  - 建立在 `InternalMessageEnvelope` 之上
  - 负责实体级调用语义

也就是说：

- 不是所有内部调用都应该是实体调用
- 也不是所有内部调用都应该只停留在 service rpc

## 十二、推荐落点

建议短期先在现有目录语义下收口，不急着大改名。

推荐落点例如：

```text
modules/net/rpc/include/apollo/net/rpc/
    service_client.hpp
    route_resolver.hpp
    service_host.hpp
    transport_adapter.hpp

modules/net/rpc/src/
    service_client.cpp
    service_host.cpp
    route_resolver.cpp
```

统一信封可以放在：

```text
modules/protocol/include/apollo/protocol/
    internal_envelope.hpp
```

### 为什么暂时不立刻重命名成 `modules/transport`

因为当前阶段更重要的是：

- 先统一主线
- 再决定目录名

## 十三、近期重构顺序建议

建议按下面顺序推进：

1. 先定义 `InternalMessageEnvelope`
2. 定义 `ServiceClient` 与 `RouteResolver` 接口
3. 让 `Gateway/Base/Login/World` 新代码只依赖这套 façade
4. 把现有 `RpcClient` / `ReqSocket` / `Channel` 逐步退到 adapter 层
5. 冻结 `rpc_legacy`

这个顺序的关键点是：

- 先收接口
- 再换实现

## 十四、结论

Apollo 的内部调用不应该继续以“哪个 socket 好用就直接拿哪个”这种方式扩写。

更合理的方式是：

- 用 `ServiceClient` 收住调用入口
- 用 `InternalMessageEnvelope` 收住消息头
- 用 `RouteResolver` 收住地址与路由选择
- 用 `TransportAdapter` 屏蔽底层介质

这样后续无论是：

- 在线主链
- world transfer
- route refresh
- distributed world 扩展

都能建立在一条稳定、可替换、可观测的内部调用主线上。

## 相关阅读

- [技术收敛与替换清单](./technology-convergence-and-replacement-plan.md)
- [技术收敛实施计划](./technology-convergence-execution-plan.md)
- [RemoteEntityCall 设计](./remote-entity-call-design.md)
- [Domain Event 与 Message Bus 设计](./domain-event-and-message-bus-design.md)
