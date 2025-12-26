# Apollo Actor 框架 - 完整指南

## 概述

Apollo Actor 框架是一个生产级的 C++ Actor 模型实现，专为游戏服务器和分布式系统设计。

### 核心特性

- **位置透明**：本地/远程 Actor 统一访问
- **本地优先通信**：自动选择 SHM → Unix Socket → TCP
- **多格式序列化**：FlatBuffers（默认）/ JSON / Protobuf / Binary
- **灵活调度**：轮询 / 最少负载 / 随机 / 固定线程
- **完整可观测性**：HTTP API / Prometheus / 健康检查 / 日志
- **线程池管理**：调度器 / IO / 后台任务分离
- **定时器服务**：单次 / 重复 / 固定速率 / 倒计时
- **服务发现集成**：SQLite / Redis / 内存模式

## 架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        应用层 (Actor)                              │
│  PlayerActor, NPCActor, ServiceActor, GatewayActor...             │
└─────────────────────────────────────────────────────────────────┘
                              ▲
                              │
┌─────────────────────────────────────────────────────────────────┐
│                      Actor 系统                                   │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │ ActorSystem  │  │ ActorManager │  │  TimerService│          │
│  │  (系统入口)   │  │  (Actor管理)  │  │  (定时器)     │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
├─────────────────────────────────────────────────────────────────┤
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │ActorRegistry │  │ MessageDisp. │  │  MessageBus  │          │
│  │  (注册表)     │  │  (调度器)     │  │  (消息总线)   │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
└─────────────────────────────────────────────────────────────────┘
                              ▲
                              │
┌─────────────────────────────────────────────────────────────────┐
│                      可观测性层                                   │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐        │
│  │ HTTP API │  │Prometheus│  │   Health │  │  Metrics │        │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘        │
└─────────────────────────────────────────────────────────────────┘
                              ▲
                              │
┌─────────────────────────────────────────────────────────────────┐
│                      基础设施层                                   │
│  Channel, ServiceDiscovery, Config, Serialization                │
└─────────────────────────────────────────────────────────────────┘
```

## 文件结构

```
include/apollo/actor/
├── message.h              # 消息序列化接口
├── actor.h                # Actor 基类
├── actor_ref.h            # Actor 引用和路径
├── actor_system.h         # Actor 系统主接口
├── actor_cell.h           # Actor 执行单元
├── actor_manager.h        # Actor 管理器和线程池
├── dispatcher.h           # 消息调度器
├── message_bus.h          # 消息总线
├── timer_service.h        # 定时器服务
├── monitoring.h           # 监控和可观测性
└── actor_utils.h          # 工具函数

src/apollo/actor/
├── message_serialization.cpp
├── actor_ref.cpp
├── actor_cell.cpp
├── actor_system.cpp
├── actor_manager.cpp
├── actor_utils.cpp
├── dispatcher.cpp
├── message_bus.cpp
├── timer_service.cpp
└── monitoring.cpp

examples/
├── actor_demo.cpp          # 基础示例
└── actor_advanced_demo.cpp # 高级示例
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

### 3. 配置系统

```cpp
// Actor 系统配置
ActorSystemConfig systemConfig;
systemConfig.systemName = "game-server";
systemConfig.address = "127.0.0.1";

// 线程池配置
ThreadPoolConfig threadConfig;
threadConfig.dispatcherThreads = 4;
threadConfig.ioThreads = 2;
threadConfig.dispatchStrategy = DispatchStrategy::RoundRobin;

// 可观测性配置
ObservabilityConfig obsConfig;
obsConfig.enableHttpApi = true;
obsConfig.httpApi.port = 8080;
obsConfig.enablePrometheus = true;
```

### 4. 启动系统

```cpp
ActorSystem system(systemConfig);
auto manager = std::make_unique<ActorManager>(threadConfig);

manager->start(system);
system.start();

Observability observability(*manager, obsConfig);
observability.start();
```

### 5. 创建 Actor

```cpp
auto chatRoom = system.spawn<ChatRoom>("chat-room");
auto userService = system.spawn<UserService>("user-service");
```

### 6. 发送消息

```cpp
// Fire-and-forget
chatRoom.tell(message);

// Request-Response
auto response = chatRoom.ask<Request, Response>(request, 5000);
```

## 线程池配置

```
┌─────────────────────────────────────────────────────────────┐
│                      Actor 系统线程池                         │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌────────────────┐  ┌────────────────┐  ┌──────────────┐  │
│  │ Dispatcher     │  │ IO Pool        │  │ Background   │  │
│  │ Threads        │  │                │  │ Pool         │  │
│  │ (处理消息)      │  │ (网络/数据库)   │  │ (后台任务)    │  │
│  │                │  │                │  │              │  │
│  │ 配置:           │  │ 配置:           │  │ 配置:         │  │
│  │ - 调度策略      │  │ - 线程数       │  │ - 线程数      │  │
│  │ - 线程数       │  │ - 队列大小      │  │ - 队列大小    │  │
│  └────────────────┘  └────────────────┘  └──────────────┘  │
│         │                    │                    │           │
│         └────────────────────┴────────────────────┘           │
│                              │                               │
│                    ┌─────────▼─────────┐                      │
│                    │   ActorManager    │                      │
│                    │  (统一管理)        │                      │
│                    └───────────────────┘                      │
└─────────────────────────────────────────────────────────────┘
```

## 调度策略

| 策略 | 说明 | 适用场景 |
|------|------|----------|
| RoundRobin | 轮询分配 | 默认，负载均衡 |
| LeastLoaded | 最少负载 | 不均匀负载 |
| Random | 随机分配 | 高并发 |
| Pinned | 固定线程 | 需要线程局部性 |

## 可观测性

### HTTP API

```
GET /api/actors          # 列出所有 Actor
GET /api/actors/{name}   # 获取 Actor 详情
GET /api/actors/stats    # 获取统计信息
GET /api/health          # 健康检查
```

### Prometheus 指标

```
actor_total              # Actor 总数
actor_running            # 运行中的 Actor
actor_messages_total     # 消息总数
thread_pool_active       # 活跃线程数
queue_size               # 队列大小
```

### 健康检查

```cpp
HealthCheck check(manager);
auto result = check.check();

if (result.healthy) {
    // 系统健康
} else {
    // 处理不健康状态
}
```

## 定时器

```cpp
// 单次定时器
uint64_t id = timer::scheduleOnce(1000, []() {
    // 1 秒后执行
});

// 重复定时器
uint64_t id = timer::scheduleRepeated(1000, []() {
    // 每 1 秒执行一次
});

// 倒计时
timer::countdown(10000,
    [](int64_t remaining) {
        // 每秒回调
    },
    []() {
        // 完成回调
    }
);

// 取消定时器
timer::cancel(id);
```

## 远程 Actor

```cpp
// 查找远程 Actor
auto remoteActor = system.lookup("game-server://remote-host/service")
    .get();

// 发送消息（自动选择最优传输）
remoteActor.tell(message);

// 传输优先级: SHM > Unix Socket > Localhost TCP > Remote TCP
```

## API 参考

### ActorSystem

| 方法 | 说明 |
|------|------|
| `spawn<T>(name, ...)` | 创建 Actor |
| `lookup(path)` | 查找 Actor |
| `shutdown()` | 关闭系统 |

### ActorManager

| 方法 | 说明 |
|------|------|
| `registerActor(cell)` | 注册 Actor |
| `stopActor(name)` | 停止 Actor |
| `suspendActor(name)` | 暂停 Actor |
| `resumeActor(name)` | 恢复 Actor |
| `submitIoTask(task)` | 提交 IO 任务 |
| `getStats()` | 获取统计 |

### ActorRef

| 方法 | 说明 |
|------|------|
| `tell(msg)` | 发送消息 |
| `ask<T>(msg, timeout)` | 请求-响应 |

### Actor

| 方法 | 说明 |
|------|------|
| `receive(msg)` | 处理消息 |
| `onStart()` | 启动回调 |
| `onStop()` | 停止回调 |
| `self()` | 自身引用 |
| `context()` | 运行时上下文 |

## 编译

```bash
# 配置 CMake
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 编译
cmake --build build

# 运行示例
./build/examples/actor_demo
./build/examples/actor_advanced_demo
```

## 依赖

- C++20 或更高
- fmt (可选，用于日志)
- nlohmann/json (可选，用于 JSON 序列化)
- FlatBuffers (可选，用于高性能序列化)
