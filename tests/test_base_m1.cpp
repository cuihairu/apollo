// Zone M1 base 面 × login M1 base 客户端 e2e 测试（P4-4 批③：ZoneM1BaseTests）。
//
// 缝工厂两侧真实现直编（同 login_m1_admission_tests 惯例）：
//   1. base_e2e：base-app 受理面（真 codec 假 handler——Activate/Bind 应答、
//      AssignWorld 回 Unknown 错误包[base-app 真实分派语义，落点归 baseappmgr]）
//      ↔ login base 客户端面全链 round-trip；
//   2. base_unreachable：对端无监听 = 同步立即空返回 → 调用方降级缝
//      （传输缺席 ≠ 会话事实，桩时代同缝）。
// Windows 下 link TCP 底座不进本批，用例本体空转。

#include "player_store/m1_server.hpp"
#include "login/m1_base_client.hpp"
#include "apollo/net/link/tcp_transport.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifndef APOLLO_BASEAPP_CONTRACT_ROUTE_JSON
#define APOLLO_BASEAPP_CONTRACT_ROUTE_JSON ""
#endif

#ifndef APOLLO_LOGIN_CONTRACT_ROUTE_JSON
#define APOLLO_LOGIN_CONTRACT_ROUTE_JSON ""
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

// base-app 真实分派同缝假 handler：Activate/Bind 应答（字段级回填）；
// AssignWorld 与未知型 = Unknown message type 错误包（base-app dispatchRequest
// 注记「落点裁决归 apps/manager」——错误包回程即 login 缝 throw 降级的前提面）。
std::vector<std::uint8_t> fake_base_handler(const std::uint8_t* frame, std::size_t size) {
    const std::vector<std::uint8_t> bytes(frame, frame + size);
    const auto header = protocol::MessageCodec::parseHeader(bytes);
    const std::vector<std::uint8_t> body(bytes.begin() + sizeof(protocol::MessageHeader),
                                         bytes.end());
    const auto msgType = static_cast<protocol::MessageType>(header.type);

    if (msgType == protocol::MessageType::PLAYER_ACTIVATE_REQUEST) {
        protocol::PlayerActivateRequest req;
        protocol::MessageCodec::decodeBody(body, req);
        protocol::PlayerActivateResponse resp;
        resp.success = req.playerId != 0;
        resp.playerId = req.playerId;
        if (!resp.success) {
            resp.errorMessage = "BaseApp rejected player activation";
        }
        return protocol::MessageCodec::encode(resp, header.sessionId);
    }
    if (msgType == protocol::MessageType::PLAYER_BIND_SESSION_REQUEST) {
        protocol::PlayerBindSessionRequest req;
        protocol::MessageCodec::decodeBody(body, req);
        protocol::PlayerBindSessionResponse resp;
        resp.success = req.sessionId != 0;
        if (!resp.success) {
            resp.errorMessage = "BaseApp rejected session binding";
        }
        return protocol::MessageCodec::encode(resp, header.sessionId);
    }

    protocol::ErrorMessage err;
    err.code = static_cast<std::uint32_t>(protocol::MessageType::ERROR);
    err.message = "Unknown message type";
    return protocol::MessageCodec::encode(err, header.sessionId);
}

// 占一个临时端口再放手（unreachable 用例的死端口探针）。
std::uint16_t pick_dead_port() {
    apollo::net::link::TcpListener probe;
    if (!probe.start("127.0.0.1", 0)) {
        return 0;
    }
    return probe.port();
}

bool wait_peer_up(login::M1BaseClient& client) {
    for (int i = 0; i < 500 && !client.peerUp(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return client.peerUp();
}

// 同步全帧调用便捷面（测试用：编帧 → call → 空返回即败）。
std::optional<std::vector<std::uint8_t>> call_frame(
    login::M1BaseClient& client, const std::vector<std::uint8_t>& frame) {
    return client.call(frame, std::chrono::milliseconds(500));
}

#endif // !_WIN32

bool test_base_e2e() {
#if !defined(_WIN32)
    player_store::PlayerStoreLinkServerConfig config;
    config.host = "127.0.0.1";
    config.port = 0;  // 内核分配，listenPort() 读回
    config.route_json = APOLLO_BASEAPP_CONTRACT_ROUTE_JSON;
    auto server = player_store::makePlayerStoreLinkServer(config, fake_base_handler);
    TEST_ASSERT(server->start(), "base face starts");
    TEST_ASSERT(server->listenPort() != 0, "ephemeral port observed");
    TEST_ASSERT(!server->peerConnected(), "no peer before dial");

    login::M1BaseClientConfig clientConfig;
    clientConfig.host = "127.0.0.1";
    clientConfig.port = server->listenPort();
    clientConfig.route_json = APOLLO_LOGIN_CONTRACT_ROUTE_JSON;
    auto client = login::makeM1BaseClient(clientConfig);
    TEST_ASSERT(client != nullptr, "login base client starts");

    TEST_ASSERT(wait_peer_up(*client), "client dials base face");
    // 握手收敛双向异步（client 先见 kUp，server 晚一拍）——等值轮而非单断言。
    for (int i = 0; i < 500 && !server->peerConnected(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    TEST_ASSERT(server->peerConnected(), "face observes handshake");

    // Activate：成功路径字段回填 + sessionId 透传。
    protocol::PlayerActivateRequest activate;
    activate.playerId = 42;
    const auto activateResp =
        call_frame(*client, protocol::MessageCodec::encode(activate, 9));
    TEST_ASSERT(activateResp.has_value(), "activate roundtrip returns");
    {
        const auto header = protocol::MessageCodec::parseHeader(*activateResp);
        TEST_ASSERT(static_cast<protocol::MessageType>(header.type) ==
                        protocol::MessageType::PLAYER_ACTIVATE_RESPONSE,
                    "activate response frame type");
        TEST_ASSERT(header.sessionId == 9, "activate sessionId echoed");
        const std::vector<std::uint8_t> body(activateResp->begin() +
                                                 sizeof(protocol::MessageHeader),
                                             activateResp->end());
        protocol::PlayerActivateResponse parsed;
        protocol::MessageCodec::decodeBody(body, parsed);
        TEST_ASSERT(parsed.success && parsed.playerId == 42, "activate echo verbatim");
    }

    // Activate：拒绝路径 success=false 原文回程（login 缝据此真拒）。
    activate.playerId = 0;
    const auto rejected =
        call_frame(*client, protocol::MessageCodec::encode(activate, 9));
    TEST_ASSERT(rejected.has_value(), "rejected activate roundtrip returns");
    {
        const std::vector<std::uint8_t> body(rejected->begin() +
                                                 sizeof(protocol::MessageHeader),
                                             rejected->end());
        protocol::PlayerActivateResponse parsed;
        protocol::MessageCodec::decodeBody(body, parsed);
        TEST_ASSERT(!parsed.success, "activate rejection verdict");
        TEST_ASSERT(parsed.errorMessage == "BaseApp rejected player activation",
                    "reject reason verbatim");
    }

    // Bind：成功路径。
    protocol::PlayerBindSessionRequest bind;
    bind.playerId = 42;
    bind.sessionId = 9;
    bind.gatewayId = 1;
    bind.gatewayAddr = "tcp://127.0.0.1:9002";
    const auto bindResp = call_frame(*client, protocol::MessageCodec::encode(bind, 9));
    TEST_ASSERT(bindResp.has_value(), "bind roundtrip returns");
    {
        const auto header = protocol::MessageCodec::parseHeader(*bindResp);
        TEST_ASSERT(static_cast<protocol::MessageType>(header.type) ==
                        protocol::MessageType::PLAYER_BIND_SESSION_RESPONSE,
                    "bind response frame type");
        const std::vector<std::uint8_t> body(bindResp->begin() +
                                                 sizeof(protocol::MessageHeader),
                                             bindResp->end());
        protocol::PlayerBindSessionResponse parsed;
        protocol::MessageCodec::decodeBody(body, parsed);
        TEST_ASSERT(parsed.success, "bind accepted");
    }

    // AssignWorld：base-app 不受理（落点归 baseappmgr）→ Unknown 错误包回程。
    // login 缝的帧型校验据此 throw → catch 降级（桩时代同缝，批④ 换面）。
    protocol::PlayerAssignWorldRequest assign;
    assign.playerId = 42;
    const auto assignResp =
        call_frame(*client, protocol::MessageCodec::encode(assign, 9));
    TEST_ASSERT(assignResp.has_value(), "assign roundtrip returns");
    {
        const auto header = protocol::MessageCodec::parseHeader(*assignResp);
        TEST_ASSERT(static_cast<protocol::MessageType>(header.type) ==
                        protocol::MessageType::ERROR,
                    "assign gets error envelope");
        const std::vector<std::uint8_t> body(assignResp->begin() +
                                                 sizeof(protocol::MessageHeader),
                                             assignResp->end());
        protocol::ErrorMessage parsed;
        protocol::MessageCodec::decodeBody(body, parsed);
        TEST_ASSERT(parsed.message == "Unknown message type", "error text verbatim");
    }

    client->stop();
    server->stop();
    return true;
#else
    std::cout << "SKIP (windows: link tcp base posix only)" << std::endl;
    return true;
#endif
}

bool test_base_unreachable() {
#if !defined(_WIN32)
    const std::uint16_t dead_port = pick_dead_port();
    TEST_ASSERT(dead_port != 0, "dead port probed");

    login::M1BaseClientConfig clientConfig;
    clientConfig.host = "127.0.0.1";
    clientConfig.port = dead_port;
    clientConfig.route_json = APOLLO_LOGIN_CONTRACT_ROUTE_JSON;
    auto client = login::makeM1BaseClient(clientConfig);
    TEST_ASSERT(client != nullptr, "client starts (dial fails in background)");
    TEST_ASSERT(!client->peerUp(), "no peer on dead port");

    // 对端缺席：call_async 非 kUp 即败 → 空返回（不在面内等满超时）。
    protocol::PlayerActivateRequest activate;
    activate.playerId = 1;
    const auto resp =
        client->call(protocol::MessageCodec::encode(activate, 3),
                     std::chrono::milliseconds(500));
    TEST_ASSERT(!resp.has_value(), "unreachable transport returns empty");

    client->stop();
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
        {"base_e2e", test_base_e2e},
        {"base_unreachable", test_base_unreachable},
    };
    int failed = 0;
    for (const auto& c : cases) {
        if (!c.fn()) {
            std::cerr << "CASE FAILED: " << c.name << std::endl;
            ++failed;
        }
    }
    if (failed == 0) {
        std::cout << "ZoneM1BaseTests: all "
                  << (sizeof(cases) / sizeof(cases[0])) << " cases passed" << std::endl;
        return 0;
    }
    std::cout << "ZoneM1BaseTests: " << failed << " case(s) failed" << std::endl;
    return 1;
}
