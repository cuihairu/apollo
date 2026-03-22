---
title: Runtime Ops Host 设计
icon: screwdriver-wrench
order: 11
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Runtime
  - Ops
  - Debug
---

# Runtime Ops Host 设计

这篇文档解决的是 Apollo 整体框架里一块很容易被低估的区域：

`运维、调试、进程治理、远程控制这些能力，到底应该放在哪一层。`

如果这层不单独定义，最后通常会出现：

- console 散在多个 app
- pidfile、health check、watcher 各自实现
- systemd/supervisor 交互没有统一语义
- tracing、远程控制、调试命令到处挂

这会让整个框架很难收口。

## 一、先说结论

Apollo 应该单独定义一层：

- `Runtime Ops Host`

这层位于：

- `Runtime Host` 之上
- `Platform Foundation` 之下

它不负责游戏业务，而是负责：

- 运行时治理
- 运维观测
- 调试控制
- 外部托管系统交互

## 二、为什么这层必须独立

### 1. 它不是纯工具层

例如：

- pidfile
- console
- health check
- watcher

这些都依赖：

- 当前进程状态
- 当前宿主生命周期
- 当前模块装配结果

所以不能放进 `Base Infrastructure`。

### 2. 它也不是业务域

例如：

- 远程调试
- OTel
- supervisor/systemd 交互
- runtime command bus

这些不属于：

- world
- battle
- social
- task

所以也不能塞进 `Domain Components`。

### 3. 它和部署拓扑不是同一回事

`nginx`、`lvs`、`systemd`、`supervisor` 这些属于外部环境。

Apollo 自身仍然需要一层运行时能力去和它们对接。

## 三、这层的职责

建议至少承接下面 8 类能力：

### 1. 进程宿主治理

- pidfile lifecycle
- 单实例保护
- reload / shutdown 协调
- 进程状态导出

### 2. 托管系统交互

- `systemd notify`
- `supervisor` 约定交互
- Windows Service 对接
- 容器 readiness / liveness

### 3. 控制台与命令总线

- 本地 console
- runtime command bus
- 管理命令注册
- 远程命令入口

### 4. 观测与诊断

- watcher tree
- runtime metrics
- health report
- diagnostics query
- dump / stack / slow-path report

### 5. 调试能力

- 实体查询
- 玩家查询
- route 查询
- world 状态查询
- 调试开关

### 6. Telemetry / OTel

- trace context 注入
- span exporter
- metrics exporter
- log correlation

### 7. 远程控制

- 热更控制
- 降级开关
- 动态配置切换
- 管理命令

### 8. 入口运维适配

- real ip 提取
- `nginx` / `lvs` 健康检查适配
- readiness 协议导出

## 四、推荐对象模型

```text
RuntimeOpsHost
    ├── ProcessGuardian
    ├── ConsoleHost
    ├── CommandBus
    ├── WatcherTree
    ├── DiagnosticsService
    ├── TelemetryBridge
    ├── HealthService
    └── SupervisorAdapter
```

### `ProcessGuardian`

职责：

- pidfile
- 单实例校验
- 启停协调

### `ConsoleHost`

职责：

- 本地命令输入
- 输出渲染

### `CommandBus`

职责：

- 本地和远程命令统一入口
- 命令权限和分发

### `WatcherTree`

职责：

- 暴露结构化运行时状态
- 支持按路径查询

### `DiagnosticsService`

职责：

- dump
- slow call report
- route/debug query

### `TelemetryBridge`

职责：

- OTel tracing / metrics / logs 关联

### `SupervisorAdapter`

职责：

- `systemd`
- `supervisor`
- 容器编排环境

## 五、典型能力归属

下面这些能力建议明确归到这一层：

- console
- pidfile
- systemd/supervisor 交互
- health check
- watcher
- profiler/exporter
- 远程调试
- 远程控制
- OTel runtime instrumentation

## 六、和其他层的边界

### 和 `Runtime Host`

`Runtime Host` 负责：

- 应用和宿主如何运行

`Runtime Ops Host` 负责：

- 这些宿主如何被治理、观测、控制

### 和 `Platform Foundation`

`Runtime Ops Host` 不负责：

- Redis
- SQL
- leaderboard
- distributed lock

这些属于平台基础能力层。

### 和 `Domain Components`

业务域可以注册 watcher 和命令，但不能自己成为运维框架。

## 七、推荐目录

建议未来显式形成：

- `modules/runtime/ops`

推荐结构：

```text
modules/runtime/ops
    ├── include/apollo/runtime/ops/
    ├── src/
    ├── adapters/
    └── tests/
```

## 八、结论

Apollo 如果要把整体框架做稳，`Runtime Ops Host` 必须单独成层。

它不是纯工具层，也不是业务域层，而是：

- 进程治理层
- 运维观测层
- 调试控制层

只有把它独立出来，Apollo 后面在复杂部署、远程诊断和运行治理上才不会继续发散。

## 相关阅读

- [Apollo 分层设计](./apollo-layering-design.md)
- [Apollo 渐进式游戏框架理论设计](./apollo-progressive-game-framework.md)
