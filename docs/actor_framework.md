# Apollo Actor 框架

## 概述

Apollo Actor 框架是基于 C++20 实现的轻量级、高性能 Actor 模型框架，专为游戏服务器和分布式系统设计。

### 核心特性

- **位置透明**：本地/远程 Actor 统一访问方式
- **本地优先**：自动选择最优传输方式（SHM → Unix Socket → TCP）
- **零拷贝**：本地通信使用共享内存，避免序列化开销
- **多格式支持**：FlatBuffers（默认）/ JSON / Protobuf / Binary
- **服务发现**：集成 SQLite/Redis 注册中心
- **背压控制**：基于 Mailbox 的流控机制

## 架构

```
┌─────────────────────────────────────────────────────────────┐
│                     应用层 (Actor)                            │
│  PlayerActor, NPCActor, ServiceActor...                      │
└─────────────────────────────────────────────────────────────┘
                              ▲
                              │
┌─────────────────────────────────────────────────────────────┐
│                  Actor 系统层                                 │
│  ActorSystem, ActorRef, ActorCell, Mailbox                   │
└─────────────────────────────────────────────────────────────┘
                              ▲
                              │
┌─────────────────────────────────────────────────────────────┐
│                  消息传输层                                   │
│  MessageBus, TransportChannel, Message                       │
└─────────────────────────────────────────────────────────────┘
                              ▲
                              │
┌─────────────────────────────────────────────────────────────┐
│                  基础设施层                                   │
│  Channel, ServiceDiscovery, Config                          │
└─────────────────────────────────────────────────────────────┘
```

## 文件结构

```
include/apollo/actor/
├── message.h          # 消息定义和序列化接口
├── actor_ref.h        # Actor 引用和路径
├── actor.h            # Actor 基类和 ActorContext
├── actor_system.h     # Actor 系统主接口
├── actor_cell.h       # Actor 执行单元和 Mailbox
├── actor_manager.h    # Actor 管理器和线程池
├── dispatcher.h       # 消息调度器
├── message_bus.h      # 消息总线（封装 Channel）
├── timer_service.h    # 定时器服务
├── monitoring.h       # 监控和可观测性
└── actor_utils.h      # 工具函数

src/apollo/actor/
├── message_serialization.cpp  # 消息序列化实现
├── actor_ref.cpp              # ActorRef 实现
├── actor_cell.cpp             # Actor 执行单元实现
├── actor_system.cpp           # Actor 系统实现
├── actor_manager.cpp          # Actor 管理器实现
├── actor_utils.cpp            # 工具函数实现
├── dispatcher.cpp             # 消息调度器实现
├── message_bus.cpp            # 消息总线实现
├── timer_service.cpp          # 定时器服务实现
└── monitoring.cpp             # 监控实现
```

## 快速开始

### 1. 定义消息

```cpp
struct ChatMessage {
    std::string content;

    static const char* typeName() { return "ChatMessage"; }
    static std::vector<uint8_t> serialize(const ChatMessage& msg);
    static ChatMessage deserialize(const std::vector<uint8_t>& data);
};
```

### 2. 定义 Actor

```cpp
class ChatRoom : public Actor {
protected:
    void onStart() override {
        std::cout << "ChatRoom started" << std::endl;
    }

    void receive(const Message& msg) override {
        if (msg.is<ChatMessage>()) {
            auto chat = msg.as<ChatMessage>();
            // 处理消息
        }
    }
};
```

### 3. 创建 Actor 系统

```cpp
ActorSystemConfig config;
config.systemName = "game-server";
config.address = "127.0.0.1";

ActorSystem system(config);
system.start();
```

### 4. 创建 Actor

```cpp
auto chatRoom = system.spawn<ChatRoom>("chat-room");
```

### 5. 发送消息

```cpp
ChatMessage msg;
msg.content = "Hello!";

// Fire-and-forget
chatRoom.tell(msg);

// Request-Response
auto response = chatRoom.ask<ChatMessage, Response>(msg, 5000);
```

## 消息序列化

### 支持的格式

| 格式 | 用途 | 性能 | 可读性 |
|------|------|------|--------|
| FlatBuffers | 默认，高性能 | ⭐⭐⭐⭐⭐ | ❌ |
| JSON | 调试，跨语言 | ⭐⭐ | ✅ |
| Binary | POD 类型 | ⭐⭐⭐⭐ | ❌ |
| Protobuf | 外部兼容 | ⭐⭐⭐⭐ | ❌ |

### 使用方式

```cpp
// 默认使用 FlatBuffers
Message msg(chatMessage, MessageFormat::FlatBuffers);

// 使用 JSON（调试）
Message msg(chatMessage, MessageFormat::JSON);

// POD 类型自动使用 Binary
Message msg(podMessage, MessageFormat::Auto);
```

## 传输优先级

本地通信自动选择最优传输方式：

1. **共享内存**（同进程）
2. **Unix Socket**（同机器）
3. **TCP**（跨机器/本地回环）

```cpp
// 服务端点自动配置多种传输方式
endpoint.transportMask = TransportMask::SharedMemory |
                       TransportMask::UnixSocket |
                       TransportMask::Tcp;

// 客户端自动选择最佳方式
auto address = endpoint.getBestAddress("127.0.0.1");
```

## 服务发现集成

### 配置

```cpp
// 使用 SQLite（本地/测试）
config.discoveryConfig.backend = DiscoveryBackend::SQLite;
config.discoveryConfig.dbPath = ":memory:";

// 使用 Redis（分布式）
config.discoveryConfig.backend = DiscoveryBackend::Redis;
config.discoveryConfig.redisHost = "localhost";
config.discoveryConfig.redisPort = 6379;
```

### Actor 自动注册

Actor 启动时自动注册到服务发现：

```cpp
void onStart() override {
    Actor::onStart();  // 自动注册
}
```

### 查找远程 Actor

```cpp
auto remoteActor = system.lookup("game-server://remote-host/user-service")
    .get();
```

## API 参考

### ActorSystem

| 方法 | 说明 |
|------|------|
| `spawn<T>(name, ...)` | 创建 Actor |
| `lookup(path)` | 查找 Actor |
| `shutdown()` | 关闭系统 |

### ActorRef

| 方法 | 说明 |
|------|------|
| `tell(msg)` | 发送消息（异步） |
| `ask<T>(msg, timeout)` | 请求-响应 |
| `path()` | 获取路径 |

### Actor

| 方法 | 说明 |
|------|------|
| `onStart()` | 启动回调 |
| `onStop()` | 停止回调 |
| `receive(msg)` | 消息处理 |
| `self()` | 自身引用 |
| `context()` | 运行时上下文 |

## 示例

完整示例请参考 `examples/actor_demo.cpp`。

编译：
```bash
g++ -std=c++20 -I../include actor_demo.cpp -lapollo -lpthread -o actor_demo
```
