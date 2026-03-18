---
title: 快速开始
icon: rocket
order: 3
prev: /guide/installation
next: /guide/concepts
---

# 快速开始

本教程将引导你创建第一个 Apollo 游戏服务器。

## 创建项目

### 1. 初始化项目结构

```bash
mkdir my-game-server
cd my-game-server
```

### 2. 创建 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyGameServer)

find_package(apollo-runtime REQUIRED)
find_package(apollo-core REQUIRED)
find_package(apollo-game REQUIRED)

add_executable(game-server src/main.cpp)

target_link_libraries(game-server
    apollo::runtime
    apollo::core
    apollo::game
)
```

### 3. 创建主程序

```cpp
// src/main.cpp
#include <apollo/runtime/application_host.hpp>
#include <apollo/core/log.hpp>
#include <apollo/game/world/aoi_manager.hpp>

using namespace apollo;

class GameServer : public core::Application {
public:
    void start() override {
        LOG_INFO("GameServer", "游戏服务器启动中...");

        // 初始化 AOI 系统
        initAOI();

        LOG_INFO("GameServer", "游戏服务器已启动");
    }

    void stop() override {
        LOG_INFO("GameServer", "游戏服务器关闭中...");
    }

private:
    void initAOI() {
        // 创建 1000x1000 的游戏世界，格子大小 100
        auto aoi = std::make_shared<game::AOIManager>(1000, 1000, 100);

        // 添加一些实体
        for (int i = 0; i < 100; ++i) {
            auto entity = std::make_shared<game::Entity>();
            entity->id = i;
            entity->x = rand() % 1000;
            entity->y = rand() % 1000;
            aoi->enter(entity);
        }

        LOG_INFO("GameServer", "AOI 系统初始化完成，已添加 100 个实体");
    }

    std::shared_ptr<game::AOIManager> aoi_;
};

int main() {
    runtime::ApplicationHost host;
    host.registerApplication<GameServer>();
    return host.run();
}
```

### 4. 构建运行

```bash
cmake -B build
cmake --build build
./build/game-server
```

## 添加网络通信

### 创建 TCP 服务器

```cpp
#include <apollo/net/tcp/server.hpp>

class GameServer : public core::Application {
public:
    void start() override {
        // 创建 TCP 服务器
        server_ = std::make_shared<net::tcp::Server>();
        server_->setMessageHandler([this](auto conn, auto msg) {
            onMessage(conn, msg);
        });

        // 启动服务器
        server_->start("0.0.0.0", 8888);
        LOG_INFO("GameServer", "TCP 服务器监听在 0.0.0.0:8888");
    }

private:
    void onMessage(net::tcp::ConnectionPtr conn, const std::string& msg) {
        LOG_INFO("Network", "收到消息: {}", msg);

        // 回显消息
        conn->send("Echo: " + msg);
    }

    std::shared_ptr<net::tcp::Server> server_;
};
```

## 下一步

- 了解 [核心概念](./concepts.md)
- 查看 [模块系统](./module-system.md)
- 阅读 [架构文档](/architecture/overview.md)
