#pragma once

#include "apollo/game/core/entity.hpp"
#include "apollo/game/world/instance.hpp"

#include <cstdint>
#include <string_view>

namespace apollo::game::world {

enum class WorldSessionState : std::uint8_t {
    Entering = 0,
    Active,
    Suspended,
    TransferringOut,
    TransferringIn,
    Leaving,
    Closed,
};

class WorldSession {
public:
    using SessionId = std::uint64_t;
    // player 身份用 core::PlayerId 强分型（P0-2）：与 EntityId 互不隐式转换，
    // 杜绝「实体 ID 当玩家 ID 传」（cell_server.cpp 历史缺陷）。SessionId 保留
    // alias（连接域身份证，跨面仍以 raw 传参，与 gateway 会话面一致）。
    using PlayerId = apollo::game::core::PlayerId;

    WorldSession(SessionId session_id, PlayerId player_id);

    SessionId session_id() const;
    PlayerId player_id() const;

    void bind_avatar(apollo::game::core::EntityId avatar_entity_id);
    apollo::game::core::EntityId avatar_entity_id() const;

    void assign_map_instance(Instance::InstanceId map_instance_id);
    Instance::InstanceId map_instance_id() const;

    void assign_world(std::uint32_t world_id);
    std::uint32_t world_id() const;

    void assign_space(std::uint64_t space_id);
    std::uint64_t space_id() const;

    void set_state(WorldSessionState state);
    WorldSessionState state() const;

    void set_route_version(std::uint64_t route_version);
    std::uint64_t route_version() const;

    // ---- 状态机守卫迁移（P0-4，lifecycle §2 审计收口）----
    // 全部返回 false 表示「当前态不允许该迁移」且状态不变；Closed/Leaving
    // 是受保护态：Closed 会话不可 resume/suspend/transfer。
    [[nodiscard]] bool suspend();   // 仅 Active
    [[nodiscard]] bool resume();    // 仅 Suspended
    [[nodiscard]] bool begin_transfer(std::uint32_t target_world_id,
                                      Instance::InstanceId target_map_instance_id,
                                      std::uint64_t target_space_id,
                                      bool inbound = false);  // 仅 Active
    // 断线发生在转移中（P1-1，lifecycle §2.3 差距 ④）：TransferringOut/In
    // → Suspended 挂机窗口；pending 目标保留，重连后二选一收口——
    // complete_transfer（继续落到目标）或 abort_transfer（回滚）。
    [[nodiscard]] bool suspend_from_transfer();
    // 转移收口守卫（P1-1 异步化）：TransferringOut/In 直呼，或断线挂机中
    // 带 pending 转移（suspend_from_transfer 之后）也可收口；纯 Suspended
    // （无 pending）仍拒绝。
    [[nodiscard]] bool can_resolve_pending_transfer() const noexcept;
    [[nodiscard]] bool complete_transfer();
    [[nodiscard]] bool abort_transfer();                      // 清 pending、回滚进转移动前态
    [[nodiscard]] bool begin_leave();                         // Leaving/Closed 拒绝

    std::uint32_t pending_world_id() const;
    Instance::InstanceId pending_map_instance_id() const;
    std::uint64_t pending_space_id() const;

    static std::string_view to_string(WorldSessionState state);

private:
    SessionId session_id_ = 0;
    PlayerId player_id_{};
    apollo::game::core::EntityId avatar_entity_id_{};    
    Instance::InstanceId map_instance_id_ = 0;
    std::uint32_t world_id_ = 0;
    std::uint64_t space_id_ = 0;
    WorldSessionState state_ = WorldSessionState::Entering;
    std::uint64_t route_version_ = 0;
    std::uint32_t pending_world_id_ = 0;
    Instance::InstanceId pending_map_instance_id_ = 0;
    std::uint64_t pending_space_id_ = 0;
    WorldSessionState pre_transfer_state_ = WorldSessionState::Active;  // abort 回滚落点
};

} // namespace apollo::game::world
