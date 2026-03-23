# MMO 框架架构分析索引

本文档组以 2026-03-23 可获取的官方资料、官方仓库与官方文档为主，聚焦 MMORPG / 大型在线游戏服务端最核心的几个问题：

- 世界如何拆分
- 实体和状态如何建模
- 同步如何做
- 持久化如何做
- 扩展和运维如何做

## 文档列表

- [KBEngine 架构分析](C:/Users/cui/Workspaces/apollo/docs/architecture/mmo-frameworks/kbengine-analysis.md)
- [TrinityCore 架构分析](C:/Users/cui/Workspaces/apollo/docs/architecture/mmo-frameworks/trinitycore-analysis.md)
- [AzerothCore 架构分析](C:/Users/cui/Workspaces/apollo/docs/architecture/mmo-frameworks/azerothcore-analysis.md)
- [Nakama 架构分析](C:/Users/cui/Workspaces/apollo/docs/architecture/mmo-frameworks/nakama-analysis.md)
- [Ryzom Core 架构分析](C:/Users/cui/Workspaces/apollo/docs/architecture/mmo-frameworks/ryzom-core-analysis.md)
- [Photon 架构分析](C:/Users/cui/Workspaces/apollo/docs/architecture/mmo-frameworks/photon-analysis.md)
- [Colyseus 架构分析](C:/Users/cui/Workspaces/apollo/docs/architecture/mmo-frameworks/colyseus-analysis.md)
- [SmartFoxServer 架构分析](C:/Users/cui/Workspaces/apollo/docs/architecture/mmo-frameworks/smartfoxserver-analysis.md)
- [Mirror 架构分析](C:/Users/cui/Workspaces/apollo/docs/architecture/mmo-frameworks/mirror-analysis.md)
- [FishNet 架构分析](C:/Users/cui/Workspaces/apollo/docs/architecture/mmo-frameworks/fishnet-analysis.md)
- [O3DE Multiplayer 架构分析](C:/Users/cui/Workspaces/apollo/docs/architecture/mmo-frameworks/o3de-analysis.md)
- [PlayFab Multiplayer Servers 架构分析](C:/Users/cui/Workspaces/apollo/docs/architecture/mmo-frameworks/playfab-analysis.md)
- [Beamable 架构分析](C:/Users/cui/Workspaces/apollo/docs/architecture/mmo-frameworks/beamable-analysis.md)
- [SpatialOS 架构分析](C:/Users/cui/Workspaces/apollo/docs/architecture/mmo-frameworks/spatialos-analysis.md)
- [Open World Server 架构分析](C:/Users/cui/Workspaces/apollo/docs/architecture/mmo-frameworks/open-world-server-analysis.md)
- [Agones 架构分析](C:/Users/cui/Workspaces/apollo/docs/architecture/mmo-frameworks/agones-analysis.md)
- [框架采用建议](C:/Users/cui/Workspaces/apollo/docs/architecture/mmo-frameworks/framework-adoption-guide.md)
- [MMO 框架总对照](C:/Users/cui/Workspaces/apollo/docs/architecture/mmo-frameworks/mmo-frameworks-comparison.md)
- [MMO 框架五维评分](C:/Users/cui/Workspaces/apollo/docs/architecture/mmo-frameworks/mmo-frameworks-scorecard.md)

## 分类说明

为了避免把完全不同类别的产品混成一类，这组文档按能力层级理解：

### 1. 世界引擎 / MMO 核心框架

- KBEngine
- Ryzom Core

这类框架自己提供较强的实体、空间、同步、服务拆分模型。

### 2. 内容服 / MMORPG Core

- TrinityCore
- AzerothCore

这类框架更像成熟的大型 MMORPG 世界服核心，强项在内容系统与业务逻辑，不以分布式大世界横向扩展见长。

### 3. 现代多人后端 / 房间型框架

- Nakama
- Photon
- Colyseus
- SmartFoxServer
- PlayFab Multiplayer Servers
- Beamable
- Mirror
- FishNet
- O3DE Multiplayer

这类产品擅长实时会话、匹配、权威房间、状态同步，但通常不直接提供完整无缝大世界 MMO 内核。

### 4. 编排 / 承载层

- Open World Server
- Agones
- SpatialOS

这类产品更偏运行时编排、实例分配、扩缩容，不等于完整 MMO 游戏框架。

## 使用建议

如果目标是 Apollo 这类通用 MMO 服务端框架，最有参考价值的是：

- 世界模型与实体 schema：KBEngine、Ryzom Core
- 业务内容与模块治理：AzerothCore、TrinityCore
- 现代多人实时基础设施：Nakama、Photon、Colyseus
- 生产部署与扩缩容：OWS、Agones
