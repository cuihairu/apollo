#include "base/squad_validation_service.hpp"

#include "squad_instance/roster_hash.hpp"

#include <chrono>

namespace base {

namespace {

constexpr std::int32_t kCodeAllEligible = 0;
constexpr std::int32_t kCodeMemberMissing = 1;
constexpr std::int32_t kCodeMemberFrozen = 2;
constexpr std::int32_t kCodeBadRoster = 3;

// 名册快照解析（逗号分隔 player_id）：空串/空段/非数字 → 非法（nullopt）。
bool parse_members(const std::string& text,
                   std::vector<std::uint64_t>& out) {
    out.clear();
    if (text.empty()) {
        return false;
    }
    std::size_t i = 0;
    while (i <= text.size()) {
        const std::size_t comma = text.find(',', i);
        const std::size_t end = (comma == std::string::npos) ? text.size() : comma;
        if (end == i) {
            return false;  // 空段（",100" / "100," / "100,,200"）
        }
        std::uint64_t value = 0;
        for (std::size_t j = i; j < end; ++j) {
            const char c = text[j];
            if (c < '0' || c > '9') {
                return false;
            }
            value = value * 10 + static_cast<std::uint64_t>(c - '0');
        }
        out.push_back(value);
        if (comma == std::string::npos) {
            break;
        }
        i = comma + 1;
    }
    return !out.empty();
}

// 拒因成员清单（player_id 逗号分隔，契约 973.failed_members 口径）。
std::string join_ids(const std::vector<std::uint64_t>& ids) {
    std::string out;
    for (const auto id : ids) {
        if (!out.empty()) {
            out.push_back(',');
        }
        out += std::to_string(id);
    }
    return out;
}

}  // namespace

SquadValidationService::SquadValidationService(
    WorldSquadClient& link, const SquadEligibilityView& view,
    SquadValidationServiceConfig config,
    std::function<std::uint64_t()> clock)
    : link_(link), view_(view), config_(config) {
    if (clock) {
        clock_ = std::move(clock);
    } else {
        clock_ = []() -> std::uint64_t {
            return static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count());
        };
    }
    link_.set_validation_handler(
        [this](const SquadValidationRequest& request) {
            on_validation(request);
        });
    link_.set_grant_handler(
        [this](const SquadCapacityGrant& grant) { on_grant(grant); });
}

SquadValidationCounters SquadValidationService::counters() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return counters_;
}

std::size_t SquadValidationService::pending_capacity() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return pending_.size();
}

void SquadValidationService::bump(std::uint64_t SquadValidationCounters::*field) {
    std::lock_guard<std::mutex> guard(mutex_);
    ++(counters_.*field);
}

void SquadValidationService::on_validation(
    const SquadValidationRequest& request) {
    bump(&SquadValidationCounters::validations_in);

    std::vector<std::uint64_t> members;
    if (!parse_members(request.members, members)) {
        bump(&SquadValidationCounters::bad_rosters);
        if (!link_.send_validation_reply(request.request_id, false, "", "",
                                         kCodeBadRoster)) {
            bump(&SquadValidationCounters::send_failures);
            return;
        }
        bump(&SquadValidationCounters::rejections_out);
        return;
    }

    // 逐成员盖资格章（档案存在 + 未冻结）：缺档与冻结并存报缺档码，
    // failed_members 带全部拒因成员。
    std::vector<std::uint64_t> missing;
    std::vector<std::uint64_t> frozen;
    for (const auto member : members) {
        if (!view_.member_exists(member)) {
            missing.push_back(member);
        } else if (view_.member_frozen(member)) {
            frozen.push_back(member);
        }
    }

    if (!missing.empty() || !frozen.empty()) {
        const std::int32_t code =
            !missing.empty() ? kCodeMemberMissing : kCodeMemberFrozen;
        if (!link_.send_validation_reply(request.request_id, false, "",
                                         join_ids(missing.empty() ? frozen
                                                                  : missing),
                                         code)) {
            bump(&SquadValidationCounters::send_failures);
            return;
        }
        bump(&SquadValidationCounters::rejections_out);
        return;
    }

    // 全员合格：序无关 FNV-1a 盖章（与 cell 侧 enter 核验同算法同口径），
    // 回 973 后即发 974 容量预留申请（槽数=成员数）。
    const std::string roster_hash =
        squad_instance::roster_hash_of(members);
    if (!link_.send_validation_reply(request.request_id, true, roster_hash,
                                     "", kCodeAllEligible)) {
        bump(&SquadValidationCounters::send_failures);
        return;
    }
    bump(&SquadValidationCounters::accepted_out);

    const auto capacity_request_id = next_request_id_++;
    if (!link_.send_capacity_request(capacity_request_id, request.party_id,
                                     request.instance_id, request.cell_id,
                                     roster_hash,
                                     static_cast<std::int32_t>(members.size()))) {
        bump(&SquadValidationCounters::send_failures);
        return;  // 未发即无在册申请（975 不会来，哑管道不重试）
    }
    bump(&SquadValidationCounters::capacity_requests_out);

    PendingCapacity pending;
    pending.party_id = request.party_id;
    pending.instance_id = request.instance_id;
    pending.cell_id = request.cell_id;
    pending.roster_hash = roster_hash;
    pending.members = members;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        pending_.emplace(capacity_request_id, std::move(pending));
    }
}

void SquadValidationService::on_grant(const SquadCapacityGrant& grant) {
    bump(&SquadValidationCounters::grants_in);

    if (!grant.granted) {
        bump(&SquadValidationCounters::grants_rejected);
        std::lock_guard<std::mutex> guard(mutex_);
        pending_.erase(grant.request_id);  // 拒绝即收册（整批不放凭证）
        return;
    }

    // 签发时预留窗已过 = 按拒绝处理（04 §4.3 预留生命周期，不放凭证；
    // expires_at=0 = 不设窗）。
    const std::uint64_t now = clock_();
    if (grant.expires_at != 0 && now >= grant.expires_at) {
        bump(&SquadValidationCounters::grants_expired);
        std::lock_guard<std::mutex> guard(mutex_);
        pending_.erase(grant.request_id);
        return;
    }

    PendingCapacity pending;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        const auto it = pending_.find(grant.request_id);
        if (it == pending_.end()) {
            ++counters_.grants_unknown;
            return;
        }
        pending = it->second;
        pending_.erase(it);  // 一次性裁决：重放同号不再签发
    }

    // 逐成员签发 976（967 每条目一帧）：expires_at 带预留窗原值（凭证
    // 有效期 = 预留窗，enter 消费须在窗内），issuer=base:<id>。
    const std::string issuer =
        "base:" + std::to_string(config_.base_id);
    for (const auto member : pending.members) {
        const auto credential_id = next_credential_id_++;
        const auto issued_at = clock_();
        if (!link_.send_credential_bundle(
                grant.request_id, grant.reservation_id, credential_id, member,
                pending.party_id, pending.instance_id, pending.cell_id,
                pending.roster_hash, issued_at, grant.expires_at, issuer)) {
            bump(&SquadValidationCounters::send_failures);
            return;  // 链路断：余员不签发（哑管道不重试，计数显形）
        }
        bump(&SquadValidationCounters::credentials_out);
    }
}

}  // namespace base
