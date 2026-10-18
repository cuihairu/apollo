#include "squad_instance/settlement_credential_store.hpp"

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
constexpr std::string_view kSuffix = ".cred";

// 单份凭据读取：段数不对/非数值的脏行跳过，可解析行保留。
std::unordered_map<std::uint64_t, std::int64_t> read_marks(const fs::path& path) {
    std::unordered_map<std::uint64_t, std::int64_t> marks;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return marks;
    }
    std::string line;
    while (std::getline(in, line)) {
        std::uint64_t player = 0;
        std::int64_t score = 0;
        std::int64_t trailing = 0;
        std::istringstream probe(line);
        probe >> player;
        probe >> score;
        if (probe.fail() || (probe >> trailing)) {
            continue;  // 少段/多段 = 脏行
        }
        marks[player] = score;
    }
    return marks;
}

}  // namespace

SettlementCredentialStore::SettlementCredentialStore(std::string dir) : dir_(std::move(dir)) {}

std::string SettlementCredentialStore::path_for(std::uint64_t party_id) const {
    return (fs::path(dir_) /
            (std::string(kPrefix) + std::to_string(party_id) + std::string(kSuffix)))
        .string();
}

bool SettlementCredentialStore::write_marks(
    std::uint64_t party_id,
    const std::unordered_map<std::uint64_t, std::int64_t>& marks) const {
    std::error_code ec;
    fs::create_directories(dir_, ec);
    if (ec) {
        return false;
    }
    const fs::path target = fs::path(path_for(party_id));
    const fs::path tmp = fs::path(dir_) /
        (std::string(kPrefix) + std::to_string(party_id) + std::string(kSuffix) + ".tmp");
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);  // binary：不落 CRLF
        if (!out) {
            return false;
        }
        for (const auto& [player, score] : marks) {
            out << player << ' ' << score << '\n';
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

bool SettlementCredentialStore::record_applied(std::uint64_t party_id,
                                               std::uint64_t player_id, std::int64_t score) {
    auto marks = read_marks(fs::path(path_for(party_id)));
    marks[player_id] = score;
    return write_marks(party_id, marks);
}

bool SettlementCredentialStore::erase(std::uint64_t party_id) {
    std::error_code ec;
    fs::remove(path_for(party_id), ec);
    return !ec;  // 凭据本不在亦视为已回收
}

SettlementCredentialStore::Marks SettlementCredentialStore::load_all() const {
    Marks out;
    std::error_code ec;
    fs::directory_iterator it(dir_, ec);
    if (ec) {
        return out;  // 目录不可读（未建/被占位）= 无凭据
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
                try {
                    const auto marks = read_marks(it->path());
                    if (!marks.empty()) {
                        out[std::stoull(digits)] = std::move(marks);
                    }
                } catch (const std::exception&) {
                    continue;  // id 超范围 = 脏文件名
                }
            }
        }
        it.increment(ec);
    }
    return out;
}

}  // namespace squad_instance
