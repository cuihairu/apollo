# Battle Service 技术设计

> 目的：将战斗系统从 ZoneServer 中适度解耦，构建可扩展、高性能的战斗服务，支撑 PvP/PvE、大型团战、副本等场景，配合 AOI/Scene Orchestrator 提升整体可扩容性。

## 1. 设计目标
1. **组件化战斗管线**：技能、Buff、状态、结算模块可独立扩展。
2. **可卸载**：对于大型战斗，可在 Battle Service 上运行，减轻 ZoneServer 负担。
3. **低延迟**：战斗计算需在 50 ms 帧内完成，支持回放/校验。
4. **一致性与容错**：服务异常时可自动接管或回滚，战斗状态可持久化。
5. **统一数据模型**：与属性/IDL（`docs/22`）同步，便于客户端/BI 使用。

## 2. 架构概览

```
               ┌───────────────────────┐
               │ Scene Orchestrator    │
               └──────────┬────────────┘
                          │ battle_spawn(scene_id, type)
                          ▼
┌───────────────────────────────────────────────────────────┐
│ Battle Service Cluster                                    │
│  • Matchmaker / Scheduler                                 │
│  • Battle Instance Manager                                │
│  • ECS + Battle Pipeline                                  │
│  • Result Dispatcher / Persistence                        │
└───────┬──────────────────────────────────────────────────┘
        │ events/results
        ▼
┌──────────────────┐        ┌─────────────────────┐
│ ZoneServer       │<------>│ AOI Service         │
└──────────────────┘   AOI/Scene sync       └─────┘
        │ Battle data (Transport)
        ▼
┌──────────────────┐
│ DataProxy / TLog │
└──────────────────┘
```

## 3. 功能模块

### 3.1 Matchmaker / Scheduler
- 接受 ZoneServer 或 Match 服务发送的战斗请求（玩家列表、场景、模式、配置）。
- 为请求分配 Battle Instance（可能在同机或其他节点）。
- 管理资源（CPU、内存），避免某节点过载；支持迁移/扩缩。

### 3.2 Battle Instance Manager
- 创建/销毁 Battle 实例，管理生命周期。
- 管理战斗场景的状态（地图配置、天气、时间、BUFF 场）。
- 维护 Battle 实例对 AOI/Zone 的引用（玩家 ID、实体映射）。

### 3.3 ECS + Pipeline
- 每个战斗实例使用 ECS（Entity, Component, System）。
- 组件示例：TransformComponent、AttributeComponent、SkillComponent、BuffComponent、StateComponent。
- Pipeline 阶段：
  1. **Input Collection**：收集来自客户端或 AI 的指令（CastSkill、Move、UseItem）。
  2. **Validation**：检查状态（CD、资源、位置）。
  3. **Execution**：更新 Buff/技能/AI，触发事件。
  4. **Settlement**：计算伤害/治疗，更新属性。
  5. **Broadcast**：生成战斗事件，发回 Zone/AOI/Gate。
- 支持脚本扩展（Lua/ECS 事件），可在 Pipeline 中注入自定义逻辑。

### 3.4 Result Dispatcher / Persistence
- 将战斗结果（胜利方、积分、掉落、奖励）发送给 Zone/DataProxy/TLog。
- 对关键战斗（如 PVP 排位）进行持久化和回放数据记录。

## 4. 数据模型

### 4.1 通用实体
```protobuf
message BattleEntity {
  uint64 entity_id;
  EntityType type;
  AttributeSnapshot attributes;
  repeated BuffState buffs;
  repeated SkillState skills;
  Transform transform;
}
```
- 该数据与 `docs/22` 属性 IDL 对应，供 Battle 与 Zone/AOI 共享。

### 4.2 事件消息
- `BattleCommand`: Client/AI 发起指令。
- `BattleEvent`: Server → Client (技能结果、飘字)。
- `BattleResult`: Final outcome（胜负、积分、掉落）。
- 使用统一 Proto ID，Gate/Zone/Battle 共用。

## 5. 协议与通信

- **Zone ↔ Battle**：使用 Transport (NNG) 传输 `BattleCommand`、`BattleEvent`、`BattleResult`。
- **Gate ↔ Battle**：Gate 不直接参与，仍通过 Zone 转发。
- **AOI ↔ Battle**：AOI 提供玩家位置/可见性，Battle 生成事件后交给 Zone/AOI 广播。

## 6. 可扩展性

- 支持多种战斗模式：Arena (1v1)、Team (5v5)、Raid (40+)、Battlefield (100+).
- 可通过配置 / 脚本选择是否 offload 到 Battle Service：小规模战斗可留在 Zone。
- 支持 Replay/Inspector：战斗过程中可记录关键帧数据以便回放/裁判。

## 7. 容灾与容错

- Battle 实例定期 checkpoint（状态快照），崩溃后可恢复或重启。
- 当 Battle 节点故障，Matchmaker 将战斗标记为异常，通知 Zone/Gate，执行回退策略（如重赛、结算补偿）。
- 对于大型战斗，Battle 实例采用冗余（双写或热备）增加可靠性。

## 8. 性能要求
- Tick 周期：20 ms - 50 ms（可配置），确保战斗流畅。
- 一台 Battle 节点可承载：  
  - Arena：1000+ 场并发  
  - 大型战场：10-20 场（取决于 CPU）  
- 使用线程池/Job 系统处理战斗实例，避免阻塞。

## 9. 监控与运维
- 指标：战斗实例数、Tick 时间、事件延迟、失败/重启次数。
- 管理接口：查看战斗实例状态、强制结束、迁移、Dump。
- 调试工具：Battle Inspector，支持实时观察战斗状态。

## 10. 实施计划概述
1. **阶段 1**：Battle Service 基础框架（实例管理、简单技能/战斗）。
2. **阶段 2**：与 Zone/AOI 集成、支持多种战斗模式，结果持久化。
3. **阶段 3**：大型战场优化、回放、容灾。
4. **阶段 4**：工具链（Inspector）、AI 对战、战斗脚本热更新。

---

该设计为战斗系统提供可扩展、可维护的基础，配合现有 AOI/Scene 架构，支持多种玩法和大规模战斗需求。***
