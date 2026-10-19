#include "apollo/game/orders/order_store.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace apollo::game::orders {

OrderStore::OrderStore(std::string journal_path)
    : journal_path_(std::move(journal_path)) {}

bool OrderStore::open() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (opened_) {
        return true;
    }
    orders_.clear();
    next_receive_seq_ = 1;
    if (!replay()) {
        return false;
    }
    // 回放重建后快照压薄（每单一行当前态）；失败不阻断 open——旧事件序
    // 仍完整，下个 open 再试压薄。
    compact_snapshot();
    out_.open(journal_path_, std::ios::app | std::ios::binary);
    opened_ = out_.good();
    return opened_;
}

OrderStore::AcceptResult OrderStore::accept(std::uint64_t order_id,
                                            std::uint64_t player_id,
                                            const std::string& payload,
                                            std::int64_t now_ms) {
    // 行格式/身份面拒绝先于锁内查找（无副作用路径不需要互斥）。
    if (order_id == 0 || player_id == 0 ||
        payload.find('\n') != std::string::npos ||
        payload.find('|') != std::string::npos) {
        return {false, false};
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (!opened_) {
        return {false, false};
    }
    if (orders_.count(order_id) != 0) {
        return {false, true};  // 渠道重发幂等命中：不重建、不重置投递进度
    }
    OrderRecord record;
    record.order_id = order_id;
    record.player_id = player_id;
    record.payload = payload;
    record.received_ms = now_ms;
    record.receive_seq = next_receive_seq_++;
    // write-ahead：先落盘后入索引——append 失败即未受理（调用方回执失败，
    // 渠道侧重发补齐；不存在「已回执但无持久记录」的窗口）。
    if (!append_line(render_line(record))) {
        return {false, false};
    }
    orders_.emplace(order_id, record);
    return {true, false};
}

bool OrderStore::mark_delivered(std::uint64_t order_id, std::uint64_t base_generation,
                                std::int64_t now_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = orders_.find(order_id);
    if (it == orders_.end() || it->second.delivered() || base_generation == 0) {
        return false;
    }
    OrderRecord updated = it->second;
    updated.delivered_ms = now_ms;
    updated.delivered_base_generation = base_generation;
    if (!append_line(render_line(updated))) {
        return false;  // 内存未动：不冒充完成，留在待办下拍重投
    }
    it->second = std::move(updated);
    return true;
}

std::vector<OrderRecord> OrderStore::pending() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<OrderRecord> out;
    for (const auto& [order_id, record] : orders_) {
        if (!record.delivered()) {
            out.push_back(record);
        }
    }
    std::sort(out.begin(), out.end(), [](const OrderRecord& a, const OrderRecord& b) {
        return a.receive_seq < b.receive_seq;
    });
    return out;
}

const OrderRecord* OrderStore::find(std::uint64_t order_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = orders_.find(order_id);
    return it == orders_.end() ? nullptr : &it->second;
}

std::size_t OrderStore::pending_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t n = 0;
    for (const auto& [order_id, record] : orders_) {
        if (!record.delivered()) {
            ++n;
        }
    }
    return n;
}

std::size_t OrderStore::delivered_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t n = 0;
    for (const auto& [order_id, record] : orders_) {
        if (record.delivered()) {
            ++n;
        }
    }
    return n;
}

std::size_t OrderStore::total_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return orders_.size();
}

// ---- 内部：行渲染/追加/回放/压薄（锁内调用）----

std::string OrderStore::render_line(const OrderRecord& record) const {
    // 7 字段统一行：state|order_id|player_id|received_ms|delivered_ms|
    // base_generation|payload（payload 已由 accept 保证无 '\n'/无 '|'）。
    std::string line = record.delivered() ? "D|" : "P|";
    line += std::to_string(record.order_id);
    line += '|';
    line += std::to_string(record.player_id);
    line += '|';
    line += std::to_string(record.received_ms);
    line += '|';
    line += std::to_string(record.delivered_ms);
    line += '|';
    line += std::to_string(record.delivered_base_generation);
    line += '|';
    line += record.payload;
    return line;
}

bool OrderStore::append_line(const std::string& line) {
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
        out_.close();  // 下拍 append 重开自愈（句柄坏不复用）
        return false;
    }
    return true;
}

bool OrderStore::replay() {
    std::ifstream in(journal_path_, std::ios::binary);
    if (!in.good()) {
        // 文件不存在 = 全新 store：建空文件验证路径可写（目录缺失在此显形）。
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
        // 切 7 字段（payload 含余量但不允许 '|'，恰 6 个分隔符）。
        std::vector<std::string> fields;
        fields.reserve(7);
        std::size_t begin = 0;
        for (int i = 0; i < 6; ++i) {
            const std::size_t sep = line.find('|', begin);
            if (sep == std::string::npos) {
                fields.clear();
                break;
            }
            fields.emplace_back(line.substr(begin, sep - begin));
            begin = sep + 1;
        }
        if (fields.size() != 6) {
            throw std::runtime_error("order journal corrupt at line "
                                     + std::to_string(line_no) + ": " + journal_path_);
        }
        fields.emplace_back(line.substr(begin));
        const std::string& state = fields[0];
        OrderRecord record;
        record.order_id = std::strtoull(fields[1].c_str(), nullptr, 10);
        record.player_id = std::strtoull(fields[2].c_str(), nullptr, 10);
        record.received_ms = static_cast<std::int64_t>(
            std::strtoll(fields[3].c_str(), nullptr, 10));
        record.delivered_ms = static_cast<std::int64_t>(
            std::strtoll(fields[4].c_str(), nullptr, 10));
        record.delivered_base_generation =
            std::strtoull(fields[5].c_str(), nullptr, 10);
        record.payload = fields[6];
        if (record.order_id == 0 || record.player_id == 0) {
            throw std::runtime_error("order journal corrupt at line "
                                     + std::to_string(line_no) + ": " + journal_path_);
        }
        // 同单后行覆盖前行（事件序）；受理序按首次出现顺序分配（待办稳定
        // 排序与原受理顺序一致）。
        const auto [it, inserted] = orders_.try_emplace(record.order_id, record);
        if (inserted) {
            it->second.receive_seq = next_receive_seq_++;
        } else {
            record.receive_seq = it->second.receive_seq;
            it->second = record;
        }
        // 状态列与 delivered_ms 一致性（P 行必须 0，D 行必须非 0）。
        if ((state == "D") != it->second.delivered()) {
            throw std::runtime_error("order journal state mismatch at line "
                                     + std::to_string(line_no) + ": " + journal_path_);
        }
    }
    return true;
}

void OrderStore::compact_snapshot() {
    const std::string tmp = journal_path_ + ".tmp";
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out.good()) {
        return;
    }
    for (const auto& [order_id, record] : orders_) {
        out << render_line(record) << '\n';
    }
    out.flush();
    if (!out.good()) {
        std::remove(tmp.c_str());
        return;
    }
    out.close();
    // Windows 覆盖已存在目标必败的坑：先删后 rename（memory 既有口径）。
    std::remove(journal_path_.c_str());
    if (std::rename(tmp.c_str(), journal_path_.c_str()) != 0) {
        std::remove(tmp.c_str());  // 留旧文件：事件序完整，仅压薄未成
    }
}

}  // namespace apollo::game::orders
