---
title: Network 模块
icon: network
order: 5
category:
  - 模块
tag:
  - net
  - 网络
---

# Network 模块

Network 模块提供网络通信能力。

> **实现对照（2026-10-04 对账）**：本页样例为旧稿接口——`apollo::net::tcp`
> （`net/tcp/*.hpp` 的 Server/Client）与内建 `http::Server` 类在仓库中不存在，
> 只作沿革参考，接口以 [Network API](/api/net) 与头文件 `include/apollo/net/` 为准。
> 现状真实面：`Listener`/`Connection`/`EventLoop`（TCP 监听与连接）、
> `http`（Request/Response/RequestParser/Router 原语）、`websocket::Server`、
> `RpcServer`，以及服务发现元数据 `modules/net/protocol`（`endpoint`）。

## TCP Server

TCP 服务器。

```cpp
#include <apollo/net/tcp/server.hpp>

using namespace apollo::net;

class MyServer {
public:
    void start() {
        server_ = std::make_shared<tcp::Server>();

        // 设置连接回调
        server_->onConnect([](tcp::ConnectionPtr conn) {
            LOG_INFO("Server", "新连接: {}", conn->remoteAddress());
        });

        // 设置消息回调
        server_->onMessage([](tcp::ConnectionPtr conn, const Buffer& msg) {
            LOG_INFO("Server", "收到消息: {}", msg.toString());
            conn->send("Echo: " + msg.toString());
        });

        // 设置断开回调
        server_->onDisconnect([](tcp::ConnectionPtr conn) {
            LOG_INFO("Server", "连接断开: {}", conn->remoteAddress());
        });

        // 启动服务器
        server_->start("0.0.0.0", 8888);
    }

private:
    std::shared_ptr<tcp::Server> server_;
};
```

## TCP Client

TCP 客户端。

```cpp
#include <apollo/net/tcp/client.hpp>

auto client = std::make_shared<tcp::Client>();

// 连接服务器
client->connect("127.0.0.1", 8888, [](bool success) {
    if (success) {
        LOG_INFO("Client", "连接成功");
    } else {
        LOG_ERROR("Client", "连接失败");
    }
});

// 发送消息
client->send("Hello Server");

// 接收消息
client->onMessage([](const Buffer& msg) {
    LOG_INFO("Client", "收到: {}", msg.toString());
});
```

## HTTP Server

HTTP 服务器。

```cpp
#include <apollo/net/http/server.hpp>

using namespace apollo::net;

http::Server server;

// GET 请求
server.get("/api/player", [](const http::Request& req, http::Response& res) {
    int64_t playerId = req.param<int64_t>("id");
    // 查询玩家数据...
    json data = {{"id", playerId}, {"name", "Player1"}};
    res.send(data);
});

// POST 请求
server.post("/api/player", [](const http::Request& req, http::Response& res) {
    auto body = req.body<json>();
    // 创建玩家...
    res.status(201).send({{"success", true}});
});

// 启动服务器
server.start("0.0.0.0", 8080);
```

## HTTP Client

HTTP 客户端。

```cpp
#include <apollo/net/http/client.hpp>

http::Client client;

// GET 请求
auto response = client.get("http://api.example.com/player/12345");
if (response.ok()) {
    json data = response.json();
    LOG_INFO("Client", "玩家: {}", data["name"]);
}

// POST 请求
json body = {{"name", "Player1"}, {"level", 10}};
auto resp = client.post("http://api.example.com/player", body);
```

## WebSocket

WebSocket 服务器。

```cpp
#include <apollo/net/websocket/server.hpp>

using namespace apollo::net;

ws::Server server;

// 连接回调
server.onConnection([](ws::ConnectionPtr conn) {
    LOG_INFO("WebSocket", "新连接");
});

// 消息回调
server.onMessage([](ws::ConnectionPtr conn, const std::string& msg) {
    LOG_INFO("WebSocket", "收到: {}", msg);
    conn->send("Echo: " + msg);
});

// 启动服务器
server.start("0.0.0.0", 9000);
```

## RPC

RPC 框架。

```cpp
#include <apollo/net/rpc/server.hpp>
#include <apollo/net/rpc/client.hpp>

// 定义服务
class PlayerService : public rpc::Service {
public:
    rpc::Response getPlayerInfo(const rpc::Request& req) {
        int64_t playerId = req.param<int64_t>("id");
        // 查询玩家...
        return rpc::Response::ok({{"id", playerId}, {"name", "Player1"}});
    }
};

// 服务端
rpc::Server server;
server.registerService<PlayerService>();
server.start("0.0.0.0", 8888);

// 客户端
rpc::Client client;
client.connect("127.0.0.1", 8888);

auto response = client.call("PlayerService.getPlayerInfo", {{"id", 12345}});
```

## M1 内核子模块（2026-10-12 起）

net M1 内核已收敛为自研四层，以下为已交付的子模块：

| 子库 | 职责 |
|---|---|
| `apollo::net_link` | 帧定界 / 契约路由分发 / 背压水位 / 单播拓扑 / InterServerLink |
| `apollo::net_gameconnection` | GameConnection L3 门面——游戏逻辑唯一入口（send/subscribe/state/close 映射 link 层） |
| `apollo::net_linktrace` | W3C traceparent 上下文 + span 结构化行发射 + 932 信封编解码 |
| `apollo::net_linkrpc` | 契约 940/941 关联信封 + corr_id 关联 + call_async/poll/wait 桥 |
| `apollo::net_remoteentity` | RemoteEntityRef / RemoteMethodSchema / RouteResolver / RpcTrace |
| `apollo::net_contract_wire` | 契约 wire codec（按 MsgMeta 驱动的编解码） |

```cmake
target_link_libraries(my_app
    apollo::net_link
    apollo::net_gameconnection
    apollo::net_linktrace
    apollo::net_linkrpc
    apollo::net_remoteentity
    apollo::net_contract_wire
)
```

### GameConnection 门面（L3）

游戏逻辑到 link 层的唯一入口（net-abstraction.md §2/§3），纯适配层：

- `send(msg_id, payload, Channel, Priority)` → link 层 SendClass 映射（kReliable/kDroppable），返回 `SendCode::kAccepted/kBackPressured`。
- `subscribe(msg_id, handler)` / `set_route_table` / `allow_domain` → Dispatcher 透传。
- `state()` → SessionState（kHandshaking/kActive/kClosed，映射 LinkState）。
- `pump(now_ms)` / `attach(transport, now_ms)` / `close(reason)` → InterServerLink 透传。
- 边界：L2 会话增量（四通道队列 / TRIMMED_LOW / Resuming / resume 回调 / 场景线程投递）留后续批；Priority 参数当前仅透传语义占位。

## 依赖

- apollo::runtime
- apollo::core
- apollo::base

## 链接

```cmake
find_package(apollo-net-core REQUIRED)
find_package(apollo-net-tcp REQUIRED)
find_package(apollo-net-http REQUIRED)

target_link_libraries(my_app
    apollo::net_core
    apollo::net_tcp
    apollo::net_http
)
```
