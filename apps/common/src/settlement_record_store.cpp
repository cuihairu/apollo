#include "squad_instance/settlement_record_store.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>

namespace squad_instance {

namespace {

namespace fs = std::filesystem;

constexpr std::string_view kPrefix = "settlement_";
constexpr std::string_view kSuffix = ".rec";

const char* lifecycle_token(SettlementLifecycle lc) {
    switch (lc) {
        case SettlementLifecycle::Done: return "done";
        case SettlementLifecycle::Cancelled: return "cancelled";
        case SettlementLifecycle::Pending:
        default: return "pending";
    }
}

std::optional<SettlementLifecycle> parse_lifecycle(std::string_view token) {
    if (token == "pending") return SettlementLifecycle::Pending;
    if (token == "done") return SettlementLifecycle::Done;
    if (token == "cancelled") return SettlementLifecycle::Cancelled;
    return std::nullopt;
}

}  // namespace

SettlementRecordStore::SettlementRecordStore(std::string dir) : dir_(std::move(dir)) {}

std::string SettlementRecordStore::path_for(std::uint64_t settlement_id) const {
    return (fs::path(dir_) /
            (std::string(kPrefix) + std::to_string(settlement_id) + std::string(kSuffix)))
        .string();
}

bool SettlementRecordStore::write(const SettlementRecord& record) const {
    std::error_code ec;
    fs::create_directories(dir_, ec);
    if (ec) {
        return false;
    }
    const fs::path target = fs::path(path_for(record.settlement_id));
    const fs::path tmp = fs::path(dir_) /
        (std::string(kPrefix) + std::to_string(record.settlement_id) +
         std::string(kSuffix) + ".tmp");
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);  // binary：不落 CRLF
        if (!out) {
            return false;
        }
        out << "lifecycle " << lifecycle_token(record.lifecycle) << '\n';
        out << "party " << record.party_id << '\n';
        out << "world " << record.world_id << '\n';
        out << "instance " << record.instance_id << '\n';
        for (const auto member : record.members) {
            out << "member " << member << '\n';
        }
        for (const auto& [player, score] : record.intent) {
            out << "intent " << player << ' ' << score << '\n';
        }
        for (const auto& [player, score] : record.confirmed) {
            out << "confirm " << player << ' ' << score << '\n';
        }
        out.flush();
        if (!out) {
            std::error_code rme;
            fs::remove(tmp, rme);
            return false;
        }
    }
    // 覆盖 rename：Windows 对已存在目标必败——先删后替
    std::error_code ec2;
    fs::remove(target, ec2);
    fs::rename(tmp, target, ec2);
    if (ec2) {
        std::error_code rme;
        fs::remove(tmp, rme);
        return false;
    }
    return true;
}

std::optional<SettlementRecord> SettlementRecordStore::read(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    SettlementRecord record;
    // 结算 ID 由文件名派生（settlement_<id>.rec）——文件体不重复携带，
    // 与 SettlementCredentialStore 的 party 键同口径（读取侧按文件名归键）。
    const std::string name = fs::path(path).filename().string();
    if (name.size() > kPrefix.size() + kSuffix.size() &&
        name.compare(0, kPrefix.size(), std::string(kPrefix)) == 0 &&
        name.compare(name.size() - kSuffix.size(), kSuffix.size(),
                     std::string(kSuffix)) == 0) {
        const auto digits =
            name.substr(kPrefix.size(), name.size() - kPrefix.size() - kSuffix.size());
        try {
            record.settlement_id = std::stoull(digits);
        } catch (const std::exception&) {
            return std::nullopt;  // id 超范围 = 脏文件名
        }
    }
    bool has_lifecycle = false;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream probe(line);
        std::string key;
        probe >> key;
        if (key == "lifecycle") {
            std::string token;
            probe >> token;
            const auto lc = parse_lifecycle(token);
            if (!lc) {
                return std::nullopt;  // 未知生命周期 = 脏记录
            }
            record.lifecycle = *lc;
            has_lifecycle = true;
        } else if (key == "party") {
            probe >> record.party_id;
        } else if (key == "world") {
            probe >> record.world_id;
        } else if (key == "instance") {
            probe >> record.instance_id;
        } else if (key == "member") {
            std::uint64_t member = 0;
            if (probe >> member) {
                record.members.push_back(member);
            }
        } else if (key == "intent") {
            std::uint64_t player = 0;
            std::int64_t score = 0;
            if (probe >> player >> score) {
                record.intent[player] = score;
            }
        } else if (key == "confirm") {
            std::uint64_t player = 0;
            std::int64_t score = 0;
            if (probe >> player >> score) {
                record.confirmed[player] = score;
            }
        } else {
            return std::nullopt;  // 未知键 = 脏记录
        }
    }
    if (!has_lifecycle) {
        return std::nullopt;  // 缺生命周期头 = 脏记录
    }
    return record;
}

bool SettlementRecordStore::open(std::uint64_t settlement_id, std::uint64_t party_id,
                                 std::uint32_t world_id, std::uint64_t instance_id,
                                 std::vector<std::uint64_t> members) {
    const auto path = path_for(settlement_id);
    auto existing = read(path);
    if (existing) {
        // 幂等：同 id 已存在（重启装载后重入）——确认位/终局态原样保留。
        return true;
    }
    SettlementRecord record;
    record.settlement_id = settlement_id;
    record.party_id = party_id;
    record.world_id = world_id;
    record.instance_id = instance_id;
    record.members = std::move(members);
    record.lifecycle = SettlementLifecycle::Pending;
    return write(record);
}

bool SettlementRecordStore::confirm(std::uint64_t settlement_id, std::uint64_t player_id,
                                    std::int64_t score) {
    const auto path = path_for(settlement_id);
    auto record = read(path);
    if (!record) {
        return false;  // 无意图记录——确认无主（不应发生；调用面据此不撤账本标记）
    }
    record->confirmed[player_id] = score;
    return write(*record);
}

bool SettlementRecordStore::set_intent(std::uint64_t settlement_id,
                                       std::uint64_t player_id,
                                       std::int64_t score) {
    const auto path = path_for(settlement_id);
    auto record = read(path);
    if (!record) {
        return false;  // 无意图记录（open 未落/已终局回收）——调用面忽略
    }
    record->intent[player_id] = score;
    return write(*record);
}

bool SettlementRecordStore::complete(std::uint64_t settlement_id) {
    const auto path = path_for(settlement_id);
    auto record = read(path);
    if (!record) {
        return true;  // 记录已不在 = 已回收
    }
    if (record->lifecycle == SettlementLifecycle::Cancelled) {
        return true;  // 已取消——终局态不回退（done 不覆盖 cancelled）
    }
    // 只有全部所需持久确认完成才删除唯一记录：此处调用面须已确证全量确认
    // （本存储不代裁——账本 snapshot().complete 是权威）。写完 done 头即删。
    record->lifecycle = SettlementLifecycle::Done;
    if (!write(*record)) {
        return false;
    }
    std::error_code ec;
    fs::remove(path, ec);
    return !ec;
}

bool SettlementRecordStore::cancel(std::uint64_t settlement_id) {
    const auto path = path_for(settlement_id);
    auto record = read(path);
    if (!record) {
        return true;  // 记录已不在——无可取消（终局已回收）
    }
    if (record->lifecycle == SettlementLifecycle::Done) {
        return true;  // 已完成——取消不回退已发奖励
    }
    record->lifecycle = SettlementLifecycle::Cancelled;
    return write(*record);
}

std::vector<SettlementRecord> SettlementRecordStore::load_all() const {
    std::vector<SettlementRecord> out;
    std::error_code ec;
    fs::directory_iterator it(dir_, ec);
    if (ec) {
        return out;  // 目录不可读（未建/被占位）= 无记录
    }
    while (!ec && it != fs::directory_iterator()) {
        const std::string name = it->path().filename().string();
        if (name.size() > kPrefix.size() + kSuffix.size() &&
            name.compare(0, kPrefix.size(), std::string(kPrefix)) == 0 &&
            name.compare(name.size() - kSuffix.size(), kSuffix.size(),
                         std::string(kSuffix)) == 0) {
            const auto digits = name.substr(kPrefix.size(), name.size() - kPrefix.size() -
                                                                                    kSuffix.size());
            bool numeric = !digits.empty();
            for (const char c : digits) {
                if (c < '0' || c > '9') {
                    numeric = false;
                    break;
                }
            }
            if (numeric) {
                auto record = read(it->path().string());
                if (record) {
                    record->settlement_id = std::stoull(digits);
                    out.push_back(std::move(*record));
                }
            }
        }
        it.increment(ec);
    }
    return out;
}

std::vector<SettlementRepairEntry> SettlementRecordStore::pending_repair_entries() const {
    std::vector<SettlementRepairEntry> out;
    for (const auto& record : load_all()) {
        if (record.lifecycle != SettlementLifecycle::Pending) continue;
        SettlementRepairEntry entry;
        entry.settlement_id = record.settlement_id;
        entry.confirmed_count = record.confirmed.size();
        for (const auto member : record.members) {
            if (record.intent.count(member) == 0) {
                entry.awaiting.push_back(member);
            }
        }
        if (!entry.awaiting.empty()) {
            out.push_back(entry);
        }
    }
    return out;
}

bool SettlementRecordStore::adjudicate(std::uint64_t settlement_id,
                                      std::uint64_t player_id, std::int64_t score) {
    const auto path = path_for(settlement_id);
    auto record = read(path);
    if (!record) {
        return false;  // 记录不存在（未开账/已回收）——修复无主
    }
    if (record->lifecycle != SettlementLifecycle::Pending) {
        // 终局态不做裁决：done 已回收、cancelled 的已发部分走后续处理状态，
        // 裁决会复活失败取消的奖励发放。
        return false;
    }
    if (record->confirmed.count(player_id) > 0) {
        // 确认位是权威事实——人工裁决不覆写已发记录。
        return false;
    }
    const bool in_roster =
        std::find(record->members.begin(), record->members.end(), player_id) !=
        record->members.end();
    if (!in_roster) {
        return false;  // 名册外玩家——无结算资格
    }
    record->intent[player_id] = score;
    return write(*record);
}

}  // namespace squad_instance
