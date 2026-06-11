# Compact GameServer 设计方案（塔防/固定地图适配）

> 目的：在通用 MMO 架构基础上，为塔防、绿色循环圈等固定地图、轻量玩法提供精简版 GameServer 形态，复用现有基础设施但减少服务拆分成本。

## 1. 场景与需求
- 地图数有限（1~数十），每张地图玩家数较少（<200）。
- 缺乏动态场景/副本调度，更多是波次、防守类玩法。
- 可接受单进程内处理 AOI/战斗/场景逻辑，降低运维复杂度。
- 仍希望保留 NetCore、DataProxy、配置、插件、监控、SDK 等统一能力，未来可平滑升级到 MMO 模式。

## 2. 架构对比

```
MMO 模式: Gate -> World -> Scene Orchestrator -> Zone + AOI + Battle
Compact 模式: Gate -> Compact GameServer (Scene + AOI + Battle in one)
```

Compact GameServer 仍通过 Transport/Registry 与其他服务交互（DataProxy、Social、LogAgent 等）。

## 3. 模块合并策略

| 模块 | MMO 模式 | Compact 模式说明 |
|------|----------|------------------|
| Scene Orchestrator | 独立服务 | 合并进 Compact GS 的 SceneManager，维护固定地图实例表 |
| ZoneServer | 多实例/多进程 | 以线程或协程形式运行，按地图分线程；可配置一个进程内多个逻辑实例 |
| AOI Service | 独立集群 | 使用内嵌 AOI 模块（与 Zone 同进程共享 AOIManager），仍可分 shard |
| Battle Service | 可选独立 | 作为内部模块；大型战斗时可仍调用外部 Battle Service |
| Transport | AOI/Zone 互通 | Compact 模式可禁用 AOI↔Zone Transport，保留与 Gate/DataProxy 的管道 |
| WorldServer | 全局调度 | 若玩法简单，可由单个 Compact GS 管理/或保留轻量 World 处理账号/匹配 |

## 4. Compact GameServer 结构

```
CompactGameServer
 ├─ NetEndpoint (与 Gate 通信)
 ├─ SceneManager
 │    └─ SceneInstance (固定地图)
 │         ├─ AOIManager (嵌入)
 │         ├─ BattleModule
 │         ├─ WaveManager (塔防特有)
 │         └─ ScriptEngine (Lua)
 ├─ PlayerManager
 ├─ DataProxyClient
 ├─ SocialClient / TransportAdapter
 └─ Monitoring/Config/Plugin
```

- SceneManager 维护一张 `scene_config` 表（map_id、最大玩家、波次配置）。
- 每个 SceneInstance 运行自己的 Tick，负责单位刷新、波次、建塔、Buff 等。
- AOIManager 使用与独立 AOI 一致的数据结构，但不再跨进程；若未来需要扩展，可将 AOI 接口保留，切换到远程实现。

## 5. 线程与扩展

- 推荐配置：单机多线程，按地图类型划分线程池。例如 4 核机器运行 4 个 Scene Thread，每个线程管理若干地图。
- 支持多实例部署：每个 Compact GS 可负责一组地图；通过 Consul 注册 `map_range`，Gate/World 根据玩家地图路由到对应实例。
- 若需要水平扩容（更多玩家/地图），可再部署多个 Compact GS 或升级为完整 Zone/AOI 分层。

## 6. 数据和配置

- 玩家数据仍通过 DataProxy 访问；Compact 模式只需在 Scene 中处理临时状态。
- 配置热更新流程与主框架一致；塔防特有配置（波次、塔属性、敌人路径）放在配置系统 Excel/Lua 中。
- 属性/IDL 同样使用统一生成工具 (`docs/22`)。

## 7. 对 Gate/SDK 的影响

- Gate 仍按会话将玩家转发到对应 `CompactGameServer`。
- SDK/客户端协议完全一致；只是逻辑服务器 fewer hops，延迟更低。
- 坐标/状态同步的消息流与 MMO 模式相同，便于客户端复用。

## 8. 迁移与升级

- 如果未来地图/玩法扩展，需要独立 AOI/Scene 服务，只需：
  1. 将 SceneManager/ AOI 模块抽出为服务。
  2. Gate/World 路由逻辑不变。
  3. Compact 模式中的 Scene/AOI 接口已与独立版本一致，迁移成本低。

## 9. 监控与运维

- 关键指标：每张地图玩家数、帧耗时、AOI 事件数、波次处理时间、战斗结算耗时。
- 失败策略：单实例异常时，World/Gate 将玩家路由到备用实例，或回滚至主城。
- 支持平滑重启：Scene 状态可序列化（保存当前波次、敌人状态），重启后恢复。

## 10. Roadmap
1. 实现 Compact GameServer 核心框架（SceneManager + AOI + Battle 内嵌）。
2. 集成 NetCore/Transport/DataProxy/插件/配置等现有模块。
3. 打通塔防玩法（波次、建塔、技能）示例，验证端到端流程。
4. 监控和运维方案与主框架共享，补充塔防特有指标。
5. 预留接口以便未来升级到完整 Zone/AOI 分层。

---

该设计让框架既能服务普通 MMO，也能用更轻的形态覆盖塔防或固定地图游戏，保持后续演进空间。
