#pragma once

#include "apollo/game/session/world_assignment.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace apollo::game::session {

enum class AnchorState : std::uint8_t {
    Loading = 0,
    Online,
    Transferring,
    Disconnected,
    Saving,
    Offline,
};

struct SessionBinding {
    std::uint64_t session_id = 0;
    std::uint32_t gateway_id = 0;
    std::string gateway_addr;
    std::int64_t bind_time_ms = 0;

    [[nodiscard]] bool is_bound() const noexcept {
        return session_id != 0;
    }
};

class PlayerAnchor {
public:
    explicit PlayerAnchor(std::uint64_t player_id);

    [[nodiscard]] std::uint64_t player_id() const noexcept;
    [[nodiscard]] AnchorState state() const noexcept;
    void set_state(AnchorState state) noexcept;

    [[nodiscard]] const SessionBinding& session_binding() const noexcept;
    void bind_session(const SessionBinding& binding);
    void unbind_session(std::uint64_t session_id);

    [[nodiscard]] const WorldAssignment& world_assignment() const noexcept;
    void assign_world(const WorldAssignment& assignment) noexcept;
    void clear_world_assignment() noexcept;

    // Home Zone 驻地（term-contract §1.1）：登录时分配，永不随场景切换迁移；
    // 0 表示未分配（登录流程落点：login-flow 分配后调用一次）
    [[nodiscard]] std::uint32_t home_zone_id() const noexcept;
    void set_home_zone_id(std::uint32_t home_zone_id) noexcept;

    // 脏数据出队（journal）钩子（P0-2，object-model O-1/O-4）：Anchor 变更时
    // 回调注入方（持久化职责在 Base 侧，P0-4 与 SaveQueue 挂钩；此处只保证
    // 「上报有变更」这一契约）。回调在 mark_dirty 的调用链上同步执行。
    // 返回 false 表示本次上报未被消费（调用方决定是否原样保留）。
    using JournalFn = std::function<bool(const PlayerAnchor& anchor, std::string_view reason)>;
    void set_journal(JournalFn journal);
    [[nodiscard]] const JournalFn& journal() const noexcept;

    void mark_dirty(std::string_view reason);
    [[nodiscard]] bool needs_save() const noexcept;
    void clear_dirty() noexcept;
    [[nodiscard]] const std::vector<std::string>& dirty_reasons() const noexcept;

private:
    std::uint64_t player_id_ = 0;
    std::uint32_t home_zone_id_ = 0;
    JournalFn journal_ = nullptr;
    AnchorState state_ = AnchorState::Loading;
    SessionBinding session_binding_{};
    WorldAssignment world_assignment_{};
    bool dirty_ = false;
    std::vector<std::string> dirty_reasons_;
};

} // namespace apollo::game::session
