---
title: BigWorld Server Application Implementation
icon: server
order: 6
category:
  - Architecture
  - BigWorld
tag:
  - BigWorld
  - Server
  - NNG
---

[English](BigWorld服务器应用实现.en.md) | [中文](BigWorld服务器应用实现.md)

# BigWorld Server Application Implementation

This document describes how the MMO / distributed-world-related server applications are assembled in Apollo.

Two important boundaries first:

- `BaseApp` is not a database server
- `GatewayApp` is not the mandatory core process in the BigWorld / KBEngine sense

The more accurate way to read Apollo today:

- A standard MMO uses a standalone `GatewayApp` by default
- Distributed world mode switches by default to `BaseApp(Proxy + PlayerAnchor) -> CellApp`

So this document should be read as "server application assembly notes", not "a verbatim copy of the BigWorld native fixed topology".

---

## Table of Contents

- [Architecture Overview](#architecture-overview)
- [Communication Protocol](#communication-protocol)
- [GatewayApp Edge Access Layer](#gatewayapp-edge-access-layer)
- [LoginApp Login Server](#loginapp-login-server)
- [BaseApp Player Anchor Host](#baseapp-player-anchor-host)
- [CellApp Game Logic Server](#cellapp-game-logic-server)
- [Deployment](#deployment)

---

## Architecture Overview

### Server Topology

The default main chain of a distributed world:

```text
Client -> LoginApp -> BaseApp(Proxy + PlayerAnchor) -> CellApp
```

The corresponding assembly:

```
                    ┌─────────────────────────────────────────┐
                    │                  Client                  │
                    │              (Unity/Unreal)              │
                    └────────────────────┬────────────────┘
                                         │ TCP/WebSocket
                                         ▼
┌─────────────────────────────────────────────────────────────────┐
│                       LoginApp (9001)                            │
│          Authentication / entry assignment / ticket issuing      │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                 BaseApp (9002, PlayerAnchor + Proxy)            │
│       Online master state / SessionBinding / Reconnect / Cell   │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                    CellApp Pool (9100+)                          │
│              AOI / Entity / Combat / Space Runtime               │
└─────────────────────────────────────────────────────────────────┘
```

If Apollo is assembled as a standard MMO or with edge governance, a standalone `GatewayApp` is added on the outside:

```
                    ┌─────────────────────────────────────────┐
                    │                  Client                  │
                    │              (Unity/Unreal)              │
                    └────────────────────┬────────────────┘
                                         │ TCP/WebSocket
                                         ▼
┌─────────────────────────────────────────────────────────────────┐
│                    GatewayApp (8888, optional)                   │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌────────────┐   │
│  │ Session   │  │ Message   │  │ Heartbeat │  │   Traffic  │   │
│  │ Manager  │  │  Router   │  │  Detector │  │   Control  │   │
│  └───────────┘  └─────┬─────┘  └───────────┘  └────────────┘   │
└───────────────────────────┬─────────────────────────────────────────┘
                            │ NNG (REQ/REP)
        ┌───────────────────┼───────────┬───────────────┐
        │                   │           │               │
        ▼                   ▼           ▼               ▼
┌──────────────┐  ┌──────────┐  ┌─────────┐  ┌──────────────┐
│ LoginApp     │  │ BaseApp  │  │CellApp  │ │  CellApp...   │
│   (9001)     │  │  (9002)  │  │ (9100)  │ │   (9101...)  │
│              │  │          │  │         │  │              │
│ - Auth       │  │ - PlayerAnchor │  │ - AOI   │  │              │
│ - Entry alloc│  │ - Proxy/Session │  │ - Entity│  │              │
└──────────────┘  └──────────┘  └─────────┘  └──────────────┘
```

### Process Responsibilities

| Server | Port | Responsibilities |
|--------|------|------|
| **GatewayApp** | 8888 | Client connection management, admission validation, message forwarding, heartbeat detection |
| **LoginApp** | 9001 | Account authentication, entry assignment, session ticket generation |
| **BaseApp** | 9002 | `PlayerAnchor`, session ownership, reconnect recovery, world or space assignment |
| **CellApp** | 9100+ | Game logic, AOI system, entity management, combat computation |

---

## Communication Protocol

### Inter-process Communication (Current State and Retirement Path)

> **Status note (2026-10-07 audit)**: access processes currently communicate via
> `modules/protocol` (REP/REQ socket wrapper, `apollo/protocol/socket.hpp`) and
> `modules/net/protocol` (endpoint/channel foundation, gated by `apollo_protocol`
> until the convergence). This foundation is a **transitional form** — the
> retirement path is decided (self-contained four-layer stack + InterServerLink,
> net M1). The feature table in this section describes the transitional
> foundation and will be updated once M1 converges.

| Feature | Description |
|------|------|
| **REQ/REP** | Request-response pattern, used for RPC calls (in use by access processes login/gateway/baseappmgr) |
| **Auto reconnect** | Built-in automatic reconnection after connection loss |
| **Cross-platform** | Supports Linux/Windows/macOS |

Other patterns such as PUB/SUB and PAIR currently have no process-level consumers; fleet discovery between processes goes over UDP broadcast
(`modules/net/discovery`, the machined fleet directory) and does not depend on this foundation.

### Message Format

```
┌──────────────────────────────────────────────────────────────┐
│ MessageHeader (26 bytes)                                       │
├──────────────────────────────────────────────────────────────┤
│ magic (4) │ version (2) │ type (2) │ length (4) │             │
│ 0x42575452 │    1        │ MSG_TYPE  │ BODY_LEN │             │
├───────────────┴──────────────┴────────────┴─────────────┤
│ sequence (8) │ sessionId (8)                                   │
├──────────────┴─────────────────────────────────────────────┤
│ MessageBody (length bytes)                                   │
│ - JSON or binary serialization                              │
└──────────────────────────────────────────────────────────────┘
```

### Message Types

The following aligns item-by-item with the `MessageType` enum in
`modules/protocol/include/apollo/protocol/messages.hpp` (2026-10-07 audit):

```cpp
// Login
LOGIN_REQUEST           = 0x0001
LOGIN_RESPONSE          = 0x0002
LOGOUT_NOTIFY           = 0x0003

// Gateway assignment
GATEWAY_ASSIGN_REQUEST    = 0x0010
GATEWAY_ASSIGN_RESPONSE   = 0x0011
GATEWAY_HEARTBEAT         = 0x0012
GATEWAY_CLIENT_CONNECT    = 0x0013
GATEWAY_CLIENT_DISCONNECT = 0x0014

// Persistence
DB_LOAD_REQUEST         = 0x0020
DB_LOAD_RESPONSE        = 0x0021
DB_SAVE_REQUEST         = 0x0022
DB_SAVE_RESPONSE        = 0x0023
DB_QUERY_REQUEST        = 0x0024
DB_QUERY_RESPONSE       = 0x0025

// Cell
CELL_CREATE_ENTITY      = 0x0030
CELL_DESTROY_ENTITY     = 0x0031
CELL_ENTITY_ENTER       = 0x0032
CELL_ENTITY_LEAVE       = 0x0033
CELL_ENTITY_MOVE        = 0x0034
CELL_ENTITY_PROPERTY    = 0x0035
CELL_CROSS_BORDER       = 0x0036

// Combat
COMBAT_SKILL_CAST       = 0x0040
COMBAT_DAMAGE           = 0x0041
COMBAT_HEAL             = 0x0042
COMBAT_DEATH            = 0x0043

// Chat
CHAT_MESSAGE            = 0x0050
CHAT_BROADCAST          = 0x0051

// Player activation
PLAYER_ACTIVATE_REQUEST  = 0x0060
PLAYER_ACTIVATE_RESPONSE = 0x0061
```

---

## GatewayApp Edge Access Layer

This section describes the optional assembly Apollo uses for standard MMO or edge-governance scenarios.

If the distributed world default main chain is used:

```text
Client -> LoginApp -> BaseApp(Proxy + PlayerAnchor) -> CellApp
```

then the `GatewayApp` described here may simply not exist.

### Responsibilities

1. **Client connection management**
   - Accepts client TCP/WebSocket connections
   - Maintains connection state
   - Handles disconnections

2. **Session management**
```cpp
struct SessionConfig {
    int sessionTimeoutMs = 30000;       // disconnect after 30s without heartbeat
    int reconnectWindowMs = 30000;     // reconnect window after drop
};
```

3. **Message routing**
   - Forwards to backend services by message type
   - CellApp → game logic messages
   - BaseApp → online main-chain and routing messages
   - ChatApp → chat messages

4. **Heartbeat detection**
```cpp
int heartbeatIntervalMs = 5000;    // client heartbeat interval
int heartbeatCheckIntervalMs = 1000; // server check interval
```

### Usage Example

```bash
# start the edge access layer
./gateway-app --port 8888 --max-connections 10000
```

### Directory Structure

```
apps/gateway-app/
├── CMakeLists.txt
├── include/gateway/
│   ├── config.hpp              # config definitions
│   ├── session_manager.hpp     # session manager
│   ├── gateway_server.hpp      # server main class
│   └── ingress/                # access surface (P1 convergence)
│       ├── client_ingress_server.hpp   # client ingress service
│       ├── client_packet_dispatcher.hpp# client packet dispatcher
│       ├── gateway_connection_registry.hpp # connection registry
│       └── session_admission_service.hpp   # session admission service
└── src/
    ├── main.cpp
    ├── gateway_server.cpp
    └── ingress/
```

---

## LoginApp Login Server

### Responsibilities

> **Illustrative interface note**: the code blocks in this section illustrate
> responsibilities (same precedent as the CellApp section's AOIManager) and are
> not real repo symbols; the real entry surface is the command-line usage in the
> usage examples.

1. **Account authentication**
```cpp
struct Authenticator {
    bool authenticate(const std::string& username,
                     const std::string& password,
                     PlayerID& outPlayerId,
                     std::string& errorMessage);
};
```

2. **Entry assignment**
```cpp
class EntryAllocator {
    // standard MMO returns a Gateway entry
    // distributed world returns a BaseApp Proxy entry
    std::string selectEntry();
};
```

Note carefully:

- `LoginApp` only assigns the entry
- It does not directly handle `Proxy` creation
- It does not directly handle real-time `CellApp` assignment either

3. **Session management**
```cpp
class SessionManager {
    // create a session
    SessionID createSession(PlayerID playerId, const std::string& entryUrl);

    // validate a session
    bool validateSession(SessionID sessionId, PlayerID& outPlayerId);
};
```

### Login Flow

There are two possible kinds of "entry" here:

- Standard MMO: returns `GatewayApp`
- Distributed world: returns `BaseApp Proxy`

```
Client                    LoginApp                BaseApp
  │                          │                       │
  │ 1. login request(username, password) │                  │
  ├─────────────────────────►│                       │
  │                          │ 2. verify account/password │          │
  │                          ├──────────────────────►│                       │
  │                          │                       │                       │
  │                          │ 3. pick the access entry │                    │
  │                          │ ┌─────────────────┐   │                       │
  │                          │ │ Gateway1: 100 players │  │                  │
  │                          │ │ BaseApp2:  50 players │  │                  │
  │                          │ └─────────────────┘   │                       │
  │                          │                       │                       │
  │ 4. return login success   │                       │
  │◄─────────────────────────┤                       │
  │  - sessionId              │                       │
  │  - playerId               │                       │
  │  - entryHost/port          │                       │
```

### Usage Example

Actual command-line arguments (`./login-app --help`, 2026-10-07 audit):

```bash
# start the login server (the --entry-mode flag does not exist today; the
# entry form is expressed via --gateway / --base-app backend attachment)
./login-app --port 9001

# standard MMO assembly: attach a Gateway backend
./login-app --port 9001 --gateway tcp://127.0.0.1:8888

# distributed world assembly: attach a BaseApp backend
./login-app --port 9001 --base-app tcp://127.0.0.1:9002
```

---

## BaseApp Player Anchor Host

### Responsibilities

1. **Player anchor management**
```cpp
class PlayerAnchorService {
    bool activatePlayer(PlayerID playerId, SessionID sessionId);
    bool bindClientEntry(PlayerID playerId, const std::string& entryId);
    bool assignRuntimeTarget(PlayerID playerId, WorldAssignment& outAssignment);
};
```

In a standard MMO, `WorldAssignment` here can represent:

- `WorldApp`
- `Instance`
- `baseapp`

In distributed world mode it is closer to:

- `CellApp`
- `Space`
- `Partition`

2. **Reconnect recovery and session ownership**
```cpp
struct BaseConfig {
    bool enableReconnect = true;
    int reconnectWindowMs = 60000;
};
```

3. **Persistence coordination**
```cpp
class PersistenceCoordinator {
    void flushPlayer(PlayerID playerId);
};
```

### Player Data Structure

```cpp
struct PlayerData {
    PlayerID playerId;
    std::string username;
    int level = 1;
    int64_t exp = 0;
    int hp = 100;
    int maxHp = 100;
    float x = 0, y = 0, z = 0;

    // JSON serialization
    std::string toJson() const;
    static PlayerData fromJson(const std::string& json);
};
```

### Usage Example

```bash
# start the BaseApp player anchor host (--db names the database)
./base-app --port 9002 --db apollo
```

---

## CellApp Game Logic Server

In distributed world mode this section maps directly to `CellApp`.

In standard MMO mode the equivalent position is closer to:

- `WorldApp`
- or a single-machine world runtime

> **Status note (2026-10-07 audit)**: `WorldApp` currently has no standalone
> process directory; the world runtime lives as library components in
> `modules/game/world` (Scene/AOI/Instance), and `apps/game-server` is the
> single-machine assembly example. The world-app examples in the startup
> commands below likewise defer to the cell-app as it exists today.

### Responsibilities

1. **Entity management**
```cpp
class EntityManager {
    Entity* createEntity(EntityID id, EntityType type);
    void destroyEntity(EntityID id);
    Entity* getEntity(EntityID id);
    void update(float dt);
};
```

2. **AOI 3x3 grid system** (implementation: `apollo::game::world::SceneAoi`, converged in P1-3; the sample is an illustrative interface)
```cpp
class AOIManager {
    void enter(Entity* entity);
    void move(Entity* entity, const Position& newPos);
    void leave(Entity* entity);
    std::vector<Entity*> getViewers(Entity* entity);
};
```

3. **Game loop**
```cpp
struct CellConfig {
    float spaceWidth = 1000.0f;
    float spaceHeight = 1000.0f;
    float gridSize = 100.0f;
    float viewRadius = 200.0f;
    int tickRateMs = 50;  // 20 ticks per second
};
```

### Entity Types

```cpp
enum class EntityType : uint8_t {
    UNKNOWN = 0,
    PLAYER = 1,
    NPC = 2,
    MONSTER = 3,
    PET = 4,
};
```

### Usage Example

```bash
# start the game logic server (--space names the Space, default main_world)
./cell-app --port 9100 --space main_world --size 2000 2000 --tick-rate 50
```

---

## Manager Scheduling Plane

> Directory: `apps/baseappmgr` (default port 9003, override with `--port`).

Responsibilities (scheduling plane, direct lineage of the BigWorld BaseAppMgr):

- **Directory**: the global player→SessionBinding / player→WorldAssignment directory
- **Placement arbitration**: `assignWorld` / `clearWorldAssignment`
- **Route resolution**: ResolveRoute (answers gateway questions of the form "which world is the player on, via which gateway")

Boundaries: it carries no player data (Avatar resident data belongs to base-app) and never mutates another process's memory across processes — this process only maintains its own directory table.

```bash
# start the scheduling plane
./baseappmgr --port 9003
```

## Machined Supervisor Daemon

> Directory: `apps/machined` (default fleet port 9600, override with `--port`; `--interval` heartbeat period in ms, `--backoff` restart backoff base in ms, `--roster` the fleet roster).

Responsibilities (fleet + supervision, two faces):

- **Fleet directory**: UDP broadcast discovery (Register/Heartbeat/Deregister + Query/Advertise replies), member-table TTL timeout marks death, graceful deregister goes offline immediately
- **Supervision face**: the roster (`name | command args...`) is fully brought up at startup; `waitpid(WNOHANG)` non-blocking reaping; linear-backoff auto restart (n-th retry delayed by n×backoff_ms), gives up and alerts past the limit; SIGTERM/SIGINT reaps all children
- Itself never joins the fleet table (service identity and member identity are separate)

POSIX only (deployment target is Linux servers); on Windows the supervision branch idles with a warning while the discovery face works as usual.

```bash
# start the supervisor daemon (brings up the roster and serves the fleet directory)
./machined --port 9600 --roster fleet.roster
```

## game-server Assembly Sample

> Directory: `apps/game-server`. **Not a topology member** — a monolithic assembly reference sample.

Demonstrates the full modular assembly chain: how `module_manifest` + `RuntimeManifest` + `ServiceHost`/`IHostedService` + DI (`ApplicationContext`) + `ConfigRegistry` + data source/ORM (`DataSource`/`MemoryConnection`/`SqlTemplate`) wire together. New service processes use it as the assembly template and pick their service set per topology needs.

---

## Deployment

> **Status note (2026-10-07 audit)**: the topology diagrams in this section are
> **target-form** illustrations — `WorldApp` currently has no standalone process
> directory (the world runtime sits at the `modules/game/world` library layer;
> see `apps/game-server` for assembly), and the Docker Compose orchestration
> files are not in the repo yet. For commands that actually run today, defer to
> the "Running Guide" section.

### Single-machine Deployment (Standard MMO)

```
┌─────────────────────────────────────────────┐
│           Standard MMO dev environment       │
│                                                     │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐        │
│  │GatewayApp│ │LoginApp  │ │BaseApp  │        │
│  │ :8888    │ │ :9001    │ │ :9002    │        │
│  └──────────┘ └──────────┘ └──────────┘        │
│                                                     │
│  ┌──────────┐ ┌──────────┐                      │
│  │WorldApp 0│ │WorldApp 1│ ...                   │
│  │ :9100    │ │ :9101    │                      │
│  └──────────┘ └──────────┘                      │
└─────────────────────────────────────────────┘
```

### Single-machine Deployment (Distributed World)

```
┌─────────────────────────────────────────────┐
│        Distributed World dev environment     │
│                                             │
│  ┌──────────┐ ┌──────────┐                  │
│  │LoginApp  │ │BaseApp   │                  │
│  │ :9001    │ │ :9002    │                  │
│  └──────────┘ └──────────┘                  │
│                                             │
│  ┌──────────┐ ┌──────────┐                  │
│  │CellApp 0 │ │CellApp 1 │ ...              │
│  │ :9100    │ │ :9101    │                  │
│  └──────────┘ └──────────┘                  │
└─────────────────────────────────────────────┘
```

### Distributed Deployment (Standard MMO)

```
┌─────────────────────────────────────────────────────────────────┐
│                        Load balancer                            │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
        ┌──────────────────────┼──────────────────────┐
        │                      │                      │
        ▼                      ▼                      ▼
┌──────────────┐          ┌──────────────┐          ┌──────────────┐
│ Gateway Pool │          │ Gateway Pool │          │ Gateway Pool │
│ Server 1-4   │          │ Server 5-8   │          │ Server 9-12  │
└──────┬───────┘          └──────┬───────┘          └──────┬───────┘
       │                         │                         │
       └─────────────────────────┴─────────────────────────┘
                               │
        ┌──────────────────────────────────────────────┐
        │           Shared backend service cluster     │
        ├─────────────┬───────────┬───────────┬───────┐
        │             │           │           │       │
        ▼             ▼           ▼           ▼       ▼
  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐
  │LoginApp │  │ BaseApp │  │WorldApp0│  │WorldApp1│
  │ (cluster)│  │ (cluster)│  │ (main   │  │ (open   │
  │         │  │         │  │  city)  │  │  world) │
  └─────────┘  └─────────┘  └─────────┘  └─────────┘
```

### Distributed Deployment (Distributed World)

```
┌──────────────────────────────────────────────────────────────┐
│                        Login cluster                         │
└──────────────────────────────┬───────────────────────────────┘
                               │
                      ┌────────▼────────┐
                      │   BaseApp Pool  │
                      │ Proxy + Anchor  │
                      └────────┬────────┘
                               │
                 ┌─────────────┼─────────────┐
                 │             │             │
                 ▼             ▼             ▼
          ┌──────────┐  ┌──────────┐  ┌──────────┐
          │ CellApp0 │  │ CellApp1 │  │ CellAppN │
          │ main city│  │  open    │  │ instance │
          │   zone   │  │  field   │  │   zone   │
          └──────────┘  └──────────┘  └──────────┘
                               │
                        ┌──────▼──────┐
                        │ DBMgr / storage │
                        └─────────────┘
```

### Docker Compose Deployment (Standard MMO)

```yaml
version: '3.8'

services:
  gateway:
    image: apollo/gateway-app:latest
    ports:
      - "8888:8888"
    environment:
      - LOGIN_APP_URL=tcp://login-app:9001
      - BASE_APP_URL=tcp://base-app:9002
      - WORLD_APP_URL=tcp://world-app:9100
    depends_on:
      - login-app
      - base-app
      - world-app

  login-app:
    image: apollo/login-app:latest
    ports:
      - "9001:9001"
    environment:
      - BASE_APP_URL=tcp://base-app:9002

  base-app:
    image: apollo/base-app:latest
    ports:
      - "9002:9002"
    environment:
      - DB_HOST=mysql
      - REDIS_HOST=redis
    depends_on:
      - mysql
      - redis

  world-app:
    image: apollo/world-app:latest
    ports:
      - "9100-9120:9100-9120"
    environment:
      - SPACE_NAME=main_world
      - SPACE_WIDTH=2000
      - SPACE_HEIGHT=2000
```

### Docker Compose Deployment (Distributed World)

```yaml
version: '3.8'

services:
  login-app:
    image: apollo/login-app:latest
    ports:
      - "9001:9001"
    environment:
      - BASE_APP_URL=tcp://base-app:9002

  base-app:
    image: apollo/base-app:latest
    ports:
      - "9002:9002"
    environment:
      - CELL_APP_URL=tcp://cell-app:9100
      - DB_HOST=mysql
      - REDIS_HOST=redis
    depends_on:
      - mysql
      - redis
      - cell-app

  cell-app:
    image: apollo/cell-app:latest
    ports:
      - "9100-9120:9100-9120"
    environment:
      - SPACE_NAME=main_world
      - SPACE_WIDTH=2000
      - SPACE_HEIGHT=2000
```

---

## Running Guide

### Building the Project

```bash
# dependencies are installed automatically via the repo vcpkg.json manifest
# (no manual vcpkg install needed)
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake \
  -DAPOLLO_BUILD_GAME_MODULE=ON

# compile
cmake --build build
```

### Starting the Servers

#### Standard MMO

> `WorldApp` has no standalone process directory (see the deployment status
> note); the world runtime position is carried by cell-app today; a fleet can
> be brought up in one shot via machined (scripts/dev_fleet.sh).

```bash
# 1. start BaseApp (player anchor host)
./build/apps/base-app/base-app --port 9002

# 2. start LoginApp (login service)
./build/apps/login-app/login-app --port 9001

# 3. start CellApp (world runtime position)
./build/apps/cell-app/cell-app --port 9100 --size 2000 2000

# 4. start GatewayApp (edge access layer)
./build/apps/gateway-app/gateway-app --port 8888
```

#### Distributed World

```bash
# 1. start BaseApp (PlayerAnchor + Proxy host)
./build/apps/base-app/base-app --port 9002

# 2. start LoginApp (login service)
./build/apps/login-app/login-app --port 9001

# 3. start CellApp (game logic service)
./build/apps/cell-app/cell-app --port 9100 --size 2000 2000
```

### Startup Order

#### Standard MMO

**Important**: the servers must be started in this order:

1. **BaseApp** - online master state host, usually started first
2. **LoginApp** - depends on BaseApp
3. **WorldApp** - can start independently, or after BaseApp
4. **GatewayApp** - depends on all backend services

#### Distributed World

1. **BaseApp** - `PlayerAnchor + Proxy` host, usually started first
2. **LoginApp** - depends on BaseApp for entry assignment
3. **CellApp** - depends on BaseApp for player activation and space assignment

### Shutdown Order

#### Standard MMO

Shutdown order is the reverse of startup:

1. **GatewayApp** - stop accepting new connections
2. **WorldApp** - save all entity state
3. **LoginApp** - finish existing sessions
4. **BaseApp** - shut down last, ensuring online state and persistence coordination complete

#### Distributed World

1. **LoginApp** - stop issuing new tickets
2. **CellApp** - save entity state within spaces, stop new authority transfers
3. **BaseApp** - shut down last, ensuring online-state unbinding and persistence coordination complete

---

## Summary

### Standard MMO

| Server | Port | Depends on | Startup priority |
|--------|------|------|------------|
| GatewayApp | 8888 | LoginApp, BaseApp, WorldApp | 4 |
| LoginApp | 9001 | BaseApp | 2 |
| BaseApp | 9002 | none | 1 |
| WorldApp | 9100+ | none | 3 |

### Distributed World

| Server | Port | Depends on | Startup priority |
|--------|------|------|------------|
| LoginApp | 9001 | BaseApp | 2 |
| BaseApp | 9002 | CellApp, DBMgr | 1 |
| CellApp | 9100+ | BaseApp | 3 |

There are only three governing principles:

- A standard MMO uses a standalone `GatewayApp` by default
- Distributed world switches by default to `BaseApp(Proxy + PlayerAnchor) -> CellApp`
- `BaseApp` is never a database server

## Related Docs

- [BigWorld Architecture Deep Dive](/architecture/bigworld)
- [BigWorld Process Architecture and Player Lifecycle](/architecture/bigworld-lifecycle)
- [AOI 3x3 Grid System](/architecture/aoi)
- [Protocol Module API](/modules/)
