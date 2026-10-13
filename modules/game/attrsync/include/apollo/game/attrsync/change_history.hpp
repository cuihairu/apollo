#pragma once
//
// ChangeHistory — 序号化变更环形日志（attribute-sync §3.1）。
//
// 每次属性变更追加一条 (seq, attr_id, value) 记录。seq 单调递增，
// 环形缓冲容量 N（默认 1024），溢出后旧条目被覆盖。
// 同步的唯一真相投影：delta = entries_since(acked_seq)，
// 快照 = 当前值集（由调用方从属性存储读取）。
//

#include <cstdint>
#include <vector>

namespace apollo::game::attrsync {

struct HistoryEntry {
    std::uint64_t seq = 0;
    std::uint32_t attr_id = 0;
    std::int64_t value = 0;  // zigzag 编码后的值（设计 §2.2 统一 int64 定标）
};

class ChangeHistory {
public:
    explicit ChangeHistory(std::size_t capacity = 1024);

    // 追加一条变更记录，返回新 seq
    std::uint64_t append(std::uint32_t attr_id, std::int64_t value);

    // 获取 (since_seq, world_seq] 区间的变更（不含 since_seq 本身）
    [[nodiscard]] std::vector<HistoryEntry> entries_since(std::uint64_t since_seq) const;

    // 当前最新 seq
    [[nodiscard]] std::uint64_t world_seq() const noexcept { return world_seq_; }

    // 环形容量
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }

    // 已存条目数
    [[nodiscard]] std::size_t size() const noexcept { return size_; }

    // 是否已覆盖（旧条目被挤出）
    [[nodiscard]] bool wrapped() const noexcept { return size_ > capacity_; }

private:
    std::size_t capacity_;
    std::vector<HistoryEntry> ring_;
    std::size_t head_ = 0;   // 下一个写入位置
    std::size_t size_ = 0;   // 当前条目数
    std::uint64_t world_seq_ = 0;
};

}  // namespace apollo::game::attrsync
