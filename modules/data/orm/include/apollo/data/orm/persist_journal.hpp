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
//   - 线程安全（PlayerStore 存档串行化批）：append/open/replay/compact
//     类内互斥；drain 串行化（drain 互斥）且 sink 在 journal 锁外执行——
//     多线程并发 append + 单/多线程 drain 均不丢条、不乱序（seq 单调）。
//
// 持久面批（2026-10-13）：append 走常驻句柄（P1-4 逐行开/关在帧预算表
// 实测超预算 1-3×，capacity-and-benchmark §2.4）——逐行 flush 耐久口径
// 不变；写失败关句柄、下拍 append 重开（自愈同旧逐行开语义）；compact
// 的 tmp+rename 原子重写会换 inode（Windows 且不许删打开中文件），故
// 压薄前先关句柄、重写后重开。

#include <cstdint>
#include <deque>
#include <fstream>
#include <functional>
#include <mutex>
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

    // 追加一条（write-ahead：先落盘再入队）；返回分配的全局序（0 = 写盘失败）
    std::uint64_t append(std::string key, std::string payload, std::int64_t timestamp_ms);

    // 未应用条目数
    std::size_t pending() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pending_.size();
    }

    // 定额出队：最多 max_entries 条交给 sink；返回实际应用条数
    std::size_t drain(const ApplySink& sink, std::size_t max_entries);

    // 崩溃恢复回放：文件全部条目交给 sink；返回回放条数。
    // 仅供启动期调用（open 后、append 前）。
    std::size_t replay(const ApplySink& sink);

    // 快照压薄：把文件重写为仅剩未应用条目；返回压薄后文件条目数。
    // applied 为空时文件截断为空。
    std::size_t compact();

    std::uint64_t next_sequence() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return next_sequence_;
    }

private:
    // 常驻写句柄（app|binary）；关闭态 = 下拍 append 重开
    bool ensure_handle();

    std::string journal_path_;
    std::deque<JournalEntry> pending_;
    std::uint64_t next_sequence_ = 1;
    bool opened_ = false;
    std::ofstream out_;

    mutable std::mutex mutex_;   // 数据面互斥（append/open/drain/compact/replay）
    std::mutex drainMutex_;      // drain 串行化（多引流者互斥；sink 锁外执行）
};

} // namespace apollo::data::journal
