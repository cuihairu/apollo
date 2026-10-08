// 在线目录跨进程镜像单测（P3-1 G-1 收尾批增量②）。
//
// 覆盖（直编源惯例同 player_directory_tests）：
//   1. Delta wire：编码→解码往返 + 布局断言（magic/version/kind/86B 定长）
//      + 守卫（截断/错 magic/错 version/错 kind/event_kind 越界/长度漂移）；
//   2. SnapshotRequest wire：8B 定长编码 + is_snapshot_request 判别守卫；
//   3. SnapshotReply wire：分片往返（多条目）+ 守卫（total_parts 0 /
//      part_index 越界 / 条目数越限 / 长度与声明不符）；
//   4. Publisher：事件→Delta seq 单调、零号玩家拒发、发送失败仍推进 seq
//      （fire-and-forget，丢包归快照 healing）；全量快照分片（300 条 →
//      3 片、同 snapshot_id、baseline_seq 取当前 seq、空表 = 1 空片）；
//   5. Mirror 全事件类应用：Up 写入 / Moved 只动定位列 / Down、Kicked 删除
//      （顶号流 = Kicked erase + Up 写入）；
//   6. Mirror seq 纪律：首包定基线 / 重复丢 / 断档 stale + 请求钩子一次性
//      触发 / 快照到货复位后可再请求；
//   7. 快照分片重组：乱序非起始片丢、跨 snapshot_id 重启、完成后全量重置
//      （旧表清除 + last_seq = baseline_seq + stale 清）；
//   8. 端到端（进程内）：真 PlayerDirectory 事件 → Publisher（sink 链）→
//      字节流 → Mirror 投影（Up/Moved/Kicked/Down 全链一致）；
//   9. UDP loopback：owner Feed 收 SnapshotRequest → 应答快照分片 + 消费侧
//      镜像收 Delta/快照全链（真 socket 面含变长 try_receive）。

#include "apollo/game/session/directory_mirror.hpp"
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
using session::DirectoryDeltaBody;
using session::DirectoryMirror;
using session::DirectoryPublisher;
using session::DirectorySnapshotPart;
using session::MirrorEntry;
using session::PlayerDirectory;
using session::SessionBinding;
using session::WorldAssignment;
using session::encode_snapshot_request;
using session::is_snapshot_request;
using session::kMaxSnapshotPartEntries;

session::MirrorEntry make_entry(std::uint64_t player_id, std::uint64_t epoch = 1,
                                std::uint32_t world_id = 7) {
    session::MirrorEntry e;
    e.player_id = player_id;
    e.anchor_epoch = epoch;
    e.session_id = 1000 + player_id;
    e.gateway_id = 3;
    e.zone_id = 2;
    e.state = PlayerDirectory::EntryState::Online;
    e.assignment.world_id = world_id;
    e.assignment.map_id = 40;
    e.assignment.instance_id = 0;
    e.assignment.space_id = 4000;
    e.assignment.route_version = 9;
    return e;
}

// ---- 1. Delta wire ----

bool test_delta_wire_roundtrip_and_guards() {
    DirectoryDeltaBody body;
    body.seq = 42;
    body.event_kind = PlayerDirectory::Event::Kind::SessionMoved;
    body.reason = PlayerDirectory::kReasonLogout;
    body.entry = make_entry(9, 3, 11);

    std::uint8_t buf[128] = {};
    const std::size_t len = encode_delta(body, buf, sizeof(buf));
    TEST_ASSERT(len == 86, "delta 定长 86B（8 头 + seq4 + kind1 + reason4 + 条目69）");
    TEST_ASSERT(buf[0] == 0x32 && buf[1] == 0x44 && buf[2] == 0x50 && buf[3] == 0x41,
                "前 4 字节即 APD2 magic（小端 0x41504432）");
    TEST_ASSERT(buf[4] == 1 && buf[5] == 0 && buf[6] == 1,
                "version=1、kind=Delta(1)、reserved=0");

    DirectoryDeltaBody out;
    TEST_ASSERT(decode_delta(buf, len, out), "合法 delta 解码成功");
    TEST_ASSERT(out.seq == 42 &&
                    out.event_kind == PlayerDirectory::Event::Kind::SessionMoved &&
                    out.reason == PlayerDirectory::kReasonLogout,
                "标量列往返一致");
    TEST_ASSERT(out.entry == body.entry, "条目列往返一致");

    // 守卫：截断 / 错 magic / 错 version / 错 kind / kind 越界 / 长度漂移
    TEST_ASSERT(!decode_delta(buf, len - 1, out), "截断包丢");
    TEST_ASSERT(!decode_delta(buf, len + 1, out), "超长包丢（长度不符即非本协议）");
    std::uint8_t bad[86];
    std::memcpy(bad, buf, 86);
    bad[0] = 0xFF;
    TEST_ASSERT(!decode_delta(bad, 86, out), "错 magic 丢");
    std::memcpy(bad, buf, 86);
    bad[5] = 9;
    TEST_ASSERT(!decode_delta(bad, 86, out), "错 version 丢");
    std::memcpy(bad, buf, 86);
    bad[6] = 9;
    TEST_ASSERT(!decode_delta(bad, 86, out), "错 kind 丢");
    std::memcpy(bad, buf, 86);
    bad[12] = 4;  // event_kind 越界（SessionKicked=3 之上）
    TEST_ASSERT(!decode_delta(bad, 86, out), "event_kind 越界整包丢");
    return true;
}

// ---- 2. SnapshotRequest wire ----

bool test_snapshot_request_wire() {
    std::uint8_t buf[16] = {};
    TEST_ASSERT(encode_snapshot_request(buf, sizeof(buf)) == 8, "请求即 8B 公共头");
    TEST_ASSERT(is_snapshot_request(buf, 8), "合法请求判别");
    TEST_ASSERT(!is_snapshot_request(buf, 7), "截断请求丢");
    TEST_ASSERT(!is_snapshot_request(buf, 9), "超长请求丢");
    buf[6] = 1;
    TEST_ASSERT(!is_snapshot_request(buf, 8), "错 kind 丢");
    return true;
}

// ---- 3. SnapshotReply wire ----

bool test_snapshot_part_roundtrip_and_guards() {
    DirectorySnapshotPart part;
    part.snapshot_id = 5;
    part.total_parts = 2;
    part.part_index = 1;
    part.baseline_seq = 77;
    part.entries = {make_entry(1), make_entry(2), make_entry(3)};

    std::uint8_t buf[2048] = {};
    const std::size_t len = encode_snapshot_part(part, buf, sizeof(buf));
    TEST_ASSERT(len == 8 + 14 + 3 * 69, "分片长度 = 头 + 14B 分片头 + 3×69B 条目");

    DirectorySnapshotPart out;
    TEST_ASSERT(decode_snapshot_part(buf, len, out), "合法分片解码");
    TEST_ASSERT(out.snapshot_id == 5 && out.total_parts == 2 && out.part_index == 1 &&
                    out.baseline_seq == 77 && out.entries == part.entries,
                "分片字段往返一致");

    // 守卫：total_parts=0 / part_index 越界 / 条目数越限 / 长度与声明不符
    std::uint8_t bad[2048];
    std::memcpy(bad, buf, len);
    bad[12] = 0;  // total_parts 低字节 → 0
    bad[13] = 0;
    TEST_ASSERT(!decode_snapshot_part(bad, len, out), "total_parts=0 丢");
    std::memcpy(bad, buf, len);
    bad[14] = 9;  // part_index=9 ≥ total_parts=2
    TEST_ASSERT(!decode_snapshot_part(bad, len, out), "part_index 越界丢");
    std::memcpy(bad, buf, len);
    bad[20] = 200;  // entry_count=200 > 128 且与长度不符
    TEST_ASSERT(!decode_snapshot_part(bad, len, out), "条目数越限丢");
    TEST_ASSERT(!decode_snapshot_part(buf, len - 1, out), "截断丢");

    part.entries.assign(kMaxSnapshotPartEntries + 1, make_entry(1));
    TEST_ASSERT(encode_snapshot_part(part, buf, sizeof(buf)) == 0,
                "超上限分片拒编（单包上限守卫在编码侧）");
    return true;
}

// ---- 4. Publisher ----

bool test_publisher_seq_and_snapshot_chunking() {
    std::vector<std::vector<std::uint8_t>> sent;
    bool fail_sends = false;
    DirectoryPublisher publisher([&](const std::uint8_t* data, std::size_t len) {
        if (fail_sends) {
            return false;
        }
        sent.emplace_back(data, data + len);
        return true;
    });

    TEST_ASSERT(publisher.seq() == 0, "初始 seq 0");

    DirectoryDeltaBody fake;
    fake.seq = 999;  // publisher 忽略调用方 seq——内部自增
    PlayerDirectory::Event up;
    up.kind = PlayerDirectory::Event::Kind::SessionUp;
    up.player_id = 8;
    up.anchor_epoch = 2;
    up.zone_id = 5;
    up.binding.session_id = 1008;
    up.binding.gateway_id = 3;
    up.assignment.world_id = 7;
    publisher.publish(up);
    TEST_ASSERT(publisher.seq() == 1 && sent.size() == 1, "Up 事件 seq 自增 1 且发出");

    PlayerDirectory::Event zero;
    zero.kind = PlayerDirectory::Event::Kind::SessionUp;
    zero.player_id = 0;  // 零号玩家拒发
    publisher.publish(zero);
    TEST_ASSERT(publisher.seq() == 1 && sent.size() == 1, "零号玩家不发不占 seq");

    fail_sends = true;
    PlayerDirectory::Event moved;
    moved.kind = PlayerDirectory::Event::Kind::SessionMoved;
    moved.player_id = 8;
    publisher.publish(moved);
    fail_sends = false;
    TEST_ASSERT(publisher.seq() == 2 && sent.size() == 1,
                "发送失败仍推进 seq（fire-and-forget，丢包归快照 healing）");

    // 全量快照分片：300 条 → 3 片（128+128+44）
    std::vector<MirrorEntry> authoritative;
    for (std::uint64_t i = 1; i <= 300; ++i) {
        authoritative.push_back(make_entry(i));
    }
    sent.clear();
    const std::size_t parts = publisher.publish_snapshot(authoritative);
    TEST_ASSERT(parts == 3 && sent.size() == 3, "300 条 → 3 片全发");

    std::uint32_t snapshot_id = 0;
    std::uint32_t baseline = 0;
    std::size_t total_entries = 0;
    for (std::size_t i = 0; i < sent.size(); ++i) {
        DirectorySnapshotPart p;
        TEST_ASSERT(decode_snapshot_part(sent[i].data(), sent[i].size(), p),
                    "分片可解码");
        TEST_ASSERT(p.part_index == i, "part_index 升序 0..2");
        if (snapshot_id == 0) {
            snapshot_id = p.snapshot_id;
            baseline = p.baseline_seq;
        }
        TEST_ASSERT(p.snapshot_id == snapshot_id, "同轮快照同 id");
        TEST_ASSERT(p.baseline_seq == 2, "baseline_seq = 发布时当前 seq");
        total_entries += p.entries.size();
    }
    TEST_ASSERT(total_entries == 300, "分片条目总数守恒");
    TEST_ASSERT(baseline == 2, "baseline 与 seq() 一致");

    // 空表 = 1 空片（镜像清空 + 基线同步）
    sent.clear();
    TEST_ASSERT(publisher.publish_snapshot({}) == 1, "空表发 1 空片");
    DirectorySnapshotPart empty_part;
    TEST_ASSERT(decode_snapshot_part(sent[0].data(), sent[0].size(), empty_part) &&
                    empty_part.entries.empty() && empty_part.total_parts == 1,
                "空片可解码且 total_parts=1");
    return true;
}

// ---- 5. Mirror 全事件类 ----

bool test_mirror_apply_all_kinds() {
    DirectoryMirror mirror;

    DirectoryDeltaBody up;
    up.seq = 1;
    up.event_kind = PlayerDirectory::Event::Kind::SessionUp;
    up.entry = make_entry(8);
    mirror.apply_delta(up);
    TEST_ASSERT(mirror.size() == 1 && mirror.find(8) != nullptr &&
                    mirror.find(8)->session_id == 1008 &&
                    mirror.find(8)->assignment.world_id == 7,
                "Up 写入条目");

    DirectoryDeltaBody moved;
    moved.seq = 2;
    moved.event_kind = PlayerDirectory::Event::Kind::SessionMoved;
    moved.entry = make_entry(8);
    moved.entry.assignment.world_id = 99;
    moved.entry.session_id = 555555;  // Moved 不该动 session 列
    mirror.apply_delta(moved);
    TEST_ASSERT(mirror.find(8) != nullptr && mirror.find(8)->assignment.world_id == 99 &&
                    mirror.find(8)->session_id == 1008,
                "Moved 只动定位列");

    DirectoryDeltaBody down;
    down.seq = 3;
    down.event_kind = PlayerDirectory::Event::Kind::SessionDown;
    down.entry = make_entry(8);
    mirror.apply_delta(down);
    TEST_ASSERT(mirror.find(8) == nullptr && mirror.size() == 0, "Down 删除条目");

    // 顶号流：Kicked erase + 新 Up 写入（新 epoch 新 session）
    DirectoryDeltaBody up_old;
    up_old.seq = 4;
    up_old.event_kind = PlayerDirectory::Event::Kind::SessionUp;
    up_old.entry = make_entry(6, 1);
    mirror.apply_delta(up_old);
    DirectoryDeltaBody kicked;
    kicked.seq = 5;
    kicked.event_kind = PlayerDirectory::Event::Kind::SessionKicked;
    kicked.entry = make_entry(6, 1);
    mirror.apply_delta(kicked);
    TEST_ASSERT(mirror.find(6) == nullptr, "Kicked 删除旧条目");
    DirectoryDeltaBody up_new;
    up_new.seq = 6;
    up_new.event_kind = PlayerDirectory::Event::Kind::SessionUp;
    up_new.entry = make_entry(6, 2);
    up_new.entry.session_id = 2006;
    mirror.apply_delta(up_new);
    TEST_ASSERT(mirror.find(6) != nullptr && mirror.find(6)->anchor_epoch == 2 &&
                    mirror.find(6)->session_id == 2006,
                "顶号后新条目可见");

    TEST_ASSERT(mirror.online_ids().size() == 1 &&
                    mirror.online_ids()[0] == 6,
                "online_ids 只含 Online 态");
    return true;
}

// ---- 6. seq 纪律 ----

bool test_mirror_seq_discipline() {
    DirectoryMirror mirror;
    int requests = 0;
    mirror.set_snapshot_requester([&requests]() { ++requests; });

    DirectoryDeltaBody d;
    d.seq = 10;  // 首包定基线（晚加入投影由快照收敛）
    d.event_kind = PlayerDirectory::Event::Kind::SessionUp;
    d.entry = make_entry(1);
    mirror.apply_delta(d);
    TEST_ASSERT(mirror.last_seq() == 10 && !mirror.stale(), "首包定基线");

    d.seq = 10;  // 重复丢
    mirror.apply_delta(d);
    TEST_ASSERT(mirror.last_seq() == 10 && mirror.size() == 1, "重复 seq 丢");

    d.seq = 8;  // 迟到丢
    mirror.apply_delta(d);
    TEST_ASSERT(mirror.last_seq() == 10, "迟到 seq 丢");

    d.seq = 13;  // 断档 → stale + 请求一次
    mirror.apply_delta(d);
    TEST_ASSERT(mirror.stale() && requests == 1, "断档进 stale 并触发请求钩子");
    mirror.apply_delta(d);
    TEST_ASSERT(requests == 1, "同轮断档不重复请求");
    TEST_ASSERT(mirror.size() == 1, "断档包不落表");

    // 快照到货复位：stale 清 + 请求位清（下轮断档可再请求）
    DirectorySnapshotPart part;
    part.snapshot_id = 1;
    part.total_parts = 1;
    part.part_index = 0;
    part.baseline_seq = 12;
    part.entries = {make_entry(2)};
    mirror.apply_snapshot_part(part);
    TEST_ASSERT(!mirror.stale() && mirror.size() == 1 && mirror.last_seq() == 12,
                "快照到货重置投影与基线");
    mirror.apply_delta(d);  // seq 13 = 12+1 连续，恢复应用
    TEST_ASSERT(!mirror.stale() && mirror.size() == 2 && mirror.last_seq() == 13,
                "快照基线上连续 delta 恢复应用");
    return true;
}

// ---- 7. 快照分片重组 ----

bool test_snapshot_reassembly_and_reset() {
    DirectoryMirror mirror;
    DirectoryDeltaBody up;
    up.seq = 1;
    up.event_kind = PlayerDirectory::Event::Kind::SessionUp;
    up.entry = make_entry(1);
    mirror.apply_delta(up);
    up.seq = 2;
    up.entry = make_entry(2);
    mirror.apply_delta(up);
    TEST_ASSERT(mirror.size() == 2, "快照前两行");

    DirectorySnapshotPart p0;
    p0.snapshot_id = 7;
    p0.total_parts = 2;
    p0.part_index = 0;
    p0.baseline_seq = 9;
    p0.entries = {make_entry(3)};

    DirectorySnapshotPart p1 = p0;
    p1.part_index = 1;
    p1.entries = {make_entry(4)};

    // 乱序：非起始片先到，无处挂靠丢
    mirror.apply_snapshot_part(p1);
    TEST_ASSERT(mirror.size() == 2 && mirror.last_seq() == 2, "乱序非起始片不落表");

    // 起始片到 → 重组中
    mirror.apply_snapshot_part(p0);
    TEST_ASSERT(mirror.size() == 2 && mirror.last_seq() == 2,
                "重组未完成不落表（原子替换）");

    // 收尾片 → 全量重置：旧表清除、基线取 baseline_seq
    mirror.apply_snapshot_part(p1);
    TEST_ASSERT(mirror.size() == 2 && mirror.find(1) == nullptr && mirror.find(2) == nullptr,
                "快照完成全量重置（旧条目清除）");
    TEST_ASSERT(mirror.find(3) != nullptr && mirror.find(4) != nullptr &&
                    mirror.last_seq() == 9,
                "快照条目落表 + 基线同步");

    // 跨 snapshot_id 重启：新一轮 part0 到达即重启重组
    DirectorySnapshotPart q0;
    q0.snapshot_id = 8;
    q0.total_parts = 2;
    q0.part_index = 0;
    q0.baseline_seq = 20;
    q0.entries = {make_entry(5)};
    mirror.apply_snapshot_part(q0);
    DirectorySnapshotPart stale_old = p1;  // 旧快照 part1 迟到（id=7 ≠ 8）丢
    mirror.apply_snapshot_part(stale_old);
    TEST_ASSERT(mirror.find(3) != nullptr, "跨 id 旧片丢（重组重启，未落表）");
    DirectorySnapshotPart q1 = q0;
    q1.part_index = 1;
    q1.entries = {make_entry(6)};
    mirror.apply_snapshot_part(q1);
    TEST_ASSERT(mirror.size() == 2 && mirror.find(5) != nullptr && mirror.find(6) != nullptr &&
                    mirror.last_seq() == 20,
                "新一轮快照完成后重置生效");
    return true;
}

// ---- 8. 端到端（进程内：真目录 → Publisher → Mirror）----

bool test_e2e_directory_to_mirror() {
    std::vector<std::vector<std::uint8_t>> sent;
    DirectoryPublisher publisher([&](const std::uint8_t* data, std::size_t len) {
        sent.emplace_back(data, data + len);
        return true;
    });

    PlayerDirectory directory;
    directory.set_event_sink([&publisher](const PlayerDirectory::Event& event) {
        publisher.publish(event);
    });

    SessionBinding binding;
    binding.session_id = 1001;
    binding.gateway_id = 3;
    WorldAssignment assignment;
    assignment.world_id = 7;
    TEST_ASSERT(directory.session_up(100, binding, assignment, 2) == 1, "目录登记");

    DirectoryMirror mirror;
    for (const auto& packet : sent) {
        mirror.on_datagram(packet.data(), packet.size());
    }
    TEST_ASSERT(mirror.size() == 1 && mirror.find(100) != nullptr &&
                    mirror.find(100)->zone_id == 2 &&
                    mirror.find(100)->assignment.world_id == 7,
                "SessionUp 事件跨发布件到镜像");

    sent.clear();
    WorldAssignment moved_to;
    moved_to.world_id = 9;
    moved_to.map_id = 50;
    TEST_ASSERT(directory.moved(100, moved_to), "目录迁移");
    for (const auto& packet : sent) {
        mirror.on_datagram(packet.data(), packet.size());
    }
    TEST_ASSERT(mirror.find(100) != nullptr && mirror.find(100)->assignment.world_id == 9,
                "SessionMoved 事件到镜像");

    sent.clear();
    TEST_ASSERT(directory.evict_for_relogin(100) == 1, "顶号踢旧");
    TEST_ASSERT(directory.session_up(100, binding, assignment, 2) == 2, "顶号新条目");
    for (const auto& packet : sent) {
        mirror.on_datagram(packet.data(), packet.size());
    }
    TEST_ASSERT(mirror.size() == 1 && mirror.find(100) != nullptr &&
                    mirror.find(100)->anchor_epoch == 2,
                "Kicked+Up 顶号流到镜像（新 epoch 行）");

    sent.clear();
    TEST_ASSERT(directory.session_down(100), "下线");
    for (const auto& packet : sent) {
        mirror.on_datagram(packet.data(), packet.size());
    }
    TEST_ASSERT(mirror.size() == 0, "SessionDown 事件到镜像（条目删除）");

    // 消费侧收到 SnapshotRequest 静默丢（请求面只对 owner 有意义）
    std::uint8_t req[8];
    TEST_ASSERT(encode_snapshot_request(req, sizeof(req)) == 8, "请求编码");
    mirror.on_datagram(req, sizeof(req));
    TEST_ASSERT(mirror.size() == 0, "消费侧请求包静默丢");
    return true;
}

// ---- 9. UDP loopback 全链 ----

namespace {

// 非阻塞轮询收一条（短重试窗——loopback 本机无丢包，给调度余量）
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

bool test_udp_mirror_loopback() {
    // 消费侧先开（owner 发送目标要其真实端口）
    disco::UdpFeed mirror_feed;
    TEST_ASSERT(mirror_feed.open(0, "127.0.0.1"), "消费侧收包口就绪");

    // owner 侧：发 Delta/快照的 Transport + 收 SnapshotRequest 的 Feed
    disco::UdpFeed owner_feed;
    disco::UdpBeaconTransport owner_transport;
    TEST_ASSERT(owner_feed.open(0, "127.0.0.1") && owner_transport.open(),
                "owner 侧 socket 就绪");

    DirectoryPublisher publisher([&](const std::uint8_t* data, std::size_t len) {
        return owner_transport.send_bytes("127.0.0.1", mirror_feed.port(), data, len);
    });

    DirectoryMirror mirror;
    int requests = 0;
    mirror.set_snapshot_requester([&]() {
        std::uint8_t req[8];
        if (encode_snapshot_request(req, sizeof(req)) != 0) {
            disco::UdpBeaconTransport requester;
            if (requester.open()) {
                (void)requester.send_bytes("127.0.0.1", owner_feed.port(), req,
                                           sizeof(req));
            }
        }
        ++requests;
    });

    // 消费侧制造断档 → 触发快照请求 → owner 收请求并应答快照
    DirectoryDeltaBody up;
    up.seq = 5;
    up.event_kind = PlayerDirectory::Event::Kind::SessionUp;
    up.entry = make_entry(1);
    mirror.apply_delta(up);
    DirectoryDeltaBody gap;
    gap.seq = 9;  // 断档
    gap.event_kind = PlayerDirectory::Event::Kind::SessionUp;
    gap.entry = make_entry(2);
    mirror.apply_delta(gap);
    TEST_ASSERT(mirror.stale() && requests == 1, "消费侧断档并发出快照请求");

    std::uint8_t buf[disco::kMaxDatagramSize];
    std::size_t len = 0;
    TEST_ASSERT(poll_receive(owner_feed, buf, sizeof(buf), len), "owner 收到请求");
    TEST_ASSERT(is_snapshot_request(buf, len), "请求报文判别");

    // owner 应答：全量快照（1 玩家 1 片）经真 UDP 到消费侧
    const std::vector<MirrorEntry> authoritative = {make_entry(1), make_entry(2)};
    TEST_ASSERT(publisher.publish_snapshot(authoritative) == 1, "owner 发快照 1 片");
    TEST_ASSERT(poll_receive(mirror_feed, buf, sizeof(buf), len), "消费侧收到快照片");
    mirror.on_datagram(buf, len);
    TEST_ASSERT(!mirror.stale() && mirror.size() == 2, "快照经 UDP 到货重置投影");
    TEST_ASSERT(mirror.last_seq() == publisher.seq(),
                "基线同步 = owner 发布序（本例未发过 delta，基线 0）");

    // delta 经真 UDP：连续 seq 恢复应用
    DirectoryDeltaBody next;
    next.seq = mirror.last_seq() + 1;
    next.event_kind = PlayerDirectory::Event::Kind::SessionMoved;
    next.entry = make_entry(1);
    next.entry.assignment.world_id = 42;
    std::uint8_t delta_buf[86];
    TEST_ASSERT(encode_delta(next, delta_buf, sizeof(delta_buf)) == 86, "delta 编码");
    TEST_ASSERT(owner_transport.send_bytes("127.0.0.1", mirror_feed.port(),
                                          delta_buf, 86),
                "owner 发 delta");
    TEST_ASSERT(poll_receive(mirror_feed, buf, sizeof(buf), len), "消费侧收到 delta");
    mirror.on_datagram(buf, len);
    TEST_ASSERT(mirror.find(1) != nullptr && mirror.find(1)->assignment.world_id == 42 &&
                    !mirror.stale(),
                "delta 经 UDP 恢复续传");
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
        {"delta_wire_roundtrip_and_guards", test_delta_wire_roundtrip_and_guards},
        {"snapshot_request_wire", test_snapshot_request_wire},
        {"snapshot_part_roundtrip_and_guards", test_snapshot_part_roundtrip_and_guards},
        {"publisher_seq_and_snapshot_chunking", test_publisher_seq_and_snapshot_chunking},
        {"mirror_apply_all_kinds", test_mirror_apply_all_kinds},
        {"mirror_seq_discipline", test_mirror_seq_discipline},
        {"snapshot_reassembly_and_reset", test_snapshot_reassembly_and_reset},
        {"e2e_directory_to_mirror", test_e2e_directory_to_mirror},
        {"udp_mirror_loopback", test_udp_mirror_loopback},
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

    std::cout << "DirectoryMirrorTests: " << passed << " passed, " << failed << " failed"
              << std::endl;
    return failed == 0 ? 0 : 1;
}
