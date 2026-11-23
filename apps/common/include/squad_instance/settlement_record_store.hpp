#pragma once

// 结算恢复记录（D13 未持久确认保留唯一结算与恢复记录）：以稳定玩法/结算
// ID（L132 幂等键，= Instance ID）为键的唯一记录，携带生命周期与逐玩家
// 确认位——结算意图的持久面，独立于去重凭据（SettlementCredentialStore，
// 只记"已发"证明，本记录记"应发什么/发到哪一步/终局态"）。
//
// 生命周期（D13 §2.1）：pending → done（全部所需持久确认完成才删唯一记录）
// / cancelled（失败取消不发成功奖励，已发部分进后续处理状态——记录保留，
// 不静默删）。
//
// 「应发什么」的持久面：`intent` 记逐成员**已定分值**（结算段已裁决奖励，
// 与业务面是否受理无关）——重启后未确认成员的分值不随之丢失，恢复扫描才能
// 重驱动（重发 958 同结算 ID，Base 去重环兜底不双记）。`confirmed` 是 intent
// 的确认子集（intent ⊇ confirmed）。分值未裁决的成员无 intent——奖励未定，
// 属人工修复面，不自动驱动。
//
// 目录 = World 数据域下 settlement_records/；每份 settlement_<id>.rec，
// 行式 "<key> <value...>"（人类可查、解析零依赖）；写 = tmp + rename 原子
// 替换（Windows 覆盖 rename 对已存在目标必败，先删后替）。并发口径与账本
// 一致（单线程落账域，串行化随后续条目）。

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace squad_instance {

enum class SettlementLifecycle : std::uint32_t {
    Pending = 0,
    Done = 1,
    Cancelled = 2,
};

struct SettlementRecord {
    std::uint64_t settlement_id = 0;
    std::uint64_t party_id = 0;
    std::uint32_t world_id = 0;
    std::uint64_t instance_id = 0;
    SettlementLifecycle lifecycle = SettlementLifecycle::Pending;
    std::vector<std::uint64_t> members;  // 受理名册（结算意图）
    // 已定分值的玩家 → 分值（应发什么）：结算段裁决即记，与业务面是否受理
    // 无关——重启后未确认成员重驱动（自动重发）的分值来源。intent ⊇ confirmed。
    std::unordered_map<std::uint64_t, std::int64_t> intent;
    // 已持久确认玩家 → 分值与结算名册对齐（幂等回执，不双记）。
    std::unordered_map<std::uint64_t, std::int64_t> confirmed;
};

class SettlementRecordStore {
public:
    explicit SettlementRecordStore(std::string dir);

    // 建立/续记唯一记录（pending 幂等：同 id 已存在则原样保留——重启装载
    // 后重入不覆写已确认位）。false = 未持久提交（目录不可造/写失败）。
    bool open(std::uint64_t settlement_id, std::uint64_t party_id,
              std::uint32_t world_id, std::uint64_t instance_id,
              std::vector<std::uint64_t> members);

    // 逐玩家持久确认（幂等：同玩家重复确认同值无副作用）。
    // false = 未持久提交——调用面据此不撤账本已落标记（账本为权威）。
    bool confirm(std::uint64_t settlement_id, std::uint64_t player_id,
                 std::int64_t score);

    // 逐玩家记已定分值（应发什么；D13 pending 自动重驱动）：结算段裁决
    // 即记，与业务面是否受理无关（幂等：同玩家重复同值无副作用）。重启后
    // 恢复扫描据此重驱动未确认成员（intent 已知、confirmed 缺位 = 重发）。
    // false = 无此记录（open 未落/已终局）——调用面忽略。
    bool set_intent(std::uint64_t settlement_id, std::uint64_t player_id,
                    std::int64_t score);

    // 全部所需持久确认完成 → done 并删除唯一记录（D13 §2.1：只有全部所需
    // 持久确认完成才删除唯一记录）。无此记录亦返回 true（已不在）。
    bool complete(std::uint64_t settlement_id);

    // 失败取消：置 cancelled（不发成功奖励；已发部分随记录保留供后续处理
    // 状态——不静默删记录）。无此记录亦返回 true。
    bool cancel(std::uint64_t settlement_id);

    // 全量装载（重启恢复扫描面）：脏行/未知扩展名跳过，可解析保留。
    [[nodiscard]] std::vector<SettlementRecord> load_all() const;

private:
    [[nodiscard]] std::string path_for(std::uint64_t settlement_id) const;
    [[nodiscard]] bool write(const SettlementRecord& record) const;
    [[nodiscard]] static std::optional<SettlementRecord> read(
        const std::string& path);

    std::string dir_;
};

}  // namespace squad_instance
