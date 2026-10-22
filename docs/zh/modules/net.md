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

## M1 内核子模块

net M1 内核已收敛为自研四层，以下为已交付的子模块：

| 子库 | 职责 |
|---|---|
| `apollo::net_link` | 帧定界 / 契约路由分发 / 背压水位 / 单播拓扑 / InterServerLink |
| `apollo::net_gameconnection` | GameConnection L2 会话层 + L3 门面——四通道队列/水位 trim 分级/场景线程投递/Resuming + 游戏逻辑唯一入口 |
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

### GameConnection 门面（L2 会话层 + L3）

游戏逻辑到 link 层的唯一入口（net-abstraction.md §2/§3/§4），链路底座 = InterServerLink，其上加客户端会话面：

- **四通道**（§4.1）：`Channel::kMovement`（不可靠，同 id 只发最新——队内覆盖 + 环满丢最旧，永不拒收）/ `kAttributes`（可靠有序，LOW 可 trim）/ `kEvents`（可靠有序，满即拒不丢）/ `kControl`（独立小预算，不受其它通道 cut 牵连）。映射 link SendClass：movement→kDroppable，其余→kReliable。
- **三返回码**（§2）：`kAccepted` / `kTrimmedLow`（入队但同通道最旧 LOW 帧被丢弃腾位，`session_stats().trimmed_low_dropped` 计数）/ `kBackPressured`（拒收，调用方自行降级）。
- **四级水位**（§4.2）：判据 = 各通道环字节占用 / 环容量（`SessionConfig` 逐通道容量，`channel_bytes(ch)` 可见）；ok 全量 → soft 起 attributes 可 trim LOW → hard 起 NORM 拒收（HIGH 放行）→ cut 冻结 reliable（control 豁免）。
- **场景线程投递**（§3 线程模型）：`subscribe` 的 handler 不在 pump 线程执行——pump 只拷贝入投递队列（跨线程面，自持互斥，深度 `SessionConfig::inbound_queue_depth` 溢出丢最旧计数），owning 线程 `drain_inbound()` 执行。
- **Resuming**（§2/§3）：同 epoch 重连且 `resume_ttl_ms` 窗口内 → 握手期与 resume 回调内 `state()==kResuming` → `on_resume(ResumeInfo{missing_seq 范围, resumed=true})` → kActive；TTL 过期 / 对端重启（epoch 变）/ 首连 = 全新会话不回调。missing 范围 = 断线时「对端已确认, 本端已发出」的 reliable seq 区间（链路同 epoch 重发保证传输恰一次；上层据范围决定快照补发，attribute-sync §6）。
- **会话事件面**：`set_session_sink({on_active, on_resume, on_closed})`（与链路 sink 解耦，GameConnection 内收链路事件）；`session_stats()` 观测 trim/覆盖/投递丢弃/恢复计数。
- `pump(now_ms)`：四通道环刷入链路（遇链路背压停留环下拍再刷）→ 读传输/入投递队列/心跳计时；`attach`/`close`/`set_route_table`/`allow_domain` 透传。
- 过渡面注记：四通道环每次 pump 刷入链路队列（链路自身水位仍作传输背压兜底）；真 L2-on-L0（IO 线程直刷环）随客户端传输批收敛。
- 测试：`tests/test_game_connection.cpp`（20 用例）。

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
