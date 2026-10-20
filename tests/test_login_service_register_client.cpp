// login 服务注册拨号客户端 e2e（L123 拨号次切片；契约 964/965 拨号侧）：
//   register_lifecycle：kUp 跃迁首注册（code 1，自报面入表 + peer 自报名）
//   → 心跳重发同代次幂等（code 2）→ set_ready 换自报面随心跳显形（登记表
//   ready 翻转）；
//   world_kind_and_stale：World 种类注册（与 Base 复合键并存）；旧代次
//   客户端（incarnation 更小）被受理面拒（code 3 原样记账），登记保持新代次；
//   config_guards：代次缺省自铸（非 0 入表）、logical_id 缺位 / 路由表不可读
//   start false、对端缺席（死端口）链路永不 up 且不空发 964。
// 断言事实源：客户端账面 state() + ServiceRegistry 观测面（日志不算）。
// 缝工厂两侧真实现直编（login_service_register_tests 惯例）；服务端自泵
// （M1ServiceRegisterServer 内置 10ms 泵线程），本线程泵客户端。
// Windows 下 link TCP 底座不进本批，用例本体空转。
#include "login/m1_service_register_client.hpp"
#include "login/m1_service_register_server.hpp"
#include "login/service_registry.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#ifndef APOLLO_TEST_ROUTE_JSON
#define APOLLO_TEST_ROUTE_JSON ""
#endif

namespace {
#if !defined(_WIN32)

using login::kServiceKindBase;
using login::kServiceKindWorld;
using login::kServiceRegisterAccepted;
using login::kServiceRegisterAcceptedIdempotent;
using login::kServiceRegisterRejectedStaleIncarnation;

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

const char* kRouteJson = APOLLO_TEST_ROUTE_JSON;

std::uint64_t now_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

// 受理端壳：registry（宿主持有）+ M1 受理面（port 0 内核分配，listenPort
// 读回；10ms 自泵线程）。
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

login::M1ServiceRegisterClientConfig base_config(std::uint16_t port) {
    login::M1ServiceRegisterClientConfig config;
    config.login_host = "127.0.0.1";
    config.login_port = port;
    config.route_json = kRouteJson;
    config.heartbeat_ms = 100;  // 心跳拍缩短：幂等刷新用例不等 5s 缺省
    return config;
}

std::unique_ptr<login::M1ServiceRegisterClient> make_client(
    login::M1ServiceRegisterClientConfig config) {
    auto client = login::makeM1ServiceRegisterClient(std::move(config));
    return client->start() ? std::move(client) : nullptr;
}

// 客户端泵至谓词成立（服务端自泵；本线程 2ms 拍冲客户端——kReliable 帧由
// 客户端 pump 冲线，回执记账同面）。
template <typename Pred>
bool wait_for(login::M1ServiceRegisterClient& client, Pred pred,
              int timeout_ms = 5000) {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        client.pump(now_ms());
        if (pred()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return pred();
}

#endif // !_WIN32

// 首注册（code 1）→ 心跳幂等刷新（code 2）→ 就绪面翻转随心跳显形。
bool test_register_lifecycle() {
#if !defined(_WIN32)
    ServerEnd login_end;
    TEST_ASSERT(login_end.start(), "server face starts");
    auto config = base_config(login_end.server->listenPort());
    config.service_kind = kServiceKindBase;
    config.logical_id = 7;
    config.incarnation = 5;  // 显式代次：幂等/旧代次用例的确定基准
    config.advertise_host = "10.0.0.7";
    config.advertise_port = 9201;
    config.compat = 12345;
    config.self_name = "base-7";
    auto client = make_client(config);
    TEST_ASSERT(client != nullptr, "client starts");

    TEST_ASSERT(wait_for(*client, [&] { return client->state().replied; }),
                "first register answered");
    const auto first = client->state();
    TEST_ASSERT(first.sent && first.replied && first.accepted,
                "first register accepted");
    TEST_ASSERT(first.code == kServiceRegisterAccepted, "first register code 1");
    TEST_ASSERT(first.service_kind == kServiceKindBase && first.logical_id == 7,
                "reply echoes identity");
    TEST_ASSERT(first.attempts == 1 && first.replies == 1, "one send one reply");
    TEST_ASSERT(client->linkUp(), "link stays up");

    const auto* entry = login_end.registry.find_service(kServiceKindBase, 7);
    TEST_ASSERT(entry != nullptr, "registry holds entry");
    TEST_ASSERT(entry->incarnation == 5 && entry->host == "10.0.0.7"
                    && entry->port == 9201 && entry->compat == 12345 && entry->ready,
                "self-report fields recorded");
    TEST_ASSERT(entry->peer == "base-7", "peer self-name recorded");

    // 心跳重发（100ms 拍）：同代次幂等刷新（code 2）
    TEST_ASSERT(wait_for(*client, [&] { return client->state().code ==
                                              kServiceRegisterAcceptedIdempotent; }),
                "heartbeat re-register idempotent (code 2)");
    TEST_ASSERT(client->state().accepted, "idempotent refresh accepted");
    TEST_ASSERT(client->state().attempts >= 2, "heartbeat resent 964");

    // 就绪面翻转随下次心跳显形（登记表自报面整体刷新）：以回执计数收敛为
    // 同步点——第 3 拍 964（ready=0）被受理并回执后，登记表已整体刷新。
    client->set_ready(false);
    TEST_ASSERT(wait_for(*client, [&] {
                    const auto s = client->state();
                    return s.attempts >= 3 && s.replies >= 3;
                }),
                "ready flip carried by next heartbeat");
    const auto* flipped = login_end.registry.find_service(kServiceKindBase, 7);
    TEST_ASSERT(flipped != nullptr && !flipped->ready,
                "ready flip lands in registry");

    client->stop();
    login_end.stop();
    return true;
#else
    std::cout << "SKIP (windows: link tcp base posix only)" << std::endl;
    return true;
#endif
}

// World 种类注册 + 旧代次客户端被拒（code 3 原样记账，登记不降级）。
bool test_world_kind_and_stale() {
#if !defined(_WIN32)
    ServerEnd login_end;
    TEST_ASSERT(login_end.start(), "server face starts");
    auto config = base_config(login_end.server->listenPort());
    config.service_kind = kServiceKindWorld;
    config.logical_id = 3;
    config.incarnation = 9;
    config.advertise_host = "10.0.0.3";
    config.advertise_port = 9101;
    config.self_name = "world-3";
    auto fresh = make_client(config);
    TEST_ASSERT(fresh != nullptr, "fresh world client starts");
    TEST_ASSERT(wait_for(*fresh, [&] { return fresh->state().replied; }),
                "fresh world register answered");
    TEST_ASSERT(fresh->state().accepted
                    && fresh->state().code == kServiceRegisterAccepted,
                "world register accepted (code 1)");
    TEST_ASSERT(fresh->state().service_kind == kServiceKindWorld,
                "world kind echoed");

    // 旧代次客户端（incarnation 8 < 9）：受理面拒，客户端原样记账
    auto stale_config = config;
    stale_config.incarnation = 8;
    stale_config.self_name = "world-3-old";
    auto stale = make_client(stale_config);
    TEST_ASSERT(stale != nullptr, "stale client starts");
    TEST_ASSERT(wait_for(*stale, [&] { return stale->state().replied; }),
                "stale register answered");
    TEST_ASSERT(!stale->state().accepted
                    && stale->state().code
                           == kServiceRegisterRejectedStaleIncarnation,
                "stale incarnation rejected (code 3)");
    const auto* entry = login_end.registry.find_service(kServiceKindWorld, 3);
    TEST_ASSERT(entry != nullptr && entry->incarnation == 9
                    && entry->peer == "world-3",
                "stale rejection keeps fresh entry");

    // 复合键并存：World 3 与 Base 7 两域互不覆盖
    auto base_cfg = base_config(login_end.server->listenPort());
    base_cfg.service_kind = kServiceKindBase;
    base_cfg.logical_id = 7;
    base_cfg.incarnation = 1;
    base_cfg.self_name = "base-7";
    auto base_client = make_client(base_cfg);
    TEST_ASSERT(base_client != nullptr, "base client starts");
    TEST_ASSERT(wait_for(*base_client, [&] { return base_client->state().replied; }),
                "base register answered");
    TEST_ASSERT(login_end.registry.registered_service_count() == 2,
                "kind namespaces coexist");

    fresh->stop();
    stale->stop();
    base_client->stop();
    login_end.stop();
    return true;
#else
    std::cout << "SKIP (windows: link tcp base posix only)" << std::endl;
    return true;
#endif
}

// 装配守卫与对端缺席：代次自铸、缺位拒起、路由表不可读拒起、死端口不空发。
bool test_config_guards() {
#if !defined(_WIN32)
    ServerEnd login_end;
    TEST_ASSERT(login_end.start(), "server face starts");

    // 代次缺省自铸：非 0 入表（重启必增的裁决前提）
    auto config = base_config(login_end.server->listenPort());
    config.service_kind = kServiceKindBase;
    config.logical_id = 11;
    config.self_name = "base-11";
    auto client = make_client(config);
    TEST_ASSERT(client != nullptr, "client with self-cast incarnation starts");
    TEST_ASSERT(wait_for(*client, [&] { return client->state().replied; }),
                "self-cast register answered");
    const auto* entry = login_end.registry.find_service(kServiceKindBase, 11);
    TEST_ASSERT(entry != nullptr && entry->incarnation != 0,
                "self-cast incarnation lands non-zero");
    client->stop();

    // logical_id 缺位：拒起（不带隐式回退）
    auto no_id = base_config(login_end.server->listenPort());
    no_id.service_kind = kServiceKindBase;
    TEST_ASSERT(!login::makeM1ServiceRegisterClient(no_id)->start(),
                "missing logical_id refused");

    // 路由表不可读：拒起
    auto bad_route = base_config(login_end.server->listenPort());
    bad_route.service_kind = kServiceKindBase;
    bad_route.logical_id = 12;
    bad_route.route_json = "/nonexistent/route.json";
    TEST_ASSERT(!login::makeM1ServiceRegisterClient(bad_route)->start(),
                "unreadable route table refused");

    // 对端缺席（端口 1 无人听）：链路永不 up，不空发 964（kUp 跃迁才发）
    auto dead = base_config(1);
    dead.service_kind = kServiceKindBase;
    dead.logical_id = 13;
    dead.self_name = "base-13";
    auto dead_client = make_client(dead);
    TEST_ASSERT(dead_client != nullptr, "dead-dial client starts");
    const auto t0 = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - t0 < std::chrono::milliseconds(400)) {
        dead_client->pump(now_ms());
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    TEST_ASSERT(!dead_client->linkUp(), "dead dial never links up");
    TEST_ASSERT(!dead_client->state().sent, "no 964 sent while link down");
    TEST_ASSERT(dead_client->state().attempts == 0, "no send attempts recorded");
    dead_client->stop();

    login_end.stop();
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
        {"world_kind_and_stale", test_world_kind_and_stale},
        {"config_guards", test_config_guards},
    };
    int failed = 0;
    for (const auto& c : cases) {
        if (!c.fn()) {
            std::cout << "CASE FAILED: " << c.name << std::endl;
            ++failed;
        }
    }
    if (failed == 0) {
        std::cout << "LoginServiceRegisterClientTests: all "
                  << (sizeof(cases) / sizeof(cases[0])) << " cases passed"
                  << std::endl;
        return 0;
    }
    std::cout << "LoginServiceRegisterClientTests: " << failed << " case(s) failed"
              << std::endl;
    return 1;
}
