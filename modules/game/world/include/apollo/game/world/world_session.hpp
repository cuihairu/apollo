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

    // ---- 断线挂机窗口（P1-6，lifecycle §2.4）----
    // suspend() 的带窗形态：deadline = now + window（tick 域由调用方定，
    // 世界侧不依赖真实时钟）；resume_token 与窗口同源 TTL——token 死即
    // 窗口死，双键 (session_id, token) 校验（与目录 resume (session_id,
    // anchor_epoch) 同形）。窗口 sweep 在 WorldSessionManager。
    [[nodiscard]] bool suspend_window(std::uint64_t now_tick, std::uint64_t window_ticks,
                                      std::uint64_t resume_token);
    // 窗口内 resume：仅 Suspended 且 token 匹配且未过期
    [[nodiscard]] bool resume(std::uint64_t resume_token, std::uint64_t now_tick);
    // 窗口是否已满（sweep 判据；无窗口=deadline 0 永不满）
    [[nodiscard]] bool resume_window_expired(std::uint64_t now_tick) const noexcept;
    [[nodiscard]] std::uint64_t resume_token() const noexcept;
    [[nodiscard]] std::uint64_t resume_deadline_tick() const noexcept;

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
    std::uint64_t resume_deadline_tick_ = 0;  // 0 = 无窗口
    std::uint64_t resume_token_ = 0;
};

} // namespace apollo::game::world
