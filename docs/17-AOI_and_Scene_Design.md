# AOI 服务与场景调度设计

> 目的：细化推荐架构中的 AOI Service、Scene Orchestrator、ZoneServer 协同方案，确保跨场景可扩展、稳定、可监控。

## 1. 设计目标
1. **关注点分离**：AOI（视野/空间计算）从 ZoneServer 脱离，减少逻辑线程负担。
2. **弹性伸缩**：Scene Orchestrator 负责场景实例生命周期，可平滑扩容/释放。
3. **统一协议**：AOI、Zone、Gate、Battle 使用统一消息格式（Protobuf），易于调试。
4. **容灾与监控**：单个 AOI/Zone 故障只影响局部场景；提供健康检查、延迟监控。
5. **跨区视野**：支持边缘玩家看到相邻区实体，必要时 AOI 可跨 Zone 合并视图。

## 2. 组件概览

```
┌─────────────────────────┐
│ Scene Orchestrator      │
│  • 场景实例管理          │
│  • 资源池/复用           │
│  • 调度/迁移             │
└───┬────────────────────┘
    │ scene_spawn(scene_id, config)
    ▼
┌─────────────────────────┐
│ ZoneServer (多实例)     │
│  • ECS/战斗/任务         │
│  • 与 AOI 协同           │
└───┬────────────────────┘
    │ position_update / event
    ▼
┌─────────────────────────┐
│ AOI Service (Cluster)   │
│  • 空间索引（网格/四叉树）│
│  • Enter/Leave 事件      │
│  • AOI 广播过滤          │
└───┬────────────────────┘
    │ aoi_event / broadcast_targets
    ▼
┌─────────────────────────┐
│ GateServer / Battle /   │
│ Social services         │
└─────────────────────────┘
```

## 3. Scene Orchestrator

### 3.1 职责
- 管理场景实例（static/dynamic）。支持副本池、Zone 绑定。
- 根据玩家请求或系统事件创建/销毁场景，调度到某个 ZoneServer。
- 维护 `scene_registry`：场景ID、类型、持有 Zone、AOI shard。
- 提供 API：`CreateScene`, `DestroyScene`, `TransferPlayer(SceneId, PlayerId)`。

### 3.2 实现要点
- **资源池**：预留一定数量的空闲场景，以降低瞬时创建成本。
- **生命周期**：见推荐架构的 Scene FSM（Create → Prepare → Active → EmptyCheck → Recycle）。
- **容错**：ZoneServer 异常时，可通知 Orchestrator 把场景迁移/销毁，并通知玩家重连或回滚。
- **调度策略**：按 CPU/内存负载、玩家数量选择 Zone；支持热迁移（通过同步状态 + 冷启动）。

## 4. ZoneServer

### 4.1 职责
- 承载实际游戏逻辑：ECS、AI、任务、技能、Buff、事件处理。
- 与 AOI Service 通信：上传位置、接收 Enter/Leave/Sync、广播结果。
- 与 DataProxy/Transport 协同保存状态、加载玩家。

### 4.2 ECS & 事件流
1. 玩家移动 → ZoneServer 更新 TransformComponent → 发送 `AOIUpdate` 给 AOI。
2. AOI 返回 `EnterSet/LeaveSet/Sync` → ZoneServer 触发逻辑（发消息、加载实体等）。
3. 战斗/技能 → ZoneServer 自己处理逻辑（或调用 Battle Service），再根据 AOI 结果广播。

### 4.3 线程模型
- ZoneServer 主线程处理 Tick（例如 20-30Hz），使用 Job System 处理复杂任务。
- 与 AOI 的通信通过 Transport（NNG TCP/IPC），采用异步 RPC 或消息队列。
- 需要缓冲来自 AOI 的事件（Enter/Leave）并按帧处理，避免频繁锁争用。

## 5. AOI Service

### 5.1 数据结构
- **空间索引**：建议使用层级化网格 + 四叉树：
  - 粗粒度：固定网格（例如 64x64m cell），快速定位 AOI shard。
  - 细粒度：每个网格内用四叉树/邻接列表管理实体，支撑动态半径。
- 支持 AOI 分片：将地图划分为多个 AOI shard，按玩家密度动态扩缩。

### 5.2 功能
- 接收 `PositionUpdate`（player_id, scene_id, pos, radius）。
- 计算 `EnterSet`, `LeaveSet`, `SyncList`，并返回给 Zone/Gate：
  - `EnterSet/LeaveSet`: 仅包含 entity_id、初始数据引用（位置、属性）。
  - `SyncList`: 周期性发送 AOI 内实体的最新位置/状态。
- 可选：提供 AOI 数据订阅给 GateServer，减少 ZoneServer 再广播的开销。
- 支持 AOI 规则：不同场景类型可定义 AOI 半径、过滤条件（阵营、隐身等）。

### 5.3 通信接口（示例）
```protobuf
message AOIUpdate {
  uint64 scene_id = 1;
  uint64 entity_id = 2;
  Vector3 position = 3;
  float aoi_radius = 4;
  uint64 timestamp = 5;
  EntityType type = 6;
}

message AOIEvent {
  uint64 scene_id = 1;
  uint64 entity_id = 2;
  repeated uint64 enter_entities = 3;
  repeated uint64 leave_entities = 4;
  repeated AOISync sync_entities = 5;
}
```

### 5.4 容灾
- AOI shard 之间支持状态复制（按场景/分区）。当某个 AOI 节点故障时，Orchestrator 将场景切换到备份或触发玩家回城。
- AOI 与 Zone 的消息需具备幂等（sequence/timestamp），防止重复更新。
- 对于关键场景，可采用双写或快速快照（例如每 N 秒保存 AOI 索引状态）。

## 6. Battle Service（可选）
- 当需要专门的战斗计算（如大型团战）时，可将部分技能/结算 offload 到 Battle Service。
- ZoneServer 发送战斗请求（技能、参与者、场景），Battle 计算后返回结果/广播指令。
- AOI 仍由 AOI Service 负责，只是 ZoneServer 可能不再承担全部战斗逻辑。

## 7. 与 Gate/Guild/Transport 的关系
- GateServer 只负责连接和消息路由，不直接参与 AOI。
- AOI/Zone 之间通信使用前面定义的 Transport（NNG/IPC/TCP）：
  - 同机 Zone ↔ AOI：优先 IPC/共享内存。
  - 跨机：NNG TCP。
- 社交模块（Guild/Chat/Match）可订阅 AOI 事件（例如公会领地可见玩家），通过消息总线或 Transport 进行。

## 8. 监控与运维
- 关键指标：AOI 事件延迟、Enter/Leave 数量、AOI shard 负载、Zone tick 时间、场景数量、玩家迁移耗时。
- 提供管理命令：查看某场景的 AOI shard、强制迁移、Dump AOI 状态、追踪某玩家可见列表。
- 故障处理：
  1. AOI shard 不可用 → Orchestrator 标记场景 degrade，通知玩家回城或切换 shard。
  2. ZoneServer 超载 → Orchestrator 调整场景分布、限流新玩家进入。
  3. Scene crash → 触发重建/回收流程，日志记录。

## 9. 实施 Roadmap
1. **Phase 1**：基础版本（AOI Service + ZoneServer 接口 + Scene Orchestrator 管理），覆盖常规副本/野外。
2. **Phase 2**：支持 AOI 分片、跨区视野、Battle Service offload。
3. **Phase 3**：容灾/迁移（快速重建 AOI 数据）、监控工具。
4. **Phase 4**：优化（共享内存通道、零拷贝、机器学习驱动 AOI 半径调整等）。

---

此设计为 AOI/场景模块提供统一蓝图，后续可以逐份文档（ZoneServer、AOI 协议、Scene Orchestrator 实施计划）展开，实现可扩展的视野/场景系统。***
