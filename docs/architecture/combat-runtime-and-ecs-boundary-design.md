---
title: Combat Runtime 与 ECS 边界设计
icon: sword
order: 27
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Combat
  - ECS
  - Runtime
---

# Combat Runtime 与 ECS 边界设计

这篇文档解决的是 Apollo 整体框架继续往战斗和高频系统推进时，一个必须写清楚的问题：

`战斗系统到底应该如何吸收 ECS / 数据导向能力，同时又不打穿 Apollo 的对象模型。`

这也是 Apollo 参考 KBE 但不照搬时，最典型的冲突点之一。

## 一、设计目标

这层设计要解决 6 个问题：

1. 战斗运行时的主边界应该放在哪里。
2. 哪些部分适合对象模型，哪些部分适合 ECS / 数据导向。
3. 技能、buff、属性、伤害结算、AI 感知如何拆层。
4. 如何和 `AvatarEntity`、`PlayerAnchor`、`World Tick`、`Script ABI` 协同。
5. 如何避免“全对象战斗”或“全 ECS 战斗”两种极端。
6. 如何兼容普通 MMO 和 BigWorld 模式。

## 二、参考来源

### 1. 参考 KBE 的对象语义经验

参考点：

- 世界中真实存在的是带身份和路由语义的实体

不照搬点：

- 不把高频战斗计算全部塞进重实体对象方法里

### 2. 参考 ECS / 数据导向经验

参考点：

- 技能和属性相关的高频系统适合批处理
- 局部系统可 job 化

### 3. 参考 Apollo 既有取舍原则

参考点：

- 外层对象语义
- 内层热点系统数据导向

## 三、为什么这样设计

Apollo 现在已经明确：

- `Proxy / Anchor / Avatar` 是对象分层
- `World Tick` 要有主序列化边界
- 不能机械照搬 KBE

这意味着战斗系统也必须走折中路线。

更合理的方式不是：

- 每个 `AvatarEntity` 自己持有全部战斗逻辑并逐个 update

也不是：

- 整个世界完全变成纯 ECS，没有对象边界

而是：

- 外层对象负责身份、归属、权威边界
- 内层 combat runtime 负责高频计算

## 四、优点

- 保留对象语义稳定性
- 给高频战斗系统留出 ECS / 数据导向空间
- 更容易和脚本、复制、生命周期协同
- 更适合未来做 job 化

## 五、代价与风险

- 对象层和 ECS 层的映射需要维护
- 实现复杂度会高于单一路线
- 如果边界不清，会出现双重状态源

## 六、为什么不选其他方案

### 不选“全对象战斗模型”

因为高频批量计算效率会越来越差。

### 不选“全 ECS 世界模型”

因为 Apollo 还需要在线身份、路由、跨宿主协作、脚本宿主等对象语义。

### 不选“战斗逻辑全脚本化”

因为高频热路径会直接失控。

Apollo 更合理的路线是：

- 对象边界明确
- 战斗热点系统数据导向

## 七、推荐总体模型

建议 Apollo 的战斗系统采用下面的两层结构：

```text
Combat Facade
    -> Combat Runtime
        -> ECS/Data-Oriented Systems
```

### `Combat Facade`

负责：

- 与 `AvatarEntity` 交互
- 处理技能释放入口
- 处理战斗事件对外暴露

### `Combat Runtime`

负责：

- 技能执行
- buff tick
- 属性计算
- 伤害结算
- 命中与目标判定

### `ECS/Data-Oriented Systems`

负责：

- 高频批量处理
- 局部 job 化

## 八、推荐边界划分

### 更适合对象模型的

- 技能释放请求入口
- 战斗身份和权威归属
- 施法者 / 目标对象引用
- 结果回写到 `AvatarEntity`
- 对外战斗事件

### 更适合 ECS / 数据导向的

- 属性聚合
- buff tick
- DOT/HOT
- 大量伤害结算
- 局部 AI 感知更新

### 设计结论

Apollo 的战斗系统更合理的路线是：

- 外层对象化
- 内层系统化

## 九、技能、buff、属性的推荐分层

### 1. Skill Layer

职责：

- 技能定义
- 技能配置
- 技能释放条件
- 技能脚本规则入口

### 2. Effect Layer

职责：

- 技能效果执行
- 目标选择
- 命中与判定

### 3. Attribute Layer

职责：

- 属性基础值
- 属性修正值
- 临时增减
- 公式求值

### 4. Buff Layer

职责：

- buff 生命周期
- tick
- 叠层
- 驱散

### 5. Combat Event Layer

职责：

- 伤害结果
- 治疗结果
- 状态变化结果
- 对外事件

## 十、和脚本层的关系

战斗系统通常最容易被误用成“全部脚本化”。

Apollo 建议明确：

### 适合脚本化的

- 技能规则
- buff 规则
- 效果组合
- 条件分支

### 不适合脚本化的

- 大规模属性批量求值主循环
- 高频 buff tick 主循环
- 大规模碰撞或范围筛选热路径

### 设计结论

- 脚本写规则
- C++ 写热路径

## 十一、和 `AvatarEntity` 的关系

`AvatarEntity` 不应该直接等于 combat runtime。

更合理的方式是：

- `AvatarEntity`
  - 提供身份、位置、归属、外层战斗入口
- `Combat Facade`
  - 负责把对象输入转换成战斗运行时输入
- `Combat Runtime`
  - 负责执行

### 为什么这么拆

这样可以避免：

- `AvatarEntity` 膨胀成超级对象

## 十二、和 `World Tick` 的关系

战斗运行时不应自己另起调度模型。

更合理的方式是：

- 战斗挂在统一 `World Tick` 的 `Simulation` 阶段

局部高频部分可以：

- 分发为 combat jobs

但提交边界仍回到统一 tick。

## 十三、和 `Replication` 的关系

战斗系统不应直接操心网络同步包拼装。

更合理的方式是：

- combat runtime 产出状态变化和战斗事件
- replication system 再决定如何同步给客户端

### 为什么

因为这有利于保持：

- 战斗逻辑
- 同步策略

解耦。

## 十四、推荐对象模型

```text
CombatFacade
    ├── SkillService
    ├── BuffService
    ├── AttributeService
    ├── DamagePipeline
    ├── CombatEventEmitter
    └── CombatExecutionContext
```

### `CombatExecutionContext`

职责：

- 施法者
- 目标集合
- tick context
- world context

### `CombatEventEmitter`

职责：

- 把伤害、治疗、死亡、状态变化统一发出

## 十五、普通 MMO 与 BigWorld 的差异

### 普通 MMO 模式

战斗通常只关心：

- 当前 world instance 内权威

### BigWorld 模式

需要额外考虑：

- 迁移窗口中的战斗上下文
- ghost 近场可见表现
- authority transfer 边界

### 设计结论

Apollo 应先把普通 MMO 下的战斗运行时做对，

再在 BigWorld 模式扩展跨 partition 的表现和权威切换边界。

## 十六、对当前 Apollo 的直接含义

Apollo 下一步如果继续往代码层推进，建议优先补：

- `CombatFacade`
- `AttributeService`
- `BuffService`
- `DamagePipeline`
- `CombatEventEmitter`

而不是先把整个 `AvatarEntity` 做成战斗大对象。

## 十七、结论

Apollo 的战斗系统更合理的路线不是纯对象，也不是纯 ECS，而是：

- 外层对象语义明确
- 内层高频系统数据导向
- 脚本只写规则，不控制热路径主循环

这样才能既吸收 KBE 的对象边界优点，又保留现代 ECS / 数据导向的性能空间。

## 相关阅读

- [KBE 参考原则与 ECS 兼容取舍](./kbe-reference-principles.md)
- [World Tick、Scheduling 与 Job Model 设计](./world-tick-scheduling-and-job-model.md)
- [统一脚本层设计](./script-layer-design.md)
