#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace apollo::game::social {

// Party（队伍——契约：社交面归 Anchor 长期态的**易失**支：队伍存活与会话
// 绑定，不跨会话持久；与 Guild 的持久域区分）。所有权域：Zone 域（组队是
// 场景玩法前置，进程形态落点随 P3）。
//
// 队长语义：首位成员即队长；队长退队/kick 自身时移交最早成员（群龙无首
// 不允许）；last member leave 即队伍自然消亡（manager 侧回收）。
class Party {
public:
    static constexpr std::size_t kMaxMembers = 5;  // 骨架容量（扩容随玩法批）

    explicit Party(std::uint64_t party_id);

    [[nodiscard]] std::uint64_t party_id() const noexcept;
    [[nodiscard]] std::uint64_t leader_player_id() const noexcept;
    [[nodiscard]] const std::vector<std::uint64_t>& members() const noexcept;
    [[nodiscard]] std::size_t member_count() const noexcept;
    [[nodiscard]] bool has_member(std::uint64_t player_id) const noexcept;
    [[nodiscard]] bool empty() const noexcept;

    bool add_member(std::uint64_t player_id);  // 满员/重复拒绝；首位即队长
    bool leave(std::uint64_t player_id);       // 队长退队自动移交最早成员
    bool kick(std::uint64_t target_player_id, std::uint64_t requester_player_id);
    bool transfer_leader(std::uint64_t new_leader_player_id);  // 须为在册成员

private:
    std::uint64_t party_id_ = 0;
    std::uint64_t leader_player_id_ = 0;
    std::vector<std::uint64_t> members_;  // 保序（入队顺序 = 移交次序）
    std::unordered_map<std::uint64_t, std::size_t> member_index_;
};

// PartyManager：party_id 唯一；空队（全员退队）即时回收。
class PartyManager {
public:
    Party* create(std::uint64_t party_id, std::uint64_t leader_player_id);
    // 仅队长可解散；队伍不存在/非队长拒绝
    bool disband(std::uint64_t party_id, std::uint64_t requester_player_id);
    Party* find(std::uint64_t party_id) const noexcept;
    std::size_t party_count() const noexcept;

private:
    std::vector<std::unique_ptr<Party>> parties_;
    std::unordered_map<std::uint64_t, std::size_t> by_id_;
};

} // namespace apollo::game::social
