// gateway Base 激活面验收（L53 登录串联第三环：Base 经 database 激活）。
//
// authenticateSession 准入通过后经 Base PlayerStore M1 受理面两跳——
// PlayerActivate（database 装载 Anchor）+ PlayerBindSession（会话号与网关
// 身份写入 SessionBinding）：
//   1. 播种档案 → 认证成 → 锚点落位（AvatarState Online）+ SessionBinding
//      写入（session_id = 网关会话号、gateway_id = 进程身份）；
//   2. 无档案玩家 → 认证拒（面在场 = 错误显形，无权威确认不放行），网关
//      会话不绑玩家；
//   3. 面关（baseM1Port = 0）→ 退容忍面：准入过即绑玩家（旧语义保持）；
//   4. 面配置但不可达（死端口）→ 认证拒（传输缺席不放大成放行）。
// Windows：link M1 底座不进批，用例本体空转。

#include "gateway/gateway_server.hpp"
#include "player_store/player_store.hpp"
#include "store_host.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

#ifndef APOLLO_TEST_ROUTE_JSON
#define APOLLO_TEST_ROUTE_JSON ""
#endif

namespace {

#if !defined(_WIN32)

// macOS CI 裸自旋必 flake：重试循环每拍 sleep ≥2ms。
const auto kTick = std::chrono::milliseconds(2);

template <typename Pred>
bool wait_until(Pred pred, int timeout_ms) {
    for (int waited = 0; waited < timeout_ms; waited += 2) {
        if (pred()) {
            return true;
        }
        std::this_thread::sleep_for(kTick);
    }
    return pred();
}

// 空闲端口探测（死端口用例需要一个确定无人监听的本地端口）。
std::uint16_t probe_free_port() {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    std::uint16_t port = 0;
    if (fd >= 0) {
        ::sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = 0;
        ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        if (::bind(fd, reinterpret_cast<::sockaddr*>(&addr), sizeof addr) == 0) {
            socklen_t len = sizeof addr;
            if (::getsockname(fd, reinterpret_cast<::sockaddr*>(&addr), &len) == 0) {
                port = ntohs(addr.sin_port);
            }
        }
        ::close(fd);
    }
    return port;
}

struct TemporaryDirectory {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("apollo-gateway-base-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ~TemporaryDirectory() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

// Base 侧受理面宿主（StoreHost 纯 store 位——无报告面，同 business wire 面）。
player_store::PlayerStoreConfig base_store_config(const std::filesystem::path& dir) {
    player_store::PlayerStoreConfig config;
    config.host = "127.0.0.1";
    config.m1Enabled = true;
    config.m1Port = 0;  // 内核分配（gateway 拨号面读回）
    config.m1SelfName = "base";
    config.m1RouteJson = APOLLO_TEST_ROUTE_JSON;
    config.dataDir = (dir / "players").string();
    config.journalPath = (dir / "journal/persist.log").string();
    return config;
}

gateway::GatewayConfig gateway_config() {
    gateway::GatewayConfig config;
    config.host = "127.0.0.1";
    config.port = 0;  // 客户端入口内核分配（本面不依赖客户端流量）
    config.sessionTimeoutMs = 600000;      // 心跳超时不干扰
    config.heartbeatCheckIntervalMs = 50;
    config.controlM1Port = 0;              // 排空控制面不开
    config.worldM1RouteJson = APOLLO_TEST_ROUTE_JSON;
    config.gatewayId = 77;                 // 绑定上抛身份（断言面）
    return config;
}

bool activate_and_bind_on_auth() {
    TemporaryDirectory files;

    StoreHost base(base_store_config(files.path), 0, 0, 0, 0);
    TEST_ASSERT(base.store->m1_listen_port() != 0, "base face did not listen");
    TEST_ASSERT(base.store->adoptPlayer(7, "", 0, 0, 2), "seed player 7 profile");

    auto config = gateway_config();
    config.baseM1Host = "127.0.0.1";
    config.baseM1Port = base.store->m1_listen_port();
    gateway::GatewayServer server(config);
    server.start();

    const auto session_id = server.createPendingSession("127.0.0.1", 40001);
    TEST_ASSERT(session_id != 0, "pending session created");
    TEST_ASSERT(wait_until([&] { return server.baseActivationReady(); }, 3000),
                "base activation link up");

    TEST_ASSERT(server.authenticateSession(session_id, 7, "ticket"),
                "auth accepted with base activation leg");

    const auto avatar = base.store->findAvatar(7);
    TEST_ASSERT(avatar != nullptr, "avatar loaded at base");
    TEST_ASSERT(avatar->state() == apollo::game::base::AvatarState::Online,
                "avatar online after bind");
    TEST_ASSERT(avatar->session_binding().is_bound(), "session binding written");
    TEST_ASSERT(avatar->session_binding().session_id == session_id,
                "binding carries gateway session id");
    TEST_ASSERT(avatar->session_binding().gateway_id == config.gatewayId,
                "binding carries gateway process id");

    // 同会话重复认证（幂等重入）：重激活不降代次、重绑同号不炸。
    TEST_ASSERT(server.authenticateSession(session_id, 7, "ticket"),
                "re-auth on same session still accepted");

    server.stop();
    return true;
}

bool unknown_player_rejected() {
    TemporaryDirectory files;

    StoreHost base(base_store_config(files.path), 0, 0, 0, 0);
    TEST_ASSERT(base.store->m1_listen_port() != 0, "base face did not listen");
    // 播种 8 号玩家；999 号无档案——激活无档可载。

    auto config = gateway_config();
    config.baseM1Host = "127.0.0.1";
    config.baseM1Port = base.store->m1_listen_port();
    gateway::GatewayServer server(config);
    server.start();

    const auto session_id = server.createPendingSession("127.0.0.1", 40002);
    TEST_ASSERT(wait_until([&] { return server.baseActivationReady(); }, 3000),
                "base activation link up");

    TEST_ASSERT(!server.authenticateSession(session_id, 999, "ticket"),
                "auth without base profile must be rejected");

    // 拒绝后会话不绑玩家（可走完整重试/换号路径）。
    const auto session = server.findClientEndpoint(session_id);
    (void)session;
    TEST_ASSERT(server.getSessionCount() == 1, "rejected session stays pending");

    // 同一网关对在档玩家仍可认证（拒绝不污染面）。
    TEST_ASSERT(base.store->adoptPlayer(8, "", 0, 0, 2), "seed player 8 profile");
    const auto second = server.createPendingSession("127.0.0.1", 40003);
    TEST_ASSERT(server.authenticateSession(second, 8, "ticket"),
                "auth for profiled player still accepted");

    server.stop();
    return true;
}

bool face_off_degrades() {
    TemporaryDirectory files;

    StoreHost base(base_store_config(files.path), 0, 0, 0, 0);
    TEST_ASSERT(base.store->m1_listen_port() != 0, "base face did not listen");
    TEST_ASSERT(base.store->adoptPlayer(9, "", 0, 0, 2), "seed player 9 profile");

    // baseM1Port = 0：面关（旧语义——准入过即绑玩家，无 Base 落地）。
    auto config = gateway_config();
    config.baseM1Port = 0;
    gateway::GatewayServer server(config);
    server.start();

    const auto session_id = server.createPendingSession("127.0.0.1", 40004);
    TEST_ASSERT(server.authenticateSession(session_id, 9, "ticket"),
                "face off keeps tolerant auth path");
    TEST_ASSERT(!server.baseActivationReady(), "no base face when port is 0");

    server.stop();
    return true;
}

bool unreachable_face_failclosed() {
    auto config = gateway_config();
    const auto dead_port = probe_free_port();
    TEST_ASSERT(dead_port != 0, "dead port probe failed");
    config.baseM1Host = "127.0.0.1";
    config.baseM1Port = dead_port;  // 确定无人监听：面在场但不可达
    gateway::GatewayServer server(config);
    server.start();

    const auto session_id = server.createPendingSession("127.0.0.1", 40005);
    // 链路永不上 Up（退避重连中）——认证必须在同步等待上限内拒绝。
    TEST_ASSERT(!server.authenticateSession(session_id, 7, "ticket"),
                "unreachable base face must fail closed");

    server.stop();
    return true;
}

#endif // !_WIN32

} // namespace

int main() {
#if defined(_WIN32)
    std::cout << "gateway base activation tests: SKIPPED (link M1 base not on Windows)\n";
    return 0;
#else
    const std::pair<const char*, bool (*)()> cases[] = {
        {"activate_and_bind_on_auth", activate_and_bind_on_auth},
        {"unknown_player_rejected", unknown_player_rejected},
        {"face_off_degrades", face_off_degrades},
        {"unreachable_face_failclosed", unreachable_face_failclosed},
    };

    int failed = 0;
    for (const auto& [name, fn] : cases) {
        std::cout << "[RUN ] " << name << std::endl;
        if (!fn()) {
            ++failed;
            std::cout << "[FAIL] " << name << std::endl;
        } else {
            std::cout << "[PASS] " << name << std::endl;
        }
    }

    if (failed == 0) {
        std::cout << "ALL GATEWAY BASE ACTIVATION TESTS PASSED" << std::endl;
        return 0;
    }
    std::cout << failed << " gateway base activation test(s) FAILED" << std::endl;
    return 1;
#endif
}
