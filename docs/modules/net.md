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

Network 模块提供网络通信能力，支持 TCP、HTTP、WebSocket 等协议。

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
