---
title: Reliability、Failover 与 Recovery 设计
icon: life-ring
order: 31
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Reliability
  - Failover
  - Recovery
---

# Reliability、Failover 与 Recovery 设计

这篇文档解决的是 Apollo 整体框架继续往生产可用性推进时，一个必须尽早定下来的问题：

`当进程、节点、连接、世界实例、平台组件出问题时，Apollo 应该如何降级、故障转移和恢复。`

如果这层不清楚，后面很容易出现：

- 普通 MMO 模式勉强能跑
- 一到故障场景就没有统一恢复模型
- BigWorld 增强层更是没有可靠性主线

## 一、设计目标

这层设计要解决 6 个问题：

1. Apollo 的故障类型如何分类。
2. 哪些故障应快速失败，哪些应尝试恢复。
3. session、anchor、world、platform 各自的恢复主线是什么。
4. 普通 MMO 和 BigWorld 模式的恢复边界如何统一。
5. 如何和 `Runtime Ops Host`、`Persistence`、`Domain Event`、`PlayerAnchor` 对齐。
6. 如何避免把“高可用”理解成无限自动重试。

## 二、参考来源

### 1. 参考在线游戏常见可靠性设计

参考点：

- 断线重连
- 会话接管
- world 重入
- 平台操作幂等

### 2. 参考 KBE/BigWorld 的经验

参考点：

- 在线对象和世界对象需要拆开恢复
- 空间权威和长期归属不能混成一层恢复逻辑

不照搬点：

- 不把所有问题都压给分布式对象迁移魔法

### 3. 参考分层架构原则

参考点：

- 不同层的故障由不同层主导恢复

## 三、为什么这样设计

Apollo 现在已经明确有：

- `Proxy`
- `PlayerAnchor`
- `WorldHost`
- `Platform Foundation`
- `Distributed World Extension`

既然对象和层次已经分开，那么可靠性模型也必须分层。

更合理的方式不是：

- 一个“万能自动恢复器”

而是：

- 连接故障由 session/proxy 主导
- 在线归属故障由 anchor 主导
- world runtime 故障由 world/instance 主导
- 平台写入故障由 repository/outbox/平台层主导

## 四、优点

- 故障边界更清楚
- 恢复策略更可解释
- 更适合普通 MMO 和 BigWorld 逐级增强
- 更适合测试和演练

## 五、代价与风险

- 模型比简单重连逻辑复杂
- 需要定义更多中间状态
- 自动恢复过强可能掩盖真实问题

## 六、为什么不选其他方案

### 不选“统一无限重试”

因为很多故障并不适合盲重试。

### 不选“所有故障都交给世界服处理”

因为 session、anchor、platform 的边界根本不同。

### 不选“所有故障都直接踢下线重来”

因为这会让体验和稳定性都很差。

Apollo 更合理的路线是：

- 分层故障分类
- 分层恢复主线

## 七、推荐故障分类

建议 Apollo 至少分成 5 类故障。

### 1. Connection Fault

例如：

- client disconnect
- gateway 链路断开

### 2. Host Fault

例如：

- world host 崩溃
- base/anchor host 崩溃

### 3. Platform Fault

例如：

- Redis 暂时不可用
- SQL 写入失败

### 4. Topology Fault

例如：

- route stale
- instance 迁移失败
- partition 不一致

### 5. Logic Fault

例如：

- 脚本异常
- 非法状态转换
- 命令处理失败

## 八、推荐恢复主线

## 九、`Session / Proxy` 恢复

主线：

- 连接断开
- `ProxyState -> Disconnected`
- `PlayerAnchor` 保留重连窗口
- 新连接接管旧 session

### 适合自动恢复的

- gateway 重连
- session rebind

### 不适合无限重试的

- 非法票据
- 冲突接管

## 十、`PlayerAnchor` 恢复

`PlayerAnchor` 应作为在线主恢复中心。

适合承接：

- session 重新绑定
- world assignment 恢复
- 切图中断恢复

### 设计结论

Apollo 不应让 gateway 或 world 独自承担玩家恢复中心角色。

## 十一、`World / Instance` 恢复

### 普通 MMO 模式

更合理的恢复策略通常是：

- world instance 故障后
- `PlayerAnchor` 决定重进、回城、回安全点或重新分配 instance

### BigWorld 模式

会额外涉及：

- partition 故障
- authority 迁移窗口
- ghost route 兜底

### 设计结论

Apollo 应先把普通 MMO 下：

- world re-entry
- instance reassignment

这条链跑顺，

再为 BigWorld 模式扩复杂恢复。

## 十二、平台层恢复

平台层不应只靠“出错就抛异常”。

建议至少支持：

- retry policy
- idempotency
- outbox
- dead letter / failure queue

### 适用场景

- 发奖
- 支付确认
- 排行更新
- 延迟写入

## 十三、可靠性状态模型

建议 Apollo 至少显式支持这些状态：

- `Healthy`
- `Degraded`
- `Recovering`
- `Unavailable`

### 为什么要有 `Degraded`

因为很多系统不是“好”或“坏”两态。

例如：

- OTel 掉了但核心服务还能跑
- Redis 某些旁路能力不可用但主链还在

## 十四、推荐对象模型

```text
ReliabilityCoordinator
    ├── FaultClassifier
    ├── RecoveryPolicyRegistry
    ├── SessionRecoveryService
    ├── WorldRecoveryService
    ├── PlatformRecoveryService
    └── DegradeController
```

### `FaultClassifier`

职责：

- 把错误映射到故障类型

### `RecoveryPolicyRegistry`

职责：

- 为不同故障定义：
  - fail fast
  - retry
  - rebind
  - reassign
  - degrade

### `DegradeController`

职责：

- 在故障场景下关闭非关键路径

例如：

- 降低 trace
- 关闭部分 ops 功能
- 降低某些旁路事件频率

## 十五、和 `Capability / Feature Flag` 的关系

可靠性策略不应散落硬编码。

建议部分恢复与降级策略通过：

- capability
- feature flag
- profile

统一表达。

例如：

- 是否启用 session takeover
- 是否启用 outbox retry
- 是否启用 distributed world auto-recovery

## 十六、和测试策略的关系

这层必须对应：

- failover scenario test
- reconnect recovery test
- degraded mode test
- platform retry/idempotency test

否则可靠性设计很容易停留在文档层。

## 十七、对当前 Apollo 的直接含义

Apollo 下一步如果继续往实现层推进，建议优先补：

- `FaultClassifier`
- `RecoveryPolicyRegistry`
- `SessionRecoveryService`
- `WorldRecoveryService`

优先先把普通 MMO 下：

- 断线重连
- 切图恢复
- world 重新分配

这三条主恢复链做清楚。

## 十八、结论

Apollo 的可靠性设计不能只是“断线了就重连”。

更合理的方式应该是：

- 按层分类故障
- 按层定义恢复主线
- 先做好普通 MMO 的恢复模型
- 再为 BigWorld 模式扩展复杂迁移和分布式恢复

只有这样，Apollo 才会真正接近生产可用的在线游戏框架。

## 相关阅读

- [Runtime Ops Host 设计](./runtime-ops-host-design.md)
- [Persistence、Repository 与 Unit of Work 设计](./persistence-repository-unitofwork-design.md)
- [PlayerAnchor 设计稿](./player-anchor-design.md)
