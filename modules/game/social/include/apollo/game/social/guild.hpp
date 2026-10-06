#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace apollo::game::social {

// Guild（公会——契约：社交面归 Anchor 长期态，player-object-model §1「跨场景
// 锚点、背包/成长等长期状态」的社交延伸；object-model §2.9 审计：代码此前
// 不存在，P2-4 骨架落地）。
//
// 所有权域：manager 域（跨 Zone 持久对象，随 improvement-plan P2-4「Guild/
// Party 对象（manager 域或 Zone 域）」——公会存活与会者会话无关，归持久域；
// 进程形态落点随 P3）。成员以裸 player_id 记账（对齐 PlayerAnchor 风格），
// 归属回写由调用方同步到 Anchor（anchor.set_guild_id）。
class Guild {
public:
    static constexpr std::size_t kMaxMembers = 100;  // 骨架容量（扩容随玩法批）

    Guild(std::uint64_t guild_id, std::string name, std::uint64_t leader_player_id);

    [[nodiscard]] std::uint64_t guild_id() const noexcept;
    [[nodiscard]] const std::string& name() const noexcept;
    [[nodiscard]] std::uint64_t leader_player_id() const noexcept;
    [[nodiscard]] const std::vector<std::uint64_t>& members() const noexcept;
    [[nodiscard]] std::size_t member_count() const noexcept;
    [[nodiscard]] bool has_member(std::uint64_t player_id) const noexcept;

    // 最小 CRUD（P2 出口判据④）：满员/重复加入拒绝；会长不可被移除
    // （退会须先 transfer_leader 或由 manager disband——单一裁决者不变式）
    bool add_member(std::uint64_t player_id);
    bool remove_member(std::uint64_t player_id);
    bool transfer_leader(std::uint64_t new_leader_player_id);  // 须为在册成员

private:
    std::uint64_t guild_id_ = 0;
    std::string name_;
    std::uint64_t leader_player_id_ = 0;
    std::vector<std::uint64_t> members_;  // 保序（入会顺序可观察）
    std::unordered_map<std::uint64_t, std::size_t> member_index_;
};

// GuildManager：guild_id/name 双唯一（创建口裁决）；骨架期进程内集合，
// 持久化落点随 P1-4 栈的 Anchor 落档延伸批。
class GuildManager {
public:
    // 重 id/重名拒绝（返回 nullptr 不产生副作用）
    Guild* create(std::uint64_t guild_id, std::string name, std::uint64_t leader_player_id);
    // 仅会长可解散；解散即销毁对象（成员归属清理由调用方回写 Anchor）
    bool disband(std::uint64_t guild_id, std::uint64_t requester_player_id);
    [[nodiscard]] Guild* find(std::uint64_t guild_id) const noexcept;
    [[nodiscard]] Guild* find_by_name(std::string_view name) const noexcept;
    [[nodiscard]] std::size_t guild_count() const noexcept;

private:
    std::vector<std::unique_ptr<Guild>> guilds_;
    std::unordered_map<std::uint64_t, std::size_t> by_id_;
    std::unordered_map<std::string, std::size_t> by_name_;
};

} // namespace apollo::game::social
