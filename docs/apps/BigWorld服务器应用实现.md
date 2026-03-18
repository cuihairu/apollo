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

本文档描述 Apollo 中 BigWorld 架构的具体服务器应用实现。

---

## 目录

- [架构概览](#架构概览)
- [通信协议](#通信协议)
- [GatewayApp 网关服务器](#gatewayapp-网关服务器)
- [LoginApp 登录服务器](#loginapp-登录服务器)
- [BaseApp 数据库服务器](#baseapp-数据库服务器)
- [CellApp 游戏逻辑服务器](#cellapp-游戏逻辑服务器)
- [部署方式](#部署方式)

---

## 架构概览

### 服务器拓扑

```
                    ┌─────────────────────────────────────────┐
                    │                  Client                  │
                    │              (Unity/Unreal)              │
                    └────────────────────┬────────────────┘
                                         │ TCP/WebSocket
                                         ▼
┌─────────────────────────────────────────────────────────────────┐
│                        GatewayApp (8888)                          │
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
│ - 认证       │  │ - 数据库  │  │ - AOI   │  │              │
│ - 网关分配   │  │ - 缓存    │  │ - 实体  │  │              │
└──────────────┘  └──────────┘  └─────────┘  └──────────────┘
```

### 进程职责

| 服务器 | 端口 | 职责 |
|--------|------|------|
| **GatewayApp** | 8888 | 客户端连接管理、消息路由、会话管理、心跳检测 |
| **LoginApp** | 9001 | 账号认证、网关分配、会话令牌生成 |
| **BaseApp** | 9002 | 数据CRUD、缓存、异步保存、玩家数据加载 |
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

// 网关相关
GATEWAY_ASSIGN_REQUEST  = 0x0010
GATEWAY_ASSIGN_RESPONSE = 0x0011
GATEWAY_HEARTBEAT       = 0x0012

// 数据库相关
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

## GatewayApp 网关服务器

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
   - BaseApp → 数据存取消息
   - ChatApp → 聊天消息

4. **心跳检测**
```cpp
int heartbeatIntervalMs = 5000;    // 客户端心跳间隔
int heartbeatCheckIntervalMs = 1000; // 服务端检查间隔
```

### 使用示例

```bash
# 启动网关服务器
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

2. **网关分配**
```cpp
class GatewayAllocator {
    // 选择最优网关 (负载最低)
    std::string selectGateway();
};
```

3. **会话管理**
```cpp
class SessionManager {
    // 创建会话
    SessionID createSession(PlayerID playerId, const std::string& gatewayUrl);

    // 验证会话
    bool validateSession(SessionID sessionId, PlayerID& outPlayerId);
};
```

### 登录流程

```
Client                    LoginApp                BaseApp
  │                          │                       │
  │ 1. 登录请求(username, password) │                       │
  ├─────────────────────────►│                       │
  │                          │ 2. 验证账号密码         │                       │
  │                          ├──────────────────────►│                       │
  │                          │                       │                       │
  │                          │ 3. 选择最优网关         │                       │
  │                          │ ┌─────────────────┐   │                       │
  │                          │ │ Gateway1: 100人 │   │                       │
  │                          │ │ Gateway2:  50人 │   │                       │
  │                          │ └─────────────────┘   │                       │
  │                          │                       │                       │
  │ 4. 返回登录成功            │                       │
  │◄─────────────────────────┤                       │
  │  - sessionId              │                       │
  │  - playerId               │                       │
  │  - gatewayHost/port        │                       │
```

### 使用示例

```bash
# 启动登录服务器
./login-app --port 9001 --gateway tcp://127.0.0.1:8888
```

---

## BaseApp 数据库服务器

### 职责

1. **数据CRUD**
```cpp
class DatabaseService {
    // 加载玩家数据
    bool loadPlayer(PlayerID playerId, PlayerData& outData);

    // 保存玩家数据
    bool savePlayer(const PlayerData& data);

    // 异步保存
    void savePlayerAsync(const PlayerData& data, std::function<void(bool)> callback);
};
```

2. **缓存管理**
```cpp
struct BaseConfig {
    bool enableCache = true;
    int cacheTimeoutMs = 60000;  // 1分钟
};
```

3. **异步保存队列**
```cpp
class SaveQueue {
    // 工作线程处理保存任务
    void enqueue(const SaveTask& task);
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
# 启动数据库服务器
./base-app --port 9002 --db apollo
```

---

## CellApp 游戏逻辑服务器

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

### 单机部署（开发）

```
┌─────────────────────────────────────────────┐
│              单机开发环境                        │
│                                                     │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐        │
│  │GatewayApp│ │LoginApp  │ │BaseApp  │        │
│  │ :8888    │ │ :9001    │ │ :9002    │        │
│  └──────────┘ └──────────┘ └──────────┘        │
│                                                     │
│  ┌──────────┐ ┌──────────┐                      │
│  │CellApp 0 │ │CellApp 1 │ ...                   │
│  │ :9100    │ │ :9101    │                      │
│  └──────────┘ └──────────┘                      │
└─────────────────────────────────────────────┘
```

### 分布式部署（生产）

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
  │LoginApp │  │ BaseApp │  │CellApp 0│  │CellApp1 │
  │ (集群)  │  │ (主从)  │  │(主城)  │  │(野外)  │
  └─────────┘  └─────────┘  └─────────┘  └─────────┘
```

### Docker Compose 部署

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
      - CELL_APP_URL=tcp://cell-app:9100
    depends_on:
      - login-app
      - base-app
      - cell-app

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

```bash
# 1. 启动 BaseApp (数据库服务)
./build/base-app/base-app --port 9002

# 2. 启动 LoginApp (登录服务)
./build/login-app/login-app --port 9001

# 3. 启动 CellApp (游戏逻辑服务)
./build/cell-app/cell-app --port 9100 --size 2000 2000

# 4. 启动 GatewayApp (网关服务)
./build/gateway-app/gateway-app --port 8888
```

### 启动顺序

**重要**: 服务器必须按以下顺序启动：

1. **BaseApp** - 数据库服务需要先启动
2. **LoginApp** - 依赖 BaseApp
3. **CellApp** - 可以独立启动，也可以在 BaseApp 之后
4. **GatewayApp** - 依赖所有后端服务

### 关闭顺序

关闭顺序与启动相反：

1. **GatewayApp** - 停止接受新连接
2. **CellApp** - 保存所有实体状态
3. **LoginApp** - 完成现有会话
4. **BaseApp** - 最后关闭，确保数据已保存

---

## 总结

| 服务器 | 端口 | 依赖 | 启动优先级 |
|--------|------|------|------------|
| GatewayApp | 8888 | LoginApp, BaseApp, CellApp | 4 |
| LoginApp | 9001 | BaseApp | 2 |
| BaseApp | 9002 | 无 | 1 |
| CellApp | 9100+ | 无 | 3 |

## 相关文档

- [BigWorld架构深度解析](/architecture/bigworld.md)
- [BigWorld进程架构与玩家生命周期](/architecture/bigworld-lifecycle.md)
- [AOI九宫格系统详解](/architecture/aoi.md)
- [Protocol 模块 API](/modules/)
