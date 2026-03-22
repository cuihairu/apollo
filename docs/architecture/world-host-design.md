---
title: WorldHost 设计稿
icon: host
order: 7
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - WorldHost
  - 设计稿
---

# WorldHost 设计稿

这篇文档是 Apollo 进入代码级重构设计的第一篇。

目标很明确：

`把当前散落在 world/cell 原型里的世界主循环、场景生命周期和服务装配，统一收口到 WorldHost。`

它不是 BigWorld 层的东西，而是 Apollo 在“普通 MMO 模式”下也必须有的基础宿主。

## 一、为什么现在就需要 `WorldHost`

当前 Apollo 已经有：

- `ApplicationHost`
- `ServiceHost`
- `Scene`
- AOI
- `apps/cell-app` 的游戏循环线程

但这些能力还是分散的。

从当前代码看：

- [application_host.hpp](/C:/Users/cui/Workspaces/apollo/modules/runtime/include/apollo/runtime/application_host.hpp)
  - 只提供通用宿主和 `IHostedService::tick()`
- [scene.cpp](/C:/Users/cui/Workspaces/apollo/modules/game/world/src/scene.cpp)
  - 只负责单个 `Scene` 的 entity 生命周期
- [cell_server.cpp](/C:/Users/cui/Workspaces/apollo/apps/cell-app/src/cell_server.cpp)
  - 自己起线程跑 `gameLoop()`
  - 自己管理实体和 AOI
  - 自己承担世界服主循环语义

这说明当前 Apollo 的 world runtime 还没有真正独立出来。

### 当前问题

如果不尽快引入 `WorldHost`，后面会出现几种典型问题：

- 每个 world/cell 类 app 自己起主循环
- scene/map lifecycle 分散在 app 层
- world 级状态和网络服务强耦合
- 后续想引入 `PlayerAnchor`、`WorldAssignment`、`MapInstance` 时没有统一挂载点

所以 `WorldHost` 的意义不是“再造一个 host”，而是：

`把游戏世界进程的运行时语义从 app 壳里抽出来`

## 二、`WorldHost` 的定位

`WorldHost` 应该位于：

- `ApplicationHost` 之上
- `world runtime` 之下

它是：

- 世界进程宿主
- 场景与实例的生命周期驱动器
- world tick 的统一入口
- world services 的装配器

它不是：

- 场景对象本身
- 实体管理器本身
- 网络层本身

更不是：

- 分布式 `CellApp`

## 三、`WorldHost` 在分层里的位置

建议放在：

- `modules/runtime`

建议头文件位置：

- `modules/runtime/include/apollo/runtime/world_host.hpp`

建议实现位置：

- `modules/runtime/src/world_host.cpp`

### 为什么先放 `runtime`

因为当前阶段的 `WorldHost` 本质上是进程宿主，而不是 game 领域对象。

它应该像 `ApplicationHost` 一样负责：

- 生命周期
- service 装配
- world tick 调度
- stop/shutdown 协调

world 领域对象，例如：

- `WorldSpace`
- `MapInstance`
- `WorldSession`

再放在 `modules/game/world`

## 四、`WorldHost` 要解决哪些问题

### 1. 统一 world tick

当前 `cell_server.cpp` 里已经有 `gameLoop()` 线程，但这个模式不能长期扩散。

`WorldHost` 应该统一提供：

- 固定 tick rate
- tick delta
- tick 开始/结束回调
- 统计 tick load

### 2. 统一 world services 装配

世界服会逐步拥有很多服务：

- map/scene manager
- entity runtime
- AOI
- replication
- session binding
- combat

这些不应该全挤在 `CellServer` 这种 app 壳里。

`WorldHost` 应该成为它们的统一挂载点。

### 3. 统一场景生命周期

Apollo 后面一定会有：

- 创建实例
- 销毁实例
- 进入地图
- 离开地图
- world stop 时统一清理

这些都应该由 `WorldHost` 驱动，而不是每个 app 单独做。

### 4. 为 `PlayerAnchor` 和 `WorldAssignment` 留接口

将来 world 进程一定需要承接：

- 玩家进入世界
- 玩家切换实例
- 玩家从 gateway 路由到 world

`WorldHost` 必须有明确接口接住这些流程。

## 五、建议的对象关系

建议先形成如下关系：

```text
ApplicationHost
    └── WorldHost
            ├── WorldService(s)
            ├── MapInstanceManager
            ├── WorldSpace(s)
            ├── SessionBinder
            └── TickScheduler
```

### 关系说明

#### `ApplicationHost`

负责应用级生命周期：

- boot
- config loaded
- initialized
- ready
- stopping

#### `WorldHost`

负责世界进程级生命周期：

- world init
- world tick
- map/space startup
- world shutdown

#### `WorldService`

负责挂载世界内服务，例如：

- AOI service
- scene service
- combat service

#### `MapInstanceManager`

负责：

- 创建/查找/销毁地图实例

#### `WorldSpace`

负责：

- 单个空间内 entity 容器与 world 语义

## 六、推荐接口草案

下面不是最终代码，只是接口方向。

```cpp
namespace apollo::runtime {

struct WorldTickContext {
    uint64_t tick_index = 0;
    double delta_seconds = 0.0;
    int64_t now_ms = 0;
};

class IWorldService {
public:
    virtual ~IWorldService() = default;
    virtual std::string_view service_name() const = 0;
    virtual bool initialize() = 0;
    virtual void tick(const WorldTickContext& ctx) = 0;
    virtual void shutdown() = 0;
};

class WorldHost : public IHostedService {
public:
    bool start() override;
    void stop() override;
    bool is_running() const override;
    void tick() override;

    void set_tick_rate(uint32_t hz);
    void add_world_service(std::shared_ptr<IWorldService> service);

    const WorldTickContext& tick_context() const;
};

} // namespace apollo::runtime
```

### 核心点

#### `WorldHost` 仍然实现 `IHostedService`

这样它就能直接挂到现有 `ApplicationHost` 里，不需要推翻当前 runtime。

#### `WorldTickContext`

不要让各 world service 自己算时间和 tick 序号。

统一上下文是后续：

- 统计
- 回放
- 调试

的基础。

#### `IWorldService`

建议和 `IHostedService` 区分开。

因为：

- `IHostedService` 是应用级
- `IWorldService` 是世界级

它们生命周期粒度不同。

## 七、`WorldHost` 要托管哪些对象

第一阶段不要塞太多，但至少要能托管这几类对象。

### 1. `MapInstanceManager`

职责：

- 创建实例
- 查询实例
- 销毁实例
- update 全部实例

建议落点：

- `modules/game/world/include/apollo/game/world/map_instance_manager.hpp`

### 2. `WorldSpace`

职责：

- entity 容器
- scene/map 内更新
- AOI 集成
- 基础广播

建议落点：

- `modules/game/world/include/apollo/game/world/world_space.hpp`

### 3. `SessionBinder`

职责：

- world 内 session -> player/entity 映射

建议落点：

- `modules/game/world/include/apollo/game/world/session_binder.hpp`

### 4. `ReplicationService`

第一阶段可以非常薄，但建议先留接口。

职责：

- 面向客户端的状态广播
- world 内更新聚合

## 八、`WorldHost` 和当前 `CellServer` 的关系

当前 `apps/cell-app` 的 `CellServer` 里做了三类事：

1. 网络消息入口
2. 实体/AOI 管理
3. 游戏循环线程

其中第 3 类最应该先抽走。

### 改造后的关系建议

改造前：

```text
CellServer
    ├── RPC server
    ├── EntityManager
    ├── AOIManager
    └── gameLoop thread
```

改造后：

```text
CellServer (或未来 WorldServer)
    ├── RPC server
    └── WorldHost
            ├── MapInstanceManager
            ├── Entity runtime
            └── AOI service
```

### 当前阶段怎么做最稳

短期不要求立即删除 `CellServer`，而是：

- 保留它作为 app 壳
- 把 world loop 收到 `WorldHost`
- 把 entity/aoi 管理逐步迁到 world services

## 九、`WorldHost` 不应该做什么

为了避免它变成新的大杂烩，边界必须写清楚。

### 1. 不直接承载具体战斗逻辑

战斗应该是 world service，不应该写进 host。

### 2. 不直接承载数据库访问

持久化是外部服务或 anchor/base 层的事情，不应该进 world host。

### 3. 不直接处理公网协议

公网协议属于 gateway/connection 层。

`WorldHost` 接到的应该是 world 语义请求，而不是裸 socket 细节。

### 4. 不直接做 distributed `CellApp`

这点最重要。

`WorldHost` 是普通 MMO 和 BigWorld 模式共用底座。

将来即使做真正 `CellApp`，它也应该是：

- `CellApp = WorldHost + distributed space extension`

而不是把 `WorldHost` 本身写成 distributed node。

## 十、第一阶段最小可交付版本

为了避免设计稿过大，建议第一阶段只做到：

### 能力范围

- `WorldHost` 能挂到 `ApplicationHost`
- 可设置 tick rate
- 可注册多个 `IWorldService`
- tick 时统一调用 world services
- stop 时统一 shutdown

### 暂不要求

- 多线程 world partition
- distributed world
- witness/ghost
- 复杂 replication pipeline

### 最小验证方式

建议验证场景：

1. `WorldHost` 启动
2. 注册一个 `SceneService`
3. 注册一个 `AOIService`
4. 每 tick 推进 `Scene`
5. 关闭时按顺序回收

只要这条链跑通，Apollo 的普通 MMO world runtime 就算有了真正起点。

## 十一、和后续 `PlayerAnchor` 的衔接

`WorldHost` 落地后，下一个最自然的对象就是 `PlayerAnchor`。

因为那时 world 进程已经有了稳定宿主，就可以很自然地接入：

- 玩家进入 world
- 玩家离开 world
- 玩家当前 world 归属

所以建议顺序是：

1. `WorldHost`
2. `MapInstance / WorldSpace`
3. `PlayerAnchor`
4. `SessionLocator / WorldAssignment`

## 十二、结论

`WorldHost` 是 Apollo 当前最值得先实现的世界运行时骨架。

它的价值不是增加一个新类，而是把：

- world tick
- world service
- scene/map 生命周期
- world 进程宿主语义

从当前的 app 原型里抽出来，形成稳定框架层。

只要这一层没有收住，后面不管是普通 MMO 模式，还是 KBE/BigWorld 模式，都会继续长在不稳定地基上。

## 相关阅读

- [Apollo 世界架构路线](./apollo-world-architecture.md)
- [Apollo 架构差距分析](./apollo-architecture-gap-analysis.md)
- [Apollo 重构路线图](./apollo-refactor-roadmap.md)
- [PlayerAnchor 设计稿](./player-anchor-design.md)
- [World 进入与切图设计](./world-entry-transfer-design.md)
