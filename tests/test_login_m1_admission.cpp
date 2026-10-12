// Login M1 准入面 e2e 测试（P4-4 批①：LoginM1AdmissionTests）。
//
// 缝工厂两侧真实现直编（同 gateway_m1_egress_tests 惯例）：
//   1. admission_e2e：login 受理面（真 codec handler——GatewayAssignRequest
//      解码、假会话规则、GatewayAssignResponse 回帧）↔ gateway 链路准入
//      服务全链 round-trip——真票收 / 假票拒（拒 = 走到了真 RPC，降级
//      容忍面不拒不拦）；
//   2. admission_unreachable：对端无监听 = 同步立即失败 → 降级容忍
//      （准入面缺席 ≠ 会话非法，桩时代同缝）。
// Windows 下 link TCP 底座不进本批，用例本体空转。

#include "gateway/ingress/session_admission_service.hpp"
#include "login/m1_admission_server.hpp"
#include "apollo/net/link/tcp_transport.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifndef APOLLO_LOGIN_CONTRACT_ROUTE_JSON
#define APOLLO_LOGIN_CONTRACT_ROUTE_JSON ""
#endif

#ifndef APOLLO_GATEWAY_CONTRACT_ROUTE_JSON
#define APOLLO_GATEWAY_CONTRACT_ROUTE_JSON ""
#endif

#define TEST_ASSERT(cond, msg)                                                           \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            std::cerr << "FAIL: " << (msg) << " (" << __FILE__ << ":" << __LINE__ << ")" \
                      << std::endl;                                                      \
            return false;                                                                \
        }                                                                                \
    } while (0)

namespace {

#if !defined(_WIN32)

namespace protocol = apollo::protocol;

// 假会话规则：ticket == "t-ok" 准，其余拒（响应走真 codec 全帧）。
std::vector<std::uint8_t> fake_login_handler(const std::uint8_t* frame, std::size_t size) {
    const std::vector<std::uint8_t> bytes(frame, frame + size);
    const auto header = protocol::MessageCodec::parseHeader(bytes);
    if (static_cast<protocol::MessageType>(header.type) !=
        protocol::MessageType::GATEWAY_ASSIGN_REQUEST) {
        protocol::GatewayAssignResponse bad;
        bad.success = false;
        bad.errorMessage = "unexpected frame type";
        return protocol::MessageCodec::encode(bad, header.sessionId);
    }
    protocol::GatewayAssignRequest req;
    const std::vector<std::uint8_t> body(bytes.begin() + sizeof(protocol::MessageHeader),
                                         bytes.end());
    protocol::MessageCodec::decodeBody(body, req);
    protocol::GatewayAssignResponse resp;
    resp.success = (req.loginTicket == "t-ok");
    resp.gatewayHost = "127.0.0.1";
    resp.gatewayPort = 9002;
    resp.token = req.loginTicket;
    if (!resp.success) {
        resp.errorMessage = "Invalid or expired session";
    }
    return protocol::MessageCodec::encode(resp, header.sessionId);
}

// 占一个临时端口再放手（unreachable 用例的死端口探针）。
std::uint16_t pick_dead_port() {
    apollo::net::link::TcpListener probe;
    if (!probe.start("127.0.0.1", 0)) {
        return 0;
    }
    return probe.port();
}

bool wait_peer_up(login::M1AdmissionServer& server) {
    for (int i = 0; i < 500 && !server.peerConnected(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return server.peerConnected();
}

bool wait_real_rpc(gateway::SessionAdmissionService& service) {
    // 拒绝 = 真 RPC round-trip 已通（降级容忍面对假票也收）——以首个
    // 「假票被拒」为链路就绪哨（拨号+握手+重拨退避的收敛预算）。
    for (int i = 0; i < 1000; ++i) {
        const auto r = service.admit({7, 9, "t-probe"});
        if (!r.accepted) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return false;
}

#endif // !_WIN32

bool test_admission_e2e() {
#if !defined(_WIN32)
    login::M1AdmissionConfig config;
    config.host = "127.0.0.1";
    config.port = 0;  // 内核分配，listenPort() 读回
    config.route_json = APOLLO_LOGIN_CONTRACT_ROUTE_JSON;
    auto server = login::makeM1AdmissionServer(config, fake_login_handler);
    TEST_ASSERT(server->start(), "admission face starts");
    TEST_ASSERT(server->listenPort() != 0, "ephemeral port observed");

    auto service = gateway::makeLinkSessionAdmissionService(
        "127.0.0.1", server->listenPort(), APOLLO_GATEWAY_CONTRACT_ROUTE_JSON);
    TEST_ASSERT(service != nullptr, "gateway link admission starts");

    TEST_ASSERT(wait_peer_up(*server), "gateway dials in");
    TEST_ASSERT(wait_real_rpc(*service), "real rpc roundtrip reached");

    const auto ok = service->admit({7, 9, "t-ok"});
    TEST_ASSERT(ok.accepted, "valid ticket admitted");
    const auto bad = service->admit({7, 9, "t-bad"});
    TEST_ASSERT(!bad.accepted, "invalid ticket rejected");
    TEST_ASSERT(bad.rejectReason == "gateway admission rejected", "reject reason verbatim");

    service->shutdown();
    server->stop();
    return true;
#else
    std::cout << "SKIP (windows: link tcp base posix only)" << std::endl;
    return true;
#endif
}

bool test_admission_unreachable() {
#if !defined(_WIN32)
    const std::uint16_t dead_port = pick_dead_port();
    TEST_ASSERT(dead_port != 0, "dead port probed");

    auto service = gateway::makeLinkSessionAdmissionService(
        "127.0.0.1", dead_port, APOLLO_GATEWAY_CONTRACT_ROUTE_JSON);
    TEST_ASSERT(service != nullptr, "service starts (dial fails in background)");

    // 对端缺席：call_async 非 kUp 即败 → 降级容忍（不在面内等 500ms）。
    const auto r = service->admit({7, 9, "t-any"});
    TEST_ASSERT(r.accepted, "unreachable transport degrades to tolerate");
    TEST_ASSERT(r.rejectReason.empty(), "no reject reason on degrade path");

    service->shutdown();
    return true;
#else
    std::cout << "SKIP (windows: link tcp base posix only)" << std::endl;
    return true;
#endif
}

} // namespace

int main() {
    const struct {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"admission_e2e", test_admission_e2e},
        {"admission_unreachable", test_admission_unreachable},
    };
    int failed = 0;
    for (const auto& c : cases) {
        if (!c.fn()) {
            std::cerr << "CASE FAILED: " << c.name << std::endl;
            ++failed;
        }
    }
    if (failed == 0) {
        std::cout << "LoginM1AdmissionTests: all "
                  << (sizeof(cases) / sizeof(cases[0])) << " cases passed" << std::endl;
        return 0;
    }
    std::cout << "LoginM1AdmissionTests: " << failed << " case(s) failed" << std::endl;
    return 1;
}
