# MMO 框架总对照

## 1. 结论先行

这些框架并不是同一类别的产品。  
如果把它们简单排成“哪个更好”，结论一定失真。

更准确的理解方式是：

- KBEngine、Ryzom Core：更接近 MMO 世界引擎
- TrinityCore、AzerothCore：更接近 MMORPG 内容服核心
- Nakama、Photon、Colyseus、SmartFoxServer、Beamable：更接近现代多人实时后端或游戏服务平台
- Mirror、FishNet、O3DE Multiplayer：更接近网络同步层或引擎级多人框架
- OWS、Agones、SpatialOS、PlayFab MPS：更接近承载与编排层或分布式世界平台

## 2. 核心对照表

| 框架 | 类别 | 世界模型 | 实体 / Schema | 同步模型 | 持久化模型 | 扩展方式 | 主要强项 | 主要短板 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| KBEngine | MMO 世界引擎 | 强，Space/Cell | 强，def/schema | schema 驱动 | schema 驱动 | Python + engine | 大世界、AOI、实体迁移 | runtime 重 |
| Ryzom Core | MMO 服务网格 | 强，服务化世界 | 中强 | 变量数据库 + 专项网络层 | 专门服务 | 多服务协作 | 工业级服务拆分 | 年代感强 |
| TrinityCore | MMORPG Core | 中，worldserver | 中 | 世界服内逻辑主导 | DB 驱动强 | core 二开 | 内容系统完整 | 不是分布式大世界引擎 |
| AzerothCore | 模块化 MMORPG Core | 中，worldserver | 中 | 世界服内逻辑主导 | DB 驱动强 | modules + hooks | 模块化治理 | 世界引擎能力一般 |
| Nakama | 现代多人后端 | 弱到中，房间化 | 弱于 MMO schema | authoritative match | 存储 API | Go/TS/Lua runtime | 在线基础设施完整 | 无完整大世界模型 |
| Photon | 实时网络框架 | 弱到中，session/world 需自建 | 中，网络对象级 | authority + interest management | 自建为主 | Unity 生态扩展 | 同步与 interest management | 非完整 MMO core |
| Colyseus | 房间型实时框架 | 弱，房间化 | 中，Schema | patch-based sync | 自建为主 | Node/TS | 轻量 state sync | 生产级 MMO 世界层不足 |
| SmartFoxServer | 多人服务器平台 | 中，MMORoom/AoI | 中 | AoI + MMORoom | 自建为主 | Extension | MMO 房间与虚拟世界支持成熟 | 非完整世界引擎 |
| Mirror | Unity 网络层 | 弱 | 中，网络对象级 | interest management | 自建 | Unity 组件式扩展 | 轻量且易接入 | 不是 backend |
| FishNet | Unity 网络层 | 弱 | 中，observer/scene visibility | observer system | 自建 | Unity 组件式扩展 | 可见性控制灵活 | 不是 backend |
| O3DE Multiplayer | 引擎级多人框架 | 中，实体/组件世界 | 中 | network properties + RPC | 自建 | 引擎级扩展 | 开源引擎内多人能力 | 不是后端平台 |
| PlayFab MPS | 托管多人平台 | 弱 | 弱到中 | 自建 server 决定 | 平台托管 | 平台生态扩展 | 商业托管和配套生态 | 不是世界引擎 |
| Beamable | 游戏服务平台 | 弱 | 弱到中 | relay / 服务逻辑结合 | 平台服务 | microservices | LiveOps + 后端平台化 | 不适合大世界内核 |
| OWS | 世界编排层 | 中，区域/实例编排 | 弱 | 依赖 Unreal server | API + DB | 微服务扩展 | Unreal 世界实例管理 | 不是完整逻辑框架 |
| Agones | 编排层 | 无 | 无 | 无 | 无 | K8s CRD | dedicated server orchestration | 不提供游戏逻辑 |
| SpatialOS | 分布式世界平台 | 强，distributed simulation | 中 | worker 协同同步 | 平台侧能力有限 | 平台扩展 | server meshing / distributed authority | 落地成本高 |

## 3. 从 MMO 核心问题看差异

### 3.1 谁最关注“大世界”本身

最强：

- KBEngine
- Ryzom Core

中等：

- OWS

偏弱：

- TrinityCore
- AzerothCore
- Nakama
- Photon
- Colyseus
- SmartFoxServer
- Mirror
- FishNet
- O3DE Multiplayer
- Agones

### 3.2 谁最关注“内容系统”

最强：

- TrinityCore
- AzerothCore

中等：

- KBEngine
- Ryzom Core

偏弱：

- Nakama
- Photon
- Colyseus
- OWS
- Agones

### 3.3 谁最关注“实时基础设施”

最强：

- Nakama
- Photon
- Colyseus
- SmartFoxServer
- O3DE Multiplayer

中等：

- KBEngine
- Ryzom Core
- Mirror
- FishNet
- Beamable

### 3.4 谁最关注“生产部署和承载”

最强：

- Agones
- OWS
- SpatialOS
- PlayFab MPS

中等：

- Nakama
- SmartFoxServer
- Beamable

## 4. 各框架最鲜明的特色

### KBEngine

- Base / Cell 二元分治
- schema 驱动属性、方法、同步、持久化

### Ryzom Core

- 细粒度服务云
- Tick / Naming / AI / Position 等专职服务

### TrinityCore

- 内容系统深
- 传统世界服逻辑非常完整

### AzerothCore

- 模块化治理比同类强
- 强调通过 hooks/modules 定制

### Nakama

- presence / streams / matchmaking / authoritative match

### Photon

- interest management
- topology 设计成熟

### Colyseus

- schema patch sync 极其轻量

### SmartFoxServer

- `MMORoom + AoI`
- 很早就把 MMO 房间模型做成正式能力

### Mirror

- Interest Management 是正式一等能力

### FishNet

- Observer System 和 Scene Visibility 很灵活

### O3DE Multiplayer

- entity/component 网络复制与引擎统一

### PlayFab MPS

- 商业托管 multiplayer server

### Beamable

- LiveOps + 微服务后端平台

### OWS

- Unreal dedicated server 动态拉起和区域承载

### Agones

- Fleet / Allocation / Autoscaling

### SpatialOS

- distributed simulation
- server meshing / distributed authority 思路

## 5. 如果只看 Apollo 最该学什么

### 第一层：MMO 世界内核

优先看：

- KBEngine
- Ryzom Core

应吸收：

- `EntitySchema`
- 空间权威模型
- 同步描述模型
- 世界服务边界

### 第二层：业务与模块治理

优先看：

- AzerothCore
- TrinityCore

应吸收：

- 模块化
- hooks
- 内容系统边界

### 第三层：现代实时基础设施

优先看：

- Nakama
- Photon
- Colyseus
- SmartFoxServer
- Mirror
- FishNet
- O3DE Multiplayer
- Beamable

应吸收：

- room / presence / stream / authoritative tick
- interest management
- patch sync

### 第四层：生产部署

优先看：

- Agones
- OWS
- SpatialOS
- PlayFab MPS

应吸收：

- allocation
- prewarm fleet
- autoscaling
- canary

## 6. 最终判断

如果 Apollo 的目标是“通用 MMO 服务端框架”，最合理的路线不是照搬其中任意一个，而是组合式吸收：

- 用 KBEngine / Ryzom Core 补世界模型
- 用 AzerothCore 补模块治理
- 用 Nakama / Photon / Colyseus / Beamable 补现代实时与平台抽象
- 用 Agones / OWS / PlayFab MPS 补生产承载与编排

换句话说：

- KBEngine 解决“世界怎么活”
- TrinityCore / AzerothCore 解决“内容怎么堆”
- Nakama / Photon / Colyseus / Beamable 解决“实时怎么跑”
- OWS / Agones / PlayFab MPS 解决“服务怎么养”

对 Apollo 最有价值的不是选一个站队，而是明确每层要解决的问题，再按层吸收。
