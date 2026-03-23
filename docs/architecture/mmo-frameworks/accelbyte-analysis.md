# AccelByte 架构分析

## 1. 定位

AccelByte 更像：

- 游戏后端平台
- dedicated server 管理平台
- 面向商业在线游戏的完整服务组合

它不是 MMO 世界引擎，但它在商业多人项目评估里很常见。

## 2. 核心架构

官方文档当前明确强调：

- `AccelByte Multiplayer Servers (AMS)`
- `Dedicated Server Hub`
- Play 模块下的多人通知

文档里直接提到：

- AMS 是 dynamic dedicated game server manager
- 具备 multi-tenant architecture
- 后端作为 central hub 处理 client 和 dedicated server 之间的通知

这说明它的核心不是对象同步，而是：

- 多租户 dedicated server 管理
- 会话与游戏服协同
- 游戏后端平台化

## 3. MMO 核心能力分析

### 3.1 世界模型

AccelByte 不提供 KBEngine 那种大世界 world model。  
它更像：

- 商业级多人平台
- dedicated server orchestration + backend services

### 3.2 同步

同步逻辑仍然主要在你的 game server 里实现。  
AccelByte 负责的是：

- 会话
- 通知
- dedicated server 生命周期和分配

### 3.3 平台能力

它最值得关注的是：

- 多租户
- dedicated server manager
- 客户端和 DS 间的官方通知通道

## 4. 特色

- 商业化程度高
- 游戏后端和 dedicated server 管理结合
- 比单纯“托管服”更接近完整平台

## 5. 优点

- 适合商业项目
- dedicated server 生命周期管理成熟
- 平台能力较完整

## 6. 缺点

- 不是世界引擎
- 世界逻辑和同步模型仍要自建
- 平台绑定和商业成本需要考虑

## 7. 适用场景

适合：

- 商业多人游戏
- 想减少自建后端与 dedicated server 管理成本的团队

## 8. 对 Apollo 的启发

最值得吸收的是：

- DS Hub / 通知中枢思路
- dedicated server 管理和游戏平台结合

## 9. 参考资料

- AMS 介绍：https://docs.accelbyte.io/gaming-services/modules/multiplayer/multiplayer-servers/
- Multiplayer Notification：https://docs.accelbyte.io/gaming-services/services/play/multiplayer-notification/
- 文档入口：https://docs.accelbyte.io/
