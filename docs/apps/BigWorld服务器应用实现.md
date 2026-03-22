---
title: BigWorld 服务器应用实现
icon: server
order: 6
category:
  - 架构
  - BigWorld
tag:
  - BigWorld
  - 服务器
  - NNG
---

# BigWorld 服务器应用实现

本文档描述 Apollo 中与 MMO / 分布式世界相关的服务器应用装配方式。

先说明两个重要边界：

- `BaseApp` 不是数据库服务器
- `GatewayApp` 不是 BigWorld / KBEngine 语义里的必选核心进程

Apollo 当前更合理的理解方式是：

- 普通 MMO 默认使用独立 `GatewayApp`
- 分布式世界模式默认切到 `BaseApp(Proxy + PlayerAnchor) -> CellApp`

因此这篇文档应理解为“服务器应用装配说明”，而不是“BigWorld 原生固定拓扑的逐字复制”。

---

## 目录

- [架构概览](#架构概览)
- [通信协议](#通信协议)
- [GatewayApp 边缘接入层](#gatewayapp-边缘接入层)
- [LoginApp 登录服务器](#loginapp-登录服务器)
- [BaseApp 玩家锚点宿主](#baseapp-玩家锚点宿主)
- [CellApp 游戏逻辑服务器](#cellapp-游戏逻辑服务器)
- [部署方式](#部署方式)

---

## 架构概览

### 服务器拓扑

先给出分布式世界默认主链：

```text
Client -> LoginApp -> BaseApp(Proxy + PlayerAnchor) -> CellApp
```

对应装配如下：

```
                    ┌─────────────────────────────────────────┐
                    │                  Client                  │
                    │              (Unity/Unreal)              │
                    └────────────────────┬────────────────┘
                                         │ TCP/WebSocket
                                         ▼
┌─────────────────────────────────────────────────────────────────┐
│                       LoginApp (9001)                            │
│                 认证 / 入口分配 / 登录票据发放                     │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                 BaseApp (9002, PlayerAnchor + Proxy)            │
│       在线主状态 / SessionBinding / Reconnect / Cell 分配        │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                    CellApp Pool (9100+)                          │
│              AOI / Entity / Combat / Space Runtime               │
└─────────────────────────────────────────────────────────────────┘
```

如果 Apollo 采用普通 MMO 或边缘治理装配，则会在外层再增加独立 `GatewayApp`：

```
                    ┌─────────────────────────────────────────┐
                    │                  Client                  │
                    │              (Unity/Unreal)              │
                    └────────────────────┬────────────────┘
                                         │ TCP/WebSocket
                                         ▼
┌─────────────────────────────────────────────────────────────────┐
│                    GatewayApp (8888, 可选)                        │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌────────────┐   │
│  │ Session   │  │ Message   │  │ Heartbeat │  │  流量控制  │   │
│  │ Manager  │  │  Router   │  │  Detector │  │           │   │
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
│ - 认证       │  │ - PlayerAnchor │  │ - AOI   │  │              │
│ - 入口分配   │  │ - Proxy/Session │  │ - 实体  │  │              │
└──────────────┘  └──────────┘  └─────────┘  └──────────────┘
```

### 进程职责

| 服务器 | 端口 | 职责 |
|--------|------|------|
| **GatewayApp** | 8888 | 客户端连接管理、接入校验、消息转发、心跳检测 |
| **LoginApp** | 9001 | 账号认证、入口分配、会话票据生成 |
| **BaseApp** | 9002 | `PlayerAnchor`、会话归属、重连恢复、世界或空间分配 |
| **CellApp** | 9100+ | 游戏逻辑、AOI系统、实体管理、战斗计算 |

---

## 通信协议

### NNG (nanomsg-next-gen)

选择 NNG 作为进程间通信协议的原因：

| 特性 | 说明 |
|------|------|
| **REQ/REP** | 请求-响应模式，用于 RPC 调用 |
| **PUB/SUB** | 发布-订阅模式，用于广播和 AOI 同步 |
| **PAIR** | 双向通信模式，用于 CellApp 间点对点通信 |
| **自动重连** | 内置连接断开自动重连 |
| **高性能** | 零拷贝传输，低延迟 |
| **跨平台** | 支持 Linux/Windows/macOS |

### 消息格式

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
│ - JSON 或二进制序列化                                       │
└──────────────────────────────────────────────────────────────┘
```

### 消息类型

```cpp
// 登录相关
LOGIN_REQUEST           = 0x0001
LOGIN_RESPONSE          = 0x0002

// 入口相关
ENTRY_ASSIGN_REQUEST    = 0x0010
ENTRY_ASSIGN_RESPONSE   = 0x0011
SESSION_HEARTBEAT       = 0x0012

// 持久化相关
DB_LOAD_REQUEST         = 0x0020
DB_LOAD_RESPONSE        = 0x0021
DB_SAVE_REQUEST         = 0x0022
DB_SAVE_RESPONSE        = 0x0023

// CellApp 相关
CELL_CREATE_ENTITY      = 0x0030
CELL_ENTITY_MOVE        = 0x0034
CELL_CROSS_BORDER       = 0x0036

// 战斗相关
COMBAT_SKILL_CAST       = 0x0040
COMBAT_DAMAGE           = 0x0041
COMBAT_DEATH            = 0x0043
```

---

## GatewayApp 边缘接入层

这一节描述的是 Apollo 在普通 MMO 或边缘治理场景下的可选装配。

如果采用分布式世界默认主链：

```text
Client -> LoginApp -> BaseApp(Proxy + PlayerAnchor) -> CellApp
```

则这里的 `GatewayApp` 可以不存在。

### 职责

1. **客户端连接管理**
   - 接受客户端 TCP/WebSocket 连接
   - 维护连接状态
   - 处理连接断开

2. **会话管理**
```cpp
struct SessionConfig {
    int sessionTimeoutMs = 30000;       // 30秒无心跳断开
    int reconnectWindowMs = 30000;     // 掉线重连窗口
};
```

3. **消息路由**
   - 根据消息类型转发到后端服务
   - CellApp → 游戏逻辑消息
   - BaseApp → 在线主链与路由消息
   - ChatApp → 聊天消息

4. **心跳检测**
```cpp
int heartbeatIntervalMs = 5000;    // 客户端心跳间隔
int heartbeatCheckIntervalMs = 1000; // 服务端检查间隔
```

### 使用示例

```bash
# 启动边缘接入层
./gateway-app --port 8888 --max-connections 10000
```

### 目录结构

```
apps/gateway-app/
├── CMakeLists.txt
├── include/gateway/
│   ├── config.hpp          # 配置定义
│   ├── session_manager.hpp # 会话管理器
│   └── gateway_server.hpp  # 服务器主类
└── src/
    ├── main.cpp
    └── gateway_server.cpp
```

---

## LoginApp 登录服务器

### 职责

1. **账号认证**
```cpp
struct Authenticator {
    bool authenticate(const std::string& username,
                     const std::string& password,
                     PlayerID& outPlayerId,
                     std::string& errorMessage);
};
```

2. **入口分配**
```cpp
class EntryAllocator {
    // 普通 MMO 返回 Gateway 入口
    // 分布式世界返回 BaseApp Proxy 入口
    std::string selectEntry();
};
```

这里要注意：

- `LoginApp` 只负责分配入口
- 它不直接承接 `Proxy` 创建
- 它也不直接承接 `CellApp` 实时分配

3. **会话管理**
```cpp
class SessionManager {
    // 创建会话
    SessionID createSession(PlayerID playerId, const std::string& entryUrl);

    // 验证会话
    bool validateSession(SessionID sessionId, PlayerID& outPlayerId);
};
```

### 登录流程

这里的“入口”有两种可能：

- 普通 MMO：返回 `GatewayApp`
- 分布式世界：返回 `BaseApp Proxy`

```
Client                    LoginApp                BaseApp
  │                          │                       │
  │ 1. 登录请求(username, password) │                       │
  ├─────────────────────────►│                       │
  │                          │ 2. 验证账号密码         │                       │
  │                          ├──────────────────────►│                       │
  │                          │                       │                       │
  │                          │ 3. 选择接入入口         │                       │
  │                          │ ┌─────────────────┐   │                       │
  │                          │ │ Gateway1: 100人 │   │                       │
  │                          │ │ BaseApp2:  50人 │   │                       │
  │                          │ └─────────────────┘   │                       │
  │                          │                       │                       │
  │ 4. 返回登录成功            │                       │
  │◄─────────────────────────┤                       │
  │  - sessionId              │                       │
  │  - playerId               │                       │
  │  - entryHost/port          │                       │
```

### 使用示例

```bash
# 启动登录服务器
./login-app --port 9001 --entry-mode gateway
```

如果是分布式世界模式，则更接近：

```bash
./login-app --port 9001 --entry-mode baseapp-proxy
```

---

## BaseApp 玩家锚点宿主

### 职责

1. **玩家锚点管理**
```cpp
class PlayerAnchorService {
    bool activatePlayer(PlayerID playerId, SessionID sessionId);
    bool bindClientEntry(PlayerID playerId, const std::string& entryId);
    bool assignRuntimeTarget(PlayerID playerId, WorldAssignment& outAssignment);
};
```

这里的 `WorldAssignment` 在普通 MMO 下可以表示：

- `WorldApp`
- `Instance`
- `Zone`

在分布式世界模式下则更接近：

- `CellApp`
- `Space`
- `Partition`

2. **重连恢复与会话归属**
```cpp
struct BaseConfig {
    bool enableReconnect = true;
    int reconnectWindowMs = 60000;
};
```

3. **持久化协调**
```cpp
class PersistenceCoordinator {
    void flushPlayer(PlayerID playerId);
};
```

### 玩家数据结构

```cpp
struct PlayerData {
    PlayerID playerId;
    std::string username;
    int level = 1;
    int64_t exp = 0;
    int hp = 100;
    int maxHp = 100;
    float x = 0, y = 0, z = 0;

    // JSON 序列化
    std::string toJson() const;
    static PlayerData fromJson(const std::string& json);
};
```

### 使用示例

```bash
# 启动 BaseApp 玩家锚点宿主
./base-app --port 9002
```

---

## CellApp 游戏逻辑服务器

这节在分布式世界模式下直接对应 `CellApp`。

如果是普通 MMO 模式，等价位置更接近：

- `WorldApp`
- 或单机版 world runtime

### 职责

1. **实体管理**
```cpp
class EntityManager {
    Entity* createEntity(EntityID id, EntityType type);
    void destroyEntity(EntityID id);
    Entity* getEntity(EntityID id);
    void update(float dt);
};
```

2. **AOI 九宫格系统**
```cpp
class AOIManager {
    void enter(Entity* entity);
    void move(Entity* entity, const Position& newPos);
    void leave(Entity* entity);
    std::vector<Entity*> getViewers(Entity* entity);
};
```

3. **游戏循环**
```cpp
struct CellConfig {
    float spaceWidth = 1000.0f;
    float spaceHeight = 1000.0f;
    float gridSize = 100.0f;
    float viewRadius = 200.0f;
    int tickRateMs = 50;  // 20 ticks per second
};
```

### 实体类型

```cpp
enum class EntityType : uint8_t {
    UNKNOWN = 0,
    PLAYER = 1,
    NPC = 2,
    MONSTER = 3,
    PET = 4,
};
```

### 使用示例

```bash
# 启动游戏逻辑服务器
./cell-app --port 9100 --size 2000 2000 --tick-rate 50
```

---

## 部署方式

### 单机部署（普通 MMO）

```
┌─────────────────────────────────────────────┐
│           Standard MMO 开发环境                  │
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

### 单机部署（分布式世界）

```
┌─────────────────────────────────────────────┐
│        Distributed World 开发环境           │
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

### 分布式部署（普通 MMO）

```
┌─────────────────────────────────────────────────────────────────┐
│                        负载均衡器                              │
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
        │              共享后端服务集群               │
        ├─────────────┬───────────┬───────────┬───────┐
        │             │           │           │       │
        ▼             ▼           ▼           ▼       ▼
  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐
  │LoginApp │  │ BaseApp │  │WorldApp0│  │WorldApp1│
  │ (集群)  │  │ (集群)  │  │(主城)   │  │(野外)   │
  └─────────┘  └─────────┘  └─────────┘  └─────────┘
```

### 分布式部署（分布式世界）

```
┌──────────────────────────────────────────────────────────────┐
│                        Login 集群                           │
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
          │ 主城区   │  │ 野外区   │  │ 副本区   │
          └──────────┘  └──────────┘  └──────────┘
                               │
                        ┌──────▼──────┐
                        │ DBMgr / 存储 │
                        └─────────────┘
```

### Docker Compose 部署（普通 MMO）

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

### Docker Compose 部署（分布式世界）

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

## 运行指南

### 构建项目

```bash
# 安装 vcpkg 依赖
vcpkg install nng:x64-windows nng:x64-linux

# 配置构建
cmake -B build -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake

# 编译
cmake --build build
```

### 启动服务器

#### 普通 MMO

```bash
# 1. 启动 BaseApp (玩家锚点宿主)
./build/base-app/base-app --port 9002

# 2. 启动 LoginApp (登录服务)
./build/login-app/login-app --port 9001

# 3. 启动 WorldApp (世界运行时)
./build/world-app/world-app --port 9100 --size 2000 2000

# 4. 启动 GatewayApp (边缘接入层)
./build/gateway-app/gateway-app --port 8888
```

#### 分布式世界

```bash
# 1. 启动 BaseApp (PlayerAnchor + Proxy 宿主)
./build/base-app/base-app --port 9002

# 2. 启动 LoginApp (登录服务)
./build/login-app/login-app --port 9001

# 3. 启动 CellApp (游戏逻辑服务)
./build/cell-app/cell-app --port 9100 --size 2000 2000
```

### 启动顺序

#### 普通 MMO

**重要**: 服务器必须按以下顺序启动：

1. **BaseApp** - 在线主状态宿主，通常需要最先启动
2. **LoginApp** - 依赖 BaseApp
3. **WorldApp** - 可以独立启动，也可以在 BaseApp 之后
4. **GatewayApp** - 依赖所有后端服务

#### 分布式世界

1. **BaseApp** - `PlayerAnchor + Proxy` 宿主，通常需要最先启动
2. **LoginApp** - 依赖 BaseApp 进行入口分配
3. **CellApp** - 依赖 BaseApp 完成玩家激活和空间分配

### 关闭顺序

#### 普通 MMO

关闭顺序与启动相反：

1. **GatewayApp** - 停止接受新连接
2. **WorldApp** - 保存所有实体状态
3. **LoginApp** - 完成现有会话
4. **BaseApp** - 最后关闭，确保在线状态和持久化协调完成

#### 分布式世界

1. **LoginApp** - 停止发放新票据
2. **CellApp** - 保存空间内实体状态，停止新的 authority 迁移
3. **BaseApp** - 最后关闭，确保在线状态解绑和持久化协调完成

---

## 总结

### Standard MMO

| 服务器 | 端口 | 依赖 | 启动优先级 |
|--------|------|------|------------|
| GatewayApp | 8888 | LoginApp, BaseApp, WorldApp | 4 |
| LoginApp | 9001 | BaseApp | 2 |
| BaseApp | 9002 | 无 | 1 |
| WorldApp | 9100+ | 无 | 3 |

### Distributed World

| 服务器 | 端口 | 依赖 | 启动优先级 |
|--------|------|------|------------|
| LoginApp | 9001 | BaseApp | 2 |
| BaseApp | 9002 | CellApp, DBMgr | 1 |
| CellApp | 9100+ | BaseApp | 3 |

总原则只有三条：

- 普通 MMO 默认使用独立 `GatewayApp`
- 分布式世界默认改为 `BaseApp(Proxy + PlayerAnchor) -> CellApp`
- `BaseApp` 始终不是数据库服务器

## 相关文档

- [BigWorld架构深度解析](/architecture/bigworld.md)
- [BigWorld进程架构与玩家生命周期](/architecture/bigworld-lifecycle.md)
- [AOI九宫格系统详解](/architecture/aoi.md)
- [Protocol 模块 API](/modules/)
