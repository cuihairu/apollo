# MMO 框架采用建议

## 1. 结论先行

如果问题是“哪些框架现在适合直接拿来使用”，答案不是统一的“能”或“不能”，而是要先分清楚你要解决的是哪一层问题：

- 世界引擎
- MMORPG 内容核心
- 在线基础设施
- 实时同步
- 服务器编排

这几层在现实项目里通常不是同一个产品同时做好的。

## 2. 最值得直接采用的框架

### 2.1 传统 MMORPG 内容服

最推荐：

- `AzerothCore`

次选：

- `TrinityCore`

原因：

- 内容系统完整
- auth/world/db 路线成熟
- 社区资料充足
- 用来做经典 MMORPG 风格世界服最现实

其中 `AzerothCore` 的优势主要在：

- 模块化
- hooks
- 二开治理

所以如果目标是长期维护和多人协作，`AzerothCore` 通常优于直接在 `TrinityCore` 上硬改。

### 2.2 现代在线后端

最推荐：

- `Nakama`

原因：

- 账号、社交、匹配、排行榜、通知、实时房间能力完整
- authoritative match 模型清晰
- 集群化和现代工程化能力强

适合：

- MMO-lite
- 副本 RPG
- 社交在线游戏
- 想快速起盘的团队

### 2.3 Node / TypeScript 房间服

最推荐：

- `Colyseus`

原因：

- Room + Schema + patch sync 非常轻
- 开发效率高
- 原型和中小型多人项目很适合

但它更适合：

- 房间型多人
- MMO 原型验证

不适合直接承担超大持续世界。

### 2.4 Unity 实时副本 / 战斗服

最推荐：

- `Photon`

原因：

- 实时同步成熟
- interest management 成熟
- topology 设计清晰

但 Photon 更适合作为：

- 战斗服
- 副本服
- PvP 实时服

而不是完整 MMORPG 后端。

### 2.5 Unity 自研网络同步层

如果团队不想直接依赖 Photon，而是想自己掌控更多上层逻辑，可以考虑：

- `Mirror`
- `FishNet`

其中：

- `Mirror` 更常见、更轻量
- `FishNet` 在 observer system 和 scene visibility 上更灵活

它们适合：

- Unity MMO-like 原型
- 自建服务端与同步层
- 需要自己掌控可见性规则

但不适合被误当成完整后端平台。

### 2.6 引擎级多人框架

如果你本来就在引擎内部做很多底层能力，可以评估：

- `O3DE Multiplayer`

它更像：

- 引擎级网络复制和 RPC 框架

而不是现成游戏后端平台。

### 2.7 商业托管多人平台

可以评估：

- `PlayFab Multiplayer Servers`
- `Beamable`

区别是：

- `PlayFab MPS` 更偏托管 dedicated server 和平台生态
- `Beamable` 更偏 LiveOps、微服务和游戏后台平台化

它们都适合商业在线项目，但都不等于大世界 MMO 内核。

### 2.8 Unreal 世界实例承载

最推荐：

- `Open World Server`

原因：

- 对 Unreal Dedicated Server 工作流友好
- 支持世界区域和实例编排
- 适合把 Unreal server 组织成更大的在线世界

前提是团队本来就准备把大量逻辑留在 Unreal 侧。

### 2.9 游戏服编排和扩缩容

最推荐：

- `Agones`

原因：

- `Fleet`
- `Allocation`
- `Autoscaling`
- Kubernetes 原生 dedicated game server orchestration

但要强调：

- `Agones` 不是游戏框架
- 它只解决承载和生命周期管理

### 2.10 分布式世界平台参考

如果你想研究超大世界、多 worker 协同和分布式权威，可以参考：

- `SpatialOS`

但当前更适合作为：

- 架构参考
- server meshing 样板

而不是多数团队的默认落地首选。

## 3. 更适合“参考和吸收”的框架

### 3.1 KBEngine

`KBEngine` 不是不能直接用，而是更适合：

- 有 MMO 引擎经验的团队直接用
- 一般团队把它作为高价值架构参考

原因：

- 世界模型很强
- schema 驱动实体、同步、持久化很先进
- 但 runtime 复杂、调试门槛高

它最值得拿来学习的是：

- Base / Cell 分治
- Entity schema
- 属性自动化
- AOI 与空间切分

### 3.2 Ryzom Core

`Ryzom Core` 更适合参考，不太适合作为现代项目直接底座。

原因：

- 服务拆分很有价值
- 代表传统商业 MMO 运营级服务架构
- 但年代感和维护门槛较高

它最值得学习的是：

- 服务网格边界
- naming / tick / AI / position 这种专职服务拆法

## 4. 各框架采用建议总表

| 框架 | 是否推荐直接采用 | 更适合的用途 | 主要理由 |
| --- | --- | --- | --- |
| KBEngine | 有条件推荐 | MMO 世界引擎 | 世界模型强，但复杂度高 |
| Ryzom Core | 不太推荐直接采用 | 架构参考 | 服务拆分有价值，但年代感重 |
| TrinityCore | 推荐 | 传统 MMORPG 内容服 | 业务完整、资料多 |
| AzerothCore | 强烈推荐 | 可维护的 MMORPG 内容服 | 模块化和二开治理更强 |
| Nakama | 强烈推荐 | 在线基础设施 / MMO-lite | 现代、完整、易落地 |
| Photon | 推荐 | Unity 实时战斗 / 副本服 | 同步和 interest management 强 |
| Colyseus | 推荐 | Node/TS 房间服 / 原型 | 开发快、同步轻 |
| OWS | 条件推荐 | Unreal 世界实例编排 | 非完整逻辑框架，但贴近 Unreal |
| Agones | 强烈推荐 | 游戏服编排层 | 生产承载能力强 |

## 5. 按项目类型推荐

### 5.1 经典 MMORPG 内容服

建议：

- 首选 `AzerothCore`
- 次选 `TrinityCore`

### 5.2 想做完整大世界 MMO 内核

建议：

- 研究和吸收 `KBEngine`
- 同时参考 `Ryzom Core`

直接落地时要谨慎，因为这条路不是“套个框架就能跑”。

### 5.3 MMO-lite / 副本型 RPG / 在线服务型游戏

建议：

- 首选 `Nakama`
- 如果技术栈偏 TS，可选 `Colyseus`
- 如果核心是 Unity 实时战斗，可选 `Photon`
- 如果 Unity 团队想自己做更多上层逻辑，可选 `Mirror` 或 `FishNet`
- 如果更看重商业平台能力，可评估 `Beamable` 或 `PlayFab`

### 5.4 Unreal MMO

建议：

- `OWS + Unreal Dedicated Server`

### 5.5 轻 MMO / 虚拟世界 / AoI 社区服

建议：

- `SmartFoxServer`

它比普通房间服更适合地图型在线世界，因为有明确的 `MMORoom + AoI` 抽象。

### 5.6 需要大规模云原生编排

建议：

- `Agones`

## 6. 从 Apollo 视角的采用建议

如果 Apollo 目标是“通用 MMO 服务端框架”，最合理的路线不是选一个框架直接替代，而是分层吸收：

- 世界模型：`KBEngine`
- 模块治理：`AzerothCore`
- 在线基础设施：`Nakama`
- 实时同步抽象：`Photon`、`Colyseus`、`SmartFoxServer`、`Mirror`、`FishNet`、`O3DE Multiplayer`
- 商业平台能力：`PlayFab`、`Beamable`
- 生产编排：`Agones`
- 分布式世界参考：`SpatialOS`

也就是说：

- 不建议把 `TrinityCore` 或 `AzerothCore` 直接当 Apollo 的未来内核
- 不建议把 `Nakama` 或 `Colyseus` 误当成完整大世界方案
- 更合理的是把它们拆成“能力来源”

## 7. 最终建议

如果只给一份现实选择清单：

- 传统 MMORPG 内容服：`AzerothCore`
- 现代在线后端：`Nakama`
- 房间同步框架：`Colyseus`
- Unity 实时副本服：`Photon`
- Unity 自研网络层：`Mirror`、`FishNet`
- 引擎级多人框架：`O3DE Multiplayer`
- 商业在线平台：`PlayFab MPS`、`Beamable`
- 轻 MMO / 虚拟世界平台：`SmartFoxServer`
- Unreal 实例世界编排：`OWS`
- 云原生游戏服编排：`Agones`
- MMO 世界引擎研究样板：`KBEngine`
- 分布式世界研究样板：`SpatialOS`

一句话总结：

- 最适合直接落地的是 `AzerothCore`、`Nakama`、`Colyseus`、`Agones`
- 最适合做大世界架构参考的是 `KBEngine`、`Ryzom Core`
- 最适合做战斗 / 房间 / 承载层补充的是 `Photon`、`OWS`、`PlayFab`、`Beamable`
