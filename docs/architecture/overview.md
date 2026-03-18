---
title: 架构概述
icon: architecture
order: 1
category:
  - 架构
tag:
  - 架构
  - 概述
---

# Apollo 架构概述

## 整体架构

Apollo 采用分层模块化架构，共分为 10 层：

```
┌─────────────────────────────────────────────────────────────────┐
│                        Applications (apps/)                      │
│  gateway-server | login-server | game-server | chat-server     │
└─────────────────────────────────────────────────────────────────┘
                                ↓
┌─────────────────────────────────────────────────────────────────┐
│                    Starters (modules/starter/)                  │
│  core | net | mysql | redis | game-server | ...                │
└─────────────────────────────────────────────────────────────────┘
                                ↓
┌─────────────────────────────────────────────────────────────────┐
│                    BigWorld Compatibility                       │
│                  modules/bigworld/                              │
│  Runtime | Entity | Timer | Callback                            │
└─────────────────────────────────────────────────────────────────┘
                                ↓
┌──────┬──────┬──────┬──────┬──────────┬──────────┬────────────┐
│  net │ data │ game │ actor│ framework│  runtime │   core     │
└──────┴──────┴──────┴──────┴──────────┴──────────┴────────────┘
                                ↓
┌─────────────────────────────────────────────────────────────────┐
│                        Base (modules/base/)                     │
│  Time | ThreadPool | IdPool | Memory | Terminal | String       │
└─────────────────────────────────────────────────────────────────┘
```

## 层级说明

### Level 1: Base (`modules/base/`)

纯基础设施，无框架语义。

| 组件 | 功能 |
|------|------|
| Time | 时间工具、高精度计时 |
| ThreadPool | 线程池、任务调度 |
| IdPool | 唯一ID生成器 |
| ObjectPool | 对象池、内存复用 |
| String | 字符串处理工具 |
| Memory | 内存池、Buffer管理 |
| Terminal | 终端适配、ANSI彩色输出 |

### Level 2: Core (`modules/core/`)

框架内核，公共运行语义。

| 组件 | 功能 |
|------|------|
| DI/IoC | 依赖注入容器 |
| Config | 配置管理、热更新 |
| Log | 日志系统、分级输出 |
| Lifecycle | 应用生命周期管理 |
| Module | 模块注册与加载 |
| Event | 事件总线 |
| Timer | 定时器调度 |
| ServiceDiscovery | 服务发现 |

### Level 3: Runtime (`modules/runtime/`)

宿主运行时，程序如何运行。

| 组件 | 功能 |
|------|------|
| ApplicationHost | 应用宿主 |
| ServiceHost | 服务宿主 |
| Console | 控制台输入监听 |
| Signal | 信号处理 (SIGINT/SIGTERM) |
| ShutdownHook | 关闭钩子 |
| HealthCheck | 健康检查 |

### Level 4: Data (`modules/data/`)

数据访问层。

| 组件 | 功能 |
|------|------|
| core | 数据访问接口 |
| orm | ORM映射、数据库操作 |
| redis | Redis客户端 |
| cache | 缓存抽象 |

### Level 5: Network (`modules/net/`)

网络能力层。

| 组件 | 功能 |
|------|------|
| core | 网络抽象接口 |
| tcp | TCP通信 |
| http | HTTP服务 |
| websocket | WebSocket支持 |
| rpc | RPC框架 |

### Level 6: Actor (`modules/actor/`)

Actor 并发模型。

| 组件 | 功能 |
|------|------|
| core | Actor实体 |
| timer | 定时器服务 |
| messaging | 消息总线 |

### Level 7: Game (`modules/game/`)

游戏领域层。

| 组件 | 功能 |
|------|------|
| core | Entity基类、组件接口 |
| world | AOI、场景管理 |
| battle | 战斗系统、ECS |
| attributes | 属性系统 |

### Level 8: BigWorld (`modules/bigworld/`)

BigWorld API 兼容层。

### Level 9: Starter (`modules/starter/`)

装配层，按需组装模块。

### Level 10: Apps (`apps/`)

最终可执行程序。

## 依赖关系

```
base ← core ← runtime ← net/data/actor/game ← bigworld ← starter ← apps
```

**硬约束**：
- base 不依赖任何 Apollo 模块
- core 不依赖 game/net/data
- game 不反向进入 core
- bigworld 作为 game 的顶层兼容层
