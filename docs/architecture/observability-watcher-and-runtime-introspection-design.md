---
title: Observability、Watcher 与 Runtime Introspection 设计
icon: binoculars
order: 30
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Observability
  - Watcher
  - Introspection
---

# Observability、Watcher 与 Runtime Introspection 设计

这篇文档解决的是 Apollo 整体框架继续往运行治理闭环推进时，一个必须尽早定下来的问题：

`系统运行时到底应该如何被观察、查询、诊断和解释。`

如果这层不清楚，后面会很容易出现：

- 指标有一部分
- 日志有一部分
- watcher 有一部分
- 调试查询又是另一套

最后变成：

- 出问题时没有统一入口
- 文档里有分层，运行时里看不到分层

## 一、设计目标

这层设计要解决 6 个问题：

1. 运行时状态如何结构化暴露。
2. 指标、日志、trace、watcher、debug query 如何统一。
3. 模块、宿主、业务域、世界实例如何形成可查询树。
4. 如何支持本地 console、远程调试和运维观测共用一套视图。
5. 如何让普通 MMO 和 BigWorld 模式共用一套运行观测语言。
6. 如何避免观测能力继续散落在各个模块内部。

## 二、参考来源

### 1. 参考 KBE 的 watcher 思路

参考点：

- 结构化运行时观察树
- 按路径查询状态
- 统一暴露内部运行信息

不照搬点：

- 不把 watcher 做成只服务某一类进程和某一类对象

### 2. 参考现代 observability 思路

参考点：

- metrics
- logs
- traces
- runtime introspection

### 3. 参考 Apollo 新分层

参考点：

- `Runtime Ops Host`
- `Platform Foundation`
- `Game Foundation`
- `Domain Components`

观测应该映射到这些分层，而不是脱离架构设计自成一套。

## 三、为什么这样设计

Apollo 现在已经明确要支持：

- runtime / ops
- platform
- script
- world
- session
- battle
- distributed world

如果没有统一的观测树，后面这些能力只会越来越散。

更合理的方式是：

- `Watcher Tree`
  - 负责结构化状态视图
- `Metrics`
  - 负责聚合指标
- `Tracing`
  - 负责链路和因果关系
- `Runtime Introspection`
  - 负责诊断查询和在线解释

## 四、优点

- 运行时视图更统一
- 更适合 console、远程控制、ops 和测试联动
- 更容易定位跨层问题
- 更容易把文档分层映射到运行时

## 五、代价与风险

- 需要维护统一路径命名规则
- 如果暴露面过大，会带来性能和安全压力
- 如果每层都随意挂 watcher，会失去一致性

## 六、为什么不选其他方案

### 不选“只有 metrics，没有结构化 watcher”

因为很多问题不是简单数值指标能解释的。

### 不选“只有日志，没有 introspection”

因为日志无法替代当前时刻的运行态查询。

### 不选“每个模块自己定义调试接口”

因为最终会失去统一观测语言。

## 七、推荐总体模型

建议 Apollo 的观测层采用：

```text
Observability Layer
    ├── Watcher Tree
    ├── Metrics Export
    ├── Trace Bridge
    ├── Runtime Query
    └── Debug Actions
```

### 含义

- `Watcher Tree`
  - 结构化状态树
- `Metrics Export`
  - 聚合指标输出
- `Trace Bridge`
  - 统一 trace 接入
- `Runtime Query`
  - 在线诊断查询
- `Debug Actions`
  - 有约束的调试动作

## 八、Watcher Tree 的推荐结构

建议 watcher 至少按下面层级组织：

```text
/runtime
/ops
/platform
/script
/session
/world
/battle
/distributed
```

### 示例路径

- `/runtime/host/state`
- `/ops/health/readiness`
- `/platform/redis/pool`
- `/world/instances/{id}/entities`
- `/session/anchors/{playerId}`
- `/battle/runtime/queues`

### 为什么必须路径化

因为 watcher 不只是“打印几个变量”，而是要支持：

- 查询
- 导航
- 聚合
- 权限控制

## 九、推荐节点类型

Watcher 节点不应只有一种。

建议至少支持：

### 1. Value Node

适合：

- 当前数值
- 状态字符串

### 2. Collection Node

适合：

- entity list
- instance list
- route table

### 3. Computed Node

适合：

- 当前负载
- tick budget 占用
- AOI 命中率

### 4. Action Node

适合：

- dump
- reload
- trace toggle

### 原则

`Action Node` 必须严格受控，不应默认向任何人开放。

## 十、Metrics 与 Watcher 的关系

这两者不能混成一层。

### Metrics

适合：

- 聚合数值
- 时间序列
- 告警

### Watcher

适合：

- 当前运行态结构化视图
- 在线诊断

### 设计结论

Apollo 应该让：

- metrics 解决“趋势”
- watcher 解决“现状”

## 十一、Tracing 与 Runtime Introspection 的关系

### Tracing

回答的是：

- 一条链路怎么走过来的

### Introspection

回答的是：

- 当前系统现在是什么状态

二者应该互补，不应相互替代。

## 十二、推荐运行时查询模型

建议 Apollo 明确区分两类查询：

### 1. Passive Query

只读查询。

例如：

- 查询某个 world instance 当前实体数
- 查询某个 anchor 当前状态

### 2. Controlled Action

受控动作。

例如：

- 触发 dump
- 切换 trace sampling
- 触发某模块 reload

### 为什么要分开

因为读和改在运维治理上风险不同。

## 十三、普通 MMO 与 BigWorld 的差异

### 普通 MMO 模式

重点观察：

- session
- world instance
- anchor
- tick
- replication

### BigWorld 模式

会额外增加：

- partition
- ghost route
- authority transfer
- distributed witness

### 设计结论

Apollo 应该让：

- 普通 MMO 的 watcher 树先稳定
- BigWorld 在此基础上扩展 distributed 分支

## 十四、推荐对象模型

```text
ObservabilityService
    ├── WatcherRegistry
    ├── MetricRegistry
    ├── TraceBridge
    ├── RuntimeQueryService
    ├── DebugActionRegistry
    └── AccessController
```

### `WatcherRegistry`

职责：

- 节点注册
- 路径解析
- 树结构维护

### `RuntimeQueryService`

职责：

- 统一查询入口
- 本地 console 和远程查询共用

### `AccessController`

职责：

- 权限校验
- 只读/动作分级

## 十五、和测试策略的关系

这层应对应：

- watcher path contract test
- metrics mapping test
- runtime query assembly test
- scenario diagnostics test

否则观测层很容易变成：

- 设计很好看
- 运行时却没人真正使用

## 十六、对当前 Apollo 的直接含义

Apollo 下一步如果继续往代码推进，建议优先补：

- `WatcherRegistry`
- `RuntimeQueryService`
- `MetricRegistry`
- `TraceBridge`

并优先让：

- runtime
- ops
- world
- session

这四类模块先挂到统一 watcher 树上。

## 十七、结论

Apollo 的运行观测层不能只靠日志和指标拼起来。

更合理的方式是：

- 用 `Watcher Tree` 表达结构化当前态
- 用 `Metrics` 表达趋势
- 用 `Tracing` 表达因果链路
- 用 `Runtime Introspection` 提供统一诊断入口

只有这样，整套框架才算真正具备可治理性。

## 相关阅读

- [Runtime Ops Host 设计](./runtime-ops-host-design.md)
- [Testing 与 Verification 策略](./testing-and-verification-strategy.md)
- [App Bootstrap 生命周期设计](./app-bootstrap-lifecycle-design.md)
