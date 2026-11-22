#include "squad_instance/admission_credential.hpp"

#include <algorithm>
#include <array>
#include <chrono>

namespace squad_instance {

namespace {

constexpr std::uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

const std::array<char, 16>& hex_digits() {
    static const std::array<char, 16> digits = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
    };
    return digits;
}

std::string to_hex(std::uint64_t value) {
    const auto& digits = hex_digits();
    std::string out(16, '\0');
    for (std::size_t i = 16; i-- > 0;) {
        out[i] = digits[value & 0xfULL];
        value >>= 4;
    }
    return out;
}

}  // namespace

std::string roster_hash_of(const std::vector<std::uint64_t>& members) {
    std::vector<std::uint64_t> sorted(members);
    std::sort(sorted.begin(), sorted.end());
    std::uint64_t hash = kFnvOffset;
    for (const auto member : sorted) {
        for (int shift = 0; shift < 8; ++shift) {
            hash ^= static_cast<std::uint8_t>((member >> (shift * 8)) & 0xffULL);
            hash *= kFnvPrime;
        }
    }
    return to_hex(hash);
}

AdmissionCredentialFace::AdmissionCredentialFace() {
    // 默认取挂钟毫秒（单调钟）；测试注入固定钟控期。
    // 注意：steady_clock 底层为纳秒，须显式换算成毫秒——直接取 count() 会把
    // ttl_ms 当成纳秒，受理即"已过期"。
    clock_ = []() -> std::uint64_t {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());
    };
}

void AdmissionCredentialFace::set_issuer(std::string issuer, std::uint64_t ttl_ms) {
    issuer_ = std::move(issuer);
    ttl_ms_ = ttl_ms;
}

void AdmissionCredentialFace::set_clock(std::function<std::uint64_t()> fn) {
    clock_ = std::move(fn);
}

std::vector<std::uint64_t> AdmissionCredentialFace::issue_for(
    std::uint64_t squad_id, std::uint64_t instance_id, std::uint64_t cell_id,
    const std::vector<std::uint64_t>& members) {
    const auto now = clock_ ? clock_() : 0;
    const auto roster_hash = roster_hash_of(members);
    std::vector<std::uint64_t> issued;
    issued.reserve(members.size());
    for (const auto player_id : members) {
        AdmissionCredential credential;
        credential.credential_id = next_credential_id_++;
        credential.player_id = player_id;
        credential.squad_id = squad_id;
        credential.target_instance = instance_id;
        credential.target_cell = cell_id;
        credential.roster_hash = roster_hash;
        credential.issued_at = now;
        credential.expires_at = ttl_ms_ != 0 ? now + ttl_ms_ : 0;
        credential.issuer = issuer_;
        player_credential_ids_[player_id].push_back(credential.credential_id);
        credentials_.emplace(credential.credential_id, std::move(credential));
        issued.push_back(credential.credential_id);
    }
    return issued;
}

std::uint64_t AdmissionCredentialFace::auto_pick(std::uint64_t player_id,
                                                  std::uint64_t instance_id) const {
    const auto it = player_credential_ids_.find(player_id);
    if (it == player_credential_ids_.end()) return 0;
    for (const auto credential_id : it->second) {
        const auto found = credentials_.find(credential_id);
        if (found == credentials_.end()) continue;
        if (found->second.target_instance != instance_id) continue;
        return credential_id;  // 含已核销：重放判 AlreadyConsumed
    }
    return 0;
}

std::pair<AdmissionReject, std::uint64_t> AdmissionCredentialFace::verify(
    std::uint64_t squad_id, std::uint64_t instance_id, std::uint64_t player_id,
    const std::vector<std::uint64_t>& current_roster, std::uint64_t presented) const {
    const auto credential_id = presented != 0 ? presented : auto_pick(player_id, instance_id);
    const auto it = credentials_.find(credential_id);
    if (it == credentials_.end())
        return {AdmissionReject::NoCredential, credential_id};
    const AdmissionCredential& credential = it->second;
    if (credential.consumed)
        return {AdmissionReject::AlreadyConsumed, credential_id};  // 重复使用
    // 身份/小队绑定：呈出的凭证必须属于该玩家的小队。
    if (credential.player_id != player_id || credential.squad_id != squad_id)
        return {AdmissionReject::NotTarget, credential_id};
    // 过期判定先于目标/名册：过期不消耗单次消费权。
    if (credential.expires_at != 0) {
        const auto now = clock_ ? clock_() : 0;
        if (now > credential.expires_at)
            return {AdmissionReject::Expired, credential_id};
    }
    if (credential.target_instance != instance_id)
        return {AdmissionReject::NotTarget, credential_id};  // 错实例
    // 资格变更：签发时快照与准入时点名册不符。
    if (credential.roster_hash != roster_hash_of(current_roster))
        return {AdmissionReject::RosterChanged, credential_id};
    return {AdmissionReject::Ok, credential_id};
}

void AdmissionCredentialFace::consume(std::uint64_t credential_id) {
    const auto it = credentials_.find(credential_id);
    if (it != credentials_.end()) it->second.consumed = true;
}

void AdmissionCredentialFace::rearm(std::uint64_t player_id, std::uint64_t instance_id) {
    for (const auto credential_id : player_credential_ids_[player_id]) {
        const auto it = credentials_.find(credential_id);
        if (it == credentials_.end()) continue;
        if (it->second.target_instance != instance_id) continue;
        it->second.consumed = false;
    }
}

const AdmissionCredential* AdmissionCredentialFace::find(std::uint64_t credential_id) const
    noexcept {
    const auto it = credentials_.find(credential_id);
    return it == credentials_.end() ? nullptr : &it->second;
}

std::size_t AdmissionCredentialFace::drop_instance(std::uint64_t instance_id) {
    std::vector<std::uint64_t> dropped;
    dropped.reserve(credentials_.size());
    for (const auto& [credential_id, credential] : credentials_) {
        if (credential.target_instance == instance_id) dropped.push_back(credential_id);
    }
    for (const auto credential_id : dropped) {
        credentials_.erase(credential_id);
        for (auto& entry : player_credential_ids_) {
            auto& ids = entry.second;
            ids.erase(std::remove_if(ids.begin(), ids.end(),
                                     [credential_id](std::uint64_t id) {
                                         return id == credential_id;
                                     }),
                      ids.end());
        }
    }
    return dropped.size();
}

void AdmissionCredentialFace::clear() noexcept {
    credentials_.clear();
    player_credential_ids_.clear();
}

}  // namespace squad_instance
