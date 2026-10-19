#include "apollo/game/orders/base_authority.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace apollo::game::orders {

BaseAuthorityRegistry::BaseAuthorityRegistry(std::string journal_path)
    : journal_path_(std::move(journal_path)) {}

bool BaseAuthorityRegistry::open() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (opened_) {
        return true;
    }
    holders_.clear();
    generation_.clear();
    if (!replay()) {
        return false;
    }
    out_.open(journal_path_, std::ios::app | std::ios::binary);
    opened_ = out_.good();
    return opened_;
}

AuthoritySnapshot BaseAuthorityRegistry::activate(std::uint64_t player_id,
                                                  const std::string& address,
                                                  std::int64_t now_ms) {
    if (player_id == 0 || address.empty()) {
        return {};  // 空身份/空地址不占代次、不写盘
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (!opened_) {
        return {};
    }
    const std::uint64_t generation = next_generation(player_id);
    AuthoritySnapshot snapshot{generation, address, now_ms};
    if (!append_line(render_line('A', player_id, generation, now_ms, address))) {
        return {};  // 写盘失败：代次未生效（下拍重新分配）
    }
    holders_[player_id] = snapshot;  // 后到顶替先到（并发登录）
    // 水位随激活推进（运行期与回放同口径；重启不回退代次）。
    generation_[player_id] = std::max(generation_[player_id], generation);
    return snapshot;
}

std::optional<BaseEndpoint> BaseAuthorityRegistry::resolve(
    std::uint64_t player_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = holders_.find(player_id);
    if (it == holders_.end()) {
        return std::nullopt;
    }
    return BaseEndpoint{it->second.address, it->second.generation};
}

bool BaseAuthorityRegistry::release(std::uint64_t player_id,
                                    std::uint64_t generation) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!opened_) {
        return false;
    }
    const auto it = holders_.find(player_id);
    if (it == holders_.end() || it->second.generation != generation || generation == 0) {
        return false;  // 被顶替旧 Base 迟到释放不动新归属
    }
    if (!append_line(render_line('R', player_id, generation, 0, ""))) {
        return false;  // 写盘失败：持有者不动，不冒充释放
    }
    holders_.erase(it);
    // 水位不降：重启后同代次不能复活为持权凭据。
    return true;
}

std::optional<AuthoritySnapshot> BaseAuthorityRegistry::holder(
    std::uint64_t player_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = holders_.find(player_id);
    return it == holders_.end() ? std::nullopt : std::optional(it->second);
}

std::size_t BaseAuthorityRegistry::holder_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return holders_.size();
}

std::uint64_t BaseAuthorityRegistry::next_generation(std::uint64_t player_id) const {
    const auto it = generation_.find(player_id);
    return it == generation_.end() ? 1 : it->second + 1;
}

std::string BaseAuthorityRegistry::render_line(char state, std::uint64_t player_id,
                                               std::uint64_t generation, std::int64_t ms,
                                               const std::string& address) const {
    std::string line;
    line += state;
    line += '|';
    line += std::to_string(player_id);
    line += '|';
    line += std::to_string(generation);
    line += '|';
    line += std::to_string(ms);
    line += '|';
    line += address;  // R 行为空串：格式仍 5 字段
    return line;
}

bool BaseAuthorityRegistry::append_line(const std::string& line) {
    if (!out_.is_open()) {
        out_.clear();
        out_.open(journal_path_, std::ios::app | std::ios::binary);
        if (!out_.good()) {
            return false;
        }
    }
    out_ << line << '\n';
    out_.flush();
    if (!out_.good()) {
        out_.close();  // 下拍 append 重开自愈
        return false;
    }
    return true;
}

bool BaseAuthorityRegistry::replay() {
    std::ifstream in(journal_path_, std::ios::binary);
    if (!in.good()) {
        std::ofstream probe(journal_path_, std::ios::app | std::ios::binary);
        return probe.good();
    }
    std::string line;
    std::uint64_t line_no = 0;
    auto corrupt = [&](const char* what) -> bool {
        throw std::runtime_error(std::string(what) + " at line " + std::to_string(line_no)
                                 + ": " + journal_path_);
    };
    while (std::getline(in, line)) {
        ++line_no;
        if (line.empty()) {
            continue;
        }
        // 切 5 字段（恰 4 个分隔符；address 为余量，禁含 '|'/'\n'）。
        std::vector<std::string> fields;
        fields.reserve(5);
        std::size_t begin = 0;
        for (int i = 0; i < 4; ++i) {
            const std::size_t sep = line.find('|', begin);
            if (sep == std::string::npos) {
                return corrupt("authority journal corrupt");
            }
            fields.emplace_back(line.substr(begin, sep - begin));
            begin = sep + 1;
        }
        fields.emplace_back(line.substr(begin));
        const char state = fields[0].size() == 1 ? fields[0][0] : '\0';
        if (state != 'A' && state != 'R') {
            return corrupt("authority journal corrupt");
        }
        const std::uint64_t player_id = std::strtoull(fields[1].c_str(), nullptr, 10);
        const std::uint64_t generation = std::strtoull(fields[2].c_str(), nullptr, 10);
        const std::int64_t ms = static_cast<std::int64_t>(std::strtoll(fields[3].c_str(), nullptr, 10));
        if (player_id == 0 || generation == 0) {
            return corrupt("authority journal corrupt");
        }
        if (state == 'A') {
            AuthoritySnapshot snapshot{generation, fields[4], ms};
            holders_[player_id] = snapshot;  // 同玩家后行覆盖前行（事件序）
        } else if (const auto it = holders_.find(player_id);
                   it != holders_.end() && it->second.generation == generation) {
            holders_.erase(it);  // 释放仅在代次仍为当前持有者时生效
        }
        generation_[player_id] = std::max(generation_[player_id], generation);
    }
    return true;
}

}  // namespace apollo::game::orders
