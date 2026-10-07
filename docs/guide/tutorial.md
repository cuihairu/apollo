---
title: 上手教程
icon: map
order: 4
prev: /guide/quick-start
next: /guide/concepts
---

# 上手教程

从零到跑通一局完整对战：登录锚 → 建副本 → 挂战斗 → 进场 → AOI → 打完 → 结算 → 离场。全程进程内单线程，对应仓库里的可运行样例 `examples/full_loop_demo.cpp`（36 个断言门全过，本教程按它逐步拆解）。

> 前置：已按[快速开始](./quick-start)完成构建（三树门禁口径见[测试套件](/guide/quick-start)引用的仓库 `tests/README`）。

## 样例怎么跑

```bash
# Examples 树构建后
cmake -S . -B build-examples -DAPOLLO_BUILD_EXAMPLES=ON -DAPOLLO_BUILD_GAME_MODULE=ON \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build-examples --target full_loop_demo
./build-examples/examples/full_loop_demo
```

跑通输出八步 `[OK]` 与结尾 AOI 事件计数。

## 第 1 步：login——登录锚激活

```cpp
AnchorManager anchors;
auto anchor = anchors.activate(kPlayer);          // 1001
// 重登幂等：anchors.find(kPlayer) == anchor
```

`PlayerAnchor` 是玩家长期态的唯一权威（ADR-001）：登录激活、跨副本持续、登出回收。后续战斗结算会单向落账到这里。

## 第 2 步：建副本——八态机起步

```cpp
Instance instance(1, 1, "demo-instance");
// Create 态进场被拒（放行面仅 Waiting/Running）
instance.initialize();
// → Waiting
```

`Instance` 是八态单向状态机（ADR-003）：`Create → Initialize → Waiting → Running → Finishing → Rewarding → Draining → Destroyed`，仅相邻推进合法，终态封死。教学点：**Create 态拒绝进场**——人只能从 Waiting/Running 进。

## 第 3 步：挂战斗负载——Waiting 窗口

```cpp
AnchorRewardSink sink(*anchor);                   // 奖励单向落锚口
auto battle = std::make_unique<BattleRuntime>(1, kSeed, &sink);
instance.attach_battle(std::move(battle));        // 只有 Waiting 接受挂接
```

挂接窗口在 Waiting；`IRewardSink` 是战斗→玩家长期态的单向落账口（结算之后战斗不持有玩家数据）。Running 起再挂接会被拒。

## 第 4 步：enter——进场 + 场景落点

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

AOI 所有权归场景（ADR-005）：`SceneAoi` 由 Scene 独享，事件经 sink 下发。进场顺序影响事件方向——观察者先落点，主角后进时只产生「观察者视角的 Enter」。

## 第 5 步：move——AOI 差集

```cpp
for (int f = 0; f < 3; ++f) {
    scene.aoi().move(EntityId{kPlayer}, SceneAoi::Vec3{41.0f + f, 0.0f, 50.0f});
}
// Sync 事件 ≥1（移动差集）；未出视野无 Leave
```

移动产生 Sync 差集（广播消息量口径）；视野内移动不产生 Leave。

## 第 6 步：battle——五段中段

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

两个关键口径（ADR-006）：

- **参战者收集期**在副本 Created/Entering 窗口（`Instance::enter` 转发收集）；Battling 起进人只观战。
- **双时间轴不可混用**：副本 tick 空间归 `Instance::tick` 内部驱动，战斗输入走 `BattleInput` 显式注入面（`battle->tick(t, inputs)`）——本教程走显式面，混用即抢拍。

确定性三件套：严格递增 tick、排序输入、`CombatRoll` 子流随机数。同种子同输入序列必得同战果（hash 链指纹可比对）。

## 第 7 步：reward——结算落账

```cpp
battle->finish();                                 // → Finished
// anchor->progress("exp") > 0：奖励落了长期态
// !anchor->dirty_reasons().empty()：变更即脏（随 SaveQueue 落档）
```

结算按玩家 Hit 伤害合计经 `AnchorRewardSink` 落 `PlayerAnchor`，`mark_dirty` 后由持久化线（ADR-007 的 write-behind）落档。

## 第 8 步：leave——离场与回收

```cpp
scene.leave(PlayerId{kObserver});                 // Leave 差集（离场双向事件）
instance.leave(PlayerId{kPlayer});
instance.leave(PlayerId{kObserver});
instance.finish(); instance.settle(); instance.drain(); instance.destroy();
// is_destroyed()：八态走完，终态封死
anchors.deactivate(kPlayer);                      // 登出锚回收
```

AOI Leave 是双向事件（离场者视角 + 他人视角）；副本尾四段单向推进到 Destroyed 后不可复用。

## 下一步：性能基准

三模型场景基准（`examples/benchmark.cpp`，直编 world 六源）：

```bash
cmake --build build-examples --target benchmark
./build-examples/examples/benchmark --models   # 只跑三模型段（全量微基准约 10+ 分钟）
```

| 模型 | 场景×人数 | 对应形态 |
|------|-----------|----------|
| A | 1 Scene / 1000 Players | 大场景千人 |
| B | 10 Scenes / 100 Players | 单进程多景 |
| C | 100 Scenes / 10 Players | 副本制典型负载 |

读数口径：tick 耗时与 AOI 事件量随模型规模线性/超线性趋势即健康；样例数字无跨机可比性（本机 CI/开发机口径），只用于回归对比自身基线。参考锚（2026-10-07 开发机一组实测）：模型 A 帧均 26.3ms（预算占比 26.32%）、模型 B 43.1ms（43.11%）、模型 C 7.0ms（6.95%）——10Hz 主循环 100ms 帧预算下三模型均在线。

## 相关文档

- [架构总览](/architecture/overview)——七层数据流与各层职责
- [架构决策记录](/architecture/adr)——本教程每个「为什么」的裁决出处
- [核心概念](./concepts)
