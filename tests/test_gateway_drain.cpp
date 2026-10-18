// gateway 排空真接线测试（P4 排空批；契约 954/955，localhost 实 socket）。
//
// 覆盖（session-and-online-directory §2、gateway-topology-survey §2 语义）：
//   1. 排空生命周期：begin 关监听 → 新连接拒 / 既有连接照常服务（PING→
//      PONG）；连接自然离开 → drained 终态（自然排空 detail）；终态后
//      监听仍关；
//   2. 期限强停：deadline 到仍在线 → 全量收线落 drained（强停结果 detail），
//      客户端见 EOF；
//   3. 受理面口径：未知 op 显式拒（不静默）；status 快照按 request_id
//      关联。
//
// Windows：link TCP 底座不进 M1 批（POSIX only）——TCP 用例本体空转。

#include "gateway/gateway_server.hpp"

#include <chrono>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifndef APOLLO_GATEWAY_CONTRACT_ROUTE_JSON
#error "APOLLO_GATEWAY_CONTRACT_ROUTE_JSON not defined"
#endif

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <fstream>
#include <sstream>

#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/frame.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/wire/contract_wire.hpp"
#include "apollo_contract.h"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"
#endif

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

namespace {

#if !defined(_WIN32)

// 阻塞式测试客户端（同 test_gateway_ingress 独立实现口径）。
class TestClient {
public:
    ~TestClient() { close(); }

    bool connect(const std::string& host, std::uint16_t port) {
        fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ < 0) {
            return false;
        }
        timeval tv{};
        tv.tv_sec = 10;  // 单收上限：CI 挂死守门
        ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        ::sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1 ||
            ::connect(fd_, reinterpret_cast<::sockaddr*>(&addr), sizeof addr) != 0) {
            ::close(fd_);
            fd_ = -1;
            return false;
        }
        return true;
    }

    void sendAll(const std::vector<std::uint8_t>& bytes) {
        std::size_t off = 0;
        while (off < bytes.size()) {
            const ssize_t n = ::send(fd_, bytes.data() + off, bytes.size() - off, 0);
            if (n <= 0) {
                if (n < 0 && errno == EINTR) {
                    continue;
                }
                return;
            }
            off += static_cast<std::size_t>(n);
        }
    }

    bool recvFrame(std::vector<std::uint8_t>& payload) {
        apollo::net::link::FrameParser parser;
        std::uint8_t buf[4096];
        for (;;) {
            const ssize_t n = ::recv(fd_, buf, sizeof buf, 0);
            if (n <= 0) {
                return false;
            }
            auto outcome = parser.feed(buf, static_cast<std::size_t>(n));
            if (outcome.fatal || outcome.frames.empty()) {
                return false;
            }
            payload = std::move(outcome.frames.front().payload);
            return true;
        }
    }

    bool waitEof(int timeoutMs) {
        pollfd pfd{fd_, POLLIN, 0};
        if (::poll(&pfd, 1, timeoutMs) <= 0) {
            return false;
        }
        std::uint8_t buf[64];
        const ssize_t n = ::recv(fd_, buf, sizeof buf, 0);
        return n <= 0;
    }

    void close() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

private:
    int fd_ = -1;
};

namespace link = apollo::net::link;
namespace wire = apollo::net::wire;

using apollo::contract::gen::MsgId;

constexpr std::uint16_t kDrainRequestId = static_cast<std::uint16_t>(MsgId::gateway_drain_request);
constexpr std::uint16_t kDrainReplyId = static_cast<std::uint16_t>(MsgId::gateway_drain_reply);

std::vector<std::uint8_t> frame_bytes(std::uint32_t seq, std::uint8_t flags,
                                      const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> out;
    apollo::net::link::append_frame(out, seq, flags, payload);
    return out;
}

// 955 回执账（ops 端投影；request_id 关联）
struct DrainReply {
    std::uint64_t request_id = 0;
    bool ok = false;
    std::int32_t state = 0;
    std::uint64_t active = 0;
    std::string detail;
};

DrainReply* g_latest = nullptr;  // handler 出口（测试线程泵 → 单线程无竞）

// ops 端（fleetctl 投影）：active 端点拨 gateway 排空控制受理面，同一路由表
struct OpsEnd {
    std::unique_ptr<link::RouteTable> table;
    std::unique_ptr<link::LinkManager> manager;

    bool start(std::uint16_t port) {
        table = std::make_unique<link::RouteTable>();
        std::ifstream in(APOLLO_GATEWAY_CONTRACT_ROUTE_JSON, std::ios::binary);
        if (!in) {
            return false;
        }
        std::ostringstream buffer;
        buffer << in.rdbuf();
        if (!link::load_route_table_json(buffer.str(), *table)) {
            return false;
        }
        link::LinkManagerConfig mc;
        mc.self_name = "fleetctl";
        mc.route_version = link::route_version_from_hash(table->schema_hash());
        manager = std::make_unique<link::LinkManager>(mc);

        link::EndpointConfig ep;
        ep.name = "gateway";
        ep.host = "127.0.0.1";
        ep.port = port;
        ep.role = link::EndpointRole::kActive;
        if (!manager->add_endpoint(ep)) {
            return false;
        }
        auto& dispatcher = manager->link("gateway")->dispatcher();
        dispatcher.set_route_table(table.get());
        dispatcher.allow_domain("internal");
        dispatcher.set_handler(
            kDrainReplyId,
            [](std::uint16_t, const std::uint8_t* data, std::size_t size) {
                std::vector<wire::FieldValue> fields;
                if (!wire::decode_payload(kDrainReplyId, data, size, fields)
                    || fields.size() != 6) {
                    return;
                }
                DrainReply r;
                r.request_id = static_cast<std::uint64_t>(fields[0].integer);
                r.ok = fields[2].integer != 0;
                r.state = static_cast<std::int32_t>(fields[3].integer);
                r.active = static_cast<std::uint64_t>(fields[4].integer);
                r.detail = fields[5].text;
                if (g_latest != nullptr) {
                    *g_latest = std::move(r);  // 单发单收：最新回执即裁决面
                }
            });
        return true;
    }

    // 954 上行（gateway_drain_request 裸帧——控制编排族不走 932 信封）
    bool send_drain(std::uint64_t request_id, std::int32_t op, std::uint64_t deadline_s) {
        std::vector<wire::FieldValue> fields(3);
        fields[0].integer = static_cast<std::int64_t>(request_id);
        fields[1].integer = op;
        fields[2].integer = static_cast<std::int64_t>(deadline_s);
        std::vector<std::uint8_t> payload;
        if (!wire::encode_payload(kDrainRequestId, fields, payload)) {
            return false;
        }
        auto* peer = manager->link("gateway");
        return peer->send(kDrainRequestId, payload.data(), payload.size(),
                          link::SendClass::kReliable)
            == link::SendCode::kAccepted;
    }

    void pump() {
        manager->pump(static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count()));
    }
};

// ops 泵等待（gateway 泵由自线程推进；ops 端由本测试线程泵）。reply =
// 期望回执落点（request_id 匹配即停）。
template <typename Pred>
bool pump_until(OpsEnd& ops, Pred pred, int timeout_ms) {
    const auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        ops.pump();
        if (pred()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ops.pump();
    return pred();
}

// 单发单收：发 954 → 等 request_id 匹配的 955
bool drain_roundtrip(OpsEnd& ops, std::uint64_t request_id, std::int32_t op,
                     std::uint64_t deadline_s, DrainReply& out, int timeout_ms = 5000) {
    DrainReply got;
    g_latest = &got;
    const bool sent = ops.send_drain(request_id, op, deadline_s);
    const bool arrived = sent
        && pump_until(ops, [&] { return got.request_id == request_id; }, timeout_ms);
    g_latest = nullptr;
    out = got;
    return arrived;
}

bool wait_up(OpsEnd& ops) {
    return pump_until(ops,
                      [&] {
                          auto* peer = ops.manager->link("gateway");
                          return peer != nullptr
                              && peer->state() == link::LinkState::kUp;
                      },
                      5000);
}

// 起一台带排空控制面的 gateway（客户端入口与控制面都走内核分配端口）
struct ServerHarness {
    std::unique_ptr<gateway::GatewayServer> server;

    bool start() {
        gateway::GatewayConfig config;
        config.host = "127.0.0.1";
        config.port = 0;           // 客户端入口：内核分配
        config.controlM1Port = 0;  // 排空控制面：内核分配（drainControlPort 读回）
        config.sessionTimeoutMs = 60000;
        config.heartbeatCheckIntervalMs = 1000;
        server = std::make_unique<gateway::GatewayServer>(config);
        server->start();
        return server->drainControlPort() != 0;
    }
};

// 排空生命周期：begin → 新连接拒 / 既有服务 → 自然离开 → drained → 监听仍关
bool test_gateway_drain_lifecycle() {
    std::cout << "Running: test_gateway_drain_lifecycle..." << std::endl;

    ServerHarness h;
    TEST_ASSERT(h.start(), "gateway started with drain control face");

    OpsEnd ops;
    TEST_ASSERT(ops.start(h.server->drainControlPort()), "ops end started");
    TEST_ASSERT(wait_up(ops), "ops link up");

    // serving 快照（未排空）
    DrainReply r;
    TEST_ASSERT(drain_roundtrip(ops, 1, 2, 0, r), "serving status arrived");
    TEST_ASSERT(r.ok && r.state == 0, "serving state");
    TEST_ASSERT(r.active == 0, "no connections while serving");

    // 两个既有连接
    TestClient a;
    TestClient b;
    TEST_ASSERT(a.connect("127.0.0.1", h.server->listenPort()), "client a connected");
    TEST_ASSERT(b.connect("127.0.0.1", h.server->listenPort()), "client b connected");
    pump_until(ops, [] { return false; }, 200);  // 泵几拍：连接落表
    // drain 起排（幂等由重复 begin 覆盖——见 roundtrip 两连发）
    TEST_ASSERT(drain_roundtrip(ops, 2, 1, 0, r), "begin drain accepted");
    TEST_ASSERT(r.ok && r.state == 1, "draining state");
    TEST_ASSERT(r.active == 2, "two existing connections served");

    TestClient newcomer;
    TEST_ASSERT(!newcomer.connect("127.0.0.1", h.server->listenPort()),
                "new connection refused after drain");

    // 已排水期重复 begin：幂等（仍 draining，不翻态不拒收）
    TEST_ASSERT(drain_roundtrip(ops, 3, 1, 0, r), "repeated begin accepted");
    TEST_ASSERT(r.ok && r.state == 1, "still draining after repeated begin");

    // 既有连接照常服务（PING→PONG）
    apollo::protocol::Ping ping;
    ping.timestamp = 777;
    const auto packet = apollo::protocol::MessageCodec::encode(ping, 0);
    a.sendAll(frame_bytes(1, 0, packet));
    std::vector<std::uint8_t> received;
    TEST_ASSERT(a.recvFrame(received), "existing connection still served (pong)");
    TEST_ASSERT(received.size() > sizeof(apollo::protocol::MessageHeader),
                "pong has header + body");

    // 自然离开 → drained（自然排空 detail）
    a.close();
    b.close();
    DrainReply drained;
    TEST_ASSERT(pump_until(ops,
                           [&] {
                               return drain_roundtrip(ops, 4, 2, 0, drained, 100)
                                   && drained.state == 2;
                           },
                           5000),
                "drained status arrived");
    TEST_ASSERT(drained.active == 0, "no active connections left");
    TEST_ASSERT(drained.detail.find("all client connections left") != std::string::npos,
                "natural drain detail");

    // 终态后监听仍关（新连接持续拒）
    TEST_ASSERT(!newcomer.connect("127.0.0.1", h.server->listenPort()),
                "listener stays closed after drained");

    h.server->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// 期限强停：deadline 到仍在线 → 全量收线落 drained（强停结果显形）
bool test_gateway_drain_deadline_force() {
    std::cout << "Running: test_gateway_drain_deadline_force..." << std::endl;

    ServerHarness h;
    TEST_ASSERT(h.start(), "gateway started with drain control face");

    OpsEnd ops;
    TEST_ASSERT(ops.start(h.server->drainControlPort()), "ops end started");
    TEST_ASSERT(wait_up(ops), "ops link up");

    TestClient a;
    TEST_ASSERT(a.connect("127.0.0.1", h.server->listenPort()), "client connected");
    pump_until(ops, [] { return false; }, 200);  // 泵几拍：连接落表

    // begin 带期限：强停在 gateway 泵线程自驱（fleetctl 只等终态）
    DrainReply r;
    TEST_ASSERT(drain_roundtrip(ops, 1, 1, 1, r), "begin drain accepted");
    TEST_ASSERT(r.ok && r.state == 1, "draining state");

    DrainReply drained;
    TEST_ASSERT(pump_until(ops,
                           [&] {
                               return drain_roundtrip(ops, 2, 2, 0, drained, 100)
                                   && drained.state == 2;
                           },
                           8000),
                "drained status arrived");
    TEST_ASSERT(drained.detail.find("force-closed") != std::string::npos,
                "forced-stop detail names the force close");
    TEST_ASSERT(drained.active == 0, "no active connections after force close");
    TEST_ASSERT(a.waitEof(3000), "client saw the forced close (EOF)");

    h.server->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// 受理面口径：未知 op 显式拒（不静默，detail 点名）
bool test_gateway_drain_unknown_op() {
    std::cout << "Running: test_gateway_drain_unknown_op..." << std::endl;

    ServerHarness h;
    TEST_ASSERT(h.start(), "gateway started with drain control face");

    OpsEnd ops;
    TEST_ASSERT(ops.start(h.server->drainControlPort()), "ops end started");
    TEST_ASSERT(wait_up(ops), "ops link up");

    DrainReply r;
    TEST_ASSERT(drain_roundtrip(ops, 1, 9, 0, r), "unknown op reply arrived");
    TEST_ASSERT(!r.ok, "unknown op rejected");
    TEST_ASSERT(r.detail.find("unknown op 9") != std::string::npos,
                "rejection names the op");
    TEST_ASSERT(r.state == 0, "state untouched");

    h.server->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

#endif // !_WIN32

} // namespace

int main() {
    int failed = 0;
#if !defined(_WIN32)
    if (!test_gateway_drain_lifecycle()) ++failed;
    if (!test_gateway_drain_deadline_force()) ++failed;
    if (!test_gateway_drain_unknown_op()) ++failed;
#endif
    if (failed == 0) {
        std::cout << "ALL GATEWAY DRAIN TESTS PASSED" << std::endl;
        return 0;
    }
    std::cerr << failed << " gateway drain test(s) failed" << std::endl;
    return 1;
}
