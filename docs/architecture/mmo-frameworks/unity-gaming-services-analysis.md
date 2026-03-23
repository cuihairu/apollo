# Unity Gaming Services 架构分析

## 1. 定位

Unity Gaming Services 更像：

- Unity 官方在线服务套件
- Lobby / Matchmaker / Relay / Hosting 的组合

它不是 MMO 世界引擎，但对 Unity 项目非常现实。

## 2. 核心架构

Unity 当前官方文档明确把 Multiplayer Services 描述为一组集成服务，包含：

- Lobby
- Matchmaker
- Relay
- Game Server Hosting / Multiplay Hosting

并且新的 Multiplayer Services SDK 统一了这些能力。

## 3. MMO 核心能力分析

### 3.1 世界模型

UGS 不提供大世界 MMO world model。  
它提供的是：

- 匹配
- 大厅
- Relay
- 托管 hosting

### 3.2 同步

UGS 本身不是对象同步框架，通常和：

- Netcode for GameObjects
- Unity Transport

配合使用。

### 3.3 平台能力

它最现实的价值是：

- Unity 项目接入门槛低
- 官方生态一体化
- 对 session-based multiplayer 很方便

## 4. 特色

- 官方生态
- Lobby/Relay/Matchmaker/Hosting 打通
- 对 Unity 团队最省心

## 5. 优点

- Unity 接入成本低
- 官方支持路径清晰
- 很适合 session-based multiplayer

## 6. 缺点

- 不提供 MMO 世界核心抽象
- 大世界与持久世界仍需自建

## 7. 适用场景

适合：

- Unity 联机游戏
- 会话型多人
- 中小型在线项目

## 8. 对 Apollo 的启发

最值得吸收的是：

- 统一 Multiplayer SDK 入口
- Lobby / Matchmaker / Relay / Hosting 解耦但可组合

## 9. 参考资料

- Unity Multiplayer Services：https://docs.unity.cn/Manual/UnityMultiplayerService.html
- Unity Support 指南：https://support.unity.com/hc/en-us/articles/30896635553940-How-to-Use-Unity-Matchmaker-with-Relay-to-Match-Players
