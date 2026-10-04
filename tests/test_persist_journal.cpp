// Write-behind journal 行为测试（P1-4 骨架验证）。
//
// 覆盖（persistence.md §5 目标模型 / attribute-sync §8.2 口径）：
//   append 落盘 → 定额 drain → 快照压薄 → 崩溃 replay 回放 → seq 续接。

#include "apollo/data/orm/persist_journal.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#define TEST_ASSERT(cond, msg)                                                           \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            std::cerr << "FAIL: " << (msg) << " (" << __FILE__ << ":" << __LINE__ << ")" \
                      << std::endl;                                                      \
            return false;                                                                \
        }                                                                                \
    } while (0)

namespace {

namespace fs = std::filesystem;

std::string make_tmp_path(const char* tag) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return (fs::temp_directory_path() /
            ("apollo_journal_test_" + std::string(tag) + "_" + std::to_string(now) + ".log"))
        .string();
}

std::size_t count_lines(const std::string& path) {
    std::ifstream in(path);
    std::size_t n = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) {
            ++n;
        }
    }
    return n;
}

bool test_append_and_pending() {
    const auto path = make_tmp_path("append");
    apollo::data::journal::PersistJournal journal(path);
    TEST_ASSERT(journal.open(), "open 成功");
    TEST_ASSERT(journal.pending() == 0, "初始空队列");

    for (int i = 0; i < 5; ++i) {
        TEST_ASSERT(journal.append("42", "{\"hp\":" + std::to_string(i) + "}", 1000 + i),
                    "append 落盘");
    }
    TEST_ASSERT(journal.pending() == 5, "pending 计数");
    TEST_ASSERT(count_lines(path) == 5, "文件行数与 append 数一致");
    TEST_ASSERT(journal.next_sequence() == 6, "seq 单调");

    std::error_code ec;
    fs::remove(path, ec);
    return true;
}

bool test_drain_quota_and_compact() {
    const auto path = make_tmp_path("drain");
    apollo::data::journal::PersistJournal journal(path);
    journal.open();
    for (int i = 0; i < 10; ++i) {
        journal.append("p" + std::to_string(i), "{}", i);
    }

    std::vector<std::string> applied;
    const auto n = journal.drain(
        [&](const apollo::data::journal::JournalEntry& e) {
            applied.push_back(e.key);
            return true;
        },
        3);
    TEST_ASSERT(n == 3, "定额出队 3 条");
    TEST_ASSERT(applied.size() == 3 && applied[0] == "p0" && applied[2] == "p2",
                "保序出队");
    TEST_ASSERT(journal.pending() == 7, "余量 7");
    TEST_ASSERT(count_lines(path) == 7, "快照压薄：已应用 3 条出文件");

    const auto rest = journal.drain(
        [](const apollo::data::journal::JournalEntry&) { return true; }, 100);
    TEST_ASSERT(rest == 7 && journal.pending() == 0, "余量清空");
    TEST_ASSERT(count_lines(path) == 0, "全应用后文件压薄为空");

    std::error_code ec;
    fs::remove(path, ec);
    return true;
}

bool test_sink_failure_retains_entry() {
    const auto path = make_tmp_path("fail");
    apollo::data::journal::PersistJournal journal(path);
    journal.open();
    journal.append("a", "{}", 1);
    journal.append("b", "{}", 2);

    const auto n = journal.drain(
        [](const apollo::data::journal::JournalEntry& e) { return e.key != "a"; }, 10);
    TEST_ASSERT(n == 0, "首条失败即停（保序）");
    TEST_ASSERT(journal.pending() == 2, "失败条目保留待重试");
    TEST_ASSERT(count_lines(path) == 2, "文件未压薄");

    std::error_code ec;
    fs::remove(path, ec);
    return true;
}

bool test_crash_replay_and_sequence_continuity() {
    const auto path = make_tmp_path("replay");
    {
        apollo::data::journal::PersistJournal journal(path);
        journal.open();
        for (int i = 0; i < 5; ++i) {
            journal.append("42", "{\"n\":" + std::to_string(i) + "}", i);
        }
    }  // 进程「崩溃」：未 drain 即退出

    apollo::data::journal::PersistJournal revived(path);
    revived.open();
    TEST_ASSERT(revived.pending() == 5, "重启后队列从文件恢复");

    std::vector<std::uint64_t> seqs;
    const auto n = revived.replay(
        [&](const apollo::data::journal::JournalEntry& e) {
            seqs.push_back(e.sequence);
            return true;
        });
    TEST_ASSERT(n == 5, "回放 5 条");
    TEST_ASSERT(seqs.size() == 5 && seqs[0] == 1 && seqs[4] == 5, "回放保序 seq 连续");
    TEST_ASSERT(revived.next_sequence() == 6, "seq 续接");

    TEST_ASSERT(revived.append("43", "{}", 99), "回放后可继续 append");
    TEST_ASSERT(revived.next_sequence() == 7, "新条目 seq 接续");

    std::error_code ec;
    fs::remove(path, ec);
    return true;
}

} // namespace

int main() {
    int passed = 0;
    int failed = 0;

    struct Case {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"journal_append_and_pending", test_append_and_pending},
        {"journal_drain_quota_and_compact", test_drain_quota_and_compact},
        {"journal_sink_failure_retains_entry", test_sink_failure_retains_entry},
        {"journal_crash_replay_and_sequence_continuity",
         test_crash_replay_and_sequence_continuity},
    };

    for (const auto& c : cases) {
        std::cout << "[ RUN  ] " << c.name << std::endl;
        if (c.fn()) {
            std::cout << "[  OK  ] " << c.name << std::endl;
            ++passed;
        } else {
            std::cout << "[ FAIL ] " << c.name << std::endl;
            ++failed;
        }
    }

    std::cout << "PersistJournalTests: " << passed << " passed, " << failed << " failed"
              << std::endl;
    return failed == 0 ? 0 : 1;
}
