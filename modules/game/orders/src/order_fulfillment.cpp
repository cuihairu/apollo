#include "apollo/game/orders/order_fulfillment.hpp"

#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace apollo::game::orders {

OrderFulfillment::OrderFulfillment(OrderStore& store,
                                   std::string credential_journal_path,
                                   BaseResolver authority_view)
    : store_(store),
      journal_path_(std::move(credential_journal_path)),
      authority_view_(std::move(authority_view)) {}

bool OrderFulfillment::open(RewardApply apply) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (opened_) {
        return true;
    }
    credentials_.clear();
    applied_.clear();
    replay_apply_failures_ = 0;
    apply_ = std::move(apply);
    if (!replay(apply_)) {
        return false;
    }
    out_.open(journal_path_, std::ios::app | std::ios::binary);
    opened_ = out_.good();
    return opened_;
}

OrderFulfillment::Result OrderFulfillment::fulfill(std::uint64_t order_id,
                                                   const BaseEndpoint& via,
                                                   std::int64_t now_ms) {
    // 校验①：受理账（未受理单号不发货）。
    const OrderRecord* found = store_.find(order_id);
    if (found == nullptr) {
        return {false, false, true, "unknown order"};
    }
    const OrderRecord record = *found;  // 拷贝：不持 OrderStore 锁跨本类临界区
    // 校验②：载荷行格式双保险。
    if (record.player_id == 0 || record.payload.empty() ||
        record.payload.find('\n') != std::string::npos ||
        record.payload.find('|') != std::string::npos) {
        return {false, false, true, "invalid order payload"};
    }
    // 校验③：Base 唯一写权（订单按稳定玩家身份查当前归属，同址同代次才可履约）。
    std::optional<BaseEndpoint> current;
    if (authority_view_) {
        current = authority_view_(record.player_id);
    }
    if (!current) {
        return {false, false, true, "no write authority"};
    }
    if (current->generation != via.generation || current->address != via.address) {
        return {false, false, true, "stale write authority"};
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (!opened_) {
        return {false, false, true, "not opened"};
    }
    // 凭据幂等：命中即短路（通知重复/回执丢失/重投不二次发奖）。
    const auto it = credentials_.find(order_id);
    if (it != credentials_.end()) {
        if (applied_.count(order_id) != 0) {
            return {true, true, false, ""};
        }
        // 本进程内记账但 apply 未落账（此前抛异常）：补放一次。
        if (!apply_) {
            return {false, false, true, "no reward executor"};
        }
        try {
            apply_(to_record(it->second));
            applied_.insert(order_id);
        } catch (...) {
            return {false, false, true, "apply failed"};
        }
        return {true, true, false, ""};
    }
    // 原子提交：先 append 凭据（write-ahead），后 apply（崩溃窗由 open 回放补）。
    if (!apply_) {
        return {false, false, true, "no reward executor"};
    }
    CredentialRecord credential;
    credential.order_id = record.order_id;
    credential.player_id = record.player_id;
    credential.base_generation = via.generation;
    credential.fulfilled_ms = now_ms;
    credential.payload = record.payload;
    if (!append_line(render_line(credential))) {
        return {false, false, true, "credential write failed"};  // 不冒充发货
    }
    credentials_.emplace(order_id, credential);
    try {
        apply_(record);
        applied_.insert(order_id);
    } catch (...) {
        return {false, false, true, "apply failed"};  // 凭据保留，重投补放
    }
    return {true, false, false, ""};
}

bool OrderFulfillment::has_credential(std::uint64_t order_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return credentials_.count(order_id) != 0;
}

std::size_t OrderFulfillment::credential_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return credentials_.size();
}

std::size_t OrderFulfillment::replay_apply_failures() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return replay_apply_failures_;
}

OrderRecord OrderFulfillment::to_record(const CredentialRecord& credential) {
    // 凭据账自带最小重放载荷：受理序/受理时刻在 OrderStore 账，此处不伪造。
    OrderRecord record;
    record.order_id = credential.order_id;
    record.player_id = credential.player_id;
    record.payload = credential.payload;
    record.delivered_ms = credential.fulfilled_ms;
    record.delivered_base_generation = credential.base_generation;
    return record;
}

std::string OrderFulfillment::render_line(const CredentialRecord& credential) const {
    std::string line = std::to_string(credential.order_id);
    line += '|';
    line += std::to_string(credential.player_id);
    line += '|';
    line += std::to_string(credential.base_generation);
    line += '|';
    line += std::to_string(credential.fulfilled_ms);
    line += '|';
    line += credential.payload;  // accept 已保证无 '|'/'\n'
    return line;
}

bool OrderFulfillment::append_line(const std::string& line) {
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

bool OrderFulfillment::replay(RewardApply& apply) {
    std::ifstream in(journal_path_, std::ios::binary);
    if (!in.good()) {
        std::ofstream probe(journal_path_, std::ios::app | std::ios::binary);
        return probe.good();
    }
    std::string line;
    std::uint64_t line_no = 0;
    while (std::getline(in, line)) {
        ++line_no;
        if (line.empty()) {
            continue;
        }
        // 切 5 字段（恰 4 个分隔符；payload 为余量）。
        std::vector<std::string> fields;
        fields.reserve(5);
        std::size_t begin = 0;
        for (int i = 0; i < 4; ++i) {
            const std::size_t sep = line.find('|', begin);
            if (sep == std::string::npos) {
                throw std::runtime_error("credential journal corrupt at line "
                                         + std::to_string(line_no) + ": " + journal_path_);
            }
            fields.emplace_back(line.substr(begin, sep - begin));
            begin = sep + 1;
        }
        fields.emplace_back(line.substr(begin));
        CredentialRecord credential;
        credential.order_id = std::strtoull(fields[0].c_str(), nullptr, 10);
        credential.player_id = std::strtoull(fields[1].c_str(), nullptr, 10);
        credential.base_generation = std::strtoull(fields[2].c_str(), nullptr, 10);
        credential.fulfilled_ms =
            static_cast<std::int64_t>(std::strtoll(fields[3].c_str(), nullptr, 10));
        credential.payload = fields[4];
        if (credential.order_id == 0 || credential.player_id == 0 ||
            credential.base_generation == 0) {
            throw std::runtime_error("credential journal corrupt at line "
                                     + std::to_string(line_no) + ": " + journal_path_);
        }
        const auto [it, inserted] = credentials_.try_emplace(credential.order_id, credential);
        if (!inserted) {
            continue;  // 同单后行忽略：凭据唯一（首记成立）
        }
        // 重放 apply：补崩溃窗（已记账未落账）。apply 可重入（幂等锚 = 凭据）。
        if (apply) {
            try {
                apply(to_record(credential));
                applied_.insert(credential.order_id);
            } catch (...) {
                ++replay_apply_failures_;  // 凭据仍在：下次 fulfill 命中补放
            }
        }
    }
    return true;
}

}  // namespace apollo::game::orders
