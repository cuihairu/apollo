# Nakama 架构分析

## 1. 定位

Nakama 是现代在线游戏后端框架，不是传统意义上的“大世界 MMO 引擎”。

它的强项是：

- 账号
- 社交
- 聊天
- 队伍
- 匹配
- 排行榜
- 存储
- 实时会话
- 权威房间

## 2. 核心架构

官方架构概览强调几块核心系统：

- 管理系统
- 消息路由
- Presence
- Streams
- 集群节点间透明路由
- Server Runtime 扩展

它更像一个“多人在线基础设施平台”。

## 3. MMO 核心能力分析

### 3.1 世界模型

Nakama 默认没有 KBEngine 那种 Space / Cell / 实体迁移世界模型。

它提供的是：

- server-authoritative match
- fixed tick 自定义逻辑
- 节点集群路由

所以它更适合：

- 房间型副本
- Match-based RPG
- MMO-lite

而不是天然适合：

- 无缝大地图
- 区域迁移
- 多 Cell 权威切换

### 3.2 权威逻辑

它的 authoritative multiplayer 非常明确：

- 服务器维护中心状态
- 客户端只发输入或请求
- 服务器按 tick 跑逻辑

这套模型对反作弊和一致性很有价值。

### 3.3 实时基础设施

Nakama 在在线能力上非常强：

- Presence
- Streams
- Matchmaking
- Leaderboards
- Tournaments
- Parties
- Notifications

这部分对 MMO 项目同样重要，而且往往比世界引擎更早成为需求。

## 4. 特色

- 在线基础设施完整
- 集群消息路由成熟
- Runtime 可用 Go、TypeScript、Lua 扩展
- 比传统 MMO 引擎更现代化

## 5. 优点

- 快速起盘
- 实时和社交能力现成
- 权威房间易落地
- 运维和集群模型比老牌 MMO 框架更现代

## 6. 缺点

- 不自带完整世界引擎
- AOI、空间分片、实体生命周期仍需自建
- 要做真正大世界 MMO，仍需额外世界层

## 7. 适用场景

适合：

- MMO-lite
- 多副本 RPG
- 社交 + 实时房间为主的在线游戏
- 想先把在线能力做完整的团队

## 8. 对 Apollo 的启发

最值得吸收的是：

- Presence / Streams 这类实时抽象
- cluster routing
- server runtime 扩展能力
- 将“在线基础设施”和“世界引擎”分层

## 9. 参考资料

- 架构总览：https://heroiclabs.com/docs/nakama/getting-started/architecture/
- 权威多人：https://heroiclabs.com/docs/nakama/concepts/multiplayer/authoritative/
- 文档首页：https://heroiclabs.com/docs/
