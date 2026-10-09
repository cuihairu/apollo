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

> 实例化多人在线游戏服务器引擎 —— Instance-based Multiplayer Game Server Engine

## 项目简介

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

中文定位：**实例化多人在线游戏服务器引擎**——房间/场景/副本/比赛为一等公民；MMO 是「由大量 Zone/实例组成」的 supported use case（instance-oriented MMO architectures），非核心身份。技术栈为现代 C++20，核心模块覆盖网络通信、数据存储、游戏逻辑与战斗系统。

## 适用场景 / Use Cases

**适合**——自然世界的边界天然是「房间 / 场景 / 副本 / 比赛 / 实例」的游戏：

| 场景 | 实例形态 |
|------|----------|
| 塔防、固定地图（绿色循环圈等） | Zone 内多房间，开局 `CreateInstance` |
| 副本 / Dungeon / Roguelike | 一局一实例，用完回收 |
| 竞技场 / Arena / 大逃杀 | Match 实例，赛毕销毁 |
| 多人 PvE / 生存 / Co-op | 小队实例 |
| 多人 PvP | 对局实例 |
| MMO 分线与副本 | 线 = scene_id 实例，大量 Zone 横向编排 |

**不适合**——**连续共享大世界**（persistent seamless world）：无缝地图、跨进程空间漫游、「一个 World 横向切成 CellApp」的部署模型。Apollo 有意不做 cell 分片 / ghost / 实体迁移（架构裁决 #9），这类需求请选 BigWorld / KBEngine 一类 continuous-world 引擎。

> 一句话判据：**世界的自然边界是房间/场景/副本/比赛 → 适用；要求跨进程连续的大世界 → 不适用。**

## 核心概念 / Core Concepts

六个词读完 Apollo 的运行时模型（全部有实件，详见[文档站](docs/guide/concepts.md)）：

| 概念 | 一句话 | 实件 |
|------|--------|------|
| **Player** | 玩家长期态——登录在场、跨副本持续的档案与锚点 | `PlayerAnchor`（modules/game/session） |
| **Scene** | 空间运行时边界——AOI 所有权归场景 | `Scene` + `SceneAoi`（modules/game/world） |
| **Instance** | 一局一实例的一等公民——八态生命周期（Create→…→Destroyed），用完回收 | `Instance`（modules/game/world） |
| **AOI** | 兴趣管理——九宫格差集 + 逐观察者水位，Enter/Sync/Leave 事件 | `SceneAoi` + `ViewerState` |
| **Battle** | 战斗域——确定性 tick 判定，结算单向落玩家长期态 | `BattleRuntime`（modules/game/battle） |
| **Persistence** | 档案与日志——JSON 档案原子写 + write-behind 日志，零外部存储依赖 | `PersistJournal`（modules/data） |

### 核心特性

- **极简构造注入 DI** - `apollo::core::di`：类型键 bean 图、拓扑序装配，`ApplicationHost` 帧驱动生命周期
- **实体契约系统** - XML+XSD 契约 + 独立生成器 `apollo_gen`，生成器不进运行时链接图
- **异步网络层** - 跨平台异步 I/O（IOCP/Epoll）
- **传输编解码（目标态）** - protobuf 反射为默认 + zstd（规划）；现网手写编码
- **AOI 九宫格** - `SceneAoi` 单实现 + `ViewerState` 逐观察者水位，事件面已交付
- **战斗系统** - `BattleSystem` 场景内骨架；ECS 收敛与技能/Buff 随 P2
- **数据存储** - 玩家档案文件原子写 + `PersistJournal` write-behind；外部 DB/Redis 未接线
- **可观测** - 多级别异步日志 + 结构化行格式 + `MetricRegistry`
- **BigWorld 兼容层** - BigWorld 风格 C++ API facade（`docs/33-BigWorld_Compatibility.md`）

## 架构设计

> 完整架构图（系统架构概览 / 核心模块架构 / 分布式部署架构）已迁至文档站：[架构总览](docs/guide/architecture.md)。进程编队、权威分解与 Zone/无缝辨析见[架构审计报告](docs/analysis/architecture-review.md) §31。

### 技术栈

- **编程语言**: C++20
- **构建系统**: CMake + Ninja，vcpkg 清单模式管理依赖
- **网络库**: 自写跨平台网络层
- **序列化/契约**: 两层分工、互不冲突（docs/36 决策 #3/#19）——**def 契约管语义**（属性/权限位/sync 掩码/内外分域；XML+XSD + 生成器 `apollo_gen`，`sdks/contract`，已交付；服务端载体为 contract.lua，业务 handler 与白名单数据化）；**protobuf 管字节编码**（反射为默认：descriptor.bin + 框架固定消息族内建强类型守热路径；有代码热更管线的客户端走生成代码、bin 兜底；L1 帧格式 + zstd，规划；`docs/design/sdk-contract.md`）。protobuf 现为依赖+测试，未上消息通路（仓库自有 .proto 为零，现网手写编码）
- **数据库**: 现状零外部存储依赖——玩家档案 = JSON 文件 + `PersistJournal` write-behind 日志（P1-4/P1-5 交付）；MySQL 8（主存储）/ Redis（缓存/会话）/ ClickHouse（分析）均为规划态未接线，PostgreSQL 留缝（docs/36 决策 #17）
- **消息队列**: Kafka 用于可观测管道（规划，决策 #14）；服务间通信用自写消息总线（规划，`docs/design/net-abstraction.md`）
- **监控系统**: Prometheus + Grafana（规划，批次8）
- **日志系统**: 自写多级别日志（`apollo::core::log`）；目标链路 LogAgent→Kafka→ClickHouse（决策 #14）
- **测试框架**: 零依赖断言式单测（统一测试树，ctest 门禁）；legacy GTest 批默认关
- **CI/CD**: GitHub Actions

## 快速开始

### 环境要求

- C++20 或更高版本
- CMake 3.16+
- GCC 9+ / Clang 10+ / MSVC 2019+

> 现状构建与运行均零外部存储依赖（玩家档案为本地文件 + journal）；MySQL/Redis 运行时接线为规划态，无需预装。

### 编译项目

```bash
# 克隆项目
git clone https://github.com/cuihairu/apollo.git
cd apollo

# 安装 vcpkg（用于依赖管理）
git clone https://github.com/microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh

# 创建构建目录
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/vcpkg/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=x64-linux   # macOS: x64-osx / arm64-osx

# 编译
cmake --build build --parallel

# 运行示例
./build/examples/session_demo
```

### 最小示例

约 40 行跑起一个宿主托管的游戏服务（完整可编译版见 [quick-start](docs/guide/quick-start.md)，已按实 API 验证）：

```cpp
#include <apollo/runtime/application_host.hpp>
#include <apollo/game/world/scene_aoi.hpp>

using apollo::game::world::SceneAoi;

class GameServer : public apollo::runtime::IHostedService {
public:
    std::string_view service_name() const override { return "game-server"; }

    bool start() override {
        aoi_ = std::make_unique<SceneAoi>(1000.0f, 1000.0f, 100.0f, 30.0f);
        aoi_->set_event_sink([](const SceneAoi::Event&) { /* 下发面 */ });
        for (int i = 1; i <= 100; ++i) {
            aoi_->enter(apollo::game::core::EntityId{static_cast<std::uint64_t>(i)},
                        {static_cast<float>(i), 0.0f, 0.0f});
        }
        return true;
    }

    void stop() override {}
    bool is_running() const override { return true; }
    void tick() override { /* 定帧业务 */ }

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
# 使用 Visual Studio 2019 或更高版本
git clone https://github.com/cuihairu/apollo.git
cd apollo

git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat

cmake -B build -G "Visual Studio 16 2019" ^
  -DCMAKE_TOOLCHAIN_FILE=%cd%\\vcpkg\\scripts\\buildsystems\\vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows

# 打开 build\\Apollo.sln 进行编译
```

## 模块说明

### Base 基础设施（modules/base）
- **定位**: 纯基础设施，无任何框架语义，可被任何 C++ 项目独立使用（不依赖 Apollo 其他模块）
- **组件**: Time 时间工具、ThreadPool 线程池、IdPool ID 分配、String 字符串工具、Memory 内存工具、Terminal 终端工具
- 详见 [Base 模块文档](docs/modules/base.md)

### Core 核心框架（modules/core · modules/runtime）
- **依赖注入**: `apollo::core::di` 极简构造注入容器——类型键 bean 图、拓扑序装配、仅 Singleton/Prototype 两档作用域
- **应用生命周期**: `ApplicationHost` 帧驱动托管——`IHostedService` start/stop/tick + 六阶段状态机（Boot→…→Stopped）
- **配置**: `apollo::core::config::ConfigRegistry` 键值注册表；热更规划走 tick 边界换 ConfigSnapshot（architecture-review §17.6）

### Game 游戏逻辑
- **AOI系统**: `SceneAoi` 九宫格（Scene 持有、scene_id 隔离）+ `ViewerState` 逐观察者水位；Enter/Sync/Leave 事件分发已交付，网关下发面留 P3-2
- **战斗系统**: `BattleSystem` 场景内骨架（实体集合 + tick 更新）；ECS 多套收敛与技能/Buff/状态机随 P2（未实现）
- **属性系统**: `AttributeContainer`/`AttributeManager` 属性容器与定义注册；逐 viewer delta 同步管线未接线（契约 `attr_batch` 已入 messages.xml）
- **场景管理**: 多场景支持 + 换幕（`scene_transfer`，prepare→detach→attach→resume + 失败回滚）

### Network 网络通信
- **传输层**: Socket封装，支持TCP/WebSocket/KCP
- **消息层**: 消息路由和处理（现网手写编码；目标 protobuf+zstd，schema 由 def 契约生成，规划）
- **RPC框架**: 远程过程调用框架

### Storage 存储层
- **玩家档案**: JSON 文件档案（tmp+rename 原子写）+ `SaveQueue` 异步保存队列；加载未命中即失败（无默认档 bootstrap）
- **Write-ahead journal**: `PersistJournal`——append 落盘 → 定额 drain → 快照压薄 → 崩溃 replay 续接（P1-4/P1-5）
- **序列化**: 档案 toJson/fromJson 对称序列化；连接池/外部 DB/Redis 客户端为规划态（P1-4 已删五套存量实现）

### Utils 工具库
- **日志系统**: 多级别异步日志，文件/控制台输出
- **线程池**: 任务调度
- **内存池**: 内存管理
- **配置系统**: JSON/XML/Lua配置支持

### Compatibility 兼容层
- **BigWorld Compatibility Layer**: BigWorld-style C++ API facade (see `docs/33-BigWorld_Compatibility.md`)
- **BigWorld API Tests**: `tests/test_bigworld_api.cpp` (CTest: `BigWorldApiTests`)

## 使用示例

### 服务器基础框架

```cpp
#include "apollo/core/di/application_context.hpp"
#include "apollo/runtime/application_host.hpp"

// 业务 bean：普通类，依赖走构造函数（无注解、无注册宏）
struct GameClockService {
    std::string name = "game_clock";
};

class LoginPipeline {
public:
    explicit LoginPipeline(GameClockService& clock) : clock_(clock) {}
private:
    GameClockService& clock_;
};

// 托管服务：实现 IHostedService，由 ApplicationHost 帧驱动
class GameServerService : public apollo::runtime::IHostedService {
public:
    explicit GameServerService(apollo::core::di::ApplicationContext& ctx)
        : clock_(ctx.get<GameClockService>()) {}

    std::string_view service_name() const override { return "game_server"; }
    bool start() override { return true; }
    void stop() override {}
    bool is_running() const override { return true; }
    void tick() override { /* 每帧业务逻辑 */ }

private:
    GameClockService& clock_;
};

int main() {
    // 装配：bean 图 = 一串 add_singleton，依赖即模板参数
    apollo::core::di::ApplicationContextBuilder builder;
    builder.add_singleton<GameClockService>().name("game_clock");
    builder.add_singleton<LoginPipeline, GameClockService>().name("login_pipeline");
    auto context = builder.build();
    if (!context.initialize()) {
        return 1;  // 拓扑序构造，失败即退出
    }

    // 托管：start → run_once 帧循环 → stop
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

> 完整可运行版本见 `apps/game-server/src/main.cpp`。装配只发生在应用入口（apps/），模块内部互不感知容器。

### 使用AOI系统

```cpp
#include "apollo/game/world/scene_aoi.hpp"

using apollo::game::core::EntityId;
using apollo::game::world::SceneAoi;

// 九宫格兴趣管理：每个 Scene 独享一个实例（天然按 scene 隔离）
SceneAoi aoi(1000.0f, 1000.0f, 100.0f, 30.0f);  // 宽、高、格子、视野半径

// 事件面：Enter / Sync / Leave（sink 缺省静默，由 Scene/宿主注入下发）
aoi.set_event_sink([](const SceneAoi::Event& e) {
    // e.kind / e.observer / e.subject —— viewer set 差集基准在 ViewerState
});

// 实体进入 / 移动 / 离开
aoi.enter(EntityId{1001}, {100.0f, 0.0f, 100.0f});
auto viewers = aoi.viewers_of(EntityId{1001});   // 视野内实体（含自身）
aoi.move(EntityId{1001}, {120.0f, 0.0f, 100.0f});
aoi.leave(EntityId{1001});
```

### 使用战斗系统

```cpp
#include "apollo/game/battle/BattleWorld.h"
#include "apollo/game/battle/components.hpp"

// 创建战斗世界
auto world = std::make_unique<apollo::BattleWorld>(1);

// 创建玩家实体
auto player = world->CreateEntity(1001);
auto transform = player->AddComponent<apollo::TransformComponent>();
auto attributes = player->AddComponent<apollo::AttributeComponent>();
auto battleState = player->AddComponent<apollo::BattleStateComponent>();

// 设置属性
attributes->SetLevel(50);
attributes->SetHp(1000);
attributes->SetAttack(150);

// 更新战斗世界
world->Update(deltaTime);
```

## 性能指标

| 指标 | 目标值 | 说明 |
|------|--------|------|
| 单服承载 | 5000+ CCU | 单GameServer并发用户数 |
| 响应延迟 | P99 < 50ms | 核心操作响应时间 |
| 内存使用 | < 2GB | 5000用户内存占用 |
| 网络吞吐 | 100MB/s | 峰值网络带宽 |
| 数据库连接 | 1000+ | 最大连接池大小 |

## 测试

项目包含完整的单元测试和示例代码：

```bash
# 运行所有测试（全量清单见 tests/CMakeLists.txt）
cd build && ctest

# 仅运行 BigWorld 兼容层测试
cd build && ctest -R BigWorldApiTests
```

测试覆盖：
- 网络与协议测试（network/net/protocol/channel）
- 数据与持久化测试（data、`PersistJournalTests`、`PersistenceChainTests`——文件档案 写盘→重启→读回）
- 世界与生命周期测试（game/scene/instance/avatar/session_world/world_host）
- 会话与恢复测试（`PlayerDirectoryTests`、`RecoveryTests`、`ReconnectTests`）
- 基础设施测试（日志/定时器/配置/线程池/序列化等）
- BigWorld 兼容层 API 测试（`BigWorldApiTests`）

## 目录结构

```
apollo/
├── apps/                   # 可执行服务：login/gateway/base/cell/game-server
├── modules/                # 模块化源码：base/core/data/game/net/runtime 等
├── include/                # 公共头文件与兼容层头文件
├── src/                    # 公共实现与兼容层实现
├── tests/                  # 测试代码
├── examples/               # 示例代码
├── sdks/                   # Unity 客户端 SDK 与契约工具（contract 契约 + gen 生成器 + cpp 生成物）
├── skds/                   # Cocos / Laya / 历史 SDK 工作区
├── docs/                   # VitePress 文档站点
├── cmake/                  # CMake 辅助脚本
└── build*/                 # 构建输出
```

详细目录说明见 [Directory Structure](docs/Directory_Structure.md)

## 客户端SDK

### Unity SDK

Unity SDK 部分模块已交付（模块状态详见 [SDK_Structure](docs/sdks/unity/SDK_Structure.md)）：

- 网络通信管理（NetworkManager，已有）
- 登录认证（AuthManager，已有）
- 属性同步（Attributes，开发中）
- 消息序列化/反序列化（Messaging，计划中）

```csharp
// Unity SDK使用示例
var config = new ApolloClientConfig {
    ServerAddress = "game.example.com",
    Port = 7700,
    Protocol = TransportProtocol.Tcp
};

var client = new ApolloClient(config);
await client.ConnectAsync();
await client.LoginAsync(loginRequest);
```

## 贡献指南

欢迎贡献代码！请遵循以下步骤：

1. Fork 项目
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 创建 Pull Request

### 代码规范

- 遵循 [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
- 使用有意义的变量和函数名
- 添加适当的注释
- 编写单元测试

## 许可证

本项目采用 [MIT License](LICENSE) 许可证。

## 来源与致谢

Apollo 为本仓原创实现，非任何现有项目的 fork；架构概念与术语参考了下列开源项目，引用处均标注来源：

- **BigWorld**：BigWorld 风格 C++ API facade（`docs/33-BigWorld_Compatibility.md`）；架构审计含 BigWorld / KBEngine / skynet 三框架源码对照（`docs/analysis/architecture-review.md` §16）
- **KBEngine**：`docs/qa/` 多篇条目为其源码机制分析
- **skynet**：观测通道等设计先例（`docs/analysis/architecture-review.md`）

第三方依赖：[vcpkg](https://github.com/microsoft/vcpkg)（依赖管理）、[nlohmann/json](https://github.com/nlohmann/json)、[protobuf](https://github.com/protocolbuffers/protobuf)（现为依赖与测试，未上消息通路）、[GTest](https://github.com/google/googletest)、NNG（`modules/protocol` 现为桩形态，未启用真实传输）、[VitePress](https://vitepress.dev)（文档站）、GitHub Actions 与 Codecov（CI 与覆盖率）。

## 联系方式

- 项目主页: https://github.com/cuihairu/apollo
- 问题反馈: [Issues](https://github.com/cuihairu/apollo/issues)
- 邮箱: cuihairu@example.com

---

**Apollo - 实例化多人在线游戏服务器引擎**
