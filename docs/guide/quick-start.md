---
title: 快速开始
icon: rocket
order: 3
prev: /guide/installation
next: /guide/concepts
---

# 快速开始

本教程将引导你创建第一个 Apollo 游戏服务器。

> **状态注记（2026-10-04 全量对账）**：本页示例为教学骨架，部分引用尚非实 API——
> 引用实 API 的完整重写随 P4-1 批次；下文已按 HEAD 实况标注：AOI 段已换成真实
> `SceneAoi` 接口，TCP 段所引 `net/tcp/server.hpp` **未实现**（真实网络面见
> `modules/net/protocol` 与 legacy `include/apollo/net/`）。

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
find_package(apollo-game-world REQUIRED)

add_executable(game-server src/main.cpp)

target_link_libraries(game-server
    apollo::runtime
    apollo::core
    apollo::game_world
)
```

### 3. 创建主程序

```cpp
// src/main.cpp
#include <apollo/runtime/application_host.hpp>
#include <apollo/core/log/log_manager.hpp>
#include <apollo/game/world/scene_aoi.hpp>

using apollo::game::core::EntityId;
using apollo::game::world::SceneAoi;

// 业务服务：实现 IHostedService（start/stop/tick 由宿主帧驱动）
class GameServer : public apollo::runtime::IHostedService {
public:
    std::string_view service_name() const override { return "game-server"; }

    bool start() override {
        APOLLO_LOG_INFO("[GameServer] 游戏服务器启动中...");

        // 1000x1000 世界、格子 100、视野半径 30（Scene 独享一个 SceneAoi）
        aoi_ = std::make_unique<SceneAoi>(1000.0f, 1000.0f, 100.0f, 30.0f);

        // 事件面：Enter / Sync / Leave（sink 缺省静默，由宿主注入）
        aoi_->set_event_sink([](const SceneAoi::Event&) { /* 下发面见 P3-2 */ });

        // 实体进入（只认 EntityId + 位置值，与实体类型解耦）
        for (int i = 1; i <= 100; ++i) {
            aoi_->enter(EntityId{static_cast<std::uint64_t>(i)},
                        {static_cast<float>(i), 0.0f, 0.0f});
        }

        APOLLO_LOG_INFO("[GameServer] 游戏服务器已启动（AOI 100 实体）");
        running_ = true;
        return true;
    }

    void stop() override {
        APOLLO_LOG_INFO("[GameServer] 游戏服务器关闭中...");
        running_ = false;
    }

    bool is_running() const override { return running_; }
    void tick() override { /* 定帧业务 */ }

private:
    std::unique_ptr<SceneAoi> aoi_;
    bool running_ = false;
};

int main() {
    apollo::runtime::ServiceHost host;
    host.add_service(std::make_shared<GameServer>());
    if (!host.start()) {
        return 1;
    }
    // 帧循环（Ctrl+C 经 signal_source 走 request_stop 收口）
    while (host.is_running()) {
        host.run_once();
    }
    host.stop();
    return 0;
}
```

### 4. 构建运行

```bash
cmake -B build
cmake --build build
./build/game-server
```

## 添加网络通信

> **未实现**（2026-10-04 对账）：本节原示例引用的 `net/tcp/server.hpp`
> （`net::tcp::Server` / `net::tcp::ConnectionPtr`）在仓库中不存在，随 P4-1
> 按实 API 重写。现状真实网络面：
> - `modules/net/protocol`（`apollo::net::protocol::endpoint/channel`，nng 底座，P1 收口前受 `apollo_protocol` 门控）；
> - legacy `include/apollo/net/`（connection/listener/session/websocket/rpc 等头，随迁移批次逐步收敛）；
> - 接入进程（login-app/gateway-app）的 REP/REQ 套接字封装在 `apollo/protocol/socket.hpp`。

## 下一步

- 了解 [核心概念](./concepts.md)
- 查看 [模块系统](./module-system.md)
- 阅读 [架构审计与现状](/analysis/architecture-review)
