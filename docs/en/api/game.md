---
title: Game API
icon: game
prev: /api/README.md
---

# Game API

> 2026-10-04 audit: this page was rewritten against the actual state of HEAD.
> After P1-3 the repo's only AOI implementation is `SceneAoi`;
> `ECSWorld`/`ECSSystem` do not exist (ECS convergence comes with P2);
> attributes follow the `AttributeContainer`/`AttributeManager` container
> model (modifier computation not implemented).

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

// strongly-typed player ID (P0-2): same shape as EntityId but no implicit
// conversion between them — PlayerId identifies the long-lived player object
// (the PlayerAnchor domain), EntityId identifies in-scene entities
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

    // components (IEntityComponent: get_type_name + on_attach/on_detach/on_update)
    template <typename T>
    std::shared_ptr<T> add_component();
    void add_component(ComponentPtr component);
    void remove_component(const std::string& component_type);
    template <typename T>
    std::shared_ptr<T> get_component() const;
};
}
```

**Thread safety**: unsafe (must be called on the same thread — scene-thread
single writer)

---

## apollo::game::world::SceneAoi

After the P1-3 convergence, the repo's only AOI implementation: the 3x3 grid,
owned by one Scene, isolated per scene, decoupled from entity type (it only
knows EntityId + position values).

```cpp
namespace apollo::game::world {
class SceneAoi {
public:
    struct Vec3 { float x = 0.0f; float y = 0.0f; float z = 0.0f; };

    struct Event {
        enum class Kind : std::uint8_t { Enter = 0, Sync, Leave };
        Kind kind = Kind::Enter;
        apollo::game::core::EntityId observer{};  // who the event goes to
        apollo::game::core::EntityId subject{};   // who it is about
    };
    using EventSink = std::function<void(const Event&)>;

    void set_event_sink(EventSink sink);   // silent by default

    SceneAoi();                            // 1x1 degenerate grid
    SceneAoi(float width, float height, float grid_size, float view_radius);

    // set maintenance (maintenance dispatches: Enter/Sync/Leave)
    void enter(apollo::game::core::EntityId id, const Vec3& position);
    void move(apollo::game::core::EntityId id, const Vec3& new_position);
    void leave(apollo::game::core::EntityId id);

    // queries (3x3 grid + radius filtering; viewers_of includes self)
    std::vector<apollo::game::core::EntityId> viewers_of(apollo::game::core::EntityId id) const;
    std::vector<apollo::game::core::EntityId> viewers_at(const Vec3& position) const;
    bool contains(apollo::game::core::EntityId id) const noexcept;
    std::size_t entity_count() const noexcept;
};
}
```

**Thread safety**: unsafe (belongs to the scene thread — single writer, no internal locks)

---

## apollo::game::world::ViewerState

Per-observer view watermark, the set-difference baseline for AOI event
dispatch (the authoritative ledger of "who sees what").

```cpp
namespace apollo::game::world {
class ViewerState {
public:
    void reset(apollo::game::core::EntityId observer);   // register (re-register resets to empty)
    void drop(apollo::game::core::EntityId observer);    // unregister
    bool tracked(apollo::game::core::EntityId observer) const noexcept;
    bool visible(apollo::game::core::EntityId observer,
                 apollo::game::core::EntityId subject) const noexcept;
    std::vector<apollo::game::core::EntityId> visible_set(
        apollo::game::core::EntityId observer) const;
    bool add(apollo::game::core::EntityId observer,
             apollo::game::core::EntityId subject);      // true = newly visible (ENTER)
    bool remove(apollo::game::core::EntityId observer,
                apollo::game::core::EntityId subject);   // true = left view (LEAVE)
    std::vector<apollo::game::core::EntityId> remove_everywhere(
        apollo::game::core::EntityId subject);
    std::size_t size() const noexcept;
};
}
```

---

## apollo::game::battle::BattleSystem (skeleton)

ECS convergence comes with P2-1/P2-2; the current state is an in-scene
skeleton of entity-set registration + tick updates.

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

## apollo::AttributeContainer (attribute container)

Header `include/apollo/game/attributes/attribute.hpp`. `AttributeValue` is a
`std::variant<int32_t, int64_t, float, double, bool, std::string>`.

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
    mutable std::mutex mutex_;   // internal mutual exclusion
};
}
```

---

## apollo::AttributeManager (attribute definition registry)

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
    AttributeManager() = default;   // singleton
    mutable std::mutex mutex_;
};
}
```

`AttributeDef` (attribute definition): `id`/`name`/`type` (value-type enum
INT32..STRING)/`defaultValue`/`minValue`/`maxValue`/`persistent`/`syncToClient`.
`AttributeChangeEvent`: `objectId`/`attributeId`/`oldValue`/`newValue`.

---

## Sessions and Directory (session sublibrary, G-1 deliverable surface)

Nine headers under `modules/game/session/` — cross-process session/fleet
machinery (the G-1 fleet-discovery closing batch + P1-5 recovery); API details
are in the headers and design docs:

- `PlayerDirectory` — online directory (epoch/suspend windows/mirrored-entry
  projection, re-report convergence; EventSink reports
  session_up/down/moved/kicked).
- `DirectoryPublisher` — cross-process directory mirroring (the "APD2" wire:
  Delta/SnapshotRequest/SnapshotReply/FullReport, owner-side snapshot
  publishing).
- `FleetRecoveryCoordinator` — recovery phase made cross-process
  (Normal↔Recovering, converges on open admission).
- `RecoveryCoordinator` — in-process recovery orchestration (journal replay →
  Anchor restore → admission gate, restore-not-kick).
- `AnchorManager`/`PlayerAnchor` — long-lived player-state anchors
  (AnchorState machine, single-writer discipline); `AnchorRewardSink` — battle
  settlement rewards → Anchor accounting.
- `SessionLocator`/`WorldAssignment` — session location and world assignment
  arbitration.

## World and Instances (remaining world-sublibrary pieces)

- `Scene` (SceneTickPhase state) — the scene runtime boundary (AOI ownership,
  judgment domain); `SceneDescriptor`/`SceneTransferRequest` — scene
  description and the five-step scene transfer.
- `Instance` — instances as first-class citizens (eight-state one-way machine
  Create→…→Destroyed).
- `World`/`WorldSession` (WorldSessionState)/`WorldSessionManager` — the world
  runtime and session management.
- `Avatar` (AvatarState) — the player avatar state.

## Battle runtime (battle_runtime / battle_replay)

- `BattleRuntime` (BattlePhase state machine) — the deterministic battle
  domain (logic tick axis only, no wall clock; RNG substream per the
  battle-determinism contract).
- `BattleReplay` (ReplayTuple) — the replay surface (four-tuple: binary_id /
  world_seed / snapshot_pos / inputs; header + free functions, not a class).

## Social (social sublibrary)

`Party` / `Guild` — the party and guild foundations (the party surface is a
session-scoped container, no cross-process sync — cross-server semantics wait
on the decision chain).

---

**Not implemented**: attribute modifier computation (addModifier/AttributeModifier).

---

## apollo::game::attrsync (attribute delta sync)

The per-viewer delta pipeline (attribute-sync
§3.1/§3.2/§3.3/§4.1/§5/§7.1/§8.2), delivered in seven batches:

```cpp
namespace apollo::game::attrsync {

// sequenced change ring log (one per entity)
struct HistoryEntry {
    std::uint64_t seq = 0;
    std::uint32_t attr_id = 0;
    std::int64_t value = 0;  // zigzag-encoded value
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

// per-viewer acked_seq watermark
class ViewerAttrState {
public:
    void set_acked(std::uint64_t entity_id, std::uint64_t seq);
    std::uint64_t acked_seq(std::uint64_t entity_id) const;
    void remove(std::uint64_t entity_id);
    std::size_t size() const noexcept;
    bool tracked(std::uint64_t entity_id) const;  // added in batch 2
};

// varint/zigzag encoding
struct AttrDelta {
    std::uint32_t attr_id = 0;
    std::int64_t value = 0;
    std::uint32_t change_n = 0;
    std::vector<std::uint8_t> encode() const;
    static std::optional<AttrDelta> decode(const std::vector<std::uint8_t>& data);
    // batch 3 fix: decode_into returns consumed byte count (not absolute offset)
    static std::optional<std::size_t> decode_into(const std::vector<std::uint8_t>& data,
                                                   std::size_t offset, AttrDelta& out);
};

// delta assembly + snapshot switching
struct AttrSyncFrame {
    std::uint64_t entity_id = 0;
    std::uint64_t base_seq = 0;
    std::vector<AttrDelta> deltas;
    bool is_snapshot = false;
    std::uint32_t epoch = 0;
};

class DeltaBuilder {
public:
    // snapshot path: deltas carry full current values, base_seq pushed to world_seq (batch 8)
    // both paths prune by viewer domain (batch 9, §2.2)
    using GroupGate = std::function<bool(std::uint64_t viewer,
                                         std::uint64_t entity, AttrDomain domain)>;
    AttrSyncFrame build(std::uint64_t entity_id, std::uint64_t viewer_id,
                        const EntityAttrs& attrs,
                        const ViewerAttrState& viewer) const;
    void set_delta_attr_threshold(std::uint32_t threshold);
    void set_group_gate(GroupGate gate);
};

// batch 9: attribute visibility domains (§2.2)
enum class AttrDomain : std::uint8_t {
    kPublic = 0,  // AOI domain (default)
    kSelf,        // owner only (viewer == entity)
    kTeam,        // owner + same team (via group gate)
    kGuild,       // guild members (via group gate)
    kWorld,       // whole-server broadcast (mechanism layer treats as public)
};

// batch 2: per-entity attribute storage
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
    // full current values (for snapshot payload assembly, batch 8)
    const std::unordered_map<std::uint32_t, std::int64_t>& values() const noexcept;
    // batch 9: attribute visibility domain (undeclared = kPublic)
    void set_domain(std::uint32_t attr_id, AttrDomain domain);
    AttrDomain domain(std::uint32_t attr_id) const;
    std::uint64_t entity_id() const noexcept;
};

// batch 2: pipeline integration layer (AOI wiring + broadcast collect + ACK)
// batch 4: ACK timeout degradation (silent_ticks + is_degraded)
// batch 6: peek read-only precheck
// batch 7: ChangeSink post-write log entry point
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
    // batch 6: read-only collection (does not advance acked, does not clear dirty),
    // for scheduler prechecks
    std::vector<AttrSyncFrame> peek(std::uint64_t viewer, std::uint64_t entity_id);
    void ack(std::uint64_t viewer, std::uint64_t entity_id, std::uint64_t seq);
    void set_ack_timeout_ticks(std::uint64_t ticks);  // batch 4
    bool is_degraded(std::uint64_t viewer, std::uint64_t entity_id) const;  // batch 4
    const EntityAttrs& entity_attrs(std::uint64_t entity_id) const;
    // batch 9: domain projection (§2.2; undeclared domain = kPublic)
    void set_attr_domain(std::uint64_t entity_id, std::uint32_t attr_id, AttrDomain domain);
    void set_group_gate(DeltaBuilder::GroupGate gate);  // team/guild membership test
    // batch 7: §8.2 post-write log entry point (fires after set_attribute succeeds)
    using ChangeSink = std::function<void(std::uint64_t entity_id,
                                          std::uint32_t attr_id,
                                          std::int64_t value)>;
    void set_change_sink(ChangeSink sink);
    void clear_change_sink();
};

// batch 3: frame codec
std::vector<std::uint8_t> encode_frame(const AttrSyncFrame& frame);
std::optional<AttrSyncFrame> decode_frame(const std::vector<std::uint8_t>& data);
std::vector<std::uint8_t> encode_batch(const std::vector<AttrSyncFrame>& frames);
std::optional<std::vector<AttrSyncFrame>> decode_batch(const std::vector<std::uint8_t>& data);

// batch 6: §5 priority and throttling scheduler
class AttrSyncScheduler {
public:
    explicit AttrSyncScheduler(AttrSyncManager& mgr);

    // §5.1 bandwidth budget: per-tick byte cap (default 3276 = 32KB/s @ 10Hz)
    void set_bandwidth_budget(std::uint64_t bytes_per_tick);
    // §5.2 entity importance: 0-255 (contract field importance(class))
    void set_entity_importance(std::uint64_t entity_id, std::uint8_t importance);
    // §5.3 min_interval: tick count; 0=INSTANT (no throttling)
    void set_min_interval(std::uint64_t entity_id, std::uint32_t min_interval);
    // §5.2 starvation protection: force send after this many deferred ticks (default 100)
    void set_max_defer_ticks(std::uint64_t ticks);

    // tick advance: increments the current tick + resets per-viewer budget
    void advance_tick();

    // scheduling: given a viewer and the visible entity set, returns the
    // frames to send this tick (priority-ordered, total encoded size within budget)
    [[nodiscard]] std::vector<AttrSyncFrame> schedule(
        std::uint64_t viewer, const std::vector<std::uint64_t>& visible);

    [[nodiscard]] std::uint64_t current_tick() const noexcept;
    [[nodiscard]] std::uint64_t budget_remaining(std::uint64_t viewer) const;
};

// batch 6: peek read-only collection (does not advance acked, does not clear dirty)
// added on AttrSyncManager:
//   std::vector<AttrSyncFrame> peek(viewer, entity_id);

}  // namespace apollo::game::attrsync
```

### Scene × AttrSync wiring (batch 5)

The AOI event consumer lives on `Scene` (attribute-sync §4.1):

```cpp
namespace apollo::game::world {
class Scene {
public:
    // attach: back-registers existing entities/avatars + takes over the SceneAoi event sink
    void attach_attrsync(apollo::game::attrsync::AttrSyncManager& mgr);
    // clears the sink + drops the reference (attrsync_ = nullptr)
    void detach_attrsync();
    [[nodiscard]] bool attrsync_attached() const noexcept;
    // takes away the snapshot frames accumulated over the first-sight period
    // (accumulation restarts afterwards)
    [[nodiscard]] std::vector<apollo::game::attrsync::AttrSyncFrame>
    drain_attrsync_frames();
};
}
```

**Semantics**: Enter/Sync on first sight (the manager has no (observer,
subject) viewer state for it) → build `ViewerState` + a snapshot frame;
in-view cross-cell Sync triggers no protocol; Leave → reclaim the
`ViewerState`. In `enter()`, attrsync registration happens before
`aoi_.enter()` (the newcomer's own snapshot frame requires the subject to be
registered already). After attach, `spawn_entity`/`enter` register the entity
id immediately.

**Thread safety**: unsafe (scene-thread single writer; the sink captures this,
and Scene is not copied — World holds it by unique_ptr)

---

**Boundaries**: the client send path and slice compound types are later batches.
