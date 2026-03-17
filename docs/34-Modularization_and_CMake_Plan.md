# Apollo 模块化与 CMake 拆分落地方案

## 目标

当前工程已经具备 `starter`、`net`、`database`、`redis`、`game` 等方向，但构建层仍以单体 `apollo` 静态库为主，源码中还存在新旧路径并存的问题，例如：

- `src/network` 与 `src/apollo/net`
- `src/storage/database` 与 `src/apollo/database`
- `src/apollo/storage/redis` 与 `src/apollo/redis`

这会带来三个直接问题：

1. 模块边界不清晰，难以按需组装。
2. 新能力只能继续堆进单体 target。
3. 应用、框架、业务领域无法稳定分层。

本方案的目标不是一次性重写，而是在保留现有可构建主干的前提下，引入可增量迁移的模块化骨架。

## 分层原则

### `base`

定位：纯基础能力，不携带 Apollo 框架语义。

放入内容：

- 时间、字符串、错误码、结果类型
- buffer、memory pool、lock-free queue
- 线程池、基础并发原语
- terminal/console 的底层终端适配

约束：

- 不知道 application、module、service、starter 这些概念
- 不依赖 `core`、`runtime`、`game`

### `core`

定位：Apollo 框架内核，负责进程内公共运行语义。

放入内容：

- `ApplicationContext`
- 配置管理
- 日志管理
- 生命周期
- 事件总线
- 模块管理
- 指标、服务发现抽象接口

约束：

- 可以依赖 `base`
- 不依赖 `game`
- 不直接依赖具体 MySQL/Redis/HTTP 实现

### `runtime`

定位：宿主运行时，负责“程序怎么跑起来”。

放入内容：

- `ApplicationHost`
- `ServiceHost`
- tick loop
- shutdown hook
- signal / console event loop
- 进程级健康检查

约束：

- 依赖 `core`
- 不直接承载具体业务玩法

### `net`

定位：网络能力层，负责收发、协议、连接管理。

建议拆分：

- `net/core`
- `net/tcp`
- `net/http`
- `net/websocket`
- `net/rpc`

约束：

- `net/core` 只放抽象和公共机制
- 具体传输协议各自独立
- 统一收敛 `src/network` 与 `src/apollo/net`

### `data`

定位：数据访问层，负责数据库、缓存、序列化接入。

建议拆分：

- `data/core`
- `data/orm`
- `data/redis`

语义边界：

- `data/core` 定义 `DataSource`、`Session`、`Transaction`、`Repository`
- `data/orm` 提供 SQL/ORM/MySQL 实现
- `data/redis` 提供 Redis client、cache primitive、排行榜、分布式锁 primitive

### `game`

定位：游戏领域层，按业务语义而不是技术类型拆分。

建议拆分：

- `game/core`
- `game/world`
- `game/battle`
- `game/social`
- `game/economy`

示例：

- AOI、Scene、Map、Movement 放 `game/world`
- Attribute、Skill、Buff、Damage pipeline 放 `game/battle`
- Chat、Guild、Team 放 `game/social`

### `starter`

定位：装配层，类似 Spring Boot starter，但采用 C++ 显式注册模式。

建议拆分：

- `starter/core`
- `starter/net`
- `starter/mysql`
- `starter/redis`
- `starter/game-server`

职责：

- 判断条件
- 注册模块
- 注入默认配置
- 组合宿主运行时

### `apps`

定位：最终可执行程序。

建议目录：

- `apps/gateway-server`
- `apps/login-server`
- `apps/game-server`
- `apps/chat-server`

职责：

- 组合所需 starter / module
- 提供进程入口
- 注册该程序专属命令和运维接口

## 依赖方向

推荐固定如下依赖方向：

```text
base
  ^
core
  ^
runtime

net/* ----\
data/* ----> game/*

starter/* -> core/runtime/net/data/game
apps/*    -> starter/* + 少量直接模块
```

硬约束：

- `base` 不依赖 Apollo 其他层
- `core` 不依赖 `game`
- `data` 不依赖 `game`
- `game` 不反向进入 `core`

## 目录规划

目标目录如下：

```text
apollo/
  modules/
    base/
    core/
    runtime/
    net/
      core/
      tcp/
      http/
      websocket/
      rpc/
    data/
      core/
      orm/
      redis/
    game/
      core/
      world/
      battle/
      social/
      economy/
    starter/
      core/
      net/
      mysql/
      redis/
      game-server/
  apps/
    gateway-server/
    login-server/
    game-server/
  tests/
```

说明：

- `modules/` 是新的模块化构建入口
- 当前根目录单体 `apollo` target 暂时保留
- 后续每迁移出一个模块，就从根 target 中删掉对应源码

## CMake 目标命名规则

推荐采用稳定、可预期的 target 名称：

- `apollo_base`
- `apollo_core`
- `apollo_runtime`
- `apollo_net_core`
- `apollo_net_tcp`
- `apollo_net_http`
- `apollo_net_websocket`
- `apollo_net_rpc`
- `apollo_data_core`
- `apollo_data_orm`
- `apollo_data_redis`
- `apollo_game_core`
- `apollo_game_world`
- `apollo_game_battle`
- `apollo_starter_core`
- `apollo_starter_mysql`
- `apollo_starter_redis`

这样有三个好处：

1. target 名称与目录一一对应。
2. 依赖关系可读。
3. 最终 app 的链接清单就是装配清单。

## 顶层 CMake 演进策略

顶层 `CMakeLists.txt` 的职责应收敛为：

1. 定义全局选项和编译标准
2. 提供现有单体 `apollo` target
3. 引入 `modules/` 和 `apps/` 两个新入口

当前阶段采用“骨架先行”策略：

- 保留已有 `apollo` target，避免一次性中断现有开发
- 增加 `APOLLO_ENABLE_MODULAR_LAYOUT`
- 增加 `APOLLO_BUILD_APPS`
- 新建 `modules/CMakeLists.txt`
- 新建 `apps/CMakeLists.txt`

这样后续可以逐个模块迁移，而不是一次性大爆炸式改造。

## 模块迁移顺序

推荐按风险从低到高迁移：

1. `base`
2. `core`
3. `runtime`
4. `data/core`
5. `data/orm`
6. `data/redis`
7. `net/core`
8. `net/tcp/http/websocket/rpc`
9. `game/core`
10. `game/world`
11. `game/battle`
12. `starter/*`
13. `apps/*`

原因：

- 先把底层通用能力稳定下来
- 再抽宿主与接入层
- 最后迁移强业务语义层

## 每个阶段的验收标准

### 第一阶段

- 顶层 CMake 已支持 `modules/` 和 `apps/`
- 架构边界形成文档
- 现有单体 target 仍能继续工作

### 第二阶段

- 抽出首批真实模块 target，例如 `apollo_base`、`apollo_core`
- 单元测试可以直接链接模块 target

### 第三阶段

- `apps/game-server` 能通过 starter/module 组合运行
- 根目录单体 `apollo` target 缩减为兼容聚合 target 或被淘汰

## console / terminal / cli 的归属

为了避免把终端适配、控制台事件和运维命令混在一起，建议拆为三层：

- `modules/base/terminal`
  - 终端输出、颜色、按键、ANSI/Windows 兼容
- `modules/runtime/console`
  - stdin 监听、Ctrl+C、关闭窗口、console event loop
- `modules/core/cli`
  - 命令注册、参数解析、命令调度

具体业务命令不放公共模块，放到具体 app：

- `apps/game-server/console`

## 当前仓库下一步建议

紧接着的落地工作建议是：

1. 先创建 `modules/base`、`modules/core`、`modules/runtime` 的空 target。
2. 从旧代码里优先迁移：
   - 通用工具类到 `base`
   - DI、生命周期、配置、日志到 `core`
3. 新建 `apps/game-server`，只做最小宿主程序，不承载业务实现。
4. 将现有 `GameServer` 从“框架总入口”降级为 runtime/game-server host 的一部分。
