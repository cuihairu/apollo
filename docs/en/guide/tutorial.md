---
title: Tutorial
icon: map
order: 4
prev: /en/guide/quick-start
next: /en/guide/concepts
---

# Tutorial

From zero to a complete battle loop: login anchor → create instance → attach battle → enter scene → AOI → fight to the end → settle → leave. Single-threaded in-process throughout, corresponding to the runnable sample `examples/full_loop_demo.cpp` in the repository (all 32 assertion gates pass; this tutorial unpacks it step by step).

> Prerequisite: build completed per [Quick Start](./quick-start) (the three-tree gate baseline is in the repository's `tests/README` referenced by [Test Suite](/en/guide/quick-start)).

## How to run the sample

```bash
# Examples 树构建后
cmake -S . -B build-examples -DAPOLLO_BUILD_EXAMPLES=ON -DAPOLLO_BUILD_GAME_MODULE=ON \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build-examples --target full_loop_demo
./build-examples/examples/full_loop_demo
```

A successful run prints eight `[OK]` steps plus the AOI event counts at the end.

## Step 1: login — activating the login anchor

```cpp
AnchorManager anchors;
auto anchor = anchors.activate(kPlayer);          // 1001
// 重登幂等：anchors.find(kPlayer) == anchor
```

`PlayerAnchor` is the sole authority for a player's long-term state (ADR-001): activated at login, owned by Base independently of Instances and unloaded under its session policy. The battle settlement later posts into it one-way.

## Step 2: create the instance — the eight-state machine starts

```cpp
Instance instance(1, 1, "demo-instance");
// Create 态进场被拒（放行面仅 Waiting/Running）
instance.initialize();
// → Waiting
```

`Instance` is an eight-state one-way state machine (ADR-003): `Create → Initialize → Waiting → Running → Finishing → Rewarding → Draining → Destroyed`; only adjacent transitions are legal, and terminal states are sealed. Teaching point: **the Create state rejects entry** — players can only enter from Waiting/Running.

## Step 3: attach the battle load — the Waiting window

```cpp
AnchorRewardSink sink(*anchor);                   // 奖励单向落锚口
auto battle = std::make_unique<BattleRuntime>(1, kSeed, &sink);
instance.attach_battle(std::move(battle));        // 只有 Waiting 接受挂接
```

The attach window is Waiting; `IRewardSink` is the one-way posting surface from battle → player long-term state (after settlement the battle holds no player data). Attaching from Running onward is rejected.

## Step 4: enter — entering the scene and scene placement

```cpp
instance.enter(PlayerId{kObserver});              // 观察者预进（Waiting 可进）
instance.enter(PlayerId{kPlayer});                // 主角预进

Scene scene(1, "demo-scene");
scene.aoi() = SceneAoi(100.0f, 100.0f, 25.0f, 20.0f);
scene.aoi().set_event_sink([&](const SceneAoi::Event& e) { /* Enter/Sync/Leave 计数 */ });

scene.enter(observer, SceneAoi::Vec3{50.0f, 0.0f, 50.0f});
scene.enter(avatar,   SceneAoi::Vec3{40.0f, 0.0f, 50.0f});
// 观察者视角收到 Enter×1（主角进视野）
```

AOI ownership belongs to the scene (ADR-005): `SceneAoi` is exclusively owned by the Scene, and events are dispatched through the sink. Entry order affects event direction — the observer places first, so when the avatar enters later only an "Enter from the observer's viewpoint" is produced.

## Step 5: move — the AOI diff

```cpp
for (int f = 0; f < 3; ++f) {
    scene.aoi().move(EntityId{kPlayer}, SceneAoi::Vec3{41.0f + f, 0.0f, 50.0f});
}
// Sync 事件 ≥1（移动差集）；未出视野无 Leave
```

Movement produces Sync diffs (the broadcast-volume baseline); moving within vision produces no Leave.

## Step 6: battle — the mid-phase, five phases

```cpp
instance.start();                                 // 内部校验参战非空 → Running + begin
// battle.phase() == Battling
// Battling 起进人只观战：battle->enter_player(...) == false
// Running 起挂接拒绝：instance.attach_battle(nullptr) == false

for (std::uint32_t t = 0; t < 5; ++t) {
    std::vector<BattleInput> inputs;
    inputs.push_back(BattleInput{t, kPlayer, 1}); // opcode 1=攻击
    battle->tick(t, inputs);
}
// hash_chain() != 0（确定性指纹滚动）
```

Two key baselines (ADR-006):

- The **participant collection window** is the instance's Created/Entering window (`Instance::enter` forwards the collection); from Battling on, joining players are spectators only.
- **The two timelines must not be mixed**: the instance tick space is driven internally by `Instance::tick`; battle input goes through the explicit `BattleInput` injection surface (`battle->tick(t, inputs)`) — this tutorial uses the explicit surface; mixing them means racing the clock.

The determinism trio: strictly increasing ticks, sorted inputs, `CombatRoll` substream random numbers. The same seed plus the same input sequence always yields the same battle outcome (hash-chain fingerprints are comparable).

## Step 7: reward — settlement posting

```cpp
battle->finish();                                 // → Finished
// anchor->progress("exp") > 0：奖励落了长期态
// !anchor->dirty_reasons().empty()：内存脏标记；持久确认另行完成
```

This demonstrates an in-process reward primitive. mark_dirty indicates an in-memory change, not durable success. In the target-role topology, cell submits settlement to the owning Base, which validates and commits through database. A local Anchor example does not establish distributed settlement or saving.

## Step 8: leave — leaving and reclamation

```cpp
scene.leave(PlayerId{kObserver});                 // Leave 差集（离场双向事件）
instance.leave(PlayerId{kPlayer});
instance.leave(PlayerId{kObserver});
instance.finish(); instance.settle(); instance.drain(); instance.destroy();
// is_destroyed()：八态走完，终态封死
anchors.deactivate(kPlayer);                      // 登出锚回收
```

AOI Leave is a bidirectional event (the leaver's viewpoint plus others' viewpoints); once the instance's final four one-way transitions reach Destroyed, it cannot be reused.

## Next: performance benchmark

The three-model scene benchmark (`examples/benchmark.cpp`, directly compiling the six world sources):

```bash
cmake --build build-examples --target benchmark
./build-examples/examples/benchmark --models   # 只跑三模型段（全量微基准约 10+ 分钟）
```

| Model | Scenario × players | Corresponding form |
|------|-----------|----------|
| A | 1 Scene / 1000 Players | Large scene, a thousand players |
| B | 10 Scenes / 100 Players | Single process, multiple scenes |
| C | 100 Scenes / 10 Players | Typical instance-based load |

Reading baseline: tick duration and AOI event volume trending linearly/superlinearly with model scale is healthy; the sample numbers are not comparable across machines (a local CI/dev-machine baseline), to be used only for regression against your own baseline. Reference anchors (one measured set on the 2026-10-07 dev machine): model A frame average 26.3ms (26.32% of budget), model B 43.1ms (43.11%), model C 7.0ms (6.95%) — all three models in line under the 10Hz main loop's 100ms frame budget.

## Related docs

- [Architecture Overview](/en/architecture/overview) — the seven-layer data flow and each layer's responsibilities
- [Architecture Decision Records](/en/architecture/adr) — the ruling source for every "why" in this tutorial
- [Core Concepts](./concepts)
