---
title: 快速开始
icon: rocket
order: 3
prev: /guide/installation
next: /guide/concepts
---

# 快速开始

本教程引导你用 Apollo 的实 API（与 HEAD 仓库一致，2026-10-07 按实对账）搭一个最小游戏服务器：宿主帧驱动 + 结构化日志 + AOI 场景兴趣管理。

> **对账口径（2026-10-07，P4-1 批）**：本页全部代码引自仓库实接头文件——
> `apollo::runtime::ServiceHost`/`IHostedService`（modules/runtime）、
> `APOLLO_LOG()`（modules/core/log）、`SceneAoi`（modules/game/world）。
> 早期版本引用的 `find_package(apollo-*)` 安装导出与 `net/tcp/server.hpp`
> 均不存在，已按现状重写；网络面现状见文末注记。

## 创建项目

### 1. 获取仓库

```bash
git clone https://github.com/cuihairu/apollo.git
mkdir my-game-server && cd my-game-server
mkdir src
```

### 2. 创建 CMakeLists.txt

Apollo 当前不提供 `find_package` 安装导出；消费方式是把仓库作为子目录加进来，直接链接模块 ALIAS target：

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyGameServer CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 上一级目录克隆的 apollo 仓库作为子项目
add_subdirectory(../apollo apollo-build)

add_executable(game-server src/main.cpp)
target_link_libraries(game-server PRIVATE
    apollo::runtime      # 宿主/生命周期（modules/runtime）
    apollo::core         # 日志/配置/指标（modules/core）
    apollo::game_world   # 场景/AOI/副本（modules/game/world）
)
```

三个 ALIAS target 均为 PUBLIC include 传导：`game_world` 自带 `game_core`（EntityId），`runtime` 自带 `core`（日志）。

### 3. 创建主程序

```cpp
// src/main.cpp
#include <apollo/runtime/application_host.hpp>
#include <apollo/core/log/log_manager.hpp>
#include <apollo/game/world/scene_aoi.hpp>

#include <csignal>
#include <cstdint>
#include <memory>
#include <string_view>

using apollo::game::core::EntityId;
using apollo::game::world::SceneAoi;
using apollo::runtime::IHostedService;
using apollo::runtime::ServiceHost;
using apollo::runtime::StopReason;

namespace {
volatile std::sig_atomic_t g_interrupted = 0;
}

// 业务服务：实现 IHostedService（start/stop/tick 由宿主帧驱动）
class GameServer : public IHostedService {
public:
    std::string_view service_name() const override { return "game-server"; }

    bool start() override {
        APOLLO_LOG()->info("[GameServer] 游戏服务器启动中...");

        // 1000x1000 世界、格子 100、视野半径 30（Scene 独享一个 SceneAoi）
        aoi_ = std::make_unique<SceneAoi>(1000.0f, 1000.0f, 100.0f, 30.0f);

        // 事件面：Enter / Sync / Leave（sink 缺省静默，由宿主注入）
        aoi_->set_event_sink([](const SceneAoi::Event&) { /* 下发面见 P3-2 */ });

        // 实体进入（只认 EntityId + 位置值，与实体类型解耦）
        for (int i = 1; i <= 100; ++i) {
            aoi_->enter(EntityId{static_cast<std::uint64_t>(i)},
                        {static_cast<float>(i), 0.0f, 0.0f});
        }

        APOLLO_LOG()->info("[GameServer] 游戏服务器已启动（AOI 100 实体）");
        running_ = true;
        return true;
    }

    void stop() override {
        APOLLO_LOG()->info("[GameServer] 游戏服务器关闭中...");
        running_ = false;
    }

    bool is_running() const override { return running_; }
    void tick() override { /* 定帧业务 */ }

private:
    std::unique_ptr<SceneAoi> aoi_;
    bool running_ = false;
};

int main() {
    // Ctrl+C：信号处理里只置 sig_atomic_t 标志（async-signal-safe），
    // 收口走宿主 request_stop。ISignalSource 注入面保留给更复杂的信号源。
    std::signal(SIGINT, [](int) { g_interrupted = 1; });

    ServiceHost host;
    host.add_service(std::make_shared<GameServer>());
    if (!host.start()) {
        return 1;
    }
    while (host.is_running() && !g_interrupted) {
        host.run_once();
    }
    if (g_interrupted) {
        host.request_stop(StopReason::SignalRequested);
    }
    host.stop();
    return 0;
}
```

要点：

- `APOLLO_LOG()` 返回默认 logger（`shared_ptr<Logger>`），未显式初始化时自动以缺省配置初始化，零配置即用；便捷方法 `debug/info/warning/error/critical`，`std::source_location` 自动溯源。
- `IHostedService` 的五个纯虚点：`service_name/start/stop/is_running` + `tick`（缺省空实现）。
- 退出原因经 `StopReason`（Completed / ConsoleRequested / SignalRequested / StartupFailed）回流，可用 `host.stop_reason()` 观察。

### 4. 构建运行

```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DAPOLLO_BUILD_GAME_MODULE=ON
cmake --build build
./build/game-server
```

两个必带参数（缺一则构建失败）：

- `CMAKE_TOOLCHAIN_FILE` 指向 vcpkg——依赖由仓库 `vcpkg.json` manifest 自动安装；
  复用已有构建树的依赖可加 `-DVCPKG_INSTALLED_DIR=<仓库>/build/vcpkg_installed` 跳过重装。
- `APOLLO_BUILD_GAME_MODULE=ON`——`apollo::game_world` 系模块缺省关闭，不带则 target 不存在。

运行后可见宿主与服务日志（`ApplicationHost starting` → `[GameServer] 游戏服务器已启动（AOI 100 实体）` → `ApplicationHost ready`），Ctrl+C 经 `request_stop(SignalRequested)` 收口。

## 添加网络通信

> **现状注记（2026-10-07 复核）**：`net/tcp/server.hpp`（`net::tcp::Server`）
> 在仓库中不存在，本节暂无教学示例。现状真实网络面：
> - `modules/net/protocol`（`apollo::net::protocol::endpoint/channel`，nng 底座，P1 收口前受 `apollo_protocol` 门控）；
> - `modules/protocol`（`apollo/protocol/socket.hpp`——接入进程 login-app/gateway-app 的 REP/REQ 套接字封装）；
> - legacy `include/apollo/net/`（connection/listener/session/websocket/rpc 等头，随迁移批次逐步收敛）；
> - 跨进程最小链路归 net M1（P3-1 G-1 收尾批之后）。

## 下一步

- 了解 [核心概念](./concepts.md)
- 查看 [模块系统](./module-system.md)
- 阅读 [架构审计与现状](/analysis/architecture-review)
