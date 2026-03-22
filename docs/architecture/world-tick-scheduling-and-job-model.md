---
title: World Tick、Scheduling 与 Job Model 设计
icon: clock
order: 25
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - WorldTick
  - Scheduling
  - JobModel
---

# World Tick、Scheduling 与 Job Model 设计

这篇文档解决的是 Apollo 世界运行时里另一块最关键的基础问题：

`世界主循环到底怎么跑，任务调度、作业模型、并发边界如何设计。`

如果这层不先定清楚，后面很容易出现：

- world tick 不统一
- 生命周期和业务逻辑在任意线程乱切
- AOI、战斗、脚本、复制链路互相抢线程
- 普通 MMO 和 BigWorld 模式在调度上没有统一基础

## 一、设计目标

这层设计要解决 6 个问题：

1. 世界主循环如何统一。
2. 哪些逻辑必须在主线程/主 tick 内执行。
3. 哪些逻辑可以分发成 job。
4. 如何避免脚本、AOI、复制和持久化互相阻塞。
5. 如何兼容普通 MMO 和 BigWorld 模式。
6. 如何兼容 ECS/数据导向热点系统。

## 二、参考来源

### 1. 参考经典 world tick 模型

参考点：

- 固定步长或半固定步长更新
- 明确 update 顺序

### 2. 参考 KBE 的世界运行经验

参考点：

- Cell 侧是空间内实时权威节点
- 生命周期、可见集、迁移边界必须有统一更新点

不照搬点：

- 不把所有逻辑都硬塞进单一重对象循环

### 3. 参考 ECS / job system 的经验

参考点：

- 局部热点系统适合批处理
- 热路径可以拆成 job

## 三、为什么这样设计

Apollo 后续要同时承载：

- world runtime
- player lifecycle
- combat
- AOI
- replication
- script execution
- distributed world extension

如果没有明确调度模型，系统最终会变成：

- 逻辑到处跑
- 顺序不清楚
- 数据竞争越来越多

更合理的方式应该是：

- `World Tick` 作为主序列化边界
- `Scheduling` 负责时序和阶段控制
- `Job Model` 负责局部可并行的计算

## 四、优点

- 世界更新顺序清晰
- 并发边界可控
- 更利于脚本、AOI、复制链路协同
- 更适合普通 MMO 和 BigWorld 统一基础模型

## 五、代价与风险

- 调度模型设计会变复杂
- 需要明确 job 可并行边界
- 如果拆得不好，会出现同步成本反而更高

## 六、为什么不选其他方案

### 不选“所有逻辑都单线程串行”

因为后续一旦到大规模世界和战斗热点，吞吐会很快碰顶。

### 不选“所有逻辑都并发 job 化”

因为很多在线状态变更本身就要求强顺序边界。

### 不选“完全按脚本回调驱动”

因为脚本不是世界调度器，不能反过来统治宿主运行时。

Apollo 更合理的路线是：

- 主 tick 保序
- 局部 job 并行

## 七、推荐总体模型

建议 Apollo 的世界运行模型是：

```text
WorldHost
    -> TickCoordinator
        -> Phase Scheduler
            -> Job Dispatcher
```

### 含义

- `WorldHost`
  - 世界宿主
- `TickCoordinator`
  - 推进每一帧/每一 tick
- `Phase Scheduler`
  - 管理更新阶段
- `Job Dispatcher`
  - 分发可并行作业

## 八、推荐 tick 阶段

建议 Apollo 至少把一个世界 tick 拆成下面 8 段。

1. `Ingress`
2. `Lifecycle`
3. `Simulation`
4. `AOI`
5. `ReplicationPrepare`
6. `ScriptCallbacks`
7. `Egress`
8. `Commit`

## 九、各阶段说明

### 1. `Ingress`

职责：

- 收取客户端输入
- 收取远程调用结果
- 收取跨模块命令和事件

### 2. `Lifecycle`

职责：

- 处理进入世界
- 离开世界
- 销毁
- 切图
- 迁移状态推进

### 3. `Simulation`

职责：

- movement
- combat
- attribute
- AI

这部分最适合局部 job 化。

### 4. `AOI`

职责：

- 更新空间可见关系
- 生成 enter / leave 候选集

### 5. `ReplicationPrepare`

职责：

- 收集 dirty entity
- 构造 delta
- 组装 witness 可见集

### 6. `ScriptCallbacks`

职责：

- 执行业务规则回调
- 触发脚本逻辑

### 7. `Egress`

职责：

- 下发复制包
- 下发 world 结果
- 推送跨模块事件

### 8. `Commit`

职责：

- 完成本 tick 状态提交
- 清理临时缓冲
- 推进 tick 计数

## 十、哪些逻辑必须保序

建议下面这些逻辑默认保持在主 tick 边界内保序执行：

- 生命周期状态切换
- world assignment 边界
- Avatar 的进入/离开世界
- authority 迁移确认
- 关键复制提交边界

### 为什么

因为这些逻辑如果乱并发，会直接打坏在线状态一致性。

## 十一、哪些逻辑适合 job 化

建议下面这些更适合局部 job 化：

- movement 批量更新
- combat 批量计算
- attribute evaluation
- AI 局部系统更新
- 某些 AOI 候选计算

### 前提

- 输入数据稳定
- 输出汇总点明确
- 不直接跨宿主改权威状态

## 十二、脚本调度边界

脚本不应直接插进任意线程。

更合理的方式是：

- 在明确的 tick 阶段执行脚本回调
- 由宿主控制 deadline 和超时

### 设计结论

- 脚本参与业务逻辑
- 但不参与调度决策

## 十三、普通 MMO 与 BigWorld 的差异

### 普通 MMO 模式

重点是：

- 单 world tick
- 单 map instance 或少量实例
- 无 distributed cell 迁移

### BigWorld 模式

会增加：

- partition 级 tick 协调
- ghost / witness / migration job
- authority transfer 阶段

### 设计结论

Apollo 应让普通 MMO 先跑通这套主 tick，

然后 BigWorld 再在其上叠加分布式阶段，而不是另起一套调度模型。

## 十四、推荐对象模型

```text
TickCoordinator
    ├── PhaseScheduler
    ├── JobDispatcher
    ├── TickContext
    ├── PhaseMetrics
    └── BackpressureController
```

### `TickContext`

职责：

- tick id
- delta time
- frame budget
- current phase

### `PhaseMetrics`

职责：

- 记录每阶段耗时
- 为 ops 和诊断导出数据

### `BackpressureController`

职责：

- 当某些阶段超预算时做退让策略

例如：

- 降低某些低优先级复制频率
- 延后某些旁路任务

## 十五、与 `Testing / Verification` 的关系

这层必须对应：

- phase order test
- job safety test
- tick budget test
- scenario test

否则调度层很容易写出“看起来正确，运行时经常抖”的系统。

## 十六、对当前 Apollo 的直接含义

Apollo 下一步如果继续往代码推进，建议优先补：

- `TickCoordinator`
- `PhaseScheduler`
- `TickContext`
- `PhaseMetrics`

优先让：

- world lifecycle
- combat
- replication

这三条主链都挂进统一 tick。

## 十七、结论

Apollo 的世界运行时如果要真正稳定，不能只有 `WorldHost`，还必须有：

- 统一 tick
- 明确 phase
- 局部 job 模型
- 脚本和复制链路的固定调度边界

这样普通 MMO 和 BigWorld 模式才能共用一条清晰的世界运行主链。

## 相关阅读

- [WorldHost 设计稿](./world-host-design.md)
- [Replication Pipeline 设计](./replication-pipeline-design.md)
- [Entity Lifecycle 与 State Machine 设计](./entity-lifecycle-and-state-machine-design.md)
