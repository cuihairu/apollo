---
title: Game Module
icon: game
order: 7
category:
  - Modules
tag:
  - game
---

# Game Module

The Game module provides the game-domain layer, split into seven sub-libraries: core
(entity primitives), world (scene/session/Avatar), session (anchors/online
directory/recovery/fleet recovery), attributes (attributes), attrsync (delta attribute
sync), battle (battle runtime), social (party/guild).

## Entity（apollo::game::core）

The game entity base class. `EntityId`/`PlayerId` are strongly typed with no implicit
conversion between them (P0-2: the compiler stops the "passing an entity ID as a player ID"
class of errors); supports component attachment.

```cpp
#include <apollo/game/core/entity.hpp>

using apollo::game::core::EntityId;
using apollo::game::core::Entity;

// 创建实体
Entity entity(EntityId{12345}, "Player");

// 组件挂载（IEntityComponent：get_type_name + on_attach/on_detach/on_update）
entity.add_component<HealthComponent>();
auto health = entity.get_component<HealthComponent>();
if (health) {
    health->take_damage(20);
}
```

## Component System (IEntityComponent)

Components are behavior/data units attached to entities, indexed by type name
(`unordered_map<string, ComponentPtr>`):

```cpp
class HealthComponent : public apollo::game::core::IEntityComponent {
public:
    std::string get_type_name() const override { return "health"; }
    void take_damage(int damage) { hp_ = std::max(0, hp_ - damage); }

private:
    int hp_ = 100;
};
```

## AOI System (SceneAoi)

The repository's only AOI implementation (after the P1-3 convergence):
`apollo::game::world::SceneAoi`, owned by Scene and isolated per scene; `ViewerState`
records the per-viewer vision watermark (the Enter/Sync/Leave diff baseline).

```cpp
#include <apollo/game/world/scene_aoi.hpp>

// 创建兴趣管理（宽、高、格子、视野半径）
apollo::game::world::SceneAoi aoi(1000.0f, 1000.0f, 100.0f, 200.0f);

// 事件面（Enter / Sync / Leave；sink 缺省静默）
aoi.set_event_sink([](const apollo::game::world::SceneAoi::Event& e) {
    // e.observer / e.subject
});

// 实体进入 / 移动 / 离开
aoi.enter(apollo::game::core::EntityId{1}, {100.0f, 0.0f, 100.0f});
aoi.move(apollo::game::core::EntityId{1}, {200.0f, 0.0f, 100.0f});

// 查询：位置点的视野 / 实体的观察者集
auto nearby = aoi.viewers_at({100.0f, 0.0f, 100.0f});
auto observers = aoi.viewers_of(apollo::game::core::EntityId{1});

aoi.leave(apollo::game::core::EntityId{1});
```

The broadcast surface down to the gateway is left to P3-2; the attribute pipeline's
consumption of AOI events is wired (batch 5, `Scene::attach_attrsync`).

## Battle System (battle sub-library)

Three layers: `BattleSystem` (in-scene entity-set registration + tick skeleton),
`BattleRuntime` (the deterministic battle domain — the BattlePhase state machine, logic tick
+ wall clock dual time axes, the RNG substream contract; delivered by the
battle-determinism batch), `BattleReplay` (the ReplayTuple replay surface).

```cpp
#include <apollo/game/battle/battle_system.hpp>
#include <apollo/game/battle/battle_runtime.hpp>

apollo::game::battle::BattleSystem battle;
battle.add_entity(std::make_shared<apollo::game::core::Entity>(EntityId{1}));
battle.update(0.1f);
```

The ECS convergence shipped with P2-1 (the whole battle::ecs branch deleted; entity sets
belong to Scene); the `BattleRuntime` deterministic domain and state machine shipped with
P2-2 (five phases + `Instance` attachment + `IRewardSink` one-way settlement); the
skill/Buff gameplay surface follows the gameplay batch. The `ECSWorld`/`ECSSystem` APIs of
the old draft do not exist in the repository.

## Session & Directory (session sub-library, the G-1 deliverable)

Nine headers for anchors/online directory/recovery/fleet recovery:
`AnchorManager`/`PlayerAnchor` (the single write point for long-lived player state) +
`AnchorRewardSink` (settlement posting) + `PlayerDirectory` (online directory,
epoch/suspend window/re-report convergence) + `DirectoryPublisher` (the "APD2" directory
mirror) + `FleetRecoveryCoordinator` (cross-process recovery phases) +
`RecoveryCoordinator` (journal replay → Anchor restore → admission gate) +
`SessionLocator`/`WorldAssignment`. For API details see the [Game API](/en/api/game) page and
the session-and-online-directory design doc.

## Social (social sub-library)

`Party` / `Guild`, the basic party and guild containers (cross-server sync semantics hang
off the pending-decision chain, not wired).

## Attribute System (attributes)

`AttributeContainer` (holds attribute values per objectId, Set/Get/Add + change listeners)
+ `AttributeManager` (attribute definition registration and container lifecycle,
`RegisterAttribute`/`CreateContainer`/`LoadFromConfig`). Headers live at the top level in
`include/apollo/game/attributes/`.

```cpp
#include <apollo/game/attributes/attribute.hpp>

auto& manager = apollo::AttributeManager::Instance();
manager.RegisterAttribute(def);                      // AttributeDef：id+类型+默认值
auto container = manager.CreateContainer(objectId);  // 每实体一个容器
container->SetAttribute(attributeId, value);         // AttributeValue
AttributeValue out;
container->GetAttribute(attributeId, out);
container->SetChangeListener([](const AttributeChangeEvent& e) { /* 变更上报 */ });
```

The per-viewer delta sync pipeline core + the Scene AOI wiring have landed (see the attrsync
sub-library below).

## Delta Attribute Sync (attrsync sub-library)

The per-viewer delta pipeline (attribute-sync §3.1/§3.2/§3.3/§4.1/§5/§7.1/§8.2), delivered
in twelve batches:

| Piece | Responsibility |
|---|---|
| `ChangeHistory` | Sequenced change ring log (one per entity, default capacity 1024) |
| `ViewerAttrState` | Per-viewer `acked_seq` watermark (one per (viewer, entity) pair) |
| `AttrDelta` | varint/zigzag encoding (attr_id + value + change_n) |
| `AttrDomain` | Attribute visibility domain (batch 9): kPublic/kSelf/kTeam/kGuild/kWorld |
| `DeltaBuilder` | Delta assembly + snapshot switching decision; on snapshot the deltas carry full current values (batch 8); domain pruning on both snapshot and delta paths (batch 9) |
| `EntityAttrs` | Per-entity attribute storage + dirty bits + ChangeHistory + the domain bit table (batch 9) |
| `AttrSyncManager` | Entity registration, attribute write path, AOI Enter/Leave, broadcast collect, ACK, ACK-timeout degradation, peek (read-only precheck), ChangeSink (§8.2 post-write journal entry), domain projection set_attr_domain/set_group_gate (batch 9), attribute hook window injection point (scripting-lua §5.1: set_attr_hook/apply_attr_hook, batch 10) |
| `AttrSyncScheduler` | §5 priority and throttling: token bucket + priority ordering + min_interval + starvation protection |
| `FrameCodec` | `AttrSyncFrame ↔ wire bytes` (varint encoding + batching) |
| `Scene::attach_attrsync` | Scene ↔ AOI event wiring (batch 5): first-seen Enter/Sync → snapshot frame, Leave → viewer state reclaimed |
| `Scene::attach_script` | Scene × ScriptRuntime adapter bridge (scripting-lua §5.1 host wiring, batch 12): runs the hook window between the tail of Recalc and Collect, frame budget defaults to 200k, adjustable |

```cpp
#include <apollo/game/attrsync/attr_sync_manager.hpp>

apollo::game::attrsync::AttrSyncManager mgr;
mgr.register_entity(/*entity_id=*/42);
mgr.set_attribute(42, /*attr_id=*/1, /*value=*/100);

// AOI Enter → 建 ViewerState + 返回快照帧
auto frames = mgr.on_viewer_enter_collect(/*viewer=*/100, 42);

// Broadcast tick → 收集 per-viewer delta 帧
auto delta = mgr.collect(100, 42);

// ACK → 推进 acked_seq + 解除降级
mgr.ack(100, 42, /*seq=*/delta[0].base_seq + delta[0].deltas.size());

// ACK 超时降级：静默 ≥ 阈值拍 → 每拍快照重置
mgr.set_ack_timeout_ticks(3);
bool degraded = mgr.is_degraded(100, 42);

// §5.1 属性钩子窗口（scripting-lua §5.1）：dirty 固定后、collect 前注入式修订。
// 白名单/预算/错误隔离由宿主实现注入（如适配 ScriptRuntime::run_attr_hook_window）；
// 修订值经正规写路径落存储+history，builder last-write-wins 收敛为单条最终值。
mgr.set_attr_hook([](std::vector<apollo::game::attrsync::AttrSyncManager::HookChange>& changes) {
    for (auto& c : changes) { /* 宿主窗口实现 */ }
});
mgr.apply_attr_hook();  // 每拍 collect 前调用；无钩子空载零开销；同 tick 幂等
```

Scene-side wiring (batch 5):

```cpp
#include <apollo/game/world/scene.hpp>

apollo::game::world::Scene scene("s1");
scene.attach_attrsync(mgr);   // 接管 AOI 事件 sink + 补注册既有实体
// ... spawn_entity / enter 即时注册 ...
auto frames = scene.drain_attrsync_frames();  // 取走首见期快照帧
scene.detach_attrsync();
```

Scene × ScriptRuntime adapter bridge (batch 11, scripting-lua §5.1 host wiring):

```cpp
#include <apollo/script/script_runtime.hpp>

apollo::script::ScriptRuntime rt(cfg);
rt.set_attr_hook("onAttrChanged");   // 钩子函数注册（修订返回整数）
rt.load_contract(contract_src);        // 白名单取契约表数据面（§3.4）

scene.attach_script(rt);              // 与 attach_attrsync 共架桥（次序无关）
scene.set_attr_hook_frame_budget(200000);  // §5.2 帧级预算（默认 200k ≈ 0.5ms）

// tick 接线（Scene::tick 六阶段内）：
//   set_tick 喂熔断/WARN 记账 → Recalc 尾 apply_attr_hook 跑窗口（白名单外跳过、
//   帧预算耗尽剩余跳过、错误保持原值）→ Collect 出修订 delta → BudgetFlush 归还帧账
```

The §7.1 admin attach execution queue (scripting-lua §7.1 host wiring batch):

```cpp
// 投递（任意线程，队列自持互斥——Scene 唯一跨线程面）
scene.enqueue_debug_eval(/*id=*/9001, "counter");  // 队列满/未 attach 拒收 false
// Scene::tick 首拍固定调试点执行（eval_debug 语义全管辖：预算/限流/截断），
// 结果按 id 关联回传；未取走的结果按深度上限丢最旧
for (const auto& r : scene.drain_debug_eval_results()) { /* 回传 transport */ }
scene.set_debug_eval_queue_depth(8);  // 默认 8（与 eval 同 tick 限流对齐）
```

The first batch of owning-host wiring has landed in world-app (scripting-lua §2, one state
per logical thread): `World` builds the `ScriptRuntime` at the top of gameThread, and
`--script-boot <file>` loads the boot source (missing/failing is a warning only, no
shutdown; faults go through the runtime event surface). The host startup load order
(scripting-lua §3.4) = `--script-handlers` (the contract-route handler surface) →
`--script-contract` (the contract table; the load gate verifies per route that a handler is
in place) → `--script-boot` (may read the `contract` global). The contract-route handler
placeholder module ships with the repo at `apps/world-app/scripts/contract_handlers.lua`
(45 routes; the gameplay batch fills in the semantics), with the golden contract sync gate
`contract_handler_tests` (2 cases) and the startup-loading `world_app_script_host_tests` (3
cases, including the boot-lands-after-contract assertion, missing-handler rejection, and
continuing service when the contract file is absent); every tick it idempotently attaches
scenes not yet attached (new scenes take effect the next tick), and on shutdown a final
sweep detaches all scenes before the thread destructs; `find_scene(scene_id)` is the
host's outward delivery-locating surface (the runtime pointer is never exposed). The admin
single-entry transport has landed (the 944/945 M1 raw-frame family: the cell-side
dispatcher accepts — not-attached/backpressure gets an immediate 945 rejection; an accepted
request posts an association entry awaiting the tick-window drain for an async reply; the
admin side has the `fleetctl eval` entry; `world_app_m1_link_tests` eval family 3 cases +
`world_app_script_host_tests` 7 cases).

**Boundaries**: the client send path, slice composite types, and attach transport (admin
single entry + control-channel forwarding to the target process) are all later batches.

## NPC AI

> **Not implemented** (2026-10-04 reconciliation): `apollo/game/ai/ai_controller.hpp` does
> not exist; the repository has no AI/pathfinding module (the pathfinding batch of the old
> 9-batch plan never entered the mainline, see the todo.md legacy registry).

## Dependencies

- apollo::core
- apollo::base

## Linking

```cmake
target_link_libraries(my_app
    apollo::game_core
    apollo::game_world
    apollo::game_session
    apollo::game_attributes
    apollo::game_attrsync
    apollo::game_battle
    apollo::game_social
)
```
