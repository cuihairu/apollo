[English](README.md) | [中文](README.zh.md)

<p align="center">
  <img src="docs/public/apollo.png" alt="Apollo Logo" height="80"/>
</p>

# Apollo

**An instance-based multiplayer game server engine.**

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/cuihairu/apollo/blob/main/LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey.svg)](https://github.com/cuihairu/apollo)
[![Language](https://img.shields.io/badge/language-C%2B%2B20-blue.svg)](https://github.com/cuihairu/apollo)
[![Build Status](https://img.shields.io/github/actions/workflow/status/cuihairu/apollo/ci.yml?branch=main)](https://github.com/cuihairu/apollo/actions/workflows/ci.yml)
[![Coverage](https://codecov.io/gh/cuihairu/apollo/branch/main/graph/badge.svg)](https://codecov.io/gh/cuihairu/apollo)

> Instance-based multiplayer game server engine

## Introduction

Apollo is a lightweight, scalable game server runtime for building instance-based multiplayer games, including co-op games, PvE/PvP instances, dungeon systems, tower defense, arena games, and instance-oriented MMO architectures.

Apollo treats a **Zone** as an authoritative simulation unit. Each Zone owns a complete scene or game instance and can be created, scheduled, recovered, and recycled independently.

```text
Client
   │
Gateway
   │
   ▼
Zone
 ├── Scene / Instance
 ├── AOI
 ├── Session
 ├── Authoritative Game Logic
 └── Journal
        │
        ▼
    DataProxy
        │
        ▼
       DB
```

The architecture intentionally does **not** require BigWorld-style continuous-world mechanisms such as cross-process spatial partitioning, ghost entities, or cell migration.

This makes Apollo a compact alternative for games where the natural world boundary is a **room, scene, dungeon, match, or instance**. For larger deployments, multiple Zones can be orchestrated across machines while preserving the same runtime model.

Positioning: **an instance-based multiplayer game server engine** — rooms/scenes/dungeons/matches are first-class citizens; MMO is a supported use case ("an MMO composed of many Zones/instances", i.e. instance-oriented MMO architectures), not the core identity. The tech stack is modern C++20, with core modules covering networking, data storage, game logic, and the battle system.

## Use Cases

**A good fit** — games whose natural world boundary is a "room / scene / dungeon / match / instance":

| Scenario | Instance Form |
|------|----------|
| Tower defense, fixed maps (e.g. tower-defense circles) | Multiple rooms in one Zone, `CreateInstance` on game start |
| Dungeons / Roguelike | One instance per run, recycled when finished |
| Arena / battle royale | Match instance, destroyed after the match |
| Multiplayer PvE / survival / co-op | Squad instances |
| Multiplayer PvP | Match instances |
| MMO sharding (lines) and dungeons | A line = a scene_id instance, many Zones orchestrated horizontally |

**Not a fit** — a **persistent seamless world**: seamless maps, cross-process spatial roaming, or a deployment model where "one World is split horizontally into CellApps". Apollo intentionally does not do cell partitioning / ghosts / entity migration (architecture decision #9); for such requirements, choose a continuous-world engine such as BigWorld / KBEngine.

> One-line rule of thumb: **if the world's natural boundary is a room/scene/dungeon/match, it fits; if it requires a cross-process continuous world, it does not.**

## Core Concepts

Six terms that cover Apollo's runtime model (all backed by real components; see the [docs site](docs/guide/concepts.md) for details):

| Concept | One-liner | Component |
|------|--------|------|
| **Player** | A player's long-term state — login presence, profiles and anchors that persist across dungeons | `PlayerAnchor` (modules/game/session) |
| **Scene** | Spatial runtime boundary — AOI ownership belongs to the scene | `Scene` + `SceneAoi` (modules/game/world) |
| **Instance** | A first-class citizen, one instance per run — eight-state lifecycle (Create→…→Destroyed), recycled when done | `Instance` (modules/game/world) |
| **AOI** | Interest management — 3×3 grid diffing + per-observer watermarks, Enter/Sync/Leave events | `SceneAoi` + `ViewerState` |
| **Battle** | Battle domain — deterministic tick resolution, settlement flows one way into the player's long-term state | `BattleRuntime` (modules/game/battle) |
| **Persistence** | Profiles and journals — atomic JSON profile writes + write-behind journal, zero external storage dependencies | `PersistJournal` (modules/data) |

### Key Features

- **Minimalist constructor-injection DI** - `apollo::core::di`: type-keyed bean graph, topologically ordered assembly, `ApplicationHost` frame-driven lifecycle
- **Entity contract system** - XML+XSD contracts + standalone generator `apollo_gen`; the generator is not in the runtime link graph
- **Async networking** - cross-platform asynchronous I/O (IOCP/Epoll)
- **Transport codec (target state)** - protobuf reflection as default + zstd (planned); the current implementation uses hand-written encoding
- **AOI 3×3 grid** - `SceneAoi` single implementation + `ViewerState` per-observer watermarks; the event surface is delivered
- **Battle system** - `BattleSystem` in-scene skeleton; ECS convergence and skills/buffs come with P2
- **Data storage** - atomic player profile file writes + `PersistJournal` write-behind; external DB/Redis not wired up
- **Observability** - multi-level async logging + structured line format + `MetricRegistry`
- **BigWorld compatibility layer** - BigWorld-style C++ API facade (`docs/33-BigWorld_Compatibility.md`)

## Architecture

> The full architecture diagrams (system overview / core module architecture / distributed deployment architecture) have moved to the docs site: [Architecture Overview](docs/guide/architecture.md). For process formation, authority decomposition, and the Zone vs. seamless-world analysis, see the [Architecture Audit Report](docs/analysis/architecture-review.md) §31.

### Tech Stack

- **Language**: C++20
- **Build system**: CMake + Ninja, vcpkg manifest mode for dependency management
- **Networking library**: hand-written cross-platform network layer
- **Serialization/contracts**: two layers with a clear division of labor and no conflicts (docs/36 decisions #3/#19) — **def contracts own semantics** (attributes/permission bits/sync masks/internal-external domains; XML+XSD + generator `apollo_gen`, `sdks/contract`, delivered; the server-side carrier is contract.lua, with business handlers and whitelist data driven by the contract); **protobuf owns byte encoding** (reflection as default: descriptor.bin + framework-fixed message families with built-in strong typing for the hot path; clients with a hot-update code pipeline use generated code, bin as fallback; L1 frame format + zstd, planned; `docs/design/sdk-contract.md`). Protobuf is currently a dependency + tests only, not on the message path (the repo has zero of its own .proto files; the current implementation uses hand-written encoding)
- **Database**: currently zero external storage dependencies — player profiles = JSON files + `PersistJournal` write-behind journal (P1-4/P1-5 delivered); MySQL 8 (primary storage) / Redis (cache/sessions) / ClickHouse (analytics) are all planned and not wired up; PostgreSQL has a reserved seam (docs/36 decision #17)
- **Message queue**: Kafka for the observability pipeline (planned, decision #14); inter-service messaging uses a hand-written message bus (planned, `docs/design/net-abstraction.md`)
- **Monitoring**: Prometheus + Grafana (planned, batch 8)
- **Logging**: hand-written multi-level logging (`apollo::core::log`); target pipeline LogAgent→Kafka→ClickHouse (decision #14)
- **Test framework**: GTest + zero-dependency assertion-style unit tests
- **CI/CD**: GitHub Actions

## Quick Start

### Requirements

- C++20 or later
- CMake 3.16+
- GCC 9+ / Clang 10+ / MSVC 2019+

> The current build and runtime have zero external storage dependencies (player profiles are local files + journal); MySQL/Redis runtime wiring is planned, so no pre-installation is needed.

### Build

```bash
# Clone the project
git clone https://github.com/cuihairu/apollo.git
cd apollo

# Install vcpkg (for dependency management)
git clone https://github.com/microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh

# Create a build directory
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/vcpkg/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=x64-linux   # macOS: x64-osx / arm64-osx

# Build
cmake --build build --parallel

# Run the example
./build/examples/session_demo
```

### Minimal Example

About 40 lines to run a hosted game service (see [quick-start](docs/guide/quick-start.md) for the complete compilable version, verified against the actual API):

```cpp
#include <apollo/runtime/application_host.hpp>
#include <apollo/game/world/scene_aoi.hpp>

using apollo::game::world::SceneAoi;

class GameServer : public apollo::runtime::IHostedService {
public:
    std::string_view service_name() const override { return "game-server"; }

    bool start() override {
        aoi_ = std::make_unique<SceneAoi>(1000.0f, 1000.0f, 100.0f, 30.0f);
        aoi_->set_event_sink([](const SceneAoi::Event&) { /* downstream delivery */ });
        for (int i = 1; i <= 100; ++i) {
            aoi_->enter(apollo::game::core::EntityId{static_cast<std::uint64_t>(i)},
                        {static_cast<float>(i), 0.0f, 0.0f});
        }
        return true;
    }

    void stop() override {}
    bool is_running() const override { return true; }
    void tick() override { /* per-frame business logic */ }

private:
    std::unique_ptr<SceneAoi> aoi_;
};

int main() {
    apollo::runtime::ServiceHost host;
    host.add_service(std::make_shared<GameServer>());
    if (!host.start()) return 1;
    while (host.is_running()) host.run_once();
    host.stop();
}
```

### Windows (Visual Studio)

```bash
# Use Visual Studio 2019 or later
git clone https://github.com/cuihairu/apollo.git
cd apollo

git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat

cmake -B build -G "Visual Studio 16 2019" ^
  -DCMAKE_TOOLCHAIN_FILE=%cd%\\vcpkg\\scripts\\buildsystems\\vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows

# Open build\\Apollo.sln to build
```

## Modules

### Base Infrastructure (modules/base)
- **Role**: pure infrastructure with no framework semantics; usable standalone in any C++ project (no dependency on other Apollo modules)
- **Components**: Time utilities, ThreadPool, IdPool, String utilities, Memory utilities, Terminal utilities
- See [Base module docs](docs/modules/base.md) for details

### Core Framework (modules/core · modules/runtime)
- **Dependency injection**: `apollo::core::di` minimalist constructor-injection container — type-keyed bean graph, topologically ordered assembly, only Singleton/Prototype scopes
- **Application lifecycle**: `ApplicationHost` frame-driven hosting — `IHostedService` start/stop/tick + six-phase state machine (Boot→…→Stopped)
- **Configuration**: `apollo::core::config::ConfigRegistry` key-value registry; hot reload is planned to swap ConfigSnapshot at tick boundaries (architecture-review §17.6)

### Game Logic
- **AOI system**: `SceneAoi` 3×3 grid (owned by Scene, isolated by scene_id) + `ViewerState` per-observer watermarks; Enter/Sync/Leave event dispatch is delivered, the gateway downstream delivery surface is left for P3-2
- **Battle system**: `BattleSystem` in-scene skeleton (entity set + tick update); converging on one ECS and skills/buffs/state machines come with P2 (not implemented)
- **Attribute system**: `AttributeContainer`/`AttributeManager` attribute container and definition registration; per-viewer delta sync pipeline not wired (the `attr_batch` contract is already in messages.xml)
- **Scene management**: multi-scene support + scene transfer (`scene_transfer`, prepare→detach→attach→resume + rollback on failure)

### Network
- **Transport layer**: Socket wrappers, supporting TCP/WebSocket/KCP
- **Message layer**: message routing and handling (hand-written encoding today; target protobuf+zstd with schemas generated from def contracts, planned)
- **RPC framework**: remote procedure call framework

### Storage
- **Player profiles**: JSON file profiles (tmp+rename atomic write) + `SaveQueue` async save queue; a load miss fails immediately (no default-profile bootstrap)
- **Write-ahead journal**: `PersistJournal` — append to disk → quota-based drain → snapshot compaction → crash replay continuation (P1-4/P1-5)
- **Serialization**: symmetric toJson/fromJson profile serialization; connection pools/external DB/Redis clients are planned (P1-4 removed five legacy implementations)

### Utils
- **Logging**: multi-level async logging, file/console output
- **Thread pool**: task scheduling
- **Memory pool**: memory management
- **Configuration**: JSON/XML/Lua config support

### Compatibility Layer
- **BigWorld Compatibility Layer**: BigWorld-style C++ API facade (see `docs/33-BigWorld_Compatibility.md`)
- **BigWorld API Tests**: `tests/test_bigworld_api.cpp` (CTest: `BigWorldApiTests`)

## Examples

### Server Base Framework

```cpp
#include "apollo/core/di/application_context.hpp"
#include "apollo/runtime/application_host.hpp"

// Business bean: a plain class, dependencies via constructor (no annotations, no registration macros)
struct GameClockService {
    std::string name = "game_clock";
};

class LoginPipeline {
public:
    explicit LoginPipeline(GameClockService& clock) : clock_(clock) {}
private:
    GameClockService& clock_;
};

// Hosted service: implements IHostedService, frame-driven by ApplicationHost
class GameServerService : public apollo::runtime::IHostedService {
public:
    explicit GameServerService(apollo::core::di::ApplicationContext& ctx)
        : clock_(ctx.get<GameClockService>()) {}

    std::string_view service_name() const override { return "game_server"; }
    bool start() override { return true; }
    void stop() override {}
    bool is_running() const override { return true; }
    void tick() override { /* per-frame business logic */ }

private:
    GameClockService& clock_;
};

int main() {
    // Assembly: the bean graph = a chain of add_singleton calls, dependencies are template parameters
    apollo::core::di::ApplicationContextBuilder builder;
    builder.add_singleton<GameClockService>().name("game_clock");
    builder.add_singleton<LoginPipeline, GameClockService>().name("login_pipeline");
    auto context = builder.build();
    if (!context.initialize()) {
        return 1;  // topologically ordered construction; exit on failure
    }

    // Hosting: start → run_once frame loop → stop
    apollo::runtime::ServiceHost host;
    host.add_service(std::make_shared<GameServerService>(context));
    if (!host.start()) {
        return 1;
    }
    while (host.is_running()) {
        host.run_once();
    }
    host.stop();
    return 0;
}
```

> See `apps/game-server/src/main.cpp` for the complete runnable version. Assembly happens only at application entry points (apps/); modules internally never know about the container.

### Using the AOI System

```cpp
#include "apollo/game/world/scene_aoi.hpp"

using apollo::game::core::EntityId;
using apollo::game::world::SceneAoi;

// 3×3-grid interest management: each Scene owns one instance (naturally isolated by scene)
SceneAoi aoi(1000.0f, 1000.0f, 100.0f, 30.0f);  // width, height, cell size, view radius

// Event surface: Enter / Sync / Leave (the sink defaults to silent; Scene/host injects delivery)
aoi.set_event_sink([](const SceneAoi::Event& e) {
    // e.kind / e.observer / e.subject — the viewer-set diff baseline lives in ViewerState
});

// Entity enter / move / leave
aoi.enter(EntityId{1001}, {100.0f, 0.0f, 100.0f});
auto viewers = aoi.viewers_of(EntityId{1001});   // entities in view (including self)
aoi.move(EntityId{1001}, {120.0f, 0.0f, 100.0f});
aoi.leave(EntityId{1001});
```

### Using the Battle System

```cpp
#include "apollo/game/battle/BattleWorld.h"
#include "apollo/game/battle/components.hpp"

// Create the battle world
auto world = std::make_unique<apollo::BattleWorld>(1);

// Create a player entity
auto player = world->CreateEntity(1001);
auto transform = player->AddComponent<apollo::TransformComponent>();
auto attributes = player->AddComponent<apollo::AttributeComponent>();
auto battleState = player->AddComponent<apollo::BattleStateComponent>();

// Set attributes
attributes->SetLevel(50);
attributes->SetHp(1000);
attributes->SetAttack(150);

// Update the battle world
world->Update(deltaTime);
```

## Performance Targets

| Metric | Target | Notes |
|------|--------|------|
| Capacity per server | 5000+ CCU | Concurrent users on a single GameServer |
| Response latency | P99 < 50ms | Core operation response time |
| Memory usage | < 2GB | Memory footprint at 5000 users |
| Network throughput | 100MB/s | Peak network bandwidth |
| Database connections | 1000+ | Max connection pool size |

## Testing

The project includes comprehensive unit tests and example code:

```bash
# Run all tests (full list in tests/CMakeLists.txt)
cd build && ctest

# Run only the BigWorld compatibility layer tests
cd build && ctest -R BigWorldApiTests
```

Test coverage:
- Network and protocol tests (network/net/protocol/channel)
- Data and persistence tests (data, `PersistJournalTests`, `PersistenceChainTests` — file profile write→restart→read-back)
- World and lifecycle tests (game/scene/instance/avatar/session_world/world_host)
- Session and recovery tests (`PlayerDirectoryTests`, `RecoveryTests`, `ReconnectTests`)
- Infrastructure tests (logging/timers/config/thread pool/serialization, etc.)
- BigWorld compatibility layer API tests (`BigWorldApiTests`)

## Directory Structure

```
apollo/
├── apps/                   # Executable services: login/gateway/base/cell/game-server
├── modules/                # Modular source: base/core/data/game/net/runtime, etc.
├── include/                # Public headers and compatibility layer headers
├── src/                    # Public implementations and compatibility layer implementations
├── tests/                  # Test code
├── examples/               # Example code
├── sdks/                   # Unity client SDK and contract tooling (contract + gen generator + cpp generated code)
├── skds/                   # Cocos / Laya / legacy SDK workspace
├── docs/                   # VitePress documentation site
├── cmake/                  # CMake helper scripts
└── build*/                 # Build output
```

See [Directory Structure](docs/Directory_Structure.md) for details.

## Client SDK

### Unity SDK

Parts of the Unity SDK are delivered (module status: see [SDK_Structure](docs/sdks/unity/SDK_Structure.md)):

- Network communication management (NetworkManager, available)
- Login authentication (AuthManager, available)
- Attribute sync (Attributes, in development)
- Message serialization/deserialization (Messaging, planned)

```csharp
// Unity SDK usage example
var config = new ApolloClientConfig {
    ServerAddress = "game.example.com",
    Port = 7700,
    Protocol = TransportProtocol.Tcp
};

var client = new ApolloClient(config);
await client.ConnectAsync();
await client.LoginAsync(loginRequest);
```

## Contributing

Contributions are welcome! Please follow these steps:

1. Fork the project
2. Create a feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

### Code Style

- Follow the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
- Use meaningful variable and function names
- Add appropriate comments
- Write unit tests

## License

This project is licensed under the [MIT License](LICENSE).

## Origin and Acknowledgements

Apollo is an original implementation of this repository, not a fork of any existing project; its architecture concepts and terminology reference the following open-source projects, with sources cited where referenced:

- **BigWorld**: BigWorld-style C++ API facade (`docs/33-BigWorld_Compatibility.md`); the architecture audit includes source-level comparison of BigWorld / KBEngine / skynet (`docs/analysis/architecture-review.md` §16)
- **KBEngine**: several entries in `docs/qa/` analyze its source mechanisms
- **skynet**: design precedents such as the observability channel (`docs/analysis/architecture-review.md`)

Third-party dependencies: [vcpkg](https://github.com/microsoft/vcpkg) (dependency management), [nlohmann/json](https://github.com/nlohmann/json), [protobuf](https://github.com/protocolbuffers/protobuf) (currently a dependency + tests, not on the message path), [GTest](https://github.com/google/googletest), NNG (`modules/protocol` is currently a stub, real transport not enabled), [VitePress](https://vitepress.dev) (docs site), GitHub Actions and Codecov (CI and coverage).

## Contact

- Project home: https://github.com/cuihairu/apollo
- Issues: [Issues](https://github.com/cuihairu/apollo/issues)
- Email: cuihairu@example.com

---

**Apollo - Instance-based multiplayer game server engine**
