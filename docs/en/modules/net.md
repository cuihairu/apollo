---
title: Network Module
icon: network
order: 5
category:
  - Modules
tag:
  - net
  - network
---

# Network Module

The Network module provides network communication.

> **Implementation reconciliation (2026-10-04)**: the samples on this page are old-draft
> interfaces — `apollo::net::tcp` (the Server/Client of `net/tcp/*.hpp`) and the built-in
> `http::Server` class do not exist in the repository; they are kept only as historical
> reference. The authoritative interfaces are the [Network API](/en/api/net) page and the
> headers under `include/apollo/net/`. The real current surface:
> `Listener`/`Connection`/`EventLoop` (TCP listening and connections), `http`
> (Request/Response/RequestParser/Router primitives), `websocket::Server`, `RpcServer`,
> plus the service-discovery metadata `modules/net/protocol` (`endpoint`).

## TCP Server

TCP server.

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

TCP client.

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

HTTP server.

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

HTTP client.

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

WebSocket server.

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

RPC framework.

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

## M1 Kernel Sub-modules (since 2026-10-12)

The net M1 kernel has converged onto the self-built four layers; these are the delivered
sub-modules:

| Sub-library | Responsibility |
|---|---|
| `apollo::net_link` | Frame delimiting / contract route dispatch / backpressure water levels / unicast topology / InterServerLink |
| `apollo::net_gameconnection` | GameConnection L2 session layer + L3 facade — four-channel queues / water-level trim tiers / scene-thread delivery / Resuming + the single entry for game logic |
| `apollo::net_linktrace` | W3C traceparent context + structured span line emission + 932 envelope codec |
| `apollo::net_linkrpc` | Contract 940/941 correlation envelopes + corr_id correlation + call_async/poll/wait bridge |
| `apollo::net_remoteentity` | RemoteEntityRef / RemoteMethodSchema / RouteResolver / RpcTrace |
| `apollo::net_contract_wire` | Contract wire codec (encode/decode driven by MsgMeta) |

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

### GameConnection Facade (L2 session layer + L3)

The single entry from game logic to the link layer (net-abstraction.md §2/§3/§4); the link
foundation = InterServerLink, with the client session surface added on top:

- **Four channels** (§4.1): `Channel::kMovement` (unreliable; per id only the latest is sent
  — in-queue overwrite + drop-oldest when the ring is full, never rejected) / `kAttributes`
  (reliable ordered, LOW frames trimmable) / `kEvents` (reliable ordered, full means reject
  rather than drop) / `kControl` (independent small budget, insulated from other channels'
  cut). Mapping to link SendClass: movement→kDroppable, the rest→kReliable.
- **Three return codes** (§2): `kAccepted` / `kTrimmedLow` (enqueued, but the oldest LOW
  frame on the same channel was dropped to make instance, counted in
  `session_stats().trimmed_low_dropped`) / `kBackPressured` (rejected; the caller degrades
  on its own).
- **Four water levels** (§4.2): the metric is each channel's ring byte usage / ring capacity
  (per-channel capacity in `SessionConfig`, visible via `channel_bytes(ch)`); ok = full
  service → soft = attributes may trim LOW → hard = NORM rejected (HIGH passes) → cut =
  reliable frozen (control exempt).
- **Scene-thread delivery** (§3 threading model): a `subscribe` handler does not run on the
  pump thread — pump only copies into the delivery queue (the cross-thread surface,
  self-held mutex, depth `SessionConfig::inbound_queue_depth` with drop-oldest and a counter
  on overflow); the owning thread executes it in `drain_inbound()`.
- **Resuming** (§2/§3): reconnect with the same epoch within the `resume_ttl_ms` window →
  `state()==kResuming` during the handshake and inside the resume callback →
  `on_resume(ResumeInfo{missing_seq range, resumed=true})` → kActive; TTL expired / peer
  restart (epoch changed) / first connect = a brand-new session with no callback. The
  missing range = the reliable seq interval "confirmed by the peer, but already sent by us"
  at disconnect (same-epoch link retransmission guarantees exactly-once delivery; the upper
  layer decides snapshot catch-up from the range, attribute-sync §6).
- **Session event surface**: `set_session_sink({on_active, on_resume, on_closed})` (decoupled
  from the link sink; GameConnection collects link events internally); `session_stats()`
  exposes trim/overwrite/delivery-drop/resume counters.
- `pump(now_ms)`: flushes the four channel rings into the link (on link backpressure the
  frames stay in the ring and flush again next tick) → reads the transport / enqueues to the
  delivery queue / heartbeat timing; `attach`/`close`/`set_route_table`/`allow_domain` pass
  through.
- Transitional note: each pump flushes the four channel rings into the link queue (the
  link's own water levels still serve as the transport backpressure fallback); true
  L2-on-L0 (the IO thread flushes rings directly) converges with the client transport batch.
- Tests: `tests/test_game_connection.cpp` (20 cases).

## Dependencies

- apollo::runtime
- apollo::core
- apollo::base

## Linking

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
