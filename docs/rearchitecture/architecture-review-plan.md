> 来源：用户 2026-10-04 经 ChatGPT 整理的任务书（原链接 https://chatgpt.com/s/t_6ac1cd99d1a88191b7f1cbcdcc79c657 ）。任务以此为准；与 docs/design/term-contract.md 术语冲突处按契约修文对齐并列差异清单。

Apollo 架构审查与改进计划

## 1. 项目重新定位

Apollo 的目标是：

> **一个面向轻量 MMO、场景化多人在线游戏和实例化多人游戏的服务器框架。**

Apollo 主要解决：

- 持久在线玩家
- 场景（Scene）
- 实例（Instance）
- AOI
- 战斗
- 玩家状态
- 场景切换
- 社交
- 公会
- 副本
- 竞技场
- 塔防
- 挂机 / 放置类在线游戏
- 轻量 MMORPG
- 数据持久化
- 断线重连
- 服务恢复

Apollo 不应该追求“所有 MMO 能力都具备”，而应该追求：

> **用相对简单的架构解决大量非无缝大世界在线游戏的问题。**

---

# 2. 本次任务的核心目标

本次不是简单增加功能，而是对 Apollo 进行一次完整的架构审查。

需要回答：

1. Apollo 当前架构是否符合“轻量 MMO”定位？
2. 当前核心对象模型是否合理？
3. Player 应该在哪里、由谁管理？
4. Scene 和 Instance 是否真正成为核心概念？
5. AOI 是否设计过度？
6. ECS 是否承担了不应该承担的职责？
7. Battle / Scene / Player 的边界是否清晰？
8. Gateway / World / Scene / Battle 的职责是否合理？
9. Persistence 是否合理？
10. 玩家在线状态是否以内存为权威？
11. Scene 切换是否清晰？
12. Instance 生命周期是否完整？
13. 断线重连和恢复机制是否完整？
14. 当前哪些设计过于复杂？
15. 当前哪些核心能力反而缺失？
16. API 是否足够简单？
17. 文档是否能够准确表达 Apollo 的架构？
18. 是否存在概念重复、职责耦合、过度抽象？
19. 是否存在为了“高性能 / 分布式 / MMO”而提前引入的复杂设计？
20. 最终是否可以用 Apollo 很自然地实现塔防、挂机 MMO、副本 RPG、竞技场、轻量 MMORPG？

---

# 3. 第一阶段：完整代码与文档审查

在修改任何代码之前，先完整分析：

- README
- 官方文档
- architecture 文档
- examples
- source code
- tests
- build system
- runtime
- networking
- scene
- player
- entity
- ECS
- AOI
- battle
- persistence
- storage
- gateway
- service
- scheduler
- lifecycle
- configuration
- deployment

不要根据文件名猜测设计。

必须阅读实际实现。

---

# 4. 输出完整架构地图

首先建立 Apollo 当前架构图。

至少包括：

```text
Client
  ↓
Gateway / Network
  ↓
Session
  ↓
Application / Runtime
  ↓
World
  ↓
Scene
  ↓
Instance
  ↓
Entity / Player / Battle
  ↓
Persistence
```

如果当前实际架构不同，应以实际代码为准。

需要明确每一个模块：

- 创建什么对象
- 持有什么对象
- 调用什么对象
- 生命周期是什么
- 谁拥有对象
- 谁修改对象
- 谁销毁对象
- 是否跨线程
- 是否跨进程
- 是否持久化
- 是否依赖其他模块

---

# 5. 建立“对象所有权”模型

这是本次审查的重点。

对以下对象逐一回答：

```text
World
Player
Entity
Scene
Instance
Battle
Session
Connection
AOI
Guild
Party
```

对于每个对象必须明确：

```text
Owner
Lifecycle
Thread
Runtime Authority
Persistence
Visibility
Transfer
Recovery
```

例如：

```text
Player

Owner:
?

Created:
?

Destroyed:
?

Runtime Authority:
?

Persistence:
?

Current Scene:
?

Connection:
?

Reconnect:
?

Scene Transfer:
?
```

如果这些问题当前无法明确回答，必须列为架构问题。

---

# 6. Player 必须成为明确的一等对象

Apollo 是轻量 MMO 框架，因此 Player 不能只是连接对应的临时对象。

需要明确：

```text
Account
   ↓
Player
   ↓
Runtime Player
```

Player 至少应该拥有：

```text
Identity
Attributes
Inventory
Equipment
Quest
Progress
Social State
Guild State
Runtime State
```

其中：

### Persistent State

例如：

```text
Level
Exp
Currency
Inventory
Equipment
Quest
Achievement
Guild
```

### Runtime State

例如：

```text
Connection
Session
Current Scene
Current Instance
Position
AOI
Combat State
```

必须明确二者的边界。

---

# 7. 明确 Memory Authority

Apollo 必须回答：

> 玩家在线时，谁是玩家状态的最终权威？

推荐模型：

```text
Database
    ↓ Load
Player Runtime
    ↓
Gameplay
    ↓
Memory State
    ↓
Dirty / Journal / Snapshot
    ↓
Database
```

即：

> **在线 Player 的内存对象是游戏运行时权威。**

不要让正常游戏逻辑变成：

```text
Gameplay
 ↓
Database
 ↓
Database Result
```

必须审查当前代码是否违反这一原则。

---

# 8. Scene 必须成为核心运行时边界

Apollo 应该明确 Scene 的职责。

Scene 应该负责：

```text
Players
Entities
AOI
Systems
Tick
Runtime State
```

需要分析当前 Scene：

- 是否是真正的 Runtime Container？
- 是否负责对象生命周期？
- 是否负责 Tick？
- 是否负责 AOI？
- 是否负责玩家进入/离开？
- 是否负责实体管理？
- 是否可以独立运行？
- 是否可以停止？
- 是否可以恢复？

如果 Scene 只是一个数据结构，而真正的运行逻辑散落在其他服务中，应考虑重构。

---

# 9. Instance 必须成为一级概念

Apollo 应该天然支持：

```text
Dungeon Instance
Arena Instance
Tower Instance
Event Instance
Battle Instance
```

需要检查当前代码是否真正支持：

```text
Create
Initialize
Waiting
Running
Finishing
Draining
Destroyed
```

建议统一 Instance 生命周期：

```text
Create
  ↓
Initialize
  ↓
Waiting
  ↓
Running
  ↓
Finishing
  ↓
Rewarding
  ↓
Draining
  ↓
Destroyed
```

如果当前没有统一 Instance 抽象，应作为重点改进项。

---

# 10. Scene 与 Instance 必须区分

必须避免：

```text
Scene == Map == Instance
```

建议明确：

```text
Map
= 静态游戏内容 / 地图定义

Scene
= 服务器运行中的游戏空间

Instance
= 一个独立运行的 Scene 实例
```

例如：

```text
Map: dungeon_001

        ↓

Instance #1001
Instance #1002
Instance #1003
```

需要审查当前代码是否存在这些概念混淆。

---

# 11. Scene Transfer

Apollo 必须支持玩家在场景之间移动。

例如：

```text
Town
  ↓
Dungeon
```

或者：

```text
Arena
  ↓
Town
```

需要定义：

```text
Prepare
 ↓
Admission
 ↓
Detach
 ↓
Transfer
 ↓
Attach
 ↓
State Sync
 ↓
Resume
```

审查当前：

- Player 如何离开 Scene
- Player 如何进入 Scene
- Player 状态如何传递
- Connection 是否改变
- Session 是否改变
- AOI 如何重新建立
- 失败如何回滚
- 断线发生时如何处理

---

# 12. Player 不应该和 Connection 强绑定

需要审查：

```text
Player
Connection
Session
```

三者是否职责混乱。

推荐关系：

```text
Connection
    ↓
Session
    ↓
Player
```

而不是：

```text
Connection
    =
Player
```

这样才能支持：

```text
Disconnect
 ↓
Player remains online temporarily
 ↓
Reconnect
 ↓
Restore Session
```

---

# 13. AOI 重新审查

Apollo 的 AOI 应主要解决：

> Scene 内实体的兴趣管理。

需要分析：

- 当前 AOI 算法
- 数据结构
- 更新方式
- 玩家进入/离开
- Entity 进入/离开
- 移动更新
- 广播
- AOI 与 Scene 的耦合
- AOI 与网络层的耦合

重点避免 AOI 变成一个过度复杂的全局空间系统。

---

# 14. Entity 模型重新审查

明确：

```text
Player
NPC
Monster
Projectile
Drop
Building
Tower
```

这些是否都属于 Entity？

如果属于：

```text
Entity
 ↓
Scene
```

应该明确生命周期。

需要避免：

```text
Entity
 ↓
全局 Manager
 ↓
各种 Service
 ↓
Scene
```

导致对象所有权不清晰。

---

# 15. ECS 重新审查

不要因为 ECS 很流行就默认 Apollo 所有对象都必须 ECS 化。

审查：

- 为什么使用 ECS？
- 哪些对象使用 ECS？
- ECS 是否解决实际问题？
- ECS 是否增加 API 复杂度？
- Player 是否必须 ECS？
- Battle 是否适合 ECS？
- Scene 是否应该拥有 ECS World？

建议重点考虑：

```text
Scene
 └── Battle Runtime
       └── ECS
```

而不是：

```text
Apollo
 └── Everything ECS
```

如果当前设计已经过度 ECS 化，需要简化。

---

# 16. Battle Runtime

战斗应该是 Apollo 的重要能力，但不应该吞掉整个 Runtime。

建议明确：

```text
Scene
 └── BattleRuntime
       ├── Tick
       ├── Systems
       ├── Entities
       └── Components
```

需要分析：

- Battle 是否独立 Tick？
- Battle 是否依赖 Scene？
- Battle 是否能够创建/销毁？
- Battle 是否支持多个 Instance？
- 战斗状态如何保存？
- 战斗结束后如何生成 Reward？
- 战斗异常如何恢复？

---

# 17. Tick / Scheduler

需要完整分析：

```text
World Tick
Scene Tick
Instance Tick
Battle Tick
```

当前是否：

- 单一全局 Tick
- 每 Scene Tick
- 每 Instance Tick
- 多线程 Tick
- 固定 Tick
- 可变 Tick

然后判断是否存在过度复杂的问题。

轻量 MMO 应优先：

> **简单、可预测、容易调试。**

而不是为了极端性能提前设计复杂调度器。

---

# 18. Runtime / Application Lifecycle

审查：

```text
Create
Initialize
Start
Running
Stop
Shutdown
```

是否统一。

所有核心对象是否拥有清晰生命周期。

特别检查：

- Scene
- Instance
- Player
- Battle
- Session
- Service

是否存在：

```text
constructor 做太多事情
析构隐式释放业务资源
全局 singleton
跨模块隐式生命周期
```

---

# 19. Persistence

需要分析：

```text
Repository
Storage
Cache
Snapshot
Journal
Write Behind
```

明确：

```text
Load
 ↓
Runtime
 ↓
Dirty
 ↓
Persist
```

同时分析：

- Save 时机
- Shutdown Save
- Crash Recovery
- Player Logout
- Instance Completion
- Critical Transaction
- 数据一致性

不要为了所谓“实时持久化”让每次游戏操作直接访问数据库。

---

# 20. Recovery

轻量 MMO 也必须具备可靠恢复能力。

重点分析：

### Player

```text
Disconnect
 ↓
Reconnect
 ↓
Restore
```

### Scene

```text
Scene Crash
 ↓
Recover
```

### Instance

```text
Instance Crash
 ↓
Recover / Abort / Restore
```

### Server

```text
Process Restart
 ↓
Load Persistent State
 ↓
Restore Runtime
```

需要明确哪些状态：

```text
必须持久化
可以重新计算
可以丢失
```

---

# 21. Gateway / Network

审查网络层是否与 Gameplay 过度耦合。

推荐：

```text
Connection
 ↓
Session
 ↓
Player
 ↓
Gameplay
```

Network 不应该直接管理大量游戏逻辑。

需要检查：

- Connection 生命周期
- Session 生命周期
- Authentication
- Reconnect
- Routing
- Backpressure
- Request/Response
- Push
- Broadcast
- Error Handling

---

# 22. Service Boundary

审查当前所有 Service。

对于每个 Service 问：

```text
它管理什么？
它拥有谁？
它修改什么？
它依赖什么？
它是否应该存在？
```

重点发现：

```text
God Service
God Manager
Global Manager
万能 Context
万能 Runtime
```

如果一个 Manager 同时负责：

```text
Player
Scene
Network
Persistence
Match
Battle
```

应该拆分。

---

# 23. Manager 泛滥检查

重点检查：

```text
XXXManager
XXXService
XXXSystem
XXXContext
XXXRegistry
```

不是 Manager 越多越好。

对于每个 Manager 判断：

> 它是否真正拥有一个明确的生命周期和职责？

如果只是为了调用几个函数创建 Manager，应考虑删除。

---

# 24. 配置系统

审查：

```text
Config
Definition
Runtime State
Static Data
```

是否混合。

建议：

```text
Config
= 服务运行配置

Definition
= 游戏静态配置

Runtime State
= 游戏运行状态
```

不要把三者混在一起。

---

# 25. 错误处理

统一分析：

```text
Network Error
Player Error
Scene Error
Instance Error
Battle Error
Persistence Error
```

明确：

```text
Recoverable
Retryable
Fatal
Player-visible
Server-only
```

避免简单：

```text
throw everywhere
assert everywhere
return bool everywhere
```

导致错误语义不一致。

---

# 26. 并发模型

重点检查：

```text
Player
Scene
Instance
Battle
AOI
Persistence
```

分别运行在哪个线程。

必须回答：

```text
谁可以修改 Player？
谁可以修改 Scene？
谁可以修改 Instance？
谁可以修改 Entity？
谁可以访问 Persistence？
```

推荐尽可能建立：

> **明确 Owner + 明确执行上下文。**

避免：

```text
任何线程
    ↓
修改 Player
```

这种设计。

---

# 27. API 易用性

Apollo 是框架，因此 API 设计非常重要。

最终希望开发者可以简单地：

```cpp
auto scene = world.create_scene(...);

auto instance = scene.create_instance(...);

auto player = instance.enter(player_id);

player.send(...);
```

而不是：

```cpp
ServiceRegistry
    → RuntimeContext
    → Manager
    → Dispatcher
    → Handler
    → Factory
    → Registry
```

如果一个简单操作需要穿越大量抽象层，应重点重构。

---

# 28. 测试体系

需要检查当前测试是否覆盖：

### Runtime

- Startup
- Shutdown
- Lifecycle

### Player

- Login
- Logout
- Reconnect
- Load
- Save

### Scene

- Create
- Enter
- Leave
- Destroy

### Instance

- Create
- Start
- Finish
- Destroy
- Recovery

### AOI

- Enter
- Leave
- Move
- Visibility

### Battle

- Start
- Tick
- Finish
- Reward

### Persistence

- Save
- Load
- Crash Recovery

---

# 29. 建立最小可运行示例

建议至少增加一个完整示例：

```text
Login
 ↓
Lobby
 ↓
Create Instance
 ↓
Enter Instance
 ↓
Spawn Player
 ↓
Spawn NPC
 ↓
AOI
 ↓
Battle
 ↓
Reward
 ↓
Leave Instance
 ↓
Return Lobby
```

这个 Example 应该成为 Apollo 最重要的架构验证。

如果这个流程非常复杂，说明 Apollo 的核心抽象仍然有问题。

---

# 30. 用典型游戏验证架构

至少用以下场景进行设计验证：

## 塔防

```text
Lobby
 ↓
Match
 ↓
Tower Instance
 ↓
Multiple Players
 ↓
Battle
 ↓
Reward
```

## 挂机 MMO

```text
Player
 ↓
Town
 ↓
Guild
 ↓
Dungeon
 ↓
Reward
```

## Arena

```text
Player
 ↓
Match
 ↓
Arena Instance
 ↓
Battle
 ↓
Result
```

## 轻量 MMORPG

```text
Town
 ↓
Field
 ↓
Dungeon
 ↓
Boss
 ↓
Town
```

不要求真正实现完整游戏，但架构必须能自然表达这些流程。

---

# 31. 性能分析

性能分析应该建立在实际模型上，而不是单纯追求 benchmark 数字。

重点测试：

```text
Player count
Entity count
Scene count
Instance count
AOI updates
Message throughput
Tick cost
Memory usage
Persistence latency
Reconnect latency
```

特别测试：

```text
1 Scene / 1000 Players

10 Scenes / 100 Players

100 Instances / 10 Players
```

因为 Apollo 的典型场景更可能是：

> 多个 Scene + 大量 Instance

而不是：

> 一个巨大无缝世界。

---

# 32. 复杂度审查

对每一个核心模块进行：

```text
代码量
依赖数量
接口数量
线程数量
锁数量
生命周期复杂度
```

然后问：

> 这个复杂度是否真正解决了 Apollo 的问题？

如果不能，应优先简化。

---

# 33. 文档改进

Apollo 文档必须围绕以下核心概念重新组织：

```text
Introduction
Architecture
Runtime
World
Player
Scene
Instance
Entity
AOI
Battle
Persistence
Recovery
Networking
Deployment
Examples
API
```

其中最重要的是：

```text
Player
Scene
Instance
```

这三个概念必须成为文档主线。

---

# 34. README 改进

README 第一屏必须回答：

### Apollo 是什么？

> Lightweight persistent multiplayer game server framework.

### 适合什么？

```text
Tower Defense
Idle MMO
Dungeon RPG
Arena
Card MMO
Lightweight MMORPG
```

### 核心概念是什么？

```text
Player
Scene
Instance
AOI
Battle
Persistence
```

### 如何运行？

提供最小 Example。

不要在 README 中堆大量底层技术名词。

---

# 35. 增加 Architecture Overview

建议增加：

```text
docs/architecture/overview.md
```

至少包含：

```text
Client
  ↓
Connection
  ↓
Session
  ↓
Player
  ↓
Scene
  ↓
Instance
  ↓
Entity / Battle
```

并明确每层职责。

---

# 36. 增加 Architecture Decision Records

建议至少记录：

```text
ADR-001 Player Runtime Authority
ADR-002 Scene as Runtime Boundary
ADR-003 Instance as First-Class Object
ADR-004 Scene Transfer
ADR-005 AOI Ownership
ADR-006 Battle Runtime
ADR-007 Persistence Model
ADR-008 Recovery Model
ADR-009 Concurrency Ownership
```

目的是防止未来架构继续失控。

---

# 37. 不要为了重构而重构

如果当前代码已经满足目标：

> 保留。

如果只是命名不理想：

> 优先小范围调整。

如果只是文档不清晰：

> 优先修正文档。

只有以下情况才进行大规模重构：

```text
职责错误
对象所有权错误
生命周期错误
并发模型错误
Player / Scene / Instance 模型错误
严重耦合
无法支持核心用例
```

---

# 38. 改造优先级

按照以下顺序执行：

## P0

```text
架构审计
对象所有权
Player 模型
Scene 模型
Instance 模型
生命周期
```

## P1

```text
Scene Transfer
PlayerDirectory
AOI
Persistence
Recovery
Reconnect
```

## P2

```text
Battle Runtime
ECS
Social
Guild
Party
```

## P3

```text
Deployment
Performance
Observability
Developer Experience
```

## P4

```text
Documentation
Examples
Benchmark
Tutorial
```

---

# 39. 最终验收标准

Apollo 最终应该做到：

```text
1. Player 是明确的一等对象

2. Player 在线状态以内存为权威

3. Scene 是明确的运行时边界

4. Instance 是明确的一等对象

5. Player 可以在 Scene / Instance 间迁移

6. Scene 拥有自己的 Entity / AOI / Runtime

7. Battle 可以独立运行

8. Persistence 与 Runtime 解耦

9. Reconnect 有明确模型

10. Recovery 有明确模型

11. 对象所有权明确

12. 并发模型明确

13. 生命周期明确

14. API 简单

15. 不存在明显的 God Manager / God Context

16. ECS 不吞噬整个框架

17. 网络层与 Gameplay 解耦

18. 可以自然实现塔防

19. 可以自然实现挂机 MMO

20. 可以自然实现副本 RPG

21. 可以自然实现竞技场

22. 可以自然实现轻量 MMORPG
```

---

# 40. 最终 Code Agent 工作方式

**第一阶段禁止修改代码。**

先完成：

```text
Repository Analysis
        ↓
Architecture Map
        ↓
Object Ownership Map
        ↓
Lifecycle Map
        ↓
Dependency Map
        ↓
Architecture Problems
        ↓
Improvement Proposal
```

输出：

```text
docs/rearchitecture/
├── audit.md
├── architecture.md
├── object-model.md
├── lifecycle.md
├── concurrency.md
├── persistence.md
└── improvement-plan.md
```

然后再根据审查结果进行代码修改。

每完成一个阶段，都必须重新运行：

```text
Build
Unit Tests
Integration Tests
Examples
```

禁止一次性进行大规模重构。

最终目标不是“代码更多”或者“架构更复杂”，而是：

> **让 Apollo 用尽可能简单、清晰、可维护的架构，解决轻量 MMO 和场景化多人在线游戏的问题。**