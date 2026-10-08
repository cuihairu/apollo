// Machined 监督面 roster 花名册解析 + 监督事件 zone 透传单测（P3-1 §6
// 死亡行窗口处置批——roster zone 列落地）。直编源惯例同 player_directory_
// tests；Windows 下 supervisor.cpp 是空转桩（POSIX only 口径），测试端
// 同步空转跳过（平台口径见 apps/machined/src/supervisor.hpp 头注）。
#include "supervisor.hpp"

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include <chrono>
#include <thread>

#if !defined(_WIN32)
#include <unistd.h>
#endif

namespace {

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

const machined::RosterEntry* find_entry(const std::vector<machined::RosterEntry>& roster,
                                        const std::string& name) {
    for (const auto& e : roster) {
        if (e.name == name) {
            return &e;
        }
    }
    return nullptr;
}

#if defined(_WIN32)

bool test_roster_zone_column() { return true; }
bool test_roster_backcompat() { return true; }
bool test_supervisor_death_carries_zone() { return true; }

#else

// 临时 roster 文件（测试私有目录，进程退出自灭）
std::string write_temp_roster(const std::string& content) {
    const std::string path = "/tmp/apollo_supervisor_roster_test_" +
                             std::to_string(static_cast<long>(::getpid())) + ".txt";
    std::FILE* f = std::fopen(path.c_str(), "w");
    if (f == nullptr) {
        return {};
    }
    std::fputs(content.c_str(), f);
    std::fclose(f);
    return path;
}

// ---- roster zone 列：三列/四列格式 + 纯数字判据 ----
bool test_roster_zone_column() {
    std::cout << "Running: test_roster_zone_column..." << std::endl;

    const auto path = write_temp_roster(
        "# comment line\n"
        "\n"
        "base-app | 30 | 2 | /bin/sleep 30 --flag\n"
        "fakegw | 41 | 0 | /bin/sleep 600\n"
        "zone-no-comp | | 3 | /bin/true\n");
    TEST_ASSERT(!path.empty(), "临时 roster 可写");

    std::vector<machined::RosterEntry> roster;
    TEST_ASSERT(machined::load_roster(path, roster), "roster 可读");
    TEST_ASSERT(roster.size() == 3, "注释/空行跳过，3 条目");

    const auto* base_app = find_entry(roster, "base-app");
    TEST_ASSERT(base_app != nullptr, "base-app 条目在");
    TEST_ASSERT(base_app->component_id == 30, "component 列解析");
    TEST_ASSERT(base_app->zone_id == 2, "zone 列解析（§6 反查键）");
    TEST_ASSERT(base_app->command == "/bin/sleep 30 --flag", "命令列完整");
    TEST_ASSERT(base_app->args.size() == 3, "argv 切分");

    const auto* fakegw = find_entry(roster, "fakegw");
    TEST_ASSERT(fakegw != nullptr && fakegw->component_id == 41 && fakegw->zone_id == 0,
                "zone=0 合法（未分配 Zone，死亡只走 component 面）");

    const auto* zone_no_comp = find_entry(roster, "zone-no-comp");
    TEST_ASSERT(zone_no_comp != nullptr && zone_no_comp->component_id == 0 &&
                    zone_no_comp->zone_id == 3,
                "component 可省而 zone 独立声明（0 = 未入编队不上 wire）");
    std::remove(path.c_str());
    return true;
}

// ---- 两列/三列旧格式兼容：第三字段非纯数字原样归命令（命令面可含 '|'）----
bool test_roster_backcompat() {
    std::cout << "Running: test_roster_backcompat..." << std::endl;

    const auto path = write_temp_roster(
        "plain | /bin/true\n"
        "comp-only | 5 | /bin/sleep 5\n"
        "cmd-with-pipe | 6 | sh -c echo-a | tr a b\n");
    TEST_ASSERT(!path.empty(), "临时 roster 可写");

    std::vector<machined::RosterEntry> roster;
    TEST_ASSERT(machined::load_roster(path, roster), "roster 可读");
    TEST_ASSERT(roster.size() == 3, "3 条目");

    const auto* plain = find_entry(roster, "plain");
    TEST_ASSERT(plain != nullptr && plain->component_id == 0 && plain->zone_id == 0 &&
                    plain->command == "/bin/true",
                "两列旧格式：无 component 无 zone");

    const auto* comp_only = find_entry(roster, "comp-only");
    TEST_ASSERT(comp_only != nullptr && comp_only->component_id == 5 &&
                    comp_only->zone_id == 0 && comp_only->command == "/bin/sleep 5",
                "三列旧格式：zone 缺省 0，命令不受牵连");

    const auto* piped = find_entry(roster, "cmd-with-pipe");
    TEST_ASSERT(piped != nullptr && piped->component_id == 6 && piped->zone_id == 0,
                "命令含 '|'：非纯数字第三字段不误认 zone 列");
    TEST_ASSERT(piped->command == "sh -c echo-a | tr a b", "管道命令原样保留");
    TEST_ASSERT(piped->args.size() == 7, "含 '|' 的 argv 切分不丢段");
    std::remove(path.c_str());
    return true;
}

// ---- 监督事件 zone 透传：真 fork 子进程死亡 → Died 事件携带 roster zone ----
bool test_supervisor_death_carries_zone() {
    std::cout << "Running: test_supervisor_death_carries_zone..." << std::endl;

    std::vector<machined::RosterEntry> roster;
    machined::RosterEntry e;
    e.name = "sleeper";
    e.component_id = 42;
    e.zone_id = 3;
    e.command = "/bin/sleep 0.2";
    e.args = {"/bin/sleep", "0.2"};
    roster.push_back(e);

    machined::Supervisor supervisor(roster, 5, 50);
    std::vector<machined::SupervisorEvent> events;
    const auto t0 = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());

    supervisor.spawn_all(t0, events);
    TEST_ASSERT(events.size() == 1 &&
                    events[0].kind == machined::SupervisorEvent::Kind::Born,
                "拉起产 Born");
    TEST_ASSERT(events[0].component_id == 42 && events[0].zone_id == 3,
                "Born 事件携带 component+zone");
    TEST_ASSERT(supervisor.alive_count() == 1, "子进程存活");

    // 收割：sleep 0.2 到点退出 → Died 事件带 roster zone 列
    events.clear();
    bool died_seen = false;
    for (int i = 0; i < 500 && !died_seen; ++i) {  // 上限 5s 防挂
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        const auto now = t0 + static_cast<std::uint64_t>(i + 1) * 10;
        events.clear();
        supervisor.poll(now, events);
        for (const auto& ev : events) {
            if (ev.kind == machined::SupervisorEvent::Kind::Died) {
                died_seen = true;
                TEST_ASSERT(ev.component_id == 42 && ev.zone_id == 3,
                            "Died 事件携带 roster zone 列（§6 反查键透传）");
                TEST_ASSERT(ev.exit_code == 0, "sleep 正常退出 exit=0");
            }
        }
    }
    TEST_ASSERT(died_seen, "5s 内收割到 Died");
    return true;
}

#endif // _WIN32

} // namespace

int main() {
    std::cout << "=== Apollo Supervisor Roster Test Suite ===" << std::endl;

    int total = 0;
    int passed = 0;

    auto run = [&](bool (*fn)()) {
        total++;
        if (fn()) {
            passed++;
        }
    };

    run(test_roster_zone_column);
    run(test_roster_backcompat);
    run(test_supervisor_death_carries_zone);

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return total == passed ? 0 : 1;
}
