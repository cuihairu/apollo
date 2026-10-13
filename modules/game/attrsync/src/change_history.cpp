#include "apollo/game/attrsync/change_history.hpp"

#include <algorithm>

namespace apollo::game::attrsync {

ChangeHistory::ChangeHistory(std::size_t capacity)
    : capacity_(capacity == 0 ? 1 : capacity), ring_(capacity_) {}

std::uint64_t ChangeHistory::append(std::uint32_t attr_id, std::int64_t value) {
    ++world_seq_;
    ring_[head_] = HistoryEntry{world_seq_, attr_id, value};
    head_ = (head_ + 1) % capacity_;
    if (size_ < capacity_) {
        ++size_;
    }
    return world_seq_;
}

std::vector<HistoryEntry> ChangeHistory::entries_since(std::uint64_t since_seq) const {
    std::vector<HistoryEntry> result;
    if (size_ == 0) {
        return result;
    }

    // 环形缓冲中条目按写入顺序排列
    // 起始位置：若已覆盖，head_ 是最旧条目；否则 0 是最早条目
    std::size_t start = (size_ == capacity_) ? head_ : 0;

    for (std::size_t i = 0; i < size_; ++i) {
        const auto& entry = ring_[(start + i) % capacity_];
        if (entry.seq > since_seq) {
            result.push_back(entry);
        }
    }
    return result;
}

}  // namespace apollo::game::attrsync
