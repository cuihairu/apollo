---
title: Navigation、Movement 与 Physics Boundary 设计
icon: route
order: 28
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Navigation
  - Movement
  - Physics
---

# Navigation、Movement 与 Physics Boundary 设计

这篇文档解决的是 Apollo 世界运行时继续往下细化时，另一个很容易失控的问题：

`导航、移动、物理边界到底应该怎么拆，哪些能力应该放在宿主里，哪些能力应该放在系统里。`

如果这层不清楚，后面很容易出现：

- 移动逻辑直接和 AvatarEntity 糊在一起
- 导航系统和 AOI、战斗、复制互相交叉依赖
- 物理检测和世界权威边界不清
- 不同游戏形态下移动模型没法统一

## 一、设计目标

这层设计要解决 6 个问题：

1. 导航、移动、物理三者的边界如何定义。
2. 哪些能力应该由 world/runtime 宿主控制。
3. 哪些能力适合数据导向系统实现。
4. 如何兼容普通 MMO 和 BigWorld 模式。
5. 如何兼容房间制、分线制、开放世界等不同游戏形态。
6. 如何避免把“移动”理解成单一位置更新逻辑。

## 二、参考来源

### 1. 参考 MMO 世界运行经验

参考点：

- 移动是世界权威的一部分
- 导航和空间边界是世界语义，不是纯表现问题

### 2. 参考 KBE / BigWorld 的空间语义

参考点：

- 位置和空间权威必须在 cell/world 宿主边界内明确
- 空间几何、导航、可见性和迁移互相关联

不照搬点：

- 不把全部移动控制都做成重对象方法链

### 3. 参考现代数据导向思路

参考点：

- movement update 可批量化
- 某些碰撞/检测计算可 job 化

## 三、为什么这样设计

Apollo 后面已经明确有：

- `World Tick`
- `Interest Management`
- `Replication Pipeline`
- `Combat Runtime`

移动与导航如果没有明确边界，就会变成这几条链的耦合点。

更合理的方式应该是：

- `Navigation`
  - 回答“能不能走、该怎么走”
- `Movement`
  - 回答“当前怎么移动、状态怎么推进”
- `Physics Boundary`
  - 回答“哪些碰撞和空间规则由服务端权威控制”

## 四、优点

- 世界语义更清楚
- 更容易和 AOI、战斗、复制配合
- 给不同游戏形态留出扩展空间
- 给数据导向优化留出空间

## 五、代价与风险

- 边界比简单 scene entity 更复杂
- 需要定义“权威物理”的范围
- 如果切得不好，会出现重复计算或双重状态源

## 六、为什么不选其他方案

### 不选“移动=直接改 position”

因为这无法表达导航、碰撞、迁移、状态切换等真实问题。

### 不选“全量完整物理引擎服务器化”

因为很多 MMO 并不需要把完整客户端物理全部复制到服务端。

### 不选“导航、移动、物理完全不分层”

因为这样最终会让 world runtime 越来越乱。

## 七、推荐总体模型

建议 Apollo 的移动主链采用：

```text
Navigation Service
    -> Movement Runtime
    -> Physics Boundary Checks
    -> Interest / Replication Update
```

### 含义

- `Navigation Service`
  - 路径与可达性
- `Movement Runtime`
  - 移动状态推进
- `Physics Boundary Checks`
  - 服务器权威边界校验
- `Interest / Replication Update`
  - 推进可见集和同步

## 八、推荐边界定义

## 九、`Navigation`

`Navigation` 负责：

- 路径查询
- 可达性判断
- 地图/导航网格读取
- 简单局部避障输入

### 不应该负责什么

- 不直接推进实体位置
- 不直接做复制
- 不直接承接 Avatar 生命周期

## 十、`Movement`

`Movement` 负责：

- 速度
- 朝向
- 位移推进
- 移动状态
- 冲刺、停止、转向、跳跃等状态变化

### 更适合数据导向的部分

- 批量位移推进
- 批量速度和方向更新
- 批量移动状态处理

## 十一、`Physics Boundary`

这里必须先说清楚：

Apollo 的服务端“物理”更合理的理解是：

- 权威边界校验

而不是：

- 全功能物理引擎模拟

### 这层更适合承接的

- 穿墙校验
- 越界校验
- 地图阻挡校验
- 地形可达性约束
- 服务端命中边界校验

### 不一定要承接的

- 完整刚体模拟
- 完整客户端表现物理
- 所有粒度的碰撞细节

## 十二、推荐对象模型

```text
MovementService
    ├── NavigationProvider
    ├── MovementStateStore
    ├── MotionIntegrator
    ├── CollisionBoundaryChecker
    ├── MovementEventEmitter
    └── MovementReplicationBinder
```

### `NavigationProvider`

职责：

- 路径查询
- 导航网格访问

### `MotionIntegrator`

职责：

- 推进位移
- 计算下一位置和速度状态

### `CollisionBoundaryChecker`

职责：

- 做最小必要的服务端权威校验

### `MovementEventEmitter`

职责：

- 推出：
  - started moving
  - stopped
  - teleport
  - blocked

## 十三、推荐状态模型

建议移动状态至少显式支持：

- `Idle`
- `Moving`
- `Turning`
- `Blocked`
- `Teleporting`
- `Falling`（如果游戏需要）

### 为什么要显式状态

因为很多系统都会依赖移动状态：

- AOI
- combat
- replication
- animation / action state

## 十四、普通 MMO 与 BigWorld 的差异

### 普通 MMO 模式

通常：

- 导航和移动只在单 world instance 内闭环
- 空间边界切换靠切图 / 传送

### BigWorld 模式

会增加：

- partition 边界移动
- authority transfer 前后的移动一致性
- ghost 附近的近场表现连续性

### 设计结论

Apollo 应先把：

- 单 world 导航/移动/边界校验

跑通，

再在 BigWorld 模式扩：

- migration-aware movement
- partition-aware boundary checks

## 十五、和 AOI / Interest 的关系

移动是 interest system 的重要输入。

### 设计原则

- movement 产生位置变化
- AOI/interest 根据位置变化更新候选
- witness / replication 再输出同步结果

不应反过来让 AOI 驱动移动主逻辑。

## 十六、和 Combat 的关系

移动和战斗经常互相影响，但两者不应互相吞掉。

### 设计原则

- combat 可读取 movement state
- movement 可受控制效果影响
- 但 combat runtime 不应直接成为 movement runtime

## 十七、和脚本层的关系

脚本适合写：

- 技能引起的移动规则
- 特殊位移效果
- 某些行为树移动决策

不适合写：

- 高频 movement integration 主循环
- 大量边界检测热路径

## 十八、对当前 Apollo 的直接含义

Apollo 下一步如果继续往代码层推进，建议优先补：

- `NavigationProvider`
- `MovementService`
- `MovementStateStore`
- `CollisionBoundaryChecker`

先把移动主链作为 `World Tick` 的 `Simulation` 子阶段收口。

## 十九、结论

Apollo 的移动设计如果要真正稳，必须把：

- 导航
- 移动
- 物理边界校验

拆成三块。

这样既能保持世界权威边界清楚，也能给不同游戏类型和数据导向优化留出空间。

## 相关阅读

- [World Tick、Scheduling 与 Job Model 设计](./world-tick-scheduling-and-job-model.md)
- [Interest Management 与 AOI Pipeline 设计](./interest-management-and-aoi-pipeline-design.md)
- [Combat Runtime 与 ECS 边界设计](./combat-runtime-and-ecs-boundary-design.md)
