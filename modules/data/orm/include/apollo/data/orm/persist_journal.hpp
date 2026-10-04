#pragma once

// Write-behind journal（契约 §1.5 PersistJournal；attribute-sync §8.2 口径）：
//   变更先追加日志（兼恢复位点）→ 每 tick 定额均匀出队 → 定期快照压薄；
//   进程重启从 journal 位点回放（P1-5 Recovery 的基础设施）。
//
// 骨架口径（P1-4）：
//   - 文件为唯一权威：append 落一行文本（seq|timestamp_ms|key|payload），
//     pending_ 是内存镜像；
//   - drain(sink, max)：定额出队，sink 返回 true 视为已应用；
//     应用过的行在 compact 时从文件压薄；
//   - replay(sink)：从文件头回放全部（崩溃恢复），回放后 seq 续接；
//   - 单写者假设：调用方保证串行（SaveQueue worker / 启动单线程）。

#include <cstdint>
#include <deque>
#include <functional>
#include <string>

namespace apollo::data::journal {

struct JournalEntry {
    std::uint64_t sequence = 0;
    std::int64_t timestamp_ms = 0;
    std::string key;      // 业务键（如玩家 id 十进制串）
    std::string payload;  // 业务载荷（JSON 行）
};

class PersistJournal {
public:
    // 返回 false 表示该条目未被应用（保留在队列，下轮重试）
    using ApplySink = std::function<bool(const JournalEntry&)>;

    explicit PersistJournal(std::string journal_path);

    // 打开（或创建）journal 文件；已存在则恢复 next_sequence_
    bool open();

    // 追加一条（write-ahead：先落盘再入队）；返回 false 表示写盘失败
    bool append(std::string key, std::string payload, std::int64_t timestamp_ms);

    // 未应用条目数
    std::size_t pending() const { return pending_.size(); }

    // 定额出队：最多 max_entries 条交给 sink；返回实际应用条数
    std::size_t drain(const ApplySink& sink, std::size_t max_entries);

    // 崩溃恢复回放：文件全部条目交给 sink；返回回放条数。
    // 仅供启动期调用（open 后、append 前）。
    std::size_t replay(const ApplySink& sink);

    // 快照压薄：把文件重写为仅剩未应用条目；返回压薄后文件条目数。
    // applied 为空时文件截断为空。
    std::size_t compact();

    std::uint64_t next_sequence() const { return next_sequence_; }

private:
    std::string journal_path_;
    std::deque<JournalEntry> pending_;
    std::uint64_t next_sequence_ = 1;
    bool opened_ = false;
};

} // namespace apollo::data::journal
