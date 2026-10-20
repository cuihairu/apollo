// login 服务注册受理面 e2e（L123 服务目录首切片；契约 964/965）：
//   lifecycle：首注册受理（code 1，自报面入表 + peer 自报名记录）→ 种类/
//   逻辑 ID 复合键并存（Base 7 与 World 7 两域互不覆盖）→ 同代次幂等刷新
//   （code 2，自报面整体更新）→ 新代次替换（code 1）→ 旧代次拒绝（code 3，
//   登记保持新代次）→ 第二拨号端并存（peer 名分账）；
//   invalid：logical_id=0 / incarnation=0 / 未知种类显式驳回（code 4 不入表），
//   request_id=0 保留（无回执无登记）；
//   malformed：残帧拒收计数（注册表不触碰，拒收不堵链路）。
// 断言事实源：965 回执内容 + ServiceRegistry 观测面（日志不算）。
// 缝工厂真实现直编（login_m1_admission_tests 惯例）；服务端自泵
// （M1ServiceRegisterServer 内置 10ms 泵线程），本线程泵拨号端。
// Windows 下 link TCP 底座不进本批，用例本体空转。
#include "login/m1_service_register_server.hpp"
#include "login/service_registry.hpp"
#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/wire/contract_wire.hpp"
#include "apollo_contract.h"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifndef APOLLO_TEST_ROUTE_JSON
#define APOLLO_TEST_ROUTE_JSON ""
#endif

namespace {
#if !defined(_WIN32)

namespace link = apollo::net::link;
namespace wire = apollo::net::wire;
using login::kServiceRegisterAccepted;
using login::kServiceRegisterAcceptedIdempotent;
using login::kServiceRegisterRejectedInvalid;
using login::kServiceRegisterRejectedStaleIncarnation;
using login::kServiceKindBase;
using login::kServiceKindWorld;
using apollo::contract::gen::MsgId;

int g_failures = 0;

#define TEST_ASSERT(cond, msg)                                                     \
    do {                                                                           \
        if (!(cond)) {                                                             \
            std::cout << "FAIL: " << (msg) << " (" << #cond << ") at "             \
                      << __LINE__ << std::endl;                                    \
            ++g_failures;                                                          \
            return false;                                                          \
        }                                                                          \
    } while (0)

constexpr std::uint16_t kRequestId = static_cast<std::uint16_t>(MsgId::service_register_request);
constexpr std::uint16_t kReplyId = static_cast<std::uint16_t>(MsgId::service_register_reply);
const char* kRouteJson = APOLLO_TEST_ROUTE_JSON;

std::uint64_t now_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

// 965 回执账（拨号端 dispatcher 写，本线程 pump 单写者——串行无锁）。
struct ServiceReply {
    bool accepted = false;
    std::int32_t code = 0;
    std::int32_t service_kind = 0;
    std::uint64_t logical_id = 0;
    std::string detail;
};

// 拨号端投影：服务进程 → login 服务注册受理面（裸 964 上行，965 按
// request_id 收账）。自报名可配（多拨号端并存用例必配不同名——回执按名
// 寻路 + 登记表 peer 分账）。
struct LinkEnd {
    std::unique_ptr<link::RouteTable> table;
    std::unique_ptr<link::LinkManager> manager;
    std::map<std::uint64_t, ServiceReply> replies;

    bool start(std::uint16_t port, const char* self_name) {
        table = std::make_unique<link::RouteTable>();
        std::ifstream in(kRouteJson, std::ios::binary);
        if (!in) {
            return false;
        }
        std::ostringstream buffer;
        buffer << in.rdbuf();
        if (!link::load_route_table_json(buffer.str(), *table)) {
            return false;
        }
        link::LinkManagerConfig mc;
        mc.self_name = self_name;
        mc.route_version = link::route_version_from_hash(table->schema_hash());
        manager = std::make_unique<link::LinkManager>(mc);

        link::EndpointConfig ep;
        ep.name = "login";
        ep.host = "127.0.0.1";
        ep.port = port;
        ep.role = link::EndpointRole::kActive;
        if (!manager->add_endpoint(ep)) {
            return false;
        }
        auto& dispatcher = manager->link("login")->dispatcher();
        dispatcher.set_route_table(table.get());
        dispatcher.allow_domain("internal");
        dispatcher.set_handler(
            kReplyId, [this](std::uint16_t, const std::uint8_t* data, std::size_t size) {
                std::vector<wire::FieldValue> fields;
                if (!wire::decode_payload(kReplyId, data, size, fields) ||
                    fields.size() != 6) {
                    return;
                }
                ServiceReply r;
                r.accepted = fields[1].integer != 0;
                r.code = static_cast<std::int32_t>(fields[2].integer);
                r.service_kind = static_cast<std::int32_t>(fields[3].integer);
                r.logical_id = static_cast<std::uint64_t>(fields[4].integer);
                r.detail = fields[5].text;
                replies[static_cast<std::uint64_t>(fields[0].integer)] = std::move(r);
            });
        return true;
    }

    void pump() { manager->pump(now_ms()); }

    bool up() { return manager->link("login")->state() == link::LinkState::kUp; }

    bool send_register(std::uint64_t request_id, std::int32_t kind,
                       std::uint64_t logical_id, std::uint64_t incarnation,
                       const std::string& host, std::int32_t port,
                       std::int64_t compat, bool ready) {
        std::vector<wire::FieldValue> f(8);
        f[0].integer = static_cast<std::int64_t>(request_id);
        f[1].integer = kind;
        f[2].integer = static_cast<std::int64_t>(logical_id);
        f[3].integer = static_cast<std::int64_t>(incarnation);
        f[4].text = host;
        f[5].integer = port;
        f[6].integer = compat;
        f[7].integer = ready ? 1 : 0;
        std::vector<std::uint8_t> payload;
        if (!wire::encode_payload(kRequestId, f, payload)) {
            return false;
        }
        return manager->link("login")
                   ->send(kRequestId, payload.data(), payload.size(),
                          link::SendClass::kReliable) == link::SendCode::kAccepted;
    }

    // 残帧：合法 964 载荷截断（decode 必败 → 服务端拒收计数）。
    bool send_truncated_register(std::uint64_t request_id) {
        std::vector<wire::FieldValue> f(8);
        f[0].integer = static_cast<std::int64_t>(request_id);
        f[1].integer = kServiceKindBase;
        f[2].integer = 7;
        f[3].integer = 1;
        f[4].text = "127.0.0.1";
        f[5].integer = 9301;
        f[6].integer = 12345;
        f[7].integer = 1;
        std::vector<std::uint8_t> payload;
        if (!wire::encode_payload(kRequestId, f, payload)) {
            return false;
        }
        payload.resize(payload.size() / 2);  // 截断 → decode 失败
        return manager->link("login")
                   ->send(kRequestId, payload.data(), payload.size(),
                          link::SendClass::kReliable) == link::SendCode::kAccepted;
    }
};

// 受理端壳：registry（宿主持有）+ M1 受理面（port 0 内核分配，listenPort
// 读回）。
struct ServerEnd {
    login::ServiceRegistry registry;
    std::unique_ptr<login::M1ServiceRegisterServer> server;

    bool start() {
        login::M1ServiceRegisterConfig config;
        config.host = "127.0.0.1";
        config.port = 0;
        config.route_json = kRouteJson;
        server = login::makeM1ServiceRegisterServer(std::move(config), &registry);
        return server && server->start() && server->listenPort() != 0;
    }

    void stop() {
        if (server) {
            server->stop();
            server.reset();
        }
    }
};

// 拨装：拨号端 964 控制面拨受面端点并等待链路 up。
bool dial(ServerEnd& server, LinkEnd& end, const char* self_name) {
    if (!end.start(server.server->listenPort(), self_name)) {
        return false;
    }
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!end.up()) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        end.pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return true;
}

// 964 单发 + 965 等（request_id 关联）。期望无回执的用例（request_id=0
// 保留）靠 timeout 判负（返回 false）。
bool register_roundtrip(LinkEnd& end, std::uint64_t request_id, std::int32_t kind,
                        std::uint64_t logical_id, std::uint64_t incarnation,
                        const std::string& host, std::int32_t port,
                        std::int64_t compat, bool ready, ServiceReply& out,
                        int timeout_ms = 5000) {
    if (!end.send_register(request_id, kind, logical_id, incarnation, host, port,
                           compat, ready)) {
        return false;
    }
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        end.pump();
        const auto it = end.replies.find(request_id);
        if (it != end.replies.end()) {
            out = it->second;
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

// 拒收计数收敛（kReliable 帧由拨号端 pump 冲线；服务端 10ms 泵拍）。
bool wait_rejected(ServerEnd& server, LinkEnd& end, std::uint64_t expect) {
    for (int i = 0; i < 500; ++i) {
        if (server.server->rejectedFrames() >= expect) {
            return true;
        }
        end.pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return false;
}

#endif // !_WIN32

bool test_register_lifecycle() {
#if !defined(_WIN32)
    ServerEnd login;
    TEST_ASSERT(login.start(), "register face starts");
    LinkEnd base;
    TEST_ASSERT(dial(login, base, "base-x"), "base dials in");

    ServiceReply r;
    TEST_ASSERT(register_roundtrip(base, 1, kServiceKindBase, 7, 1, "10.0.0.7",
                                   9301, 12345, true, r),
                "first register answered");
    TEST_ASSERT(r.accepted && r.code == kServiceRegisterAccepted,
                "first register accepted (code 1)");
    TEST_ASSERT(r.service_kind == kServiceKindBase && r.logical_id == 7,
                "reply echoes kind and logical id");
    const auto* e = login.registry.find_service(kServiceKindBase, 7);
    TEST_ASSERT(e != nullptr, "base entry recorded");
    TEST_ASSERT(e->incarnation == 1 && e->host == "10.0.0.7" && e->port == 9301 &&
                    e->compat == 12345 && e->ready,
                "self-report fields recorded");
    TEST_ASSERT(e->peer == "base-x", "peer self-name recorded");
    TEST_ASSERT(login.registry.registered_service_count() == 1, "one entry");

    // 种类/逻辑 ID 复合键：World 7 与 Base 7 两域并存互不覆盖
    LinkEnd world;
    TEST_ASSERT(dial(login, world, "world-y"), "world dials in");
    TEST_ASSERT(register_roundtrip(world, 2, kServiceKindWorld, 7, 1, "10.0.0.8",
                                   9302, 67890, true, r),
                "world register answered");
    TEST_ASSERT(r.accepted && r.code == kServiceRegisterAccepted,
                "world register accepted (namespace separation)");
    TEST_ASSERT(login.registry.registered_service_count() == 2, "two entries");
    TEST_ASSERT(login.registry.find_service(kServiceKindWorld, 7) != nullptr &&
                    login.registry.find_service(kServiceKindWorld, 7)->peer ==
                        "world-y",
                "world entry peer separated");
    TEST_ASSERT(login.registry.find_service(kServiceKindBase, 7)->peer == "base-x",
                "base entry untouched by world register");

    // 同代次幂等刷新：自报面整体更新（host/ready 换新）
    TEST_ASSERT(register_roundtrip(base, 3, kServiceKindBase, 7, 1, "10.0.0.70",
                                   9310, 12345, false, r),
                "re-register answered");
    TEST_ASSERT(r.accepted && r.code == kServiceRegisterAcceptedIdempotent,
                "same incarnation idempotent (code 2)");
    e = login.registry.find_service(kServiceKindBase, 7);
    TEST_ASSERT(e->host == "10.0.0.70" && e->port == 9310 && !e->ready,
                "self-report refreshed");
    TEST_ASSERT(login.registry.registered_service_count() == 2,
                "idempotent refresh adds no entry");

    // 新代次替换（重启换代）
    TEST_ASSERT(register_roundtrip(base, 4, kServiceKindBase, 7, 2, "10.0.0.7",
                                   9301, 12345, true, r),
                "new incarnation answered");
    TEST_ASSERT(r.accepted && r.code == kServiceRegisterAccepted,
                "new incarnation replaces (code 1)");
    TEST_ASSERT(login.registry.find_service(kServiceKindBase, 7)->incarnation == 2,
                "entry holds new incarnation");

    // 旧代次拒绝（不可覆盖）
    TEST_ASSERT(register_roundtrip(base, 5, kServiceKindBase, 7, 1, "10.0.0.7",
                                   9301, 12345, true, r),
                "stale incarnation answered");
    TEST_ASSERT(!r.accepted && r.code == kServiceRegisterRejectedStaleIncarnation,
                "stale incarnation rejected (code 3)");
    e = login.registry.find_service(kServiceKindBase, 7);
    TEST_ASSERT(e->incarnation == 2 && e->host == "10.0.0.7",
                "stale rejection keeps fresh entry");
    TEST_ASSERT(login.registry.registered_service_count() == 2,
                "stale rejection adds no entry");
    return true;
#else
    std::cout << "SKIP (windows: link tcp base posix only)" << std::endl;
    return true;
#endif
}

bool test_register_invalid() {
#if !defined(_WIN32)
    ServerEnd login;
    TEST_ASSERT(login.start(), "register face starts");
    LinkEnd base;
    TEST_ASSERT(dial(login, base, "base-x"), "base dials in");

    ServiceReply r;
    TEST_ASSERT(register_roundtrip(base, 1, kServiceKindBase, 0, 1, "127.0.0.1",
                                   9301, 0, true, r),
                "logical_id=0 answered");
    TEST_ASSERT(!r.accepted && r.code == kServiceRegisterRejectedInvalid,
                "logical_id=0 rejected (code 4)");
    TEST_ASSERT(register_roundtrip(base, 2, kServiceKindBase, 7, 0, "127.0.0.1",
                                   9301, 0, true, r),
                "incarnation=0 answered");
    TEST_ASSERT(!r.accepted && r.code == kServiceRegisterRejectedInvalid,
                "incarnation=0 rejected (code 4)");
    TEST_ASSERT(register_roundtrip(base, 3, 5, 7, 1, "127.0.0.1", 9301, 0, true,
                                   r),
                "unknown kind answered");
    TEST_ASSERT(!r.accepted && r.code == kServiceRegisterRejectedInvalid,
                "unknown kind rejected (code 4)");
    TEST_ASSERT(login.registry.registered_service_count() == 0,
                "invalid registers add no entry");

    // request_id=0 保留：无回执无登记（timeout 判负即通过）
    TEST_ASSERT(!register_roundtrip(base, 0, kServiceKindBase, 7, 1, "127.0.0.1",
                                    9301, 0, true, r, 500),
                "request_id=0 reserved: no reply");
    TEST_ASSERT(login.registry.registered_service_count() == 0,
                "request_id=0 adds no entry");
    return true;
#else
    std::cout << "SKIP (windows: link tcp base posix only)" << std::endl;
    return true;
#endif
}

bool test_register_malformed() {
#if !defined(_WIN32)
    ServerEnd login;
    TEST_ASSERT(login.start(), "register face starts");
    LinkEnd base;
    TEST_ASSERT(dial(login, base, "base-x"), "base dials in");

    TEST_ASSERT(base.send_truncated_register(1), "truncated frame sent");
    TEST_ASSERT(wait_rejected(login, base, 1),
                "truncated frame counted as rejected");
    TEST_ASSERT(login.registry.registered_service_count() == 0,
                "rejected frame touches no entry");

    // 拒收不堵链路：后续合法注册照常受理
    ServiceReply r;
    TEST_ASSERT(register_roundtrip(base, 2, kServiceKindBase, 7, 1, "127.0.0.1",
                                   9301, 0, true, r),
                "valid register after rejection answered");
    TEST_ASSERT(r.accepted && r.code == kServiceRegisterAccepted,
                "valid register accepted after rejected frame");
    TEST_ASSERT(login.server->rejectedFrames() == 1, "reject count stays at one");
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
        {"register_lifecycle", test_register_lifecycle},
        {"register_invalid", test_register_invalid},
        {"register_malformed", test_register_malformed},
    };
    int failed = 0;
    for (const auto& c : cases) {
        if (!c.fn()) {
            std::cout << "CASE FAILED: " << c.name << std::endl;
            ++failed;
        }
    }
    if (failed == 0) {
        std::cout << "LoginServiceRegisterTests: all "
                  << (sizeof(cases) / sizeof(cases[0])) << " cases passed"
                  << std::endl;
        return 0;
    }
    std::cout << "LoginServiceRegisterTests: " << failed << " case(s) failed"
              << std::endl;
    return 1;
}
