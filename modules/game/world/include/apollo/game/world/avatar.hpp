#pragma once

#include "apollo/game/core/entity.hpp"
#include "apollo/game/session/player_anchor.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace apollo::game::world {

// Avatar 生命周期状态（term-contract §1.2 / player-object-model §3）。
// 「进 scene 生、出 scene 死」：
//   Active    —— 已在 scene 内，可参与三职责（移动/战斗/AOI 广播）；
//   Suspended —— 断线保活窗口内挂机（#12 掉线保活窗口）；窗口满由 scene 移除销毁；
//   Leaving   —— 已发起出 scene（换幕/登出），不再接收新输入，等待 scene 侧销毁。
enum class AvatarState : std::uint8_t {
    Active = 0,
    Suspended,
    Leaving,
};

// 场景内空间权威（player-object-model §2 Cell 精简）：Cell 职责收窄后只剩三件——
// 移动权威（reconcile）、战斗权威、AOI 广播。连接/持久化/跨场景/断线全部不归它。
//
// 生命周期由 Scene 管理（P0-3 起 instance.enter / instance.leave 接管进出）；
// 换幕时状态从 PlayerAnchor（Base）投影（home_zone_id 等长期属性）。
class Avatar {
public:
    // name 为创建时投影快照（anchor 域暂无该字段，P0-2 由外部传入；
    // 属性/装备/长期 buff 等投影面随 P0-4 对象模型扩展）。
    Avatar(apollo::game::core::PlayerId player_id,
           apollo::game::core::EntityId entity_id,
           std::string name = {});

    [[nodiscard]] apollo::game::core::PlayerId player_id() const noexcept;
    [[nodiscard]] apollo::game::core::EntityId entity_id() const noexcept;
    [[nodiscard]] const std::string& name() const noexcept;
    void set_name(std::string name) noexcept;

    // ---- 场景归属（scene 是唯一空间单元；attach 由 scene 进出 API 调用） ----
    [[nodiscard]] std::uint64_t scene_id() const noexcept;
    void attach_scene(std::uint64_t scene_id) noexcept;
    void detach_scene() noexcept;

    // ---- 生命周期 ----
    [[nodiscard]] AvatarState state() const noexcept;
    void suspend() noexcept;
    void resume() noexcept;
    void begin_leave() noexcept;

    // ---- 移动权威（Cell 职责一）：server-authority 位置 + 客户端 reconcile ----
    struct Position {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    [[nodiscard]] const Position& position() const noexcept;
    void set_position(const Position& position) noexcept;  // 场景初始化/换幕落点

    // 客户端移动输入（view 序号）：序号大于等于已应用序号才接受——拒绝乱序/过期
    // 回滚包（反作弊最小面，P3-2 性能批再评估代价模型）。返回是否接受。
    bool reconcile(std::uint64_t view_seq, const Position& target) noexcept;
    [[nodiscard]] std::uint64_t last_reconciled_seq() const noexcept;

    // ---- 战斗权威（Cell 职责二）：结算唯一侧在场景线程 tick 边界 ----
    [[nodiscard]] int hp() const noexcept;
    [[nodiscard]] int max_hp() const noexcept;
    void set_hp(int hp, int max_hp) noexcept;
    bool apply_damage(int amount) noexcept;  // 返回是否被该次结算击杀（hp<=0）

    // ---- AOI 广播（Cell 职责三）：viewer 集合 + 广播槽位 ----
    void add_viewer(apollo::game::core::EntityId viewer);
    void remove_viewer(apollo::game::core::EntityId viewer);
    [[nodiscard]] bool is_viewer(apollo::game::core::EntityId viewer) const noexcept;
    [[nodiscard]] const std::vector<apollo::game::core::EntityId>& viewers() const noexcept;
    [[nodiscard]] std::size_t viewer_count() const noexcept;

    // 广播出口：OA·OI 集合由 Scene 维护（P0-3），此处仅计数（可测试可观察）。
    // P0-2 语义：viewer 集合为骨架，实际广播路径随 scene 集成。
    void broadcast() noexcept;
    [[nodiscard]] std::uint64_t broadcast_count() const noexcept;

    // ---- Anchor 投影（player-object-model §3）：长期属性从 Base 单向拷贝 ----
    void project_from(const apollo::game::session::PlayerAnchor& anchor) noexcept;
    [[nodiscard]] bool has_projection() const noexcept;
    [[nodiscard]] std::uint32_t home_zone_id() const noexcept;

private:
    apollo::game::core::PlayerId player_id_;
    apollo::game::core::EntityId entity_id_;
    std::string name_;
    std::uint64_t scene_id_ = 0;
    AvatarState state_ = AvatarState::Active;
    Position position_{};
    std::uint64_t last_reconciled_seq_ = 0;
    int hp_ = 100;
    int max_hp_ = 100;
    std::vector<apollo::game::core::EntityId> viewers_;
    std::uint64_t broadcast_count_ = 0;
    std::uint32_t home_zone_id_ = 0;
    bool projected_ = false;  // 是否完成至少一次 Anchor 投影（换幕正确性校验）
};

using AvatarPtr = std::shared_ptr<Avatar>;

} // namespace apollo::game::world