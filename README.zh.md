[English](README.md) | [中文](README.zh.md)

<p align="center">
  <img src="docs/zh/public/apollo.png" alt="Apollo Logo" height="80"/>
</p>

# Apollo

**轻量副本制多人在线游戏服务器引擎。**

<p align="center">
<a href="https://github.com/cuihairu/apollo/releases"><img alt="Status" src="https://img.shields.io/badge/status-in%20development-orange"></a>
<a href="https://github.com/cuihairu/apollo/blob/main/LICENSE"><img alt="License" src="https://img.shields.io/badge/license-MIT-blue.svg"></a>
<a href="https://github.com/cuihairu/apollo"><img alt="Platform" src="https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey.svg"></a>
<a href="https://github.com/cuihairu/apollo"><img alt="Language" src="https://img.shields.io/badge/language-C%2B%2B20-blue.svg"></a>
<a href="https://github.com/cuihairu/apollo/actions/workflows/ci.yml"><img alt="Build Status" src="https://img.shields.io/github/actions/workflow/status/cuihairu/apollo/ci.yml?branch=main"></a>
<a href="https://codecov.io/gh/cuihairu/apollo"><img alt="Coverage" src="https://codecov.io/gh/cuihairu/apollo/branch/main/graph/badge.svg"></a>
</p>

> 轻量副本制多人在线游戏服务器引擎

## 项目简介

Apollo 是一个 C++20 游戏服务器运行时，面向以副本和对局为边界的轻量多人游戏，例如塔防、合作 PvE/PvP、竞技场和副本玩法。当前交付轻量副本路径，预留未来无缝地图的 Space/Cell、权威和可见性边界。

可运行进程为 `gateway`、`login`、`base`、`world`、`database`、`interfaces`、`machined`；设计文档里的 loginServer/baseServer/worldServer 是 `login`/`base`/`world` 二进制的角色称谓，`cell` 是 `world` 二进制的 cell 执行角色（`--role=cell`）。Cell 为分区。

<picture>
  <source media="(max-width: 640px)" srcset="docs/zh/public/architecture/system-zh-mobile.svg" />
  <img src="docs/zh/public/architecture/system-zh.svg" alt="Apollo 目标架构：Base 长期资料、cell 模拟与 World 管理分别负责" />
</picture>

目标设计图：Base ↔ database 独立原型已具备；World/cell 拆分与整体接线仍待完成。

baseServer 管长期资料，worldServer 管执行端和空间拓扑，cell 运行实体与临时玩法。跨服保持原 Base 和数据库归属；同一个 World 可以通过逐玩家的 Base 关联接待多个区服，无需各服数据库凭证，也不要求同机部署 machined。

支持协议变化的更新通过状态转换、单权威交接、路由切换和源端清理，分别替换 Base、cell 或 World 管理端。活动副本要连续，须迁移完整必要 Instance 状态。目标角色拆分及运行接线仍需实施，见[架构总览](docs/zh/guide/architecture.md)、[服务器应用](docs/zh/apps/index.md)和[跨服场景](docs/zh/design/cross-server-scenes.md)。

### 入口与交付面

- 游戏 WS/WSS（已验收）：gateway 同时监听 TCP / WS / WSS，登录、会话恢复、异步下行共用同一条游戏帧路径；WSS 覆盖证书链/主机名错误拒绝与密钥配置负例，掉线可在保留窗内跨传输续接。
- HTTP 回调（受理已落地）：支付/渠道服务端回调走独立 interfaces HTTP/HTTPS 入口——原始请求帧逐字节保留、请求超时与受理队列预算、有界单写者受理链，不阻塞游戏模拟；渠道验签执行器规划中。
- 动态 Base 查询与持久发货（模块已落地）：订单按稳定玩家身份受理并持久记账，每次投递重新解析「当前 Base + 归属代次」，玩家离线、Base 不可达或迁移升级期间待办不丢；发货以去重凭据原子提交，重复通知、回执丢失与崩溃重启收敛为一次发货。interfaces→订单受理、baseServer 写权生命周期、奖励落账 executor 等进程间接线规划中。

<picture>
  <source media="(max-width: 640px)" srcset="docs/zh/public/architecture/callbacks-zh-mobile.svg" />
  <img src="docs/zh/public/architecture/callbacks-zh.svg" alt="Apollo 外部回调与发货：interfaces 受理、动态 Base 解析与幂等发货" />
</picture>

### 进程、工具与 CI

| 类别 | 成员 | 边界 |
|---|---|---|
| 运行进程 | `gateway`、`login`、`base`、`world`、`database`、`interfaces`、`machined` | 在线业务进程；`cell` 为 `world` 二进制的执行角色 |
| CLI 工具 | `apolloc`（项目初始化 init）、`fleetctl`、`admin`、`logger` | 开发与运维工具，不参与在线业务 |
| CI | GitHub Actions：构建/测试、契约、文档 | 仓库内执行，不参与在线业务 |

## 适用场景

**适合**——玩法边界天然是副本或对局的游戏：

| 场景 | 实例形态 |
|------|----------|
| 塔防、固定地图 | 一场对局一个 Instance，结束后回收副本状态 |
| 副本 / Roguelike | 一次玩法一个 Instance |
| 竞技场 / 大逃杀 | 一场比赛一个 Instance |
| 多人 PvE / 生存 / Co-op | 小队共用一个 Instance |
| 多人 PvP | 一场对局一个 Instance |

**当前不实施无缝地图。** 默认一个执行进程、每 Space 一个 Cell；预留同 Space 多 Cell、邻接可见性与权威交接，不另建一套架构。

> MMO 描述游戏规模和产品形态，不决定服务端拓扑。Apollo 可以支持由副本和对局组成的 MMO 玩法，当前优先副本路径，无缝能力保留设计边界。

[BigWorld 与 Apollo 体系对照](docs/zh/design/bigworld-apollo-mapping.md)

## 核心概念

核心玩法概念（当前代码组件详见[概念指南](docs/zh/guide/concepts.md)）：

| 概念 | 职责 | 现有代码与边界 |
|---|---|---|
| baseServer | 长期资料对象与基础业务 | baseServer 独立入口和远程存储已具备；目录归属及默认链路待迁移 |
| worldServer | cell 注册、Space/Cell 拓扑与迁移 | 当前 WorldApp 管理/执行混合，需拆分 |
| cell | 本地 Cell 模拟、实体、AOI、战斗 | 复用 WorldHost、Scene、Instance、Avatar，独立入口待实现 |
| Space / Cell | 运行空间 / 空间分区 | Space 对应现有 Scene 语义，不新增同义容器 |
| database | 加载、保存、存储路由与持久结果 | database 独立入口承载文件档案与 journal；完整写权/队列待补 |
| Instance | 临时小队玩法 | 复用已有八态状态机 |
| AOI | 场景可见性 | SceneAoi |
| loginServer / gateway / machined | 认证与目录、连接转发、本机监督 | 复用现有入口，更新路由与部署接线 |

### 核心特性

- **极简构造注入 DI** - `apollo::core::di`：类型键 bean 图、拓扑序装配，`ApplicationHost` 帧驱动生命周期
- **实体契约系统** - XML+XSD 契约 + 独立生成器 `apollo_gen`，生成器不进运行时链接图
- **异步网络层** - 非阻塞 socket + select()/epoll 事件循环（Windows 走 select()，不使用 IOCP）
- **传输编解码（目标态）** - protobuf 反射为默认 + zstd（规划）；现网手写编码
- **AOI 九宫格** - `SceneAoi` 单实现 + `ViewerState` 逐观察者水位，事件面已交付
- **战斗系统** - `BattleRuntime` 确定性战斗域（五段相位 + hash 链指纹），结算经所属 Base 与 database 确认
- **数据存储** - 玩家档案文件原子写 + `PersistJournal` write-behind；外部 DB/Redis 未接线
- **可观测** - 自研日志、结构化事件、MetricRegistry 和 span/trace 原语；尚未集成专用 Profiler 或 OpenTelemetry SDK/exporter
- **BigWorld 兼容层** - BigWorld 风格 C++ API facade（`docs/zh/33-BigWorld_Compatibility.md`）

## 更新方案

协议更新启动新的 Base、cell 或 World 管理进程，转换并恢复旧状态，条件提交权威与路由，清理并退出旧进程。cell 更新交接模拟状态，World 更新交接拓扑与在途管理记录，Base 更新交接长期对象；资料与存储归属保持。新旧消息解码、状态转换、回滚边界及旧客户端兼容需明确验收，握手兼容窗口不等于业务兼容。

仅代码更新保持契约编号、字段和 wire 不变。Lua 可预备、提交、观察与回滚模块，C++ 使用进程替换。保留 gateway 可维持其既有 TCP；替换 gateway 需要排空或客户端恢复。现有原语不代表目标角色的完整升级链路已接通。

详见[更新边界](docs/zh/design/gateway-topology-survey.md)和[契约版本管理](docs/zh/design/sdk-contract.md)。

## 架构设计

> 目标副本制模型见[架构总览](docs/zh/guide/architecture.md)；仓库现有可执行原型及其状态见[服务器应用](docs/zh/apps/index.md)。

### 技术栈

- **编程语言**: C++20
- **构建系统**: CMake + Ninja，vcpkg 清单模式管理依赖
- **网络库**: 自写跨平台网络层
- **序列化/契约**：XML+XSD 契约定义游戏数据语义，`apollo_gen` 生成客户端/服务端契约产物（见 `docs/zh/design/sdk-contract.md`）。protobuf 目前仅作为依赖和测试使用，尚未进入消息通路；当前消息编码由手写 codec 实现。
- **数据库**：当前无需外部存储依赖。玩家档案使用 JSON 文件和 `PersistJournal`；MySQL、Redis、ClickHouse 接入仍属规划。
- **消息队列**: Kafka 用于可观测管道（规划，决策 #14）；服务间通信用自写消息总线（规划，`docs/zh/design/net-abstraction.md`）
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

### 本地测试口径

CI 与本地开发按改动面三路分流——改一行文档不该全量编译。

1. **文档改动**（`*.md`、`docs/zh/**`、图片）：不编译。主 CI workflow 的
   `paths-ignore` 已跳过这些路径。
2. **代码改动**：只跑改动子集，不全量。
   - `ctest -R '<改动测试>' -C Debug`
3. **全量**（`ctest --output-on-failure -C Debug`）：仅以下情形触发——CMakeLists.txt、
   `.github/**`、Docker/Compose 配置、发布前。

### 最小示例

约 40 行跑起一个宿主托管的游戏服务（完整可编译版见 [quick-start](docs/zh/guide/quick-start.md)，已按实 API 验证）：

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
- 详见 [Base 模块文档](docs/zh/modules/base.md)

### Core 核心框架（modules/core · modules/runtime）
- **依赖注入**: `apollo::core::di` 极简构造注入容器——类型键 bean 图、拓扑序装配、仅 Singleton/Prototype 两档作用域
- **应用生命周期**: `ApplicationHost` 帧驱动托管——`IHostedService` start/stop/tick + 六阶段状态机（Boot→…→Stopped）
- **配置**: `apollo::core::config::ConfigRegistry` 键值注册表；热更规划走 tick 边界换 ConfigSnapshot（architecture-review §17.6）

### Game 游戏逻辑
- **AOI系统**: `SceneAoi` 九宫格（Scene 持有、scene_id 隔离）+ `ViewerState` 逐观察者水位；Enter/Sync/Leave 事件分发已交付，网关下发面留 P3-2
- **战斗系统**: `BattleRuntime` 确定性战斗域——五段相位、seed 子流随机、hash 链指纹、经 `IRewardSink` 单向落账；挂接 `Instance`（P2-2 交付）。技能/Buff 玩法面随玩法批
- **属性系统**: `AttributeContainer`/`AttributeManager` 属性容器与定义注册；逐 viewer delta 同步管线未接线（契约 `attr_batch` 已入 messages.xml）
- **场景管理**: 多场景支持 + 换幕（`scene_transfer`，prepare→detach→attach→resume + 失败回滚）

### Network 网络通信
- **传输层**: Socket封装，支持TCP/WebSocket
- **消息层**: 消息路由和处理（现网手写编码；目标 protobuf+zstd，schema 由 def 契约生成，规划）
- **RPC框架**: 远程过程调用框架

### Storage 存储层
- **玩家档案**: JSON 文件档案（tmp+rename 原子写）+ `SaveQueue` 异步保存队列；加载未命中即失败（无默认档 bootstrap）
- **Write-ahead journal**: `PersistJournal`——append 落盘 → 定额 drain → 快照压薄 → 崩溃 replay 续接（P1-4/P1-5）
- **序列化**: 档案 toJson/fromJson 对称序列化；连接池/外部 DB/Redis 客户端为规划态（P1-4 已删五套存量实现）

### Utils 工具库
- **日志系统**: 多级别日志，文件/控制台输出
- **线程池**: 任务调度
- **内存池**: 内存管理
- **配置系统**: `ConfigRegistry` 键值注册表（内存）

### Compatibility 兼容层
- **BigWorld Compatibility Layer**: BigWorld-style C++ API facade (see `docs/zh/33-BigWorld_Compatibility.md`)
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

> 完整可运行版本见 `tests/test_runtime.cpp`（ApplicationHost 用例）。装配只发生在应用入口（apps/），模块内部互不感知容器。

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
#include <apollo/game/battle/battle_runtime.hpp>
#include <apollo/game/core/entity.hpp>
#include <apollo/game/world/instance.hpp>

using namespace apollo::game;
core::PlayerId player_id{1001};

// 奖励单向落账口：战斗结算只出不进，写入玩家长期态
struct ScoreSink : battle::IRewardSink {
    void on_reward(std::uint64_t player_id, std::int64_t score) override {
        // 写入 base::Avatar（mark_dirty）
    }
};

// Instance = 八态生命周期；Waiting 窗口挂接玩法负载，enter 收集参战者
world::Instance instance(1, 1, "demo");
instance.initialize();
instance.ready();

ScoreSink sink;
auto battle = std::make_unique<battle::BattleRuntime>(1, /*seed*/ 42, &sink);
instance.attach_battle(std::move(battle));
instance.enter(player_id);
instance.start();  // → Running（参战者收集完成，战斗开局）

// 确定性 tick：同 seed + 同输入序列 ⇒ 同战果（hash 链滚动指纹；1 = 攻击意图）
instance.battle()->tick(0, {{0, player_id.value(), 1}});
instance.battle()->finish();  // → Rewarding → Finished
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
├── apps/                   # 可执行原型与工具；现状见 docs/zh/apps/index.md
├── modules/                # 模块化源码：base/core/data/game/net/runtime 等
├── include/                # 公共头文件与兼容层头文件
├── src/                    # 公共实现与兼容层实现
├── tests/                  # 测试代码
├── examples/               # 示例代码
├── sdks/                   # Unity 客户端 SDK 与契约工具（contract 契约 + gen 生成器 + cpp 生成物）
├── skds/                   # Cocos / Laya / 历史 SDK 工作区
├── docs/zh/                   # VitePress 文档站点
├── cmake/                  # CMake 辅助脚本
└── build*/                 # 构建输出
```

详细目录说明见 [Directory Structure](docs/zh/Directory_Structure.md)

## 客户端SDK

### Unity SDK

Unity SDK 部分模块已交付（模块状态详见 [SDK_Structure](docs/zh/sdks/unity/SDK_Structure.md)）：

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

- **BigWorld**：BigWorld 风格 C++ API facade（`docs/zh/33-BigWorld_Compatibility.md`）；架构审计含 BigWorld / KBEngine / skynet 三框架源码对照（`docs/zh/analysis/architecture-review.md` §16）
- **KBEngine**：`docs/zh/qa/` 多篇条目为其源码机制分析
- **skynet**：观测通道等设计先例（`docs/zh/analysis/architecture-review.md`）

第三方依赖：[vcpkg](https://github.com/microsoft/vcpkg)（依赖管理）、[nlohmann/json](https://github.com/nlohmann/json)、[protobuf](https://github.com/protocolbuffers/protobuf)（现为依赖与测试，未上消息通路）、[GTest](https://github.com/google/googletest)、OpenSSL、libcurl、pugixml、readline、crashpad、[VitePress](https://vitepress.dev)（文档站）、GitHub Actions 与 Codecov（CI 与覆盖率）。传输层自持（`modules/net/link`），无 nng。

## 联系方式

- 项目主页: https://github.com/cuihairu/apollo
- 问题反馈: [Issues](https://github.com/cuihairu/apollo/issues)
- 邮箱: cuihairu@example.com

---

**Apollo - 实例化多人在线游戏服务器引擎**

## 临时副本生命周期

几名玩家组队发起 → World 创建 Instance 及临时 Scene → 校验成员并进入 → 刷怪与完成目标 → 结算 → 玩家离开 → 销毁 Instance、场景和临时资源。

复用现有 Instance 八态状态机。worldServer 管拓扑，cell 管模拟，Base 管长期资料并通过 database 确认结算。现有测试须重接该边界，持久结算、完整副本迁移和跨 gateway 恢复分别验收。
