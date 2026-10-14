// GameConnection 门面测试（net-abstraction.md §2/§3 L3）。
//
// 覆盖：
//   - 会话状态映射（kHandshaking→kActive→kClosed）
//   - 发送通道映射（kReliable/kDroppable）
//   - 订阅分发（loopback 对端收到）
//   - 背压拒收映射
//   - 断连
//
// 用 loopback 传输对：A 发 B 收，验证门面透传链路语义。

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
using apollo::net::link::LinkEventSink;
using apollo::net::link::LinkState;
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

// 收端记录器（handler 在 pump 线程调用）
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

// 双泵 N 轮
void pump_rounds(GameConnection& a, GameConnection& b, std::uint64_t start, int rounds) {
    for (int i = 0; i < rounds; ++i) {
        a.pump(start + i);
        b.pump(start + i);
    }
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
    auto rc = a.send(901, body, Channel::kReliable, Priority::kNormal);
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
    auto rc = a.send(902, body, Channel::kDroppable, Priority::kLow);
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
    a.send(901, body, Channel::kReliable, Priority::kNormal);
    pump_rounds(a, b, 2000, 3);
    EXPECT_EQ(rx.got.size(), 1u);

    // 5 是 client 域 id（表里有 client 域定义但 b 只允许 internal）
    a.send(5, body, Channel::kReliable, Priority::kNormal);
    pump_rounds(a, b, 3000, 3);
    EXPECT_EQ(rx.got.size(), 1u);  // 未新增
    EXPECT_EQ(b.stats().dispatch_rejected, 1u);  // 域越权在收端计数
}

// ── 发送码映射 ────────────────────────────────────────────

TEST(GameConnection, SendBackPressuredWhenNoTransport) {
    GameConnection a(make_config("a", 1));
    // 未 attach 传输（kIdle）→ Droppable 拒收
    const std::vector<std::uint8_t> body = {'x'};
    auto rc = a.send(901, body, Channel::kDroppable, Priority::kNormal);
    EXPECT_EQ(rc, SendCode::kBackPressured);

    // Reliable 在 kIdle 受理（断连排队语义）
    rc = a.send(901, body, Channel::kReliable, Priority::kNormal);
    EXPECT_EQ(rc, SendCode::kAccepted);
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
    auto rc = a.send(901, body, sizeof(body), Channel::kReliable, Priority::kNormal);
    EXPECT_EQ(rc, SendCode::kAccepted);

    pump_rounds(a, b, 2000, 3);
    ASSERT_EQ(rx.got.size(), 1u);
    EXPECT_EQ(rx.got[0], "901:point");
}

// ── 背压水位 ──────────────────────────────────────────────

TEST(GameConnection, WatermarkBackPressure) {
    auto table = make_table();
    // 极小容量触发背压
    LinkConfig cfg_a = make_config("a", 1);
    cfg_a.watermark.capacity_bytes = 32;  // 极小
    cfg_a.watermark.soft_permille = 100;
    cfg_a.watermark.hard_permille = 200;
    cfg_a.watermark.cut_permille = 400;

    auto [ep_a, ep_b] = make_loopback_pair();
    GameConnection a(cfg_a);
    GameConnection b(make_config("b", 1));
    a.set_route_table(&table);
    b.set_route_table(&table);
    b.allow_domain("internal");

    RxLog rx;
    b.subscribe(901, rx.handler());

    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    EXPECT_TRUE(up_both(a, b, 1100));

    // 持续发送不泵，直到队列满
    auto rc = SendCode::kAccepted;
    for (int i = 0; i < 50; ++i) {
        const std::vector<std::uint8_t> body(64, 'x');
        rc = a.send(901, body, Channel::kReliable, Priority::kNormal);
    }
    // 最终应拒收
    EXPECT_EQ(rc, SendCode::kBackPressured);
    EXPECT_GT(a.stats().send_rejected, 0u);
}

// ── 对端重启检测 ──────────────────────────────────────────

TEST(GameConnection, PeerDownDetectedOnClose) {
    auto [ep_a, ep_b] = make_loopback_pair();
    GameConnection a(make_config("a", 1));
    GameConnection b(make_config("b", 1));

    std::vector<apollo::net::link::LinkDownReason> a_down_reasons;
    a.link().set_sink(apollo::net::link::LinkEventSink{
        nullptr,
        [&](apollo::net::link::LinkDownReason r) { a_down_reasons.push_back(r); },
        nullptr,
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
