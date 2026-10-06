#pragma once

#include "apollo/game/session/world_assignment.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
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

// ItemStack：背包物品格（P2-4 长期态字段模型，object-model 差异清单 #1——
// 任务书六字段 Inventory/Equipment/Quest/Progress/Social/Guild 的 Inventory 支）
struct ItemStack {
    std::uint64_t item_id = 0;
    std::uint64_t count = 0;

    [[nodiscard]] bool operator==(const ItemStack&) const noexcept = default;
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

    // ---- 长期态字段模型（P2-4，object-model 差异清单 #1 消账）----
    // 任务书 §6 玩家一等对象字段面的契约落点：承载者 = PlayerAnchor（持久）
    // + Avatar（运行时）；四域变更一律 mark_dirty（长期态变更即脏，随
    // SaveQueue/write-behind 落档）。物品/任务 id 用裸 uint64（对齐本类风格；
    // 强分型随玩法批需求再加）。

    // Inventory：同 id 合并计数；扣除不足整体拒绝（无部分扣减）
    [[nodiscard]] const std::vector<ItemStack>& inventory() const noexcept;
    bool add_item(std::uint64_t item_id, std::uint64_t count);
    bool remove_item(std::uint64_t item_id, std::uint64_t count);

    // Equipment：固定槽位（骨架 6 位；槽位语义表随装备玩法批）；空槽 = 0
    static constexpr std::size_t kEquipSlotCount = 6;
    [[nodiscard]] std::uint64_t equipment_at(std::size_t slot) const noexcept;
    bool equip(std::size_t slot, std::uint64_t item_id);
    bool unequip(std::size_t slot);

    // Quest：quest_id → 进度值（骨架通用计数；任务定义面随任务玩法批）
    void set_quest_progress(std::uint64_t quest_id, std::uint64_t progress);
    [[nodiscard]] std::uint64_t quest_progress(std::uint64_t quest_id) const noexcept;
    [[nodiscard]] const std::unordered_map<std::uint64_t, std::uint64_t>& quests() const noexcept;

    // Progress：通用成长键值累计（exp/货币/声望等；battle 结算经
    // AnchorRewardSink 落「exp」键——奖励单向落 Anchor 的真实接线口）
    void add_progress(std::string_view key, std::int64_t delta);
    [[nodiscard]] std::int64_t progress(std::string_view key) const noexcept;
    [[nodiscard]] const std::unordered_map<std::string, std::int64_t>& progresses() const noexcept;

    // Social：所属公会（0 = 未入会；Guild 对象与管理在 social 模块，Anchor
    // 只记归属——「契约只承认 Avatar 的玩家面」修文后的社交长期态落点）
    [[nodiscard]] std::uint64_t guild_id() const noexcept;
    void set_guild_id(std::uint64_t guild_id);

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

    // 长期态四域 + 社交归属（P2-4）
    std::vector<ItemStack> inventory_;
    std::array<std::uint64_t, kEquipSlotCount> equipment_{};
    std::unordered_map<std::uint64_t, std::uint64_t> quests_;
    std::unordered_map<std::string, std::int64_t> progresses_;
    std::uint64_t guild_id_ = 0;

    bool dirty_ = false;
    std::vector<std::string> dirty_reasons_;
};

} // namespace apollo::game::session
