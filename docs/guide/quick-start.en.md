---
title: Quick Start
icon: rocket
order: 3
prev: /guide/installation
next: /guide/concepts
---

[English](quick-start.en.md) | [中文](quick-start.md)

# Quick Start

This tutorial walks you through building a minimal game server with Apollo's real APIs (consistent with the HEAD repository, reconciled against reality 2026-10-07): host frame driving + structured logging + AOI scene interest management.

> **Reconciliation baseline (2026-10-07, P4-1 batch)**: all code on this page is quoted from the repository's actual headers —
> `apollo::runtime::ServiceHost`/`IHostedService` (modules/runtime),
> `APOLLO_LOG()` (modules/core/log), `SceneAoi` (modules/game/world).
> The `find_package(apollo-*)` install exports and `net/tcp/server.hpp` referenced by the
> earlier version do not exist; the page has been rewritten to match current state; see the note at the end for the current networking surface.

## Create the project

### 1. Get the repository

```bash
git clone https://github.com/cuihairu/apollo.git
mkdir my-game-server && cd my-game-server
mkdir src
```

### 2. Create CMakeLists.txt

Apollo does not currently provide `find_package` install exports; the way to consume it is to add the repository as a subdirectory and link the module ALIAS targets directly:

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

All three ALIAS targets propagate PUBLIC includes: `game_world` brings `game_core` (EntityId), `runtime` brings `core` (logging).

### 3. Create the main program

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

Key points:

- `APOLLO_LOG()` returns the default logger (`shared_ptr<Logger>`); when not explicitly initialized it auto-initializes with default configuration — zero-config and ready to use; convenience methods `debug/info/warning/error/critical`, with automatic source tracking via `std::source_location`.
- The five pure virtual points of `IHostedService`: `service_name/start/stop/is_running` + `tick` (empty default).
- The stop reason flows back through `StopReason` (Completed / ConsoleRequested / SignalRequested / StartupFailed) and can be observed via `host.stop_reason()`.

### 4. Build and run

```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DAPOLLO_BUILD_GAME_MODULE=ON
cmake --build build
./build/game-server
```

Two mandatory parameters (missing either fails the build):

- `CMAKE_TOOLCHAIN_FILE` points to vcpkg — dependencies install automatically from the repository's `vcpkg.json` manifest;
  to reuse an existing build tree's dependencies, add `-DVCPKG_INSTALLED_DIR=<repo>/build/vcpkg_installed` to skip reinstalling.
- `APOLLO_BUILD_GAME_MODULE=ON` — the `apollo::game_world` module family is off by default; without the flag the target does not exist.

Once running you will see the host and service logs (`ApplicationHost starting` → `[GameServer] 游戏服务器已启动（AOI 100 实体）` → `ApplicationHost ready`), and Ctrl+C is closed out via `request_stop(SignalRequested)`.

## Adding network communication

> **Current-state note (re-verified 2026-10-07)**: `net/tcp/server.hpp` (`net::tcp::Server`)
> does not exist in the repository, so this section has no teaching example yet. The real current networking surface:
> - `modules/net/protocol` (`apollo::net/protocol::endpoint` — service discovery metadata; the nng Channel has been retired);
> - `modules/protocol` (`apollo/protocol/messages.hpp` + `codec.hpp` — contract messages and codec);
> - legacy `include/apollo/net/` (connection/listener/session/websocket/rpc and other headers, converging gradually with the migration batches);
> - the minimal cross-process link belongs to net M1 (after the P3-1 G-1 closing batch).

## Next steps

- Learn the [Core Concepts](./concepts.md)
- Check the [Module System](./module-system.md)
- Read the [Architecture Review & Current State](/analysis/architecture-review)
