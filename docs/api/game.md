---
title: Game API
icon: game
prev: /api/README.md
---

# Game API

> 2026-10-04 对账：本页按 HEAD 实况重写。P1-3 后全仓唯一 AOI 实现为
> `SceneAoi`；`ECSWorld`/`ECSSystem` 不存在（ECS 收敛随 P2）；属性为
> `AttributeContainer`/`AttributeManager` 容器模型（修饰符计算未实现）。

## apollo::game::core::EntityId / PlayerId

```cpp
namespace apollo::game::core {
class EntityId {
public:
    constexpr EntityId();
    constexpr explicit EntityId(uint64_t id);
    constexpr uint64_t value() const;
    constexpr bool is_valid() const;
    static constexpr EntityId invalid();
};

// 玩家 ID 强分型（P0-2）：与 EntityId 同构但互不隐式转换——
// PlayerId 标识长期玩家对象（PlayerAnchor 域），EntityId 标识场景内实体
class PlayerId {
public:
    constexpr explicit PlayerId(uint64_t id);
    constexpr uint64_t value() const;
    constexpr bool is_valid() const;
};
}
```

---

## apollo::game::core::Entity

```cpp
namespace apollo::game::core {
class IEntity {
public:
    virtual ~IEntity() = default;
    virtual EntityId get_id() const = 0;
    virtual std::string get_type() const = 0;
    virtual void on_spawn() {}
    virtual void on_despawn() {}
    virtual void on_update(float delta_time) {}
};

class Entity : public IEntity {
public:
    explicit Entity(EntityId id, std::string type = "Entity");

    EntityId get_id() const override;
    std::string get_type() const override;

    void on_spawn() override;
    void on_despawn() override;
    void on_update(float delta_time) override;

    // 组件（IEntityComponent：get_type_name + on_attach/on_detach/on_update）
    template <typename T>
    std::shared_ptr<T> add_component();
    void add_component(ComponentPtr component);
    void remove_component(const std::string& component_type);
    template <typename T>
    std::shared_ptr<T> get_component() const;
};
}
```

**线程安全**: 不安全（必须在同一线程调用（场景线程单写者））

---

## apollo::game::world::SceneAoi

P1-3 收敛后的全仓唯一 AOI 实现：九宫格，Scene 独享、按 scene 隔离，
与实体类型解耦（只认 EntityId + 位置值）。

```cpp
namespace apollo::game::world {
class SceneAoi {
public:
    struct Vec3 { float x = 0.0f; float y = 0.0f; float z = 0.0f; };

    struct Event {
        enum class Kind : std::uint8_t { Enter = 0, Sync, Leave };
        Kind kind = Kind::Enter;
        apollo::game::core::EntityId observer{};  // 事件发给谁
        apollo::game::core::EntityId subject{};   // 关于谁
    };
    using EventSink = std::function<void(const Event&)>;

    void set_event_sink(EventSink sink);   // 缺省静默

    SceneAoi();                            // 1x1 退化网格
    SceneAoi(float width, float height, float grid_size, float view_radius);

    // 集合维护（维护即分发：Enter/Sync/Leave）
    void enter(apollo::game::core::EntityId id, const Vec3& position);
    void move(apollo::game::core::EntityId id, const Vec3& new_position);
    void leave(apollo::game::core::EntityId id);

    // 查询（九宫格 + 半径过滤；viewers_of 含自身）
    std::vector<apollo::game::core::EntityId> viewers_of(apollo::game::core::EntityId id) const;
    std::vector<apollo::game::core::EntityId> viewers_at(const Vec3& position) const;
    bool contains(apollo::game::core::EntityId id) const noexcept;
    std::size_t entity_count() const noexcept;
};
}
```

**线程安全**: 不安全（归场景线程（单写者，无内锁））

---

## apollo::game::world::ViewerState

逐观察者视野水位，AOI 事件分发的差集基准（「谁看见什么」的权威账本）。

```cpp
namespace apollo::game::world {
class ViewerState {
public:
    void reset(apollo::game::core::EntityId observer);   // 登记（重复登记清空重置）
    void drop(apollo::game::core::EntityId observer);    // 注销
    bool tracked(apollo::game::core::EntityId observer) const noexcept;
    bool visible(apollo::game::core::EntityId observer,
                 apollo::game::core::EntityId subject) const noexcept;
    std::vector<apollo::game::core::EntityId> visible_set(
        apollo::game::core::EntityId observer) const;
    bool add(apollo::game::core::EntityId observer,
             apollo::game::core::EntityId subject);      // true = 新进（ENTER）
    bool remove(apollo::game::core::EntityId observer,
                apollo::game::core::EntityId subject);   // true = 出视野（LEAVE）
    std::vector<apollo::game::core::EntityId> remove_everywhere(
        apollo::game::core::EntityId subject);
    std::size_t size() const noexcept;
};
}
```

---

## apollo::game::battle::BattleSystem（骨架）

ECS 收敛随 P2-1/P2-2；现状为实体集合登记 + tick 更新的场景内骨架。

```cpp
namespace apollo::game::battle {
class BattleSystem {
public:
    BattleSystem();
    void update(float delta_time);
    void add_entity(EntityPtr entity);
    void remove_entity(apollo::game::core::EntityId id);
    size_t get_entity_count() const;
};
}
```

---

## apollo::AttributeContainer（属性容器）

头文件 `include/apollo/game/attributes/attribute.hpp`。`AttributeValue` 为
`std::variant<int32_t, int64_t, float, double, bool, std::string>`。

```cpp
namespace apollo {
class AttributeContainer {
public:
    explicit AttributeContainer(uint64_t objectId);

    bool SetAttribute(uint32_t attributeId, const AttributeValue& value);
    bool GetAttribute(uint32_t attributeId, AttributeValue& value) const;
    template<typename T>
    T GetAttribute(uint32_t attributeId, const T& defaultValue = T{}) const;
    bool AddAttribute(uint32_t attributeId, const AttributeValue& delta);
    void SetAttributes(const std::vector<std::pair<uint32_t, AttributeValue>>& attrs);
    const std::unordered_map<uint32_t, AttributeValue>& GetAllAttributes() const;
    void Clear();
    void SetChangeListener(std::function<void(const AttributeChangeEvent&)> listener);

private:
    mutable std::mutex mutex_;   // 内部互斥
};
}
```

---

## apollo::AttributeManager（属性定义注册）

```cpp
namespace apollo {
class AttributeManager {
public:
    static AttributeManager& Instance();

    bool RegisterAttribute(const AttributeDef& def);
    const AttributeDef* GetAttributeDef(uint32_t attributeId) const;
    const std::unordered_map<uint32_t, AttributeDef>& GetAllAttributes() const;

    std::shared_ptr<AttributeContainer> CreateContainer(uint64_t objectId);
    std::shared_ptr<AttributeContainer> GetContainer(uint64_t objectId);
    void DestroyContainer(uint64_t objectId);

    bool LoadFromConfig(const std::string& configFile);

private:
    AttributeManager() = default;   // 单例
    mutable std::mutex mutex_;
};
}
```

`AttributeDef`（属性定义）：`id`/`name`/`type`（值类型枚举 INT32..STRING）/
`defaultValue`/`minValue`/`maxValue`/`persistent`/`syncToClient`。
`AttributeChangeEvent`：`objectId`/`attributeId`/`oldValue`/`newValue`。

---

## 会话与目录（session 子库，G-1 交付面）

`modules/game/session/` 九头——跨进程会话/编队 machinery（G-1 编队发现收尾批
+ P1-5 恢复），API 明细见各头与设计件：

- `PlayerDirectory`——在线目录（epoch/suspend 窗口/镜像条目投影，重报收敛；
  EventSink 上报 session_up/down/moved/kicked）。
- `DirectoryPublisher`——目录跨进程镜像（"APD2" wire：Delta/SnapshotRequest/
  SnapshotReply/FullReport，owner 侧快照发布）。
- `FleetRecoveryCoordinator`——恢复相位跨进程化（Normal↔Recovering，收敛
  开放准入）。
- `RecoveryCoordinator`——进程内恢复编排（journal replay → Anchor restore →
  准入闸，restore-not-kick）。
- `AnchorManager`/`PlayerAnchor`——长期玩家态锚点（AnchorState 状态机、
  唯一写点纪律）；`AnchorRewardSink`——battle 结算奖励 → Anchor 落账。
- `SessionLocator`/`WorldAssignment`——会话定位与世界分配裁决。

## 世界与副本（world 子库其余件）

- `Scene`（SceneTickPhase 状态）——场景运行时边界（AOI 所有权、判定域）；
  `SceneDescriptor`/`SceneTransferRequest`——场景描述与换幕五步。
- `Instance`——副本一等公民（八态单向机 Create→…→Destroyed）。
- `World`/`WorldSession`（WorldSessionState）/`WorldSessionManager`——世界
  运行时与会话管理。
- `Avatar`（AvatarState）——玩家化身态。

## 战斗运行时（battle_runtime / battle_replay）

- `BattleRuntime`（BattlePhase 状态机）——确定性战斗域（仅逻辑拍轴，无 wall clock；RNG 子流 battle-determinism 契约）。
- `BattleReplay`（ReplayTuple）——回放面（四元组：binary_id / world_seed / snapshot_pos / inputs；header + 自由函数，非类）。

## 社交（social 子库）

`Party` / `Guild`——组队与公会基础结构（组队面为会话型容器，无跨进程
同步——跨服语义挂拍板链）。

---

**未实现**：属性修饰符计算（addModifier/AttributeModifier）。

---

## apollo::game::attrsync（属性增量同步）

per-viewer delta 管线（attribute-sync §3.1/§3.2/§3.3/§4.1/§5/§7.1/§8.2），七批交付：

```cpp
namespace apollo::game::attrsync {

// 序号化变更环形日志（每实体一份）
struct HistoryEntry {
    std::uint64_t seq = 0;
    std::uint32_t attr_id = 0;
    std::int64_t value = 0;  // zigzag 编码后的值
};

class ChangeHistory {
public:
    explicit ChangeHistory(std::size_t capacity = 1024);
    std::uint64_t append(std::uint32_t attr_id, std::int64_t value);
    std::vector<HistoryEntry> entries_since(std::uint64_t since_seq) const;
    std::uint64_t world_seq() const noexcept;
    std::size_t capacity() const noexcept;
    std::size_t size() const noexcept;
    bool wrapped() const noexcept;
};

// per-viewer acked_seq 水位
class ViewerAttrState {
public:
    void set_acked(std::uint64_t entity_id, std::uint64_t seq);
    std::uint64_t acked_seq(std::uint64_t entity_id) const;
    void remove(std::uint64_t entity_id);
    std::size_t size() const noexcept;
    bool tracked(std::uint64_t entity_id) const;  // 批 2 新增
};

// varint/zigzag 编码
struct AttrDelta {
    std::uint32_t attr_id = 0;
    std::int64_t value = 0;
    std::uint32_t change_n = 0;
    std::vector<std::uint8_t> encode() const;
    static std::optional<AttrDelta> decode(const std::vector<std::uint8_t>& data);
    // 批 3 修复：decode_into 返回消耗字节数（非绝对偏移）
    static std::optional<std::size_t> decode_into(const std::vector<std::uint8_t>& data,
                                                   std::size_t offset, AttrDelta& out);
};

// delta 组装 + 快照切换
struct AttrSyncFrame {
    std::uint64_t entity_id = 0;
    std::uint64_t base_seq = 0;
    std::vector<AttrDelta> deltas;
    bool is_snapshot = false;
    std::uint32_t epoch = 0;
};

class DeltaBuilder {
public:
    // 快照路径：deltas 填全量当前值，base_seq 推到 world_seq（批 8）
    AttrSyncFrame build(std::uint64_t entity_id,
                        const EntityAttrs& attrs,
                        const ViewerAttrState& viewer) const;
    void set_delta_attr_threshold(std::uint32_t threshold);
};

// 批 2：每实体属性存储
class EntityAttrs {
public:
    explicit EntityAttrs(std::uint64_t entity_id, std::size_t history_capacity = 1024);
    void set(std::uint32_t attr_id, std::int64_t value);
    std::int64_t get(std::uint32_t attr_id) const;
    void set_all(const std::vector<std::pair<std::uint32_t, std::int64_t>>& attrs);
    bool is_dirty(std::uint32_t attr_id) const;
    void clear_dirty();
    std::size_t dirty_count() const;
    const ChangeHistory& history() const noexcept;
    // 全量当前值（快照负载组装用，批 8）
    const std::unordered_map<std::uint32_t, std::int64_t>& values() const noexcept;
    std::uint64_t entity_id() const noexcept;
};

// 批 2：管线集成层（AOI 接线 + broadcast collect + ACK）
// 批 4：ACK 超时降级（silent_ticks + is_degraded）
// 批 6：peek 只读预检
// 批 7：ChangeSink 写后日志入口
class AttrSyncManager {
public:
    void register_entity(std::uint64_t entity_id);
    bool has_entity(std::uint64_t entity_id) const;
    void set_attribute(std::uint64_t entity_id, std::uint32_t attr_id, std::int64_t value);
    std::int64_t get_attribute(std::uint64_t entity_id, std::uint32_t attr_id) const;
    void on_viewer_enter(std::uint64_t viewer, std::uint64_t entity_id);
    std::vector<AttrSyncFrame> on_viewer_enter_collect(std::uint64_t viewer, std::uint64_t entity_id);
    void on_viewer_leave(std::uint64_t viewer, std::uint64_t entity_id);
    bool has_viewer(std::uint64_t viewer, std::uint64_t entity_id) const;
    std::vector<AttrSyncFrame> collect(std::uint64_t viewer, std::uint64_t entity_id);
    // 批 6：只读收集（不推进 acked、不清 dirty），供调度器预检
    std::vector<AttrSyncFrame> peek(std::uint64_t viewer, std::uint64_t entity_id);
    void ack(std::uint64_t viewer, std::uint64_t entity_id, std::uint64_t seq);
    void set_ack_timeout_ticks(std::uint64_t ticks);  // 批 4
    bool is_degraded(std::uint64_t viewer, std::uint64_t entity_id) const;  // 批 4
    const EntityAttrs& entity_attrs(std::uint64_t entity_id) const;
    // 批 7：§8.2 写后日志入口（set_attribute 成功后触发）
    using ChangeSink = std::function<void(std::uint64_t entity_id,
                                          std::uint32_t attr_id,
                                          std::int64_t value)>;
    void set_change_sink(ChangeSink sink);
    void clear_change_sink();
};

// 批 3：帧编解码
std::vector<std::uint8_t> encode_frame(const AttrSyncFrame& frame);
std::optional<AttrSyncFrame> decode_frame(const std::vector<std::uint8_t>& data);
std::vector<std::uint8_t> encode_batch(const std::vector<AttrSyncFrame>& frames);
std::optional<std::vector<AttrSyncFrame>> decode_batch(const std::vector<std::uint8_t>& data);

// 批 6：§5 优先级与节流调度
class AttrSyncScheduler {
public:
    explicit AttrSyncScheduler(AttrSyncManager& mgr);

    // §5.1 带宽预算：每拍字节上限（默认 3276 = 32KB/s @ 10Hz）
    void set_bandwidth_budget(std::uint64_t bytes_per_tick);
    // §5.2 实体重要性：0-255（契约字段 importance(class)）
    void set_entity_importance(std::uint64_t entity_id, std::uint8_t importance);
    // §5.3 min_interval：tick 数；0=INSTANT（不节流）
    void set_min_interval(std::uint64_t entity_id, std::uint32_t min_interval);
    // §5.2 饥饿保护：推迟超过此拍数强制发送（默认 100）
    void set_max_defer_ticks(std::uint64_t ticks);

    // 拍推进：递增当前拍号 + 重置每 viewer 预算
    void advance_tick();

    // 调度：给定 viewer 与可见实体集，返回本拍应下发的帧
    // （按优先级排序，总编码量 ≤ 预算）
    [[nodiscard]] std::vector<AttrSyncFrame> schedule(
        std::uint64_t viewer, const std::vector<std::uint64_t>& visible);

    [[nodiscard]] std::uint64_t current_tick() const noexcept;
    [[nodiscard]] std::uint64_t budget_remaining(std::uint64_t viewer) const;
};

// 批 6：peek 只读收集（不推进 acked、不清 dirty）
// 在 AttrSyncManager 上新增：
//   std::vector<AttrSyncFrame> peek(viewer, entity_id);

}  // namespace apollo::game::attrsync
```

### Scene × AttrSync 接线（批 5）

AOI 事件消费者落在 `Scene`（attribute-sync §4.1）：

```cpp
namespace apollo::game::world {
class Scene {
public:
    // attach：补注册既有实体/avatar + 接管 SceneAoi 事件 sink
    void attach_attrsync(apollo::game::attrsync::AttrSyncManager& mgr);
    // 清 sink + 断引用（attrsync_ = nullptr）
    void detach_attrsync();
    [[nodiscard]] bool attrsync_attached() const noexcept;
    // 取走首见期累积的快照帧（此后重新累积）
    [[nodiscard]] std::vector<apollo::game::attrsync::AttrSyncFrame>
    drain_attrsync_frames();
};
}
```

**语义**：Enter/Sync 首见（manager 无该 (observer, subject) viewer 态）→ 建 `ViewerState` + 快照帧；视野内跨格 Sync 不触发协议；Leave → 回收 `ViewerState`。`enter()` 中 attrsync 注册先于 `aoi_.enter()`（新人自身快照帧要求 subject 已注册）。attach 后 `spawn_entity`/`enter` 即时注册 entity id。

**线程安全**: 不安全（归场景线程单写者；sink 自捕获 this，Scene 不拷贝——World 以 unique_ptr 持有）

---

**边界**：客户端发送路径、slice 复合类型 均为后续批次。
