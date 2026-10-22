---
title: Network API
icon: network
prev: /api/index.md
---

# Network API

> 2026-10-04 对账：本页按 HEAD 实况重写。旧稿的 `apollo::net::tcp::Server/
> Client`（`net/tcp/*.hpp`）不存在；HTTP 面为解析与路由原语（无内建
> http::Server 类）；WebSocket 实名空间是 `websocket`（非 `ws`）。

## apollo::net::Connection（TCP 连接抽象）

```cpp
namespace apollo::net {
class Connection {
public:
    virtual ~Connection() = default;
    bool isConnected() const;
    ConnectionState getState() const;
    // 同步/带缓冲发送
    int32_t send(const char* data, uint32_t length);
    bool sendAsync(const char* data, uint32_t length);
    void disconnect(const char* reason = nullptr);
    void close();
    const std::string& getRemoteAddress() const;
};
}
```

---

## apollo::net::Listener（监听接口）

```cpp
namespace apollo::net {
class Listener {
public:
    virtual ~Listener() = default;
    virtual bool start(const std::string& ip, uint16_t port) = 0;  // 空 ip = INADDR_ANY
    virtual bool stop() = 0;
    virtual bool isRunning() const = 0;
    virtual uint32_t getConnectionCount() const = 0;
    virtual void setMaxConnections(uint32_t maxConn) = 0;
};
}
```

---

## apollo::net::EventLoop

```cpp
namespace apollo::net {
class EventLoop {
public:
    bool init(const EventLoopConfig& config = {});
    bool run();          // 或 runInThread() 后台跑
    void stop();
    void wakeup();
    bool isRunning() const;
    void executeInLoop(Task task);
};
}
```

同文件还有 `Timer`、`HeartbeatManager`、`ReconnectManager`。

---

## apollo::net::http（HTTP 原语）

`include/apollo/net/http.h`：解析与路由原语，**没有**内建 `http::Server`
类（起 HTTP 服务 = Listener/EventLoop + RequestParser/Router 自组）。

```cpp
namespace apollo::net::http {
struct Request {
    Method method = Method::Get;
    std::string uri;
    Version version;
    Headers headers;
    std::string body;
    std::unordered_map<std::string, std::string> queryParams;
    std::unordered_map<std::string, std::string> pathParams;
    std::string remoteAddress;
    uint16_t remotePort = 0;
    std::string fullPath() const;
};

struct Response { /* 状态行/头/体字段 */ };

class RequestParser { /* 流式解析 → Request */ };
class Router { /* 路径 → Handler 匹配（含 pathParams 提取） */ };
}
```

`http/rest_client.h` 另有 REST 客户端原语。

---

## apollo::net::websocket::Server

```cpp
namespace apollo::net::websocket {
class Server {
public:
    virtual ~Server() = default;
    virtual bool start(const std::string& host, uint16_t port,
                       const Callbacks& callbacks, const Config& config = {}) = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    virtual void broadcast(const Message& message) = 0;
    virtual void broadcastText(const std::string& text) = 0;
    virtual size_t getConnectionCount() const = 0;
    virtual ServerConnection* getConnection(uint64_t connId) const = 0;
    using RouteHandler = std::function<bool(ServerConnection*, const std::string& path)>;
    virtual void setRoute(const std::string& path, RouteHandler handler) = 0;
};
}
```

---

## apollo::net::RpcServer

`include/apollo/net/rpc.h`：进程间 RPC（服务注册 + 调用），详见头文件。

---

## modules/net/protocol（apollo::net::protocol）

服务发现元数据（现役）：`endpoint.{hpp,cpp}`——`Protocol` 枚举（Ipc/Tcp/InProc/Ws/Auto）+ `Endpoint` 结构（服务名/主机/端口），发现面词汇，与传输实现解耦。
`channel.hpp` 与 `apollo/protocol/socket.hpp` 不存在（旧稿引用，随 nng Channel 退役删除，P4-4 批⑤ 9121ebc2）。

---

## net M1 内核子模块

自研四层内核（2026-10-12 收敛），API 明细见各头文件：

- `modules/net/link/`（`apollo::net_link`）——帧定界 / `Dispatcher` 契约路由 / `Watermark` 背压水位 / `InterServerLink` 单播拓扑（reliable seq 去重 + 累积 ack）。
- `modules/net/gameconnection/`（`apollo::net_gameconnection`）——`GameConnection` L3 门面（net-abstraction.md §2/§3），游戏逻辑唯一入口。
  - `enum class Channel { kReliable, kDroppable }` / `Priority { kLow, kNormal, kHigh }` / `SessionState { kHandshaking, kActive, kClosed }` / `SendCode { kAccepted, kBackPressured }`。
  - `send(u16 msg_id, payload, Channel, Priority) -> SendCode`（两个重载：`vector<u8>` / `const u8* + size`）。
  - `subscribe(u16 msg_id, Handler)` / `set_route_table(const RouteTable*)` / `allow_domain(string)`。
  - `state() -> SessionState` / `pump(now_ms) -> bool` / `attach(transport, now_ms)` / `close(reason)`。
  - 内部持 `InterServerLink`；`link()` / `stats()` 直访 link 层。
  - L2 会话增量（四通道队列 / `TRIMMED_LOW` / `Resuming` / resume 回调 / 场景线程投递）留后续批；`Priority` 当前仅语义占位。
- `modules/net/linktrace/`（`apollo::net_linktrace`）——W3C traceparent 上下文 + span 结构化行发射 + 932 internal_envelope 编解码缝。
- `modules/net/linkrpc/`（`apollo::net_linkrpc`）——契约 940/941 关联信封 + corr_id 关联 + `call_async`/`poll`/`wait` 桥。
- `modules/net/remoteentity/`（`apollo::net_remoteentity`）——`RemoteEntityRef` / `RemoteMethodSchema` / `RouteResolver` / `RpcTrace`（remote-entity-call 四件套余件，2026-10-13 落地）。
- `modules/net/contract_wire/`（`apollo::net_contract_wire`）——按生成头 MsgMeta 驱动的契约载荷编解码。

---

**未实现**：`apollo::net::tcp::Server/Client`（旧稿 API，从不存在）、
内建 `http::Server/Client` 类；这些旧稿接口随 P4-1 按实 API 重写清理。
