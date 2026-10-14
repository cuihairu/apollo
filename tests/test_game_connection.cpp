// GameConnection 门面测试（net-abstraction.md §2/§3/§4 L2+L3）。
//
// 覆盖：
//   - 会话状态映射（kHandshaking→kActive→kClosed；kResuming 恢复窗口）
//   - 四通道队列（§4.1）：movement 只发最新 / attributes trim 分级 /
//     events 满拒 / control 独立预算
//   - 三返回码（§2）：kAccepted / kTrimmedLow / kBackPressured
//   - 场景线程投递（§3 线程模型）：pump 只入队，drain_inbound 执行
//   - Resuming（§2/§3）：missing_seq 范围 / TTL 过期 / 对端重启不恢复
//   - 背压（§4.2）：环水位 ok/soft/hard/cut 行为表
//   - 断连
//
// 用 loopback 传输对：A 发 B 收，验证门面会话语义。

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "apollo/net/gameconnection/gameconnection.hpp"
#include "apollo/net/link/dispatcher.hpp"
#include "apollo/net/link/interserver_link.hpp"
#include "apollo/net/link/route_table.hpp"
#include "apollo/net/link/transport.hpp"
#include "apollo/net/link/watermark.hpp"

using namespace apollo::net::gameconnection;
using apollo::net::link::Dispatcher;
using apollo::net::link::InterServerLink;
using apollo::net::link::LinkConfig;
using apollo::net::link::LinkDownReason;
using apollo::net::link::make_loopback_pair;
using apollo::net::link::RouteTable;
using apollo::net::link::SendClass;

namespace {

LinkConfig make_config(const char* name, std::uint64_t route_version,
                       std::uint64_t epoch = 1) {
    LinkConfig cfg;
    cfg.self_name = name;
    cfg.route_version = route_version;
    cfg.authority_epoch = epoch;
    return cfg;
}

RouteTable make_table() {
    RouteTable t;
    t.add({901, "test.echo", "P2P", "control", "internal", "native"});
    t.add({902, "test.drop", "P2P", "control", "internal", "native"});
    return t;
}

// 收端记录器（handler 由 owning 线程 drain_inbound() 执行）
struct RxLog {
    std::vector<std::string> got;
    Dispatcher::Handler handler() {
        return [this](std::uint16_t id, const std::uint8_t* data, std::size_t size) {
            std::string s(reinterpret_cast<const char*>(data), size);
            got.push_back(std::to_string(id) + ":" + s);
        };
    }
};

// 握手完成 + 双泵
bool up_both(GameConnection& a, GameConnection& b, std::uint64_t start) {
    for (std::uint64_t t = start; t < start + 200; t += 10) {
        a.pump(t);
        b.pump(t);
    }
    return a.state() == SessionState::kActive && b.state() == SessionState::kActive;
}

// 双泵 N 轮 + 双端投递（场景线程投递语义：handler 在 drain 中执行）
void pump_rounds(GameConnection& a, GameConnection& b, std::uint64_t start, int rounds) {
    for (int i = 0; i < rounds; ++i) {
        a.pump(start + i);
        b.pump(start + i);
    }
    a.drain_inbound();
    b.drain_inbound();
}

}  // namespace

// ── 会话状态 ──────────────────────────────────────────────

TEST(GameConnection, InitialStateClosed) {
    GameConnection a(make_config("a", 1));
    EXPECT_EQ(a.state(), SessionState::kClosed);
}

TEST(GameConnection, HandshakeToActive) {
    auto [ep_a, ep_b] = make_loopback_pair();
    GameConnection a(make_config("a", 1));
    GameConnection b(make_config("b", 1));

    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);

    EXPECT_EQ(a.state(), SessionState::kHandshaking);
    EXPECT_EQ(b.state(), SessionState::kHandshaking);

    EXPECT_TRUE(up_both(a, b, 1100));
    EXPECT_EQ(a.state(), SessionState::kActive);
    EXPECT_EQ(b.state(), SessionState::kActive);
}

TEST(GameConnection, CloseTransitionsToClosed) {
    auto [ep_a, ep_b] = make_loopback_pair();
    GameConnection a(make_config("a", 1));
    GameConnection b(make_config("b", 1));
    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    EXPECT_TRUE(up_both(a, b, 1100));

    a.close("test");
    EXPECT_EQ(a.state(), SessionState::kClosed);
}

// ── 发送与订阅 ────────────────────────────────────────────

TEST(GameConnection, SendReliableDispatchesToSubscriber) {
    auto table = make_table();
    auto [ep_a, ep_b] = make_loopback_pair();
    GameConnection a(make_config("a", 1));
    GameConnection b(make_config("b", 1));
    a.set_route_table(&table);
    b.set_route_table(&table);
    b.allow_domain("internal");

    RxLog rx;
    b.subscribe(901, rx.handler());

    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    EXPECT_TRUE(up_both(a, b, 1100));

    const std::vector<std::uint8_t> body = {'h', 'i'};
    auto rc = a.send(901, body, Channel::kAttributes, Priority::kNormal);
    EXPECT_EQ(rc, SendCode::kAccepted);

    pump_rounds(a, b, 2000, 3);
    ASSERT_EQ(rx.got.size(), 1u);
    EXPECT_EQ(rx.got[0], "901:hi");
}

TEST(GameConnection, SendDroppableDispatches) {
    auto table = make_table();
    auto [ep_a, ep_b] = make_loopback_pair();
    GameConnection a(make_config("a", 1));
    GameConnection b(make_config("b", 1));
    a.set_route_table(&table);
    b.set_route_table(&table);
    b.allow_domain("internal");

    RxLog rx;
    b.subscribe(902, rx.handler());

    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    EXPECT_TRUE(up_both(a, b, 1100));

    const std::vector<std::uint8_t> body = {'d', 'r'};
    auto rc = a.send(902, body, Channel::kMovement, Priority::kLow);
    EXPECT_EQ(rc, SendCode::kAccepted);

    pump_rounds(a, b, 2000, 3);
    ASSERT_EQ(rx.got.size(), 1u);
    EXPECT_EQ(rx.got[0], "902:dr");
}

TEST(GameConnection, DomainGateRejectsForbidden) {
    auto table = make_table();
    auto [ep_a, ep_b] = make_loopback_pair();
    GameConnection a(make_config("a", 1));
    GameConnection b(make_config("b", 1));
    a.set_route_table(&table);
    b.set_route_table(&table);
    // b 只允许 internal 域；901 在 internal，但若发 client 域 id 应拒
    b.allow_domain("internal");

    RxLog rx;
    b.subscribe(901, rx.handler());

    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    EXPECT_TRUE(up_both(a, b, 1100));

    const std::vector<std::uint8_t> body = {'x'};
    // 901 是 internal 域 → 通过
    a.send(901, body, Channel::kAttributes, Priority::kNormal);
    pump_rounds(a, b, 2000, 3);
    EXPECT_EQ(rx.got.size(), 1u);

    // 5 是 client 域 id（表里有 client 域定义但 b 只允许 internal）
    a.send(5, body, Channel::kAttributes, Priority::kNormal);
    pump_rounds(a, b, 3000, 3);
    EXPECT_EQ(rx.got.size(), 1u);  // 未新增
    EXPECT_EQ(b.stats().dispatch_rejected, 1u);  // 域越权在收端计数
}

TEST(GameConnection, SendPointerOverload) {
    auto table = make_table();
    auto [ep_a, ep_b] = make_loopback_pair();
    GameConnection a(make_config("a", 1));
    GameConnection b(make_config("b", 1));
    a.set_route_table(&table);
    b.set_route_table(&table);
    b.allow_domain("internal");

    RxLog rx;
    b.subscribe(901, rx.handler());

    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    EXPECT_TRUE(up_both(a, b, 1100));

    const std::uint8_t body[] = {'p', 'o', 'i', 'n', 't'};
    auto rc = a.send(901, body, sizeof(body), Channel::kAttributes, Priority::kNormal);
    EXPECT_EQ(rc, SendCode::kAccepted);

    pump_rounds(a, b, 2000, 3);
    ASSERT_EQ(rx.got.size(), 1u);
    EXPECT_EQ(rx.got[0], "901:point");
}

// ── 断连排队与重连续发 ────────────────────────────────────

// 未挂传输（kIdle）：四通道均受理入环；movement 留环（链路拒易弃），
// attributes 转入链路可靠队列（断连排队）；重连后一并送达。
TEST(GameConnection, SendQueuedAtIdleFlushesAfterUp) {
    auto table = make_table();
    GameConnection a(make_config("a", 1));
    GameConnection b(make_config("b", 1));
    a.set_route_table(&table);
    b.set_route_table(&table);
    b.allow_domain("internal");

    RxLog rx;
    b.subscribe(901, rx.handler());
    b.subscribe(902, rx.handler());

    const std::vector<std::uint8_t> body = {'x'};
    EXPECT_EQ(a.send(901, body, Channel::kAttributes, Priority::kNormal),
              SendCode::kAccepted);
    EXPECT_EQ(a.send(902, body, Channel::kMovement, Priority::kNormal),
              SendCode::kAccepted);
    a.pump(100);  // kIdle → false，但环/队列保持
    // attributes 已转入链路可靠队列（断连排队）；movement 留环
    EXPECT_EQ(a.channel_bytes(Channel::kAttributes), 0u);
    EXPECT_GT(a.channel_bytes(Channel::kMovement), 0u);
    EXPECT_EQ(a.link().reliable_pending(), 1u);

    auto [ep_a, ep_b] = make_loopback_pair();
    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    EXPECT_TRUE(up_both(a, b, 1100));

    pump_rounds(a, b, 2000, 3);
    ASSERT_EQ(rx.got.size(), 2u);
    EXPECT_EQ(rx.got[0], "901:x");
    EXPECT_EQ(rx.got[1], "902:x");
}

// ── 四通道队列（§4.1） ────────────────────────────────────

// movement 同 id 只发最新：覆盖队内未发送旧帧，最新值必达。
TEST(GameConnection, MovementLatestWins) {
    auto table = make_table();
    auto [ep_a, ep_b] = make_loopback_pair();
    GameConnection a(make_config("a", 1));
    GameConnection b(make_config("b", 1));
    a.set_route_table(&table);
    b.set_route_table(&table);
    b.allow_domain("internal");

    RxLog rx;
    b.subscribe(902, rx.handler());

    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    EXPECT_TRUE(up_both(a, b, 1100));

    const std::vector<std::uint8_t> v1 = {'v', '1'};
    const std::vector<std::uint8_t> v2 = {'v', '2'};
    // 两次发送之间不泵：同 id 旧帧在环内被覆盖
    EXPECT_EQ(a.send(902, v1, Channel::kMovement, Priority::kNormal),
              SendCode::kAccepted);
    EXPECT_EQ(a.send(902, v2, Channel::kMovement, Priority::kNormal),
              SendCode::kAccepted);
    EXPECT_EQ(a.session_stats().movement_overwritten, 1u);

    pump_rounds(a, b, 2000, 3);
    ASSERT_EQ(rx.got.size(), 1u);
    EXPECT_EQ(rx.got[0], "902:v2");
}

// attributes 环满腾位：丢最旧 LOW 帧，返回 kTrimmedLow，计数上报。
TEST(GameConnection, AttributesTrimLowReturnsTrimmed) {
    auto table = make_table();
    SessionConfig sc;
    sc.attributes_capacity_bytes = 120;  // 5 帧（24B/帧）
    auto [ep_a, ep_b] = make_loopback_pair();
    GameConnection a(make_config("a", 1), sc);
    GameConnection b(make_config("b", 1));
    a.set_route_table(&table);
    b.set_route_table(&table);
    b.allow_domain("internal");

    RxLog rx;
    b.subscribe(901, rx.handler());

    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    EXPECT_TRUE(up_both(a, b, 1100));

    const std::vector<std::uint8_t> small = {'n'};
    const std::vector<std::uint8_t> big(40, 'B');  // wire 62B
    EXPECT_EQ(a.send(901, small, Channel::kAttributes, Priority::kNormal),
              SendCode::kAccepted);
    EXPECT_EQ(a.send(901, small, Channel::kAttributes, Priority::kNormal),
              SendCode::kAccepted);
    EXPECT_EQ(a.send(901, small, Channel::kAttributes, Priority::kLow),
              SendCode::kAccepted);
    EXPECT_EQ(a.send(901, small, Channel::kAttributes, Priority::kLow),
              SendCode::kAccepted);
    // 96 + 62 > 120 → 连丢两个 LOW 腾位
    EXPECT_EQ(a.send(901, big, Channel::kAttributes, Priority::kLow),
              SendCode::kTrimmedLow);
    EXPECT_EQ(a.session_stats().trimmed_low_dropped, 2u);

    pump_rounds(a, b, 2000, 3);
    ASSERT_EQ(rx.got.size(), 3u);
    EXPECT_EQ(rx.got[0], "901:n");    // 两个 NORM 保全
    EXPECT_EQ(rx.got[1], "901:n");
    EXPECT_EQ(rx.got[2], "901:" + std::string(40, 'B'));
}

// attributes 水位分级（§4.2）：hard 起 NORM 拒收、HIGH 放行；cut 全冻结。
TEST(GameConnection, AttributesHardRejectsNormalAdmitsHigh) {
    auto table = make_table();
    SessionConfig sc;
    sc.attributes_capacity_bytes = 96;  // 4 帧（24B/帧）
    auto [ep_a, ep_b] = make_loopback_pair();
    GameConnection a(make_config("a", 1), sc);
    GameConnection b(make_config("b", 1));
    a.set_route_table(&table);
    b.set_route_table(&table);
    b.allow_domain("internal");

    RxLog rx;
    b.subscribe(901, rx.handler());

    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    EXPECT_TRUE(up_both(a, b, 1100));

    const std::vector<std::uint8_t> body = {'x', 'x'};
    SendCode rc = SendCode::kAccepted;
    for (int i = 0; i < 3; ++i) {  // 0/24/48 占用 → ok/ok/soft，均受理
        rc = a.send(901, body, Channel::kAttributes, Priority::kNormal);
        EXPECT_EQ(rc, SendCode::kAccepted);
    }
    EXPECT_EQ(a.send(901, body, Channel::kAttributes, Priority::kNormal),
              SendCode::kBackPressured);  // 72/96 = hard → NORM 拒
    EXPECT_EQ(a.send(901, body, Channel::kAttributes, Priority::kHigh),
              SendCode::kAccepted);       // hard 放行 HIGH → 96（cut）
    EXPECT_EQ(a.send(901, body, Channel::kAttributes, Priority::kNormal),
              SendCode::kBackPressured);  // cut 冻结
    EXPECT_EQ(a.channel_bytes(Channel::kAttributes), 96u);
}

// events（可靠不丢）：环满即拒；cut 冻结——无 trim 资格。
TEST(GameConnection, EventsRejectOnOverflow) {
    auto table = make_table();
    SessionConfig sc;
    sc.events_capacity_bytes = 40;  // 1 帧
    auto [ep_a, ep_b] = make_loopback_pair();
    GameConnection a(make_config("a", 1), sc);
    GameConnection b(make_config("b", 1));
    a.set_route_table(&table);
    b.set_route_table(&table);
    b.allow_domain("internal");

    RxLog rx;
    b.subscribe(902, rx.handler());

    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    EXPECT_TRUE(up_both(a, b, 1100));

    const std::vector<std::uint8_t> body = {'e'};
    EXPECT_EQ(a.send(902, body, Channel::kEvents, Priority::kHigh),
              SendCode::kAccepted);
    EXPECT_EQ(a.send(902, body, Channel::kEvents, Priority::kHigh),
              SendCode::kBackPressured);  // 满即拒（不 trim）
    pump_rounds(a, b, 2000, 3);
    ASSERT_EQ(rx.got.size(), 1u);
    // 排空后恢复受理
    EXPECT_EQ(a.send(902, body, Channel::kEvents, Priority::kHigh),
              SendCode::kAccepted);
}

// control 独立小预算：预算内放行、满即拒；attributes cut 时仍可用。
TEST(GameConnection, ControlOwnBudgetAndNeverFrozen) {
    auto table = make_table();
    SessionConfig sc;
    sc.attributes_capacity_bytes = 96;  // 4 帧
    sc.control_capacity_bytes = 40;     // 1 帧
    auto [ep_a, ep_b] = make_loopback_pair();
    GameConnection a(make_config("a", 1), sc);
    GameConnection b(make_config("b", 1));
    a.set_route_table(&table);
    b.set_route_table(&table);
    b.allow_domain("internal");

    RxLog rx;
    b.subscribe(901, rx.handler());
    b.subscribe(902, rx.handler());

    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    EXPECT_TRUE(up_both(a, b, 1100));

    const std::vector<std::uint8_t> body = {'c'};
    // attributes 打满至 cut
    for (int i = 0; i < 4; ++i) {
        const std::vector<std::uint8_t> pad(2, 'x');
        EXPECT_EQ(a.send(901, pad, Channel::kAttributes, Priority::kHigh),
                  SendCode::kAccepted);
    }
    EXPECT_EQ(a.send(901, body, Channel::kAttributes, Priority::kNormal),
              SendCode::kBackPressured);  // attributes cut
    // control 独立预算：不受 attributes cut 牵连
    EXPECT_EQ(a.send(902, body, Channel::kControl, Priority::kHigh),
              SendCode::kAccepted);
    EXPECT_EQ(a.send(902, body, Channel::kControl, Priority::kHigh),
              SendCode::kBackPressured);  // 自身预算满
    pump_rounds(a, b, 2000, 3);
    ASSERT_EQ(rx.got.size(), 5u);  // 4×attributes pad + 1×control
}

// ── 场景线程投递（§3 线程模型） ───────────────────────────

TEST(GameConnection, SceneThreadDeliveryDefersHandler) {
    auto table = make_table();
    auto [ep_a, ep_b] = make_loopback_pair();
    GameConnection a(make_config("a", 1));
    GameConnection b(make_config("b", 1));
    a.set_route_table(&table);
    b.set_route_table(&table);
    b.allow_domain("internal");

    RxLog rx;
    b.subscribe(901, rx.handler());

    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    EXPECT_TRUE(up_both(a, b, 1100));

    const std::vector<std::uint8_t> body = {'d', 'e', 'f'};
    EXPECT_EQ(a.send(901, body, Channel::kAttributes, Priority::kNormal),
              SendCode::kAccepted);
    // 只泵不 drain：帧已到收端但 handler 未执行（场景线程投递）
    for (int i = 0; i < 3; ++i) {
        a.pump(2000 + i);
        b.pump(2000 + i);
    }
    EXPECT_TRUE(rx.got.empty());

    b.drain_inbound();
    ASSERT_EQ(rx.got.size(), 1u);
    EXPECT_EQ(rx.got[0], "901:def");
}

// 投递队列深度：溢出丢最旧（§4.3 上行限流归 IO 线程，此处为兜底闸）。
TEST(GameConnection, InboundQueueDropsOldest) {
    auto table = make_table();
    SessionConfig sc;
    sc.inbound_queue_depth = 2;
    auto [ep_a, ep_b] = make_loopback_pair();
    GameConnection a(make_config("a", 1));
    GameConnection b(make_config("b", 1), sc);
    a.set_route_table(&table);
    b.set_route_table(&table);
    b.allow_domain("internal");

    RxLog rx;
    b.subscribe(901, rx.handler());

    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    EXPECT_TRUE(up_both(a, b, 1100));

    for (std::uint8_t c = '1'; c <= '3'; ++c) {
        const std::vector<std::uint8_t> body = {c};
        a.send(901, body, Channel::kAttributes, Priority::kNormal);
    }
    pump_rounds(a, b, 2000, 3);  // b 单泵读入 3 帧 → 队列深度 2 丢最旧
    ASSERT_EQ(rx.got.size(), 2u);
    EXPECT_EQ(rx.got[0], "901:2");
    EXPECT_EQ(rx.got[1], "901:3");
    EXPECT_EQ(b.session_stats().inbound_dropped, 1u);
}

// ── Resuming（§2/§3） ─────────────────────────────────────

// 同 epoch 重连：kResuming 窗口 → on_resume 报 missing_seq 范围 → kActive；
// 链路续发未确认帧恰一次。
TEST(GameConnection, ResumeCallbackWithMissingRange) {
    auto table = make_table();
    auto [ep_a, ep_b] = make_loopback_pair();
    GameConnection a(make_config("a", 1));
    GameConnection b(make_config("b", 1));
    a.set_route_table(&table);
    b.set_route_table(&table);
    b.allow_domain("internal");

    bool resume_fired = false;
    ResumeInfo got;
    SessionState seen_in_callback = SessionState::kClosed;
    a.set_session_sink(SessionSink{
        nullptr,
        [&](const ResumeInfo& r) {
            resume_fired = true;
            got = r;
            seen_in_callback = a.state();
        },
        nullptr,
    });

    RxLog rx;
    b.subscribe(901, rx.handler());

    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    EXPECT_TRUE(up_both(a, b, 1100));

    // 发可靠帧并只泵 a（b 不泵 → 无 ack → missing 区间非空）
    const std::vector<std::uint8_t> body = {'m'};
    EXPECT_EQ(a.send(901, body, Channel::kAttributes, Priority::kNormal),
              SendCode::kAccepted);
    a.pump(1300);
    a.pump(1301);
    EXPECT_EQ(a.link().last_sent_seq(), 1u);
    EXPECT_EQ(a.link().acked_seq(), 0u);

    // 对端断连 → a 检出，记 resume 候选
    b.close("restart");
    pump_rounds(a, b, 2000, 3);
    EXPECT_EQ(a.state(), SessionState::kClosed);

    // 同 epoch 重连 → 恢复
    auto [ep2_a, ep2_b] = make_loopback_pair();
    a.attach(std::move(ep2_a), 3000);
    b.attach(std::move(ep2_b), 3000);
    EXPECT_TRUE(up_both(a, b, 3100));
    EXPECT_TRUE(resume_fired);
    EXPECT_TRUE(got.resumed);
    EXPECT_EQ(got.first_missing_seq, 1u);
    EXPECT_EQ(got.last_missing_seq, 1u);
    EXPECT_EQ(seen_in_callback, SessionState::kResuming);
    EXPECT_EQ(a.session_stats().resumes, 1u);
    EXPECT_EQ(a.state(), SessionState::kActive);

    // 链路续发：未确认帧恰一次到达
    pump_rounds(a, b, 4000, 3);
    ASSERT_EQ(rx.got.size(), 1u);
    EXPECT_EQ(rx.got[0], "901:m");
}

// resume 窗口过期：同 epoch 但超 TTL → 全新会话，不回调。
TEST(GameConnection, ResumeTtlExpiryFreshSession) {
    auto table = make_table();
    SessionConfig sc;
    sc.resume_ttl_ms = 500;
    auto [ep_a, ep_b] = make_loopback_pair();
    GameConnection a(make_config("a", 1), sc);
    GameConnection b(make_config("b", 1));
    a.set_route_table(&table);
    b.set_route_table(&table);
    b.allow_domain("internal");

    bool resume_fired = false;
    a.set_session_sink(SessionSink{
        nullptr,
        [&](const ResumeInfo&) { resume_fired = true; },
        nullptr,
    });

    RxLog rx;
    b.subscribe(901, rx.handler());

    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    EXPECT_TRUE(up_both(a, b, 1100));

    const std::vector<std::uint8_t> body = {'m'};
    EXPECT_EQ(a.send(901, body, Channel::kAttributes, Priority::kNormal),
              SendCode::kAccepted);
    a.pump(1300);
    b.close("restart");
    pump_rounds(a, b, 2000, 3);

    // 重连时刻距断线 1000ms > TTL 500ms → 全新会话
    auto [ep2_a, ep2_b] = make_loopback_pair();
    a.attach(std::move(ep2_a), 3000);
    b.attach(std::move(ep2_b), 3000);
    EXPECT_TRUE(up_both(a, b, 3100));
    EXPECT_FALSE(resume_fired);
    EXPECT_EQ(a.session_stats().resumes, 0u);
    EXPECT_EQ(a.state(), SessionState::kActive);
}

// 对端重启（epoch 变）：不恢复——missing 范围与旧代 seq 一起作废。
TEST(GameConnection, PeerRestartNotResume) {
    auto table = make_table();
    auto [ep_a, ep_b] = make_loopback_pair();
    GameConnection a(make_config("a", 1));
    GameConnection b(make_config("b", 1));
    a.set_route_table(&table);
    b.set_route_table(&table);
    b.allow_domain("internal");

    bool resume_fired = false;
    a.set_session_sink(SessionSink{
        nullptr,
        [&](const ResumeInfo&) { resume_fired = true; },
        nullptr,
    });

    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    EXPECT_TRUE(up_both(a, b, 1100));

    const std::vector<std::uint8_t> body = {'m'};
    EXPECT_EQ(a.send(901, body, Channel::kAttributes, Priority::kNormal),
              SendCode::kAccepted);
    a.pump(1300);
    b.close("restart");
    pump_rounds(a, b, 2000, 3);

    // 新一代对端（epoch 2）→ 无恢复；旧代未确认帧随可靠队列清除
    auto [ep2_a, ep2_b] = make_loopback_pair();
    GameConnection b2(make_config("b2", 1, /*epoch=*/2));
    b2.set_route_table(&table);
    b2.allow_domain("internal");
    a.attach(std::move(ep2_a), 3000);
    b2.attach(std::move(ep2_b), 3000);
    EXPECT_TRUE(up_both(a, b2, 3100));
    EXPECT_FALSE(resume_fired);
    EXPECT_EQ(a.session_stats().resumes, 0u);
    EXPECT_EQ(a.link().peer_epoch(), 2u);
}

// ── 背压水位（§4.2，环判据） ──────────────────────────────

TEST(GameConnection, WatermarkBackPressure) {
    auto table = make_table();
    SessionConfig sc;
    sc.attributes_capacity_bytes = 96;  // 4 帧（24B/帧）
    auto [ep_a, ep_b] = make_loopback_pair();
    GameConnection a(make_config("a", 1), sc);
    GameConnection b(make_config("b", 1));
    a.set_route_table(&table);
    b.set_route_table(&table);
    b.allow_domain("internal");

    RxLog rx;
    b.subscribe(901, rx.handler());

    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    EXPECT_TRUE(up_both(a, b, 1100));

    // 持续发送不泵：ok/soft 受理 → hard 拒 NORM → cut 全冻
    const std::vector<std::uint8_t> body(2, 'x');
    SendCode rc = SendCode::kAccepted;
    for (int i = 0; i < 50; ++i) {
        rc = a.send(901, body, Channel::kAttributes, Priority::kNormal);
        if (rc != SendCode::kAccepted) {
            break;
        }
    }
    EXPECT_EQ(rc, SendCode::kBackPressured);
    EXPECT_GT(a.channel_bytes(Channel::kAttributes), 0u);
}

// ── 对端断连检出 ──────────────────────────────────────────

TEST(GameConnection, PeerDownDetectedOnClose) {
    auto [ep_a, ep_b] = make_loopback_pair();
    GameConnection a(make_config("a", 1));
    GameConnection b(make_config("b", 1));

    std::vector<LinkDownReason> a_down_reasons;
    a.set_session_sink(SessionSink{
        nullptr,
        nullptr,
        [&](LinkDownReason r) { a_down_reasons.push_back(r); },
    });

    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    EXPECT_TRUE(up_both(a, b, 1100));

    // b 主动断连 → a 经 EOF 检出
    b.close("peer restart");
    pump_rounds(a, b, 2000, 3);
    EXPECT_EQ(a.state(), SessionState::kClosed);
    EXPECT_EQ(a_down_reasons.size(), 1u);
}
