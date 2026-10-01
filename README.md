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

## 📖 项目简介

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

## ✅ 适用场景 / Use Cases

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

### 核心特性

- 🏗️ **极简构造注入 DI** - `apollo::core::di`：类型键 bean 图、拓扑序装配，+ `ApplicationHost` 帧驱动生命周期（`IHostedService` start/stop/tick）
- 📜 **实体契约系统** - XML+XSD 契约（attrs/messages/entities/errors，错拼即报错）+ 独立生成器 `apollo_gen`，生成器不进运行时链接图（`sdks/contract`，docs/36 决策 #3/#4/#5）
- 🌐 **高性能网络层** - 跨平台异步I/O（IOCP/Epoll）
- 📦 **传输编解码（目标态）** - L1 帧格式 + protobuf descriptor（`descriptor.bin`，反射为默认）+ Lua 契约表（`contract.lua`）+ zstd 压缩（`docs/design/sdk-contract.md` §10-§12）——框架固定消息族内建强类型守热路径、业务消息反射进 Lua（服务端契约变更零重编）、有代码热更管线的客户端走生成代码（docs/36 决策 #19）；现网为手写编码，仓库自有 .proto 为零
- 🔥 **ECS战斗系统** - 灵活的实体-组件-系统架构
- 👁 **AOI九宫格系统** - 高效的视野管理
- 💾 **数据存储层** - 数据库连接池和Redis缓存
- ⚡ **日志系统** - 多级别异步日志
- 🔧 **工具类库** - 线程池、内存池、配置管理等
- 🧩 **BigWorld兼容层** - BigWorld 风格 C++ API facade（见 `docs/33-BigWorld_Compatibility.md`）

## 🏛️ 架构设计

> 完整架构图（系统架构概览 / 核心模块架构 / 分布式部署架构）已迁至文档站：[架构总览](docs/guide/architecture.md)。进程编队、权威分解与 Zone/无缝辨析见[架构审计报告](docs/analysis/architecture-review.md) §31。

### 技术栈

- **编程语言**: C++20
- **构建系统**: CMake + Ninja，vcpkg 清单模式管理依赖
- **网络库**: 自实现跨平台网络层
- **序列化/契约**: 两层分工、互不冲突（docs/36 决策 #3/#19）——**def 契约管语义**（属性/权限位/sync 掩码/内外分域；XML+XSD + 生成器 `apollo_gen`，`sdks/contract`，已交付；服务端载体为 contract.lua，业务 handler 与白名单数据化）；**protobuf 管字节编码**（反射为默认：descriptor.bin + 框架固定消息族内建强类型守热路径；有代码热更管线的客户端走生成代码、bin 兜底；L1 帧格式 + zstd，规划；`docs/design/sdk-contract.md`）。protobuf 现为依赖+测试，未上消息通路（仓库自有 .proto 为零，现网手写编码）
- **数据库**: MySQL 8（主存储）+ Redis（缓存/会话）+ ClickHouse（分析，规划）；PostgreSQL 留缝（docs/36 决策 #17）
- **消息队列**: Kafka 用于可观测管道（规划，决策 #14）；服务间通信用自研消息总线（规划，`docs/design/net-abstraction.md`）
- **监控系统**: Prometheus + Grafana（规划，批次8）
- **日志系统**: 自研多级别日志（`apollo::core::log`）；目标链路 LogAgent→Kafka→ClickHouse（决策 #14）
- **测试框架**: GTest + 零依赖断言式单测
- **CI/CD**: GitHub Actions

## 🚀 快速开始

### 环境要求

- C++20 或更高版本
- CMake 3.16+
- GCC 9+ / Clang 10+ / MSVC 2019+
- MySQL 8.0+ (可选)
- Redis 6.0+ (可选)

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
./build/examples/all_features_demo
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

## 📚 模块说明

### Base 基础设施（modules/base）
- **定位**: 纯基础设施，无任何框架语义，可被任何 C++ 项目独立使用（不依赖 Apollo 其他模块）
- **组件**: Time 时间工具、ThreadPool 线程池、IdPool ID 分配、String 字符串工具、Memory 内存工具、Terminal 终端工具
- 详见 [Base 模块文档](docs/modules/base.md)

### Core 核心框架（modules/core · modules/runtime）
- **依赖注入**: `apollo::core::di` 极简构造注入容器——类型键 bean 图、拓扑序装配、仅 Singleton/Prototype 两档作用域
- **应用生命周期**: `ApplicationHost` 帧驱动托管——`IHostedService` start/stop/tick + 六阶段状态机（Boot→…→Stopped）
- **配置**: `apollo::core::config::ConfigRegistry` 键值注册表；热更规划走 tick 边界换 ConfigSnapshot（architecture-review §17.6）

### Game 游戏逻辑
- **AOI系统**: 九宫格空间索引，高效视野管理
- **战斗系统**: ECS架构，支持技能、Buff、状态机
- **属性系统**: 灵活的属性计算和同步机制
- **场景管理**: 多场景支持和场景迁移

### Network 网络通信
- **传输层**: Socket封装，支持TCP/WebSocket/KCP
- **消息层**: 消息路由和处理（现网手写编码；目标 protobuf+zstd，schema 由 def 契约生成，规划）
- **RPC框架**: 远程过程调用框架

### Storage 存储层
- **数据库连接池**: MySQL连接管理和复用
- **Redis客户端**: 完整的Redis命令支持
- **序列化**: 多种数据序列化方案

### Utils 工具库
- **日志系统**: 多级别异步日志，文件/控制台输出
- **线程池**: 高性能任务调度
- **内存池**: 优化的内存管理
- **配置系统**: JSON/XML/Lua配置支持

### Compatibility 兼容层
- **BigWorld Compatibility Layer**: BigWorld-style C++ API facade (see `docs/33-BigWorld_Compatibility.md`)
- **BigWorld API Tests**: `tests/test_bigworld_api.cpp` (CTest: `BigWorldApiTests`)

## 🔧 使用示例

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
#include "apollo/game/aoi/AOIManager.h"

// 初始化AOI管理器
auto& aoi = apollo::AOIManager::Instance();
aoi.Initialize(100.0f);  // 100米网格

// 创建实体
apollo::AOIEntity player(1001, 1);
player.position = {100.0f, 0.0f, 100.0f};
player.aoiRadius = 30.0f;

// 更新到AOI
aoi.UpdateEntity(player);

// 获取可见实体
auto visible = aoi.GetVisibleEntities(1001);
for (auto id : visible) {
    std::cout << "Entity " << id << " is visible" << std::endl;
}
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

## 📊 性能指标

| 指标 | 目标值 | 说明 |
|------|--------|------|
| 单服承载 | 5000+ CCU | 单GameServer并发用户数 |
| 响应延迟 | P99 < 50ms | 核心操作响应时间 |
| 内存使用 | < 2GB | 5000用户内存占用 |
| 网络吞吐 | 100MB/s | 峰值网络带宽 |
| 数据库连接 | 1000+ | 最大连接池大小 |

## 🧪 测试

项目包含完整的单元测试和示例代码：

```bash
# 运行所有测试
cd build && ctest

# 仅运行 BigWorld 兼容层测试
cd build && ctest -R BigWorldApiTests

# 运行功能演示
./examples/all_features_demo
```

测试覆盖：
- 网络通信测试
- 数据库操作测试
- Redis操作测试
- AOI系统测试
- 属性系统测试
- BigWorld 兼容层 API 测试（`BigWorldApiTests`）

## 📋 目录结构

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

## 🔌 客户端SDK

### Unity SDK

提供完整的Unity客户端SDK，支持：

- 网络通信管理
- 消息序列化/反序列化
- 自动重连机制
- 资源热更新
- 性能监控

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

## 🤝 贡献指南

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

## 📄 许可证

本项目采用 [MIT License](LICENSE) 许可证。

## 🙏 致谢

- 感谢所有贡献者的努力
- 感谢开源社区的支持
- 感谢所有用户的反馈和建议

## 📞 联系方式

- 项目主页: https://github.com/cuihairu/apollo
- 问题反馈: [Issues](https://github.com/cuihairu/apollo/issues)
- 邮箱: cuihairu@example.com

---

**Apollo - 构建你的MMORPG世界** 🎮
