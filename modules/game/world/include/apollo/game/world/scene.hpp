#pragma once

#include "apollo/game/core/entity.hpp"
#include "apollo/game/world/avatar.hpp"
#include "apollo/game/world/scene_aoi.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace apollo::game::world {

using EntityPtr = std::shared_ptr<apollo::game::core::Entity>;

// Scene tick 六阶段（attribute-sync 口径，concurrency.md §表）：
//   simulate → recalc → aoidecay → collect → budget+flush → persist-batch
// P0-3 落骨架：阶段顺序固定、逐阶段计数可观察；实体 on_update 在 simulate。
enum class SceneTickPhase : std::uint8_t {
    Simulate = 0,
    Recalc,
    AoiDecay,
    Collect,
    BudgetFlush,
    PersistBatch,
};

[[nodiscard]] std::string_view to_string(SceneTickPhase phase);

// Scene（契约 §1.1）：世界中一块有归属的逻辑单元——实体容器、AOI 边界、
// 地理数据载体。P0-3 升级为运行时容器：
//   - 拥有实体集合（承接 EntityManager 职责，行为等价迁移）；
//   - 拥有 AOI（SceneAoi，按 scene 隔离）；
//   - 玩家进出 API（enter/leave，Avatar 进 scene 生出 scene 死）；
//   - tick 六阶段骨架（单写者，scene 线程内权威）。
class Scene {
public:
    explicit Scene(std::string name = "DefaultScene");
    Scene(std::uint64_t scene_id, std::string name);

    // ---- 身份 ----
    [[nodiscard]] std::uint64_t scene_id() const noexcept;
    const std::string& get_name() const { return name_; }

    // ---- 实体集合（Scene 拥有）----
    void spawn_entity(EntityPtr entity);
    void despawn_entity(apollo::game::core::EntityId id);
    EntityPtr get_entity(apollo::game::core::EntityId id) const;
    size_t get_entity_count() const { return entities_.size(); }

    // ---- 玩家进出（Avatar 随进出生死；player-object-model §3）----
    // enter：Avatar 挂 scene + AOI 入列；重复进入拒绝（返回 false）。
    // position 为进场落点（出生点/门口由玩法规则决定）。
    bool enter(const AvatarPtr& avatar, const SceneAoi::Vec3& position);
    // leave：Avatar 出 scene（对象由调用方/Instance 处置——P0-3 由
    // cell-server 持容器，Instance::destroy 时清场）。
    bool leave(apollo::game::core::PlayerId player_id);
    // 断线挂机窗口（P1-6，lifecycle §2.4）：Avatar Active↔Suspended，
    // 驻留场景（AOI 保留）；窗口满由调用方经 leave() 移除销毁
    bool suspend_avatar(apollo::game::core::PlayerId player_id);
    bool resume_avatar(apollo::game::core::PlayerId player_id);

    [[nodiscard]] AvatarPtr get_avatar(apollo::game::core::PlayerId player_id) const;
    [[nodiscard]] bool has_avatar(apollo::game::core::PlayerId player_id) const noexcept;
    [[nodiscard]] std::size_t avatar_count() const noexcept;
    [[nodiscard]] const std::vector<apollo::game::core::PlayerId>& avatars() const noexcept;

    // ---- AOI（Scene 拥有；scene_id 隔离）----
    [[nodiscard]] SceneAoi& aoi() noexcept;
    [[nodiscard]] const SceneAoi& aoi() const noexcept;

    // ---- tick 六阶段 ----
    void update(float delta_time);        // 兼容旧调用（转发 tick）
    void tick(double delta_seconds) noexcept;
    [[nodiscard]] std::uint64_t tick_count() const noexcept;
    // 各阶段累计执行次数（可观察；正常时各阶段 == tick_count）
    [[nodiscard]] std::uint64_t phase_count(SceneTickPhase phase) const noexcept;

private:
    void run_phase(SceneTickPhase phase) noexcept;

    std::uint64_t scene_id_ = 0;
    std::string name_;
    std::unordered_map<std::uint64_t, EntityPtr> entities_;
    std::unordered_map<std::uint64_t, AvatarPtr> avatars_;  // key = PlayerId.value()
    std::vector<apollo::game::core::PlayerId> avatar_order_;  // 进场顺序可观察
    SceneAoi aoi_;
    std::uint64_t tick_count_ = 0;
    std::uint64_t phase_counts_[6] = {0, 0, 0, 0, 0, 0};
};

} // namespace apollo::game::world