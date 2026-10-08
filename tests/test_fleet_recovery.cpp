// 恢复相位跨进程化单测（P3-1 G-1 收尾批增量③）。
//
// 覆盖（直编源惯例同 directory_mirror_tests）：
//   1. FullReport wire（APD2 kind=4）：编码→解码往返 + 布局断言（kind/
//      component/zone/分片字段）+ 守卫（截断/零组件号/轮次 0/total_parts
//      0/part_index 越界/条目数越限/长度与声明不符/条目越限拒编）；
//   2. Reporter：分片连发（234 条 → 2 片、同轮 report_id、组件/域打戳）、
//      空表 = 1 空片（intake 空集语义）、零组件号拒发、发送失败计片；
//   3. Coordinator 重组 + intake：多分片按序重组完整收讫回调（组件/域/
//      条目透传）、乱序非起始片丢、换轮重启、完整后记入已报集；
//   4. Coordinator 收敛：begin 排他（admissible false）/ 部分已报不开放 /
//      全报收敛开放 / 超时收敛开放 / 在册 0 靠超时 / Normal 期报告照收
//      intake（Zone 重启重报路径）；
//   5. PlayerDirectory::intake_full_report（restore-not-kick）：已有条目
//      保 epoch 刷绑定列（绝不出 Kicked）、无条目 session_up 新 epoch、
//      空 report 不清目录、零号玩家条目拒收；
//   6. UDP loopback 全链：真 socket——Reporter 分片 → manager Feed →
//      Coordinator 重组 intake（恢复相位收敛开放）。

#include "apollo/game/session/fleet_recovery.hpp"
#include "apollo/game/session/player_directory.hpp"
#include "apollo/net/discovery/udp_transport.hpp"

#include <cstddef>
#include <cstdint>
#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

#define TEST_ASSERT(cond, msg)                                                               \
    do {                                                                                     \
        if (!(cond)) {                                                                       \
            std::cerr << "FAIL: " << (msg) << " (" << __FILE__ << ":" << __LINE__ << ")"     \
                      << std::endl;                                                          \
            return false;                                                                    \
        }                                                                                    \
    } while (0)

namespace {

namespace session = apollo::game::session;
namespace disco = apollo::net::discovery;
using session::FleetReportPart;
using session::FleetRecoveryCoordinator;
using session::FleetReporter;
using session::MirrorEntry;
using session::PlayerDirectory;
using session::encode_fleet_report;
using session::is_fleet_report;

session::MirrorEntry make_entry(std::uint64_t player_id, std::uint64_t session_id) {
    session::MirrorEntry e;
    e.player_id = player_id;
    e.anchor_epoch = 0;  // 报告方无 epoch——裁决键归 manager
    e.session_id = session_id;
    e.gateway_id = 3;
    e.zone_id = 2;
    e.state = PlayerDirectory::EntryState::Online;
    e.assignment.world_id = 7;
    return e;
}

// ---- 1. FullReport wire ----

bool test_fleet_report_wire_roundtrip_and_guards() {
    FleetReportPart part;
    part.component_id = 30;
    part.zone_id = 2;
    part.report_id = 4;
    part.total_parts = 2;
    part.part_index = 1;
    part.entries = {make_entry(1, 1001), make_entry(2, 1002)};

    std::uint8_t buf[2048] = {};
    const std::size_t len = encode_fleet_report(part, buf, sizeof(buf));
    TEST_ASSERT(len == 30 + 2 * 69, "分片长度 = 30B 头 + 2×69B 条目");
    TEST_ASSERT(buf[0] == 0x32 && buf[1] == 0x44 && buf[2] == 0x50 && buf[3] == 0x41,
                "APD2 magic（与镜像面同族）");
    TEST_ASSERT(buf[6] == 4, "kind=FullReport(4)");

    FleetReportPart out;
    TEST_ASSERT(decode_fleet_report(buf, len, out), "合法分片解码");
    TEST_ASSERT(out.component_id == 30 && out.zone_id == 2 && out.report_id == 4 &&
                    out.total_parts == 2 && out.part_index == 1 &&
                    out.entries == part.entries,
                "分片字段往返一致");

    // 守卫
    TEST_ASSERT(!decode_fleet_report(buf, len - 1, out), "截断丢");
    std::uint8_t bad[2048];
    std::memcpy(bad, buf, len);
    bad[8] = 0;  // component_id 低字节 → 0
    TEST_ASSERT(!decode_fleet_report(bad, len, out), "零组件号丢");
    std::memcpy(bad, buf, len);
    bad[20] = 0;  // report_id → 0
    TEST_ASSERT(!decode_fleet_report(bad, len, out), "轮次 0 丢");
    std::memcpy(bad, buf, len);
    bad[24] = 0;
    bad[25] = 0;  // total_parts → 0
    TEST_ASSERT(!decode_fleet_report(bad, len, out), "total_parts=0 丢");
    std::memcpy(bad, buf, len);
    bad[26] = 9;  // part_index=9 ≥ total_parts=2
    TEST_ASSERT(!decode_fleet_report(bad, len, out), "part_index 越界丢");
    std::memcpy(bad, buf, len);
    bad[28] = 200;  // entry_count 越限且与长度不符
    TEST_ASSERT(!decode_fleet_report(bad, len, out), "条目数越限丢");
    TEST_ASSERT(!is_fleet_report(buf, 10), "短包非 FullReport");
    buf[6] = 3;
    TEST_ASSERT(!is_fleet_report(buf, len), "镜像快照 kind 不判 FullReport");
    buf[6] = 4;

    part.entries.assign(session::kMaxSnapshotPartEntries + 1, make_entry(1, 1));
    TEST_ASSERT(encode_fleet_report(part, buf, sizeof(buf)) == 0, "条目越限拒编");
    return true;
}

// ---- 2. Reporter ----

bool test_reporter_chunking() {
    std::vector<std::vector<std::uint8_t>> sent;
    int fail_after = -1;  // -1 = 不失败
    FleetReporter reporter(
        30, 2,
        [&](const std::uint8_t* data, std::size_t len) {
            if (fail_after == 0) {
                return false;
            }
            if (fail_after > 0) {
                --fail_after;
            }
            sent.emplace_back(data, data + len);
            return true;
        });

    std::vector<MirrorEntry> sessions;
    for (std::uint64_t i = 1; i <= 234; ++i) {
        sessions.push_back(make_entry(i, 1000 + i));
    }
    const std::size_t parts = reporter.report_full(sessions);
    TEST_ASSERT(parts == 2 && sent.size() == 2, "234 条 → 2 片全发");

    std::size_t total = 0;
    for (std::size_t i = 0; i < sent.size(); ++i) {
        FleetReportPart p;
        TEST_ASSERT(session::decode_fleet_report(sent[i].data(), sent[i].size(), p),
                    "分片可解码");
        TEST_ASSERT(p.part_index == i && p.total_parts == 2, "part_index 升序");
        TEST_ASSERT(p.component_id == 30 && p.zone_id == 2, "组件/域打戳");
        TEST_ASSERT(p.report_id == 1, "同轮同 report_id（自 1 起）");
        total += p.entries.size();
    }
    TEST_ASSERT(total == 234, "分片条目守恒");

    // 空表 = 1 空片
    sent.clear();
    TEST_ASSERT(reporter.report_full({}) == 1, "空表发 1 空片");
    FleetReportPart empty_part;
    TEST_ASSERT(session::decode_fleet_report(sent[0].data(), sent[0].size(),
                                             empty_part) &&
                    empty_part.entries.empty(),
                "空片可解码");

    // 零组件号拒发
    FleetReporter zero_reporter(0, 0, [&](const std::uint8_t*, std::size_t) {
        return true;
    });
    TEST_ASSERT(zero_reporter.report_full(sessions) == 0, "零组件号拒发");

    // 发送失败计片（fire-and-forget，重试归下一轮全量重报）
    sent.clear();
    fail_after = 0;
    TEST_ASSERT(reporter.report_full(sessions) == 0, "全败片不计入发送数");
    TEST_ASSERT(sent.empty(), "失败片未发出");
    return true;
}

// ---- 3. Coordinator 重组 + intake ----

bool test_coordinator_reassembly_and_intake() {
    struct Intake {
        std::uint64_t component_id = 0;
        std::uint32_t zone_id = 0;
        std::vector<MirrorEntry> sessions;
        int calls = 0;
    } intake_log;
    FleetRecoveryCoordinator coordinator(
        1, 10000,
        [&](std::uint64_t cid, std::uint32_t zone,
            const std::vector<MirrorEntry>& sessions) {
            intake_log.component_id = cid;
            intake_log.zone_id = zone;
            intake_log.sessions = sessions;
            ++intake_log.calls;
        });

    FleetReportPart p0;
    p0.component_id = 30;
    p0.zone_id = 2;
    p0.report_id = 1;
    p0.total_parts = 2;
    p0.part_index = 0;
    p0.entries = {make_entry(1, 1001)};

    FleetReportPart p1 = p0;
    p1.part_index = 1;
    p1.entries = {make_entry(2, 1002)};

    // 乱序非起始片丢（无处挂靠返回 false）
    TEST_ASSERT(!coordinator.on_report_part(p1), "乱序非起始片丢");
    TEST_ASSERT(intake_log.calls == 0, "乱序片不触发 intake");

    TEST_ASSERT(coordinator.on_report_part(p0), "起始片收讫（重组中）");
    TEST_ASSERT(intake_log.calls == 0, "重组未完成不 intake");

    TEST_ASSERT(coordinator.on_report_part(p1), "收尾片收讫");
    TEST_ASSERT(intake_log.calls == 1 && intake_log.component_id == 30 &&
                    intake_log.zone_id == 2 && intake_log.sessions.size() == 2 &&
                    intake_log.sessions[0].player_id == 1 &&
                    intake_log.sessions[1].player_id == 2,
                "完整收讫 intake 透传组件/域/条目");
    TEST_ASSERT(coordinator.reported_count() == 1, "报告方记入已报集");

    // 重复收尾片丢（重组已擦除）
    TEST_ASSERT(!coordinator.on_report_part(p1), "重组后残片丢");
    TEST_ASSERT(intake_log.calls == 1, "不重复 intake");
    return true;
}

// ---- 4. Coordinator 收敛 ----

bool test_coordinator_convergence() {
    std::vector<MirrorEntry> last_intake;
    FleetRecoveryCoordinator coordinator(
        2, 5000,
        [&](std::uint64_t, std::uint32_t, const std::vector<MirrorEntry>& sessions) {
            last_intake = sessions;
        });

    TEST_ASSERT(coordinator.admissible(), "初始 Normal 放行");
    coordinator.begin(1000);
    TEST_ASSERT(coordinator.recovering() && !coordinator.admissible(),
                "begin 进恢复相位排他");
    coordinator.begin(2000);
    TEST_ASSERT(coordinator.recovering(), "重复 begin 幂等");

    FleetReportPart part;
    part.component_id = 30;
    part.report_id = 1;
    part.total_parts = 1;
    part.part_index = 0;
    part.entries = {make_entry(1, 1001)};

    TEST_ASSERT(coordinator.on_report_part(part), "第一方报告收讫");
    TEST_ASSERT(coordinator.recovering(), "部分已报不开放（在册 2 只到 1）");
    coordinator.tick(5000);
    TEST_ASSERT(coordinator.recovering(), "未到超时且未全报——仍排他");

    FleetReportPart part2 = part;
    part2.component_id = 31;
    TEST_ASSERT(coordinator.on_report_part(part2), "第二方报告收讫");
    TEST_ASSERT(coordinator.admissible(), "全报收敛开放（判据①）");
    TEST_ASSERT(coordinator.last_reported_count() == 2,
                "收敛读数留存（reported_ 清零后观测面照读）");

    // 超时收敛（判据②）：新一轮恢复相位无报告
    coordinator.begin(10000);
    TEST_ASSERT(coordinator.recovering(), "第二轮恢复相位");
    coordinator.tick(14999);
    TEST_ASSERT(coordinator.recovering(), "超时线上不提前开放");
    coordinator.tick(15000);
    TEST_ASSERT(coordinator.admissible(), "超时收敛开放");

    // 在册 0：begin 后靠超时开放
    FleetRecoveryCoordinator zero_expected(0, 100,
                                          [](std::uint64_t, std::uint32_t,
                                             const std::vector<MirrorEntry>&) {});
    zero_expected.begin(0);
    TEST_ASSERT(zero_expected.recovering(), "在册 0 也进排他");
    zero_expected.tick(99);
    TEST_ASSERT(zero_expected.recovering(), "在册 0 未超时仍排他");
    zero_expected.tick(100);
    TEST_ASSERT(zero_expected.admissible(), "在册 0 超时开放");

    // Normal 期报告照收 intake（Zone 重启重报路径——不进恢复相位）
    TEST_ASSERT(coordinator.admissible(), "Normal 期");
    TEST_ASSERT(coordinator.on_report_part(part), "Normal 期报告照收");
    TEST_ASSERT(last_intake.size() == 1, "Normal 期 intake 照调（Zone 重启重报）");
    return true;
}

// ---- 5. PlayerDirectory intake（restore-not-kick）----

bool test_directory_intake_restore_not_kick() {
    PlayerDirectory directory;
    int kicked = 0;
    directory.set_event_sink([&kicked](const PlayerDirectory::Event& event) {
        if (event.kind == PlayerDirectory::Event::Kind::SessionKicked) {
            ++kicked;
        }
    });

    // 预置条目：player 1 epoch 1
    session::SessionBinding binding;
    binding.session_id = 1001;
    binding.gateway_id = 9;
    session::WorldAssignment assignment;
    assignment.world_id = 1;
    directory.session_up(1, binding, assignment, 5);

    std::vector<MirrorEntry> report;
    // player 1：已有条目——保 epoch 刷绑定列
    auto refreshed = make_entry(1, 7777);
    refreshed.gateway_id = 4;
    refreshed.zone_id = 6;
    refreshed.assignment.world_id = 99;
    report.push_back(refreshed);
    // player 2：无条目——session_up 新 epoch
    report.push_back(make_entry(2, 2002));

    const auto taken = directory.intake_full_report(report);
    TEST_ASSERT(taken == 2, "intake 两行");
    TEST_ASSERT(kicked == 0, "恢复 intake 绝不出 Kicked");

    const auto* p1 = directory.find(1);
    TEST_ASSERT(p1 != nullptr && p1->anchor_epoch == 1,
                "已有条目保 epoch（裁决键不漂移）");
    TEST_ASSERT(p1->binding.session_id == 7777 && p1->binding.gateway_id == 4 &&
                    p1->zone_id == 6 && p1->assignment.world_id == 99 &&
                    p1->state == PlayerDirectory::EntryState::Online,
                "绑定/定位列刷新");

    const auto* p2 = directory.find(2);
    TEST_ASSERT(p2 != nullptr && p2->anchor_epoch == 2 &&
                    p2->binding.session_id == 2002,
                "无条目走 session_up 新 epoch");

    // 空 report 不清目录（intake 是增量重建不是重置）
    TEST_ASSERT(directory.intake_full_report({}) == 0, "空 report 零 intake");
    TEST_ASSERT(directory.size() == 2, "空 report 不清目录");

    // 零号玩家条目拒收
    std::vector<MirrorEntry> bad = {make_entry(0, 1)};
    TEST_ASSERT(directory.intake_full_report(bad) == 0, "零号玩家条目拒收");
    return true;
}

// ---- 6. UDP loopback 全链 ----

namespace {

bool poll_receive(disco::UdpFeed& feed, std::uint8_t* buf, std::size_t cap,
                  std::size_t& len_out) {
    for (int i = 0; i < 100; ++i) {
        if (feed.try_receive(buf, cap, len_out)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

} // namespace

bool test_udp_fleet_recovery_loopback() {
    disco::UdpFeed manager_feed;
    TEST_ASSERT(manager_feed.open(0, "127.0.0.1"), "manager 收包口就绪");

    std::vector<MirrorEntry> intake;
    std::uint64_t intake_component = 0;
    FleetRecoveryCoordinator coordinator(
        1, 60000,
        [&](std::uint64_t cid, std::uint32_t, const std::vector<MirrorEntry>& s) {
            intake_component = cid;
            intake = s;
        });
    coordinator.begin(0);
    TEST_ASSERT(!coordinator.admissible(), "恢复相位排他");

    // 报告方经真 UDP 发全量重报（2 条 → 2 片？2 条 1 片；用 130 条验分片
    // 重组跨真 socket——130 条 → 2 片）
    disco::UdpBeaconTransport report_transport;
    TEST_ASSERT(report_transport.open(), "报告方传输就绪");
    FleetReporter reporter(
        30, 2, [&](const std::uint8_t* data, std::size_t len) {
            return report_transport.send_bytes("127.0.0.1", manager_feed.port(),
                                               data, len);
        });
    std::vector<MirrorEntry> sessions;
    for (std::uint64_t i = 1; i <= 130; ++i) {
        sessions.push_back(make_entry(i, 1000 + i));
    }
    TEST_ASSERT(reporter.report_full(sessions) == 2, "130 条 → 2 片");

    std::uint8_t buf[disco::kMaxDatagramSize];
    std::size_t len = 0;
    int received = 0;
    while (poll_receive(manager_feed, buf, sizeof(buf), len)) {
        FleetReportPart part;
        if (session::decode_fleet_report(buf, len, part)) {
            TEST_ASSERT(coordinator.on_report_part(part), "分片入协调器");
            ++received;
        }
    }
    TEST_ASSERT(received == 2, "两片全收");
    TEST_ASSERT(coordinator.admissible(), "全报收敛开放");
    TEST_ASSERT(intake_component == 30 && intake.size() == 130,
                "重组 intake 全量透传");
    TEST_ASSERT(intake.front().player_id == 1 && intake.back().player_id == 130,
                "分片条目序守恒");
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
        {"fleet_report_wire_roundtrip_and_guards", test_fleet_report_wire_roundtrip_and_guards},
        {"reporter_chunking", test_reporter_chunking},
        {"coordinator_reassembly_and_intake", test_coordinator_reassembly_and_intake},
        {"coordinator_convergence", test_coordinator_convergence},
        {"directory_intake_restore_not_kick", test_directory_intake_restore_not_kick},
        {"udp_fleet_recovery_loopback", test_udp_fleet_recovery_loopback},
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

    std::cout << "FleetRecoveryTests: " << passed << " passed, " << failed << " failed"
              << std::endl;
    return failed == 0 ? 0 : 1;
}
