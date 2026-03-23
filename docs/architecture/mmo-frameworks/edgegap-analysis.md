# Edgegap 架构分析

## 1. 定位

Edgegap 更像：

- edge orchestration 平台
- 面向多人游戏的低延迟 dedicated server hosting

它不是完整 MMO 框架，但在近几年多人项目里越来越常被评估。

## 2. 核心架构

公开资料和官方宣传长期强调：

- distributed orchestration platform
- game server hosting
- 低延迟全球分布部署

这说明它的核心是：

- 把 game server 更靠近玩家部署
- 自动化编排实时会话承载

## 3. MMO 核心能力分析

### 3.1 世界模型

Edgegap 不提供世界模型。

### 3.2 同步

同步完全依赖你的游戏服务器。

### 3.3 平台价值

Edgegap 的真正价值在：

- edge 部署
- 低延迟
- orchestration

对实时竞技、副本型多人尤其有吸引力。

## 4. 特色

- edge-first
- 对 latency 很敏感的项目很有吸引力
- 和多种 netcode / backend 平台可以集成

## 5. 优点

- 低延迟承载思路明确
- 对会话型实时多人友好
- 平台较轻，集成面广

## 6. 缺点

- 不是世界引擎
- 不是完整后台平台
- MMO 核心逻辑仍需自建

## 7. 适用场景

适合：

- latency-sensitive multiplayer
- dedicated server 副本服
- 想优化全球玩家接入延迟的团队

## 8. 对 Apollo 的启发

最值得吸收的是：

- 区域就近部署思路
- orchestration 与世界逻辑分层

## 9. 参考资料

- 官网：https://edgegap.com/
- 文档入口：https://docs.edgegap.com/
