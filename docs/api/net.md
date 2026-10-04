---
title: Network API
icon: network
prev: /api/README.md
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

接入面协议原语（nng 底座，P1 收口前受 `apollo_protocol` 门控）：
`endpoint.hpp`、`channel.hpp`——login-app/gateway-app/base-app 的 REP/REQ
套接字封装（`apollo/protocol/socket.hpp`）即由此而来。

---

**未实现**：`apollo::net::tcp::Server/Client`（旧稿 API，从不存在）、
内建 `http::Server/Client` 类；这些旧稿接口随 P4-1 按实 API 重写清理。
