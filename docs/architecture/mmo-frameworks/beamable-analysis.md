# Beamable 架构分析

## 1. 定位

Beamable 更像：

- LiveOps + backend platform
- serverless APIs + microservices
- 带有多人和 relay 能力的游戏服务平台

它不是 MMO 世界引擎，但在现代 Unity / Unreal 在线项目里是会被评估到的平台。

## 2. 核心架构

官方文档强调几个核心点：

- Microservices
- Serverless Game APIs
- Multiplayer relay
- 社交、内容、排行榜、商业化等在线能力

所以它的核心价值在于：

- 游戏后台平台化
- 微服务化逻辑承载
- 多人功能与 LiveOps 一体化

## 3. MMO 核心能力分析

### 3.1 世界模型

Beamable 不提供完整大世界 MMO world model。

### 3.2 同步

官方多人概览提到它采用 relay server 技术，说明它在多人联机层有现成能力，但重点不是复杂大世界同步。

### 3.3 服务端逻辑

Beamable 最值得关注的是：

- C# microservices
- 游戏逻辑可下沉到后端微服务

这对：

- LiveOps
- 经济系统
- 活动系统
- 轻量多人服务逻辑

很有吸引力。

## 4. 特色

- LiveOps 平台和多人能力结合
- microservices 开发体验较现代
- 更像“游戏后端平台”而不是“世界引擎”

## 5. 优点

- 适合现代商业在线项目
- 后端逻辑平台化
- 对 Unity / Unreal 接入友好

## 6. 缺点

- 不适合直接承担完整大世界 MMO 内核
- relay / 微服务能力并不能替代世界模型

## 7. 适用场景

适合：

- LiveOps 强的在线游戏
- 中小型多人项目
- 想减少自建后端成本的团队

## 8. 对 Apollo 的启发

最值得吸收的是：

- 微服务化游戏逻辑
- LiveOps 和多人能力结合
- 平台产品化思路

## 9. 参考资料

- Multiplayer Overview：https://docs.beamable.com/docs/multiplayer-feature-overview
- Microservices：https://docs.beamable.com/docs/microservice-framework
- 文档入口：https://beamable.com/documentation
