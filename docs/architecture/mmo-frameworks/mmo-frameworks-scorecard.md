# MMO 框架五维评分

## 1. 评分说明

这份评分不是“谁最好”的总分，而是把框架按五个关键维度拆开看：

- 世界模型
- 同步模型
- 持久化能力
- 扩展能力
- 运维与承载能力

评分范围：

- `1` 很弱
- `2` 偏弱
- `3` 中等
- `4` 强
- `5` 很强

注意：

- 这不是产品成熟度排行榜
- 不同类别框架天生强项不同

## 2. 评分表

| 框架 | 世界模型 | 同步模型 | 持久化 | 扩展能力 | 运维承载 | 备注 |
| --- | --- | --- | --- | --- | --- | --- |
| KBEngine | 5 | 5 | 4 | 4 | 3 | 典型 MMO 世界引擎 |
| Ryzom Core | 4 | 4 | 3 | 3 | 4 | 传统商业 MMO 服务网格 |
| TrinityCore | 3 | 3 | 4 | 3 | 2 | 经典世界服 |
| AzerothCore | 3 | 3 | 4 | 4 | 2 | 模块化内容服 |
| Nakama | 2 | 4 | 4 | 5 | 4 | 在线基础设施很强 |
| Photon | 2 | 5 | 2 | 3 | 3 | 实时同步强 |
| Colyseus | 2 | 4 | 2 | 4 | 3 | patch sync 很适合房间服 |
| SmartFoxServer | 3 | 4 | 2 | 4 | 3 | MMORoom + AoI 很有特点 |
| Mirror | 1 | 4 | 1 | 3 | 2 | Unity 网络层 |
| FishNet | 1 | 4 | 1 | 4 | 2 | Observer System 更灵活 |
| O3DE Multiplayer | 3 | 4 | 1 | 4 | 2 | 引擎级多人框架 |
| PlayFab MPS | 1 | 2 | 4 | 3 | 4 | 商业托管平台 |
| Beamable | 1 | 3 | 4 | 4 | 4 | LiveOps 与微服务平台 |
| AccelByte | 1 | 2 | 4 | 4 | 4 | 商业游戏平台与 DS 管理 |
| Unity Gaming Services | 1 | 3 | 3 | 3 | 4 | Unity 官方服务套件 |
| AWS GameLift | 1 | 2 | 2 | 3 | 5 | 云托管 dedicated server |
| Edgegap | 1 | 2 | 1 | 2 | 5 | edge orchestration / hosting |
| Pragma | 1 | 2 | 4 | 4 | 4 | 商业 backend stack |
| Open World Server | 3 | 2 | 3 | 3 | 4 | Unreal 编排层 |
| Agones | 1 | 1 | 1 | 2 | 5 | 编排层，不是逻辑框架 |
| Hathora | 1 | 2 | 1 | 2 | 4 | 低延迟 multiplayer hosting |
| SpatialOS | 5 | 4 | 2 | 3 | 4 | 分布式世界思路强，现实落地门槛高 |

## 3. 维度解读

### 3.1 世界模型

看的是：

- 是否有明确的大世界建模
- 是否支持空间划分
- 是否有世界级权威模型

最强：

- KBEngine
- SpatialOS
- Ryzom Core

### 3.2 同步模型

看的是：

- 是否有成熟状态同步机制
- 是否有 AOI / interest management / patch sync
- 是否适合大规模多人

最强：

- KBEngine
- Photon
- Colyseus
- SmartFoxServer
- Nakama
- O3DE Multiplayer

### 3.3 持久化

看的是：

- 是否有正式后端存储模型
- 是否和实体或在线系统打通

最强：

- KBEngine
- TrinityCore
- AzerothCore
- Nakama
- PlayFab MPS
- Beamable
- AccelByte
- Unity Gaming Services
- Pragma

### 3.4 扩展能力

看的是：

- module / runtime / hook / extension 生态
- 是否适合长期定制

最强：

- Nakama
- AzerothCore
- SmartFoxServer
- Colyseus
- FishNet
- O3DE Multiplayer
- Beamable
- AccelByte
- Pragma

### 3.5 运维承载

看的是：

- 是否有集群、扩缩容、分配、编排能力
- 是否适合生产部署

最强：

- Agones
- Nakama
- Open World Server
- SpatialOS
- Ryzom Core
- PlayFab MPS
- Beamable
- AccelByte
- Unity Gaming Services
- Hathora
- AWS GameLift
- Edgegap
- Pragma

## 4. 如何使用这张表

### 想做大世界 MMO

优先看高世界模型分：

- KBEngine
- Ryzom Core
- SpatialOS

### 想做实时副本 / 房间型 MMORPG

优先看高同步分：

- Photon
- Nakama
- Colyseus
- SmartFoxServer
- O3DE Multiplayer

### 想做长期可维护内容服

优先看高持久化和高扩展：

- AzerothCore
- TrinityCore

### 想做生产部署体系

优先看高运维承载：

- Agones
- OWS
- Nakama
- PlayFab MPS
- AccelByte
- Unity Gaming Services
- Hathora
- AWS GameLift
- Edgegap

## 5. 结论

最重要的不是谁分最高，而是不要拿错层：

- `Agones` 分不是低，而是它根本不解决世界和同步
- `Mirror` 和 `FishNet` 分不是差，而是它们本来就是网络层
- `KBEngine` 和 `SpatialOS` 世界模型分高，但并不意味着它们是所有团队最现实的落地选项

正确的使用方式是：

- 按目标问题选框架
- 不要按“总分”选框架
