---
title: Game 模块
icon: game
order: 7
category:
  - 模块
tag:
  - game
  - 游戏
---

# Game 模块

Game 模块提供游戏领域层功能，分六个子库：core（实体基元）、world（场景/会话/Avatar）、
session（锚点/在线目录/恢复/编队恢复）、attributes（属性）、battle（战斗运行时）、
social（组队/公会）。

## Entity（apollo::game::core）

游戏实体基类。`EntityId`/`PlayerId` 强分型互不隐式转换（P0-2：编译器拦住
「把实体 ID 当玩家 ID 传」一类错误）；支持组件挂载。

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

## 组件系统（IEntityComponent）

组件是挂在实体上的行为/数据单元，按类型名索引（`unordered_map<string, ComponentPtr>`）：

```cpp
class HealthComponent : public apollo::game::core::IEntityComponent {
public:
    std::string get_type_name() const override { return "health"; }
    void take_damage(int damage) { hp_ = std::max(0, hp_ - damage); }

private:
    int hp_ = 100;
};
```

## AOI 系统（SceneAoi）

全仓唯一 AOI 实现（P1-3 收敛后）：`apollo::game::world::SceneAoi`，Scene 独享、
按 scene 隔离；`ViewerState` 记录逐观察者视野水位（Enter/Sync/Leave 差集基准）。

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

下发到网关的广播面留 P3-2；属性管线对 AOI 事件的消费未接线（attribute-sync §4.1）。

## 战斗系统（battle 子库）

三层：`BattleSystem`（场景内实体集合登记 + tick 骨架）、`BattleRuntime`
（确定性战斗域——BattlePhase 状态机、逻辑拍 + wall clock 双时间轴、RNG
子流契约，battle-determinism 批交付）、`BattleReplay`（ReplayTuple 回放面）。

```cpp
#include <apollo/game/battle/battle_system.hpp>
#include <apollo/game/battle/battle_runtime.hpp>

apollo::game::battle::BattleSystem battle;
battle.add_entity(std::make_shared<apollo::game::core::Entity>(EntityId{1}));
battle.update(0.1f);
```

ECS 多套收敛、技能/Buff/状态机随 P2-1/P2-2（未实现）；
旧稿里的 `ECSWorld`/`ECSSystem` API 在仓库中不存在。

## 会话与目录（session 子库，G-1 交付面）

锚点/在线目录/恢复/编队恢复九头：`AnchorManager`/`PlayerAnchor`（长期玩家态
唯一写点）+ `AnchorRewardSink`（结算落账）+ `PlayerDirectory`（在线目录，
epoch/suspend 窗口/重报收敛）+ `DirectoryPublisher`（"APD2" 目录镜像）+
`FleetRecoveryCoordinator`（恢复相位跨进程化）+ `RecoveryCoordinator`
（journal replay → Anchor restore → 准入闸）+ `SessionLocator`/
`WorldAssignment`。API 明细见 [Game API](/api/game) 与
session-and-online-directory 设计件。

## 社交（social 子库）

`Party` / `Guild` 组队与公会基础容器（跨服同步语义挂拍板链，未接线）。

## 属性系统（attributes）

`AttributeContainer`（按 objectId 承载属性值，Set/Get/Add + 变更监听）+
`AttributeManager`（属性定义注册与容器生命周期，`RegisterAttribute`/
`CreateContainer`/`LoadFromConfig`）。头文件在顶层 `include/apollo/game/attributes/`。

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

逐 viewer 的 delta 同步管线未接线（契约 `attr_batch` 已入 messages.xml，管线随属性批次）。

## NPC AI

> **未实现**（2026-10-04 对账）：`apollo/game/ai/ai_controller.hpp` 不存在，
> 仓库无 AI/寻路模块（旧 9 批次计划的寻路批次未列入主线，见 todo.md 遗留登记）。

## 依赖

- apollo::core
- apollo::base

## 链接

```cmake
target_link_libraries(my_app
    apollo::game_core
    apollo::game_world
    apollo::game_session
    apollo::game_attributes
    apollo::game_battle
)
```
