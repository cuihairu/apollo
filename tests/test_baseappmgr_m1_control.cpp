// OnlineDirectory M1 控制面 × login/gateway 客户端面 e2e 测试（P4-4 批④：
// OnlineDirectoryM1ControlTests）。
//
// 缝工厂两侧真实现直编 + **真 OnlineDirectory 分派面**（比批③ 假 handler 更强：
// dispatchRequest 直通 = 会话语义零分叉，目录裁决/路由解析全真）：
//   1. control_e2e：双拨入方 fan-out（login + gateway 同面各自握手入表）
//      — AssignWorld 经 login 客户端落真目录、ResolveRoute 经 gateway
//      客户端全字段回填、Ping/未知型两翼；
//   2. control_unreachable：对端无监听 = 同步立即空返回 → 调用方降级缝。
// Windows 下 link TCP 底座不进本批，用例本体空转。

#include "gateway/m1_route_client.hpp"
#include "login/m1_base_client.hpp"
#include "online_directory/m1_server.hpp"
#include "online_directory/online_directory.hpp"
#include "apollo/net/link/tcp_transport.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifndef APOLLO_BASEAPPMGR_CONTRACT_ROUTE_JSON
#define APOLLO_BASEAPPMGR_CONTRACT_ROUTE_JSON ""
#endif

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

// 占一个临时端口再放手（unreachable 用例的死端口探针）。
std::uint16_t pick_dead_port() {
    apollo::net::link::TcpListener probe;
    if (!probe.start("127.0.0.1", 0)) {
        return 0;
    }
    return probe.port();
}

// 握手收敛双向异步（client 先见 kUp，server 晚一拍）——等值轮而非单断言
// （macos-ci-yield-polls：每拍 sleep ≥2ms，裸自旋必 flake）。
bool wait_peer_up(login::M1BaseClient& client) {
    for (int i = 0; i < 500 && !client.peerUp(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return client.peerUp();
}

bool wait_peer_up(gateway::M1RouteClient& client) {
    for (int i = 0; i < 500 && !client.peerUp(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return client.peerUp();
}

#endif // !_WIN32

bool test_control_e2e() {
#if !defined(_WIN32)
    // 真 OnlineDirectory（不 start，纯内存目录面——online_directory_tests 同例）；
    // handler 直通 dispatchRequest，受理语义与 nng REP 路径零分叉。
    online_directory::OnlineDirectory mgr(0);

    online_directory::M1ControlServerConfig serverConfig;
    serverConfig.host = "127.0.0.1";
    serverConfig.port = 0;  // 内核分配，listenPort() 读回
    serverConfig.route_json = APOLLO_BASEAPPMGR_CONTRACT_ROUTE_JSON;
    auto server = online_directory::makeM1ControlServer(
        serverConfig, [&mgr](const std::uint8_t* frame, std::size_t size) {
            return mgr.dispatchRequest(std::vector<std::uint8_t>(frame, frame + size));
        });
    TEST_ASSERT(server->start(), "control face starts");
    TEST_ASSERT(server->listenPort() != 0, "ephemeral port observed");
    TEST_ASSERT(server->peerCount() == 0, "no peers before dial");

    // 拨入方一：login（AssignWorld 落点裁决链，批④ peer_name="login"）。
    login::M1BaseClientConfig loginConfig;
    loginConfig.host = "127.0.0.1";
    loginConfig.port = server->listenPort();
    loginConfig.route_json = APOLLO_LOGIN_CONTRACT_ROUTE_JSON;
    loginConfig.peer_name = "login";
    auto loginClient = login::makeM1BaseClient(loginConfig);
    TEST_ASSERT(loginClient != nullptr, "login baseappmgr client starts");
    TEST_ASSERT(wait_peer_up(*loginClient), "login client dials control face");

    // 拨入方二：gateway（ResolveRoute 路由解析链）——单被动端点双拨入方
    // fan-out（按自报名入表，peerCount 读回 2）。
    gateway::M1RouteClientConfig gwConfig;
    gwConfig.host = "127.0.0.1";
    gwConfig.port = server->listenPort();
    gwConfig.route_json = APOLLO_GATEWAY_CONTRACT_ROUTE_JSON;
    auto gwClient = gateway::makeM1RouteClient(gwConfig);
    TEST_ASSERT(gwClient != nullptr, "gateway route client starts");
    TEST_ASSERT(wait_peer_up(*gwClient), "gateway client dials control face");
    for (int i = 0; i < 500 && server->peerCount() < 2; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    TEST_ASSERT(server->peerCount() == 2, "face observes both dialed peers");

    // AssignWorld 经 login 客户端 → 真 OnlineDirectory 落点裁决（真目录写入）。
    // L116 分型域（cell/模拟代次）随落点成行；L117 版本域分立——拓扑版本
    // 与路由版本异值成行（2 ≠ 71），证明分域不折叠。
    protocol::PlayerAssignWorldRequest assign;
    assign.playerId = 42;
    assign.worldId = 7;
    assign.mapId = 3;
    assign.instanceId = 11;
    assign.spaceId = 5;
    assign.routeVersion = 2;
    assign.topologyVersion = 71;
    assign.cellId = 21;
    assign.simulationEpoch = 88;
    const auto assignResp = loginClient->call(
        protocol::MessageCodec::encode(assign, 9), std::chrono::milliseconds(500));
    TEST_ASSERT(assignResp.has_value(), "assign roundtrip returns");
    {
        const auto header = protocol::MessageCodec::parseHeader(*assignResp);
        TEST_ASSERT(static_cast<protocol::MessageType>(header.type) ==
                        protocol::MessageType::PLAYER_ASSIGN_WORLD_RESPONSE,
                    "assign response frame type");
        TEST_ASSERT(header.sessionId == 9, "assign sessionId echoed");
        const std::vector<std::uint8_t> body(assignResp->begin() +
                                                 sizeof(protocol::MessageHeader),
                                             assignResp->end());
        protocol::PlayerAssignWorldResponse parsed;
        protocol::MessageCodec::decodeBody(body, parsed);
        TEST_ASSERT(parsed.success, "assign accepted by real directory");
        TEST_ASSERT(parsed.routeVersion == 2, "assign routeVersion echoed");
    }

    // gateway 侧绑定直写目录（wire bind 面 ≠ baseappmgr 分派——nng 时代同位），
    // ResolveRoute 才有完整 binding+assignment 可解析。session_version = 绑定
    // 代次随绑定面入册（L116）。
    apollo::game::base::SessionBinding binding;
    binding.session_id = 9;
    binding.gateway_id = 1;
    binding.gateway_addr = "tcp://127.0.0.1:9002";
    binding.session_version = 4;
    TEST_ASSERT(mgr.bindSession(42, binding), "session bound in directory");

    // ResolveRoute 经 gateway 客户端 → 全字段回填（真解析非假 echo）。
    protocol::PlayerResolveRouteRequest resolve;
    resolve.playerId = 42;
    resolve.sessionId = 9;
    const auto resolveResp = gwClient->call(
        protocol::MessageCodec::encode(resolve, 9), std::chrono::milliseconds(500));
    TEST_ASSERT(resolveResp.has_value(), "resolve roundtrip returns");
    {
        const auto header = protocol::MessageCodec::parseHeader(*resolveResp);
        TEST_ASSERT(static_cast<protocol::MessageType>(header.type) ==
                        protocol::MessageType::PLAYER_RESOLVE_ROUTE_RESPONSE,
                    "resolve response frame type");
        const std::vector<std::uint8_t> body(resolveResp->begin() +
                                                 sizeof(protocol::MessageHeader),
                                             resolveResp->end());
        protocol::PlayerResolveRouteResponse parsed;
        protocol::MessageCodec::decodeBody(body, parsed);
        TEST_ASSERT(parsed.success, "resolve succeeds");
        TEST_ASSERT(parsed.playerId == 42 && parsed.sessionId == 9,
                    "resolve identity backfilled");
        TEST_ASSERT(parsed.worldId == 7 && parsed.mapId == 3 &&
                        parsed.instanceId == 11 && parsed.spaceId == 5 &&
                        parsed.routeVersion == 2,
                    "resolve assignment fields verbatim");
        TEST_ASSERT(parsed.topologyVersion == 71,
                    "resolve topology domain verbatim (L117)");
        TEST_ASSERT(parsed.cellId == 21 && parsed.simulationEpoch == 88,
                    "resolve typed domains verbatim (L116)");
        TEST_ASSERT(parsed.sessionVersion == 4,
                    "resolve session version verbatim (L116)");
        TEST_ASSERT(parsed.gatewayId == 1 &&
                        parsed.gatewayAddr == "tcp://127.0.0.1:9002",
                    "resolve binding fields verbatim");
    }

    // 未登记玩家：真目录查无 → success=false（真裁决语义，非传输缺席）。
    resolve.playerId = 77;
    resolve.sessionId = 0;
    const auto missResp = gwClient->call(
        protocol::MessageCodec::encode(resolve, 77), std::chrono::milliseconds(500));
    TEST_ASSERT(missResp.has_value(), "miss resolve roundtrip returns");
    {
        const std::vector<std::uint8_t> body(missResp->begin() +
                                                 sizeof(protocol::MessageHeader),
                                             missResp->end());
        protocol::PlayerResolveRouteResponse parsed;
        protocol::MessageCodec::decodeBody(body, parsed);
        TEST_ASSERT(!parsed.success, "miss resolve rejected by directory");
    }

    // Ping 两翼：受理面存活观测。
    protocol::Ping ping;
    ping.timestamp = 1234;
    const auto pongResp = loginClient->call(
        protocol::MessageCodec::encode(ping, 9), std::chrono::milliseconds(500));
    TEST_ASSERT(pongResp.has_value(), "ping roundtrip returns");
    {
        const auto header = protocol::MessageCodec::parseHeader(*pongResp);
        TEST_ASSERT(static_cast<protocol::MessageType>(header.type) ==
                        protocol::MessageType::PONG,
                    "ping gets pong frame");
    }

    // 未知型：Unknown message type 错误包回程（分派 default 翼）。
    protocol::PlayerActivateRequest unknown;
    unknown.playerId = 1;
    const auto errResp = loginClient->call(
        protocol::MessageCodec::encode(unknown, 9), std::chrono::milliseconds(500));
    TEST_ASSERT(errResp.has_value(), "unknown roundtrip returns");
    {
        const auto header = protocol::MessageCodec::parseHeader(*errResp);
        TEST_ASSERT(static_cast<protocol::MessageType>(header.type) ==
                        protocol::MessageType::ERROR,
                    "unknown gets error envelope");
        const std::vector<std::uint8_t> body(errResp->begin() +
                                                 sizeof(protocol::MessageHeader),
                                             errResp->end());
        protocol::ErrorMessage parsed;
        protocol::MessageCodec::decodeBody(body, parsed);
        TEST_ASSERT(parsed.message == "Unknown message type", "error text verbatim");
    }

    gwClient->stop();
    loginClient->stop();
    server->stop();
    return true;
#else
    std::cout << "SKIP (windows: link tcp base posix only)" << std::endl;
    return true;
#endif
}

// 913/915/918 分型域 codec 往返（L116 首切片 + L117 版本域分立）：新字段
// 全量回带、版本域异值不折叠 + 旧端残帧缺字段解码缺省 0（未声明口径，不炸
// 解析；路由版本等旧域原值保留）。
bool test_player_record_wire_roundtrip() {
    { // 915 assign：拓扑/路由版本异值分域 + cell/模拟代次随落点
        protocol::PlayerAssignWorldRequest assign;
        assign.playerId = 42;
        assign.worldId = 7;
        assign.mapId = 3;
        assign.instanceId = 11;
        assign.spaceId = 5;
        assign.routeVersion = 2;
        assign.topologyVersion = 71;
        assign.cellId = 21;
        assign.simulationEpoch = 88;
        const auto frame = protocol::MessageCodec::encode(assign, 9);
        const std::vector<std::uint8_t> body(frame.begin() + sizeof(protocol::MessageHeader),
                                             frame.end());
        const auto decoded =
            protocol::MessageCodec::decodeBody<protocol::PlayerAssignWorldRequest>(body);
        TEST_ASSERT(decoded.routeVersion == 2 && decoded.topologyVersion == 71 &&
                        decoded.cellId == 21 && decoded.simulationEpoch == 88,
                    "assign version domains stay distinct through roundtrip");
        const std::string legacy = R"({"playerId":42,"worldId":7,"mapId":3,)"
            R"("instanceId":11,"spaceId":5,"routeVersion":2})";
        const std::vector<std::uint8_t> legacyBody(legacy.begin(), legacy.end());
        const auto legacyDecoded =
            protocol::MessageCodec::decodeBody<protocol::PlayerAssignWorldRequest>(legacyBody);
        TEST_ASSERT(legacyDecoded.routeVersion == 2 &&
                        legacyDecoded.topologyVersion == 0 &&
                        legacyDecoded.cellId == 0 && legacyDecoded.simulationEpoch == 0,
                    "legacy assign keeps route domain, defaults new domains zero");
    }
    { // 913 bind：会话绑定代次
        protocol::PlayerBindSessionRequest bind;
        bind.playerId = 42;
        bind.sessionId = 9;
        bind.gatewayId = 1;
        bind.gatewayAddr = "tcp://127.0.0.1:9002";
        bind.sessionVersion = 4;
        const auto frame = protocol::MessageCodec::encode(bind, 9);
        const std::vector<std::uint8_t> body(frame.begin() + sizeof(protocol::MessageHeader),
                                             frame.end());
        const auto decoded =
            protocol::MessageCodec::decodeBody<protocol::PlayerBindSessionRequest>(body);
        TEST_ASSERT(decoded.sessionVersion == 4, "bind session version roundtrip");
        const std::string legacy = R"({"playerId":42,"sessionId":9,)"
            R"("gatewayId":1,"gatewayAddr":"tcp://127.0.0.1:9002"})";
        const std::vector<std::uint8_t> legacyBody(legacy.begin(), legacy.end());
        const auto legacyDecoded =
            protocol::MessageCodec::decodeBody<protocol::PlayerBindSessionRequest>(legacyBody);
        TEST_ASSERT(legacyDecoded.sessionVersion == 0,
                    "legacy bind defaults version zero");
    }
    { // 918 resolve：四版本域异值同帧回带 + 旧端残帧缺省
        protocol::PlayerResolveRouteResponse response;
        response.success = true;
        response.playerId = 42;
        response.sessionId = 9;
        response.gatewayId = 1;
        response.gatewayAddr = "tcp://127.0.0.1:9002";
        response.worldId = 7;
        response.mapId = 3;
        response.instanceId = 11;
        response.spaceId = 5;
        response.routeVersion = 2;
        response.topologyVersion = 71;
        response.cellId = 21;
        response.simulationEpoch = 88;
        response.sessionVersion = 4;
        const auto frame = protocol::MessageCodec::encode(response, 9);
        const std::vector<std::uint8_t> body(frame.begin() + sizeof(protocol::MessageHeader),
                                             frame.end());
        const auto decoded =
            protocol::MessageCodec::decodeBody<protocol::PlayerResolveRouteResponse>(body);
        TEST_ASSERT(decoded.routeVersion == 2 && decoded.topologyVersion == 71 &&
                        decoded.simulationEpoch == 88 && decoded.sessionVersion == 4,
                    "resolve version domains stay distinct through roundtrip");
        const std::string legacy = R"({"success":true,"playerId":42,"sessionId":9,)"
            R"("gatewayId":1,"gatewayAddr":"tcp://127.0.0.1:9002","worldId":7,)"
            R"("mapId":3,"instanceId":11,"spaceId":5,"routeVersion":2,)"
            R"("errorMessage":""})";
        const std::vector<std::uint8_t> legacyBody(legacy.begin(), legacy.end());
        const auto legacyDecoded =
            protocol::MessageCodec::decodeBody<protocol::PlayerResolveRouteResponse>(legacyBody);
        TEST_ASSERT(legacyDecoded.success && legacyDecoded.routeVersion == 2 &&
                        legacyDecoded.topologyVersion == 0 &&
                        legacyDecoded.cellId == 0 &&
                        legacyDecoded.simulationEpoch == 0 &&
                        legacyDecoded.sessionVersion == 0,
                    "legacy resolve keeps route domain, defaults new domains zero");
    }
    return true;
}

bool test_control_unreachable() {
#if !defined(_WIN32)
    const std::uint16_t dead_port = pick_dead_port();
    TEST_ASSERT(dead_port != 0, "dead port probed");

    gateway::M1RouteClientConfig gwConfig;
    gwConfig.host = "127.0.0.1";
    gwConfig.port = dead_port;
    gwConfig.route_json = APOLLO_GATEWAY_CONTRACT_ROUTE_JSON;
    auto gwClient = gateway::makeM1RouteClient(gwConfig);
    TEST_ASSERT(gwClient != nullptr, "client starts (dial fails in background)");
    TEST_ASSERT(!gwClient->peerUp(), "no peer on dead port");

    // 对端缺席：call_async 非 kUp 即败 → 空返回（不在面内等满超时）。
    protocol::PlayerResolveRouteRequest resolve;
    resolve.playerId = 1;
    const auto resp = gwClient->call(protocol::MessageCodec::encode(resolve, 3),
                                     std::chrono::milliseconds(500));
    TEST_ASSERT(!resp.has_value(), "unreachable transport returns empty");

    gwClient->stop();
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
        {"control_e2e", test_control_e2e},
        {"player_record_wire_roundtrip", test_player_record_wire_roundtrip},
        {"control_unreachable", test_control_unreachable},
    };
    int failed = 0;
    for (const auto& c : cases) {
        if (!c.fn()) {
            std::cerr << "CASE FAILED: " << c.name << std::endl;
            ++failed;
        }
    }
    if (failed == 0) {
        std::cout << "OnlineDirectoryM1ControlTests: all "
                  << (sizeof(cases) / sizeof(cases[0])) << " cases passed" << std::endl;
        return 0;
    }
    std::cout << "OnlineDirectoryM1ControlTests: " << failed << " case(s) failed" << std::endl;
    return 1;
}
