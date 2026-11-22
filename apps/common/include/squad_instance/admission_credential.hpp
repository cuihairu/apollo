#pragma once

// 副本入场准入凭证（D11「小队资格与有期限准入凭证」）：受理副本时按受理
// 快照逐成员签发有期限、单次消费的入场凭证；cell 准入时校验四负例——
// 过期 / 错实例 / 重复使用 / 资格变更。
//
// 与 SettlementCredentialStore 区分：结算凭据是"落账去重"面（持久、按小队），
// 本凭证是"入场准入"面（进程内单次消费、按成员、副本终态回收）。
//
// 字段口径（docs/review/2026-10-30-actions/04-login-and-routing.md §4.1）：
// credential_id / player_id / squad_id / target_instance / target_cell /
// roster_hash / issued_at / expires_at / issuer。
//
// 签发方：本切片为受理面自签（cell 宿主 SquadInstanceService）；Base 经
// World 预留容量后签发（设计口径 issuer = Base）随后续链路批次切换。
//
// 线程契约：与宿主服务一致（受理/准入同宿主玩法线程），无自持锁。

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace squad_instance {

// 准入拒绝码（四负例 + 未持凭证）。0 = 通过。
enum class AdmissionReject : std::uint32_t {
    Ok = 0,
    NoCredential,     // 未持凭证 / 凭证号不存在
    Expired,          // 已过期（expires_at 已过）
    NotTarget,        // 错实例（目标副本不匹配）
    AlreadyConsumed,  // 重复使用（单次消费已核销）
    RosterChanged,    // 资格变更（roster_hash 与当前名册不符）
};

struct AdmissionCredential {
    std::uint64_t credential_id = 0;     // 凭证唯一 ID（单次消费）
    std::uint64_t player_id = 0;         // 绑定玩家
    std::uint64_t squad_id = 0;          // 所属小队
    std::uint64_t target_instance = 0;   // 目标副本（用途绑定）
    std::uint64_t target_cell = 0;       // 目标执行端（embedded 下为 World id）
    std::string roster_hash;             // 成员/资格快照摘要
    std::uint64_t issued_at = 0;         // 签发时刻（ms）
    std::uint64_t expires_at = 0;        // 有效期止（0 = 无期限）
    std::string issuer;                  // 签发方
    bool consumed = false;               // 已核销（单次消费）
};

// 名册摘要：受理快照成员的可复算指纹（签发侧与准入侧同算法）。成员作集合
// 处理（排序后 FNV-1a 64），与入队次序无关——名册变更即摘要变化。
[[nodiscard]] std::string roster_hash_of(const std::vector<std::uint64_t>& members);

// 单进程内凭证表（受理签发 + 准入核销）。宿主（SquadInstanceService）
// 持有并驱动；签发随 launch 受理收口，核销随 enter 准入，回收随副本终态。
class AdmissionCredentialFace {
public:
    AdmissionCredentialFace();

    // 签发方身份与有效期（ms）。ttl_ms = 0 表不限制（expires_at = 0）。
    void set_issuer(std::string issuer, std::uint64_t ttl_ms);
    void set_clock(std::function<std::uint64_t()> fn);

    // 受理副本时按受理快照逐成员签发；返回凭证号列表（与 members 同序）。
    std::vector<std::uint64_t> issue_for(std::uint64_t squad_id,
                                         std::uint64_t instance_id,
                                         std::uint64_t cell_id,
                                         const std::vector<std::uint64_t>& members);

    // 准入校验（不核销）：身份/期限/目标/单次消费/名册五关。presented = 0
    // 表"以玩家身份自动取该副本凭证"；非 0 表显式呈出（错实例/已消费负例
    // 直测）。current_roster = 准入时点名册（资格变更判据）。返回拒绝码与
    // 命中凭证号（未命中 = 0）；核销须待副本进入成功后由 consume 完成——
    // 校验过但进入失败不烧凭证。
    std::pair<AdmissionReject, std::uint64_t> verify(
        std::uint64_t squad_id, std::uint64_t instance_id, std::uint64_t player_id,
        const std::vector<std::uint64_t>& current_roster,
        std::uint64_t presented = 0) const;

    // 单次消费核销（重复使用判据）：verify 通过且副本进入成功后调用。
    void consume(std::uint64_t credential_id);

    // 重臂（成员退出回程窗）：解除该成员该副本凭证的消费标记——退出后重进
    // 走 enter 准入窗口的既有语义不变；凭证期内重放（未退出再进）仍拒。
    void rearm(std::uint64_t player_id, std::uint64_t instance_id);

    [[nodiscard]] std::size_t size() const noexcept { return credentials_.size(); }
    [[nodiscard]] const AdmissionCredential* find(
        std::uint64_t credential_id) const noexcept;
    // 副本终态回收（release/dismiss）：清空该副本全部凭证，返回回收数。
    std::size_t drop_instance(std::uint64_t instance_id);
    void clear() noexcept;

private:
    // 玩家自动取凭证：同 (玩家, 目标副本) 下首个凭证号（含已核销——重放
    // 判 AlreadyConsumed 而非 NoCredential）；无则 0。
    [[nodiscard]] std::uint64_t auto_pick(std::uint64_t player_id,
                                          std::uint64_t instance_id) const;

    std::unordered_map<std::uint64_t, AdmissionCredential> credentials_;  // id → 凭证
    std::unordered_map<std::uint64_t, std::vector<std::uint64_t>>
        player_credential_ids_;  // player_id → 凭证号（含已核销，便于负例回溯）
    std::function<std::uint64_t()> clock_;
    std::string issuer_;
    std::uint64_t ttl_ms_ = 0;
    std::uint64_t next_credential_id_ = 1;
};

}  // namespace squad_instance
