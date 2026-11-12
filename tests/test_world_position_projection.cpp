// World 执行位置投影发布面 e2e（L115 位置投影首切片；契约 968）：
//   registry 水位裁决（纯表，无网络）：首投影受理（版本前进）→ 重复投影幂
//   等忽略（版本 == 水位）→ 乱序旧投影拒绝（版本 < 水位）→ 新投影替换（重
//   认领）→ 释放墓碑命中水位抹除（幂等——重复墓碑无操作）→ 乱序墓碑拒
//   （不抹新归属）→ 无认领墓碑拒 → 非法帧拒（world/space 缺位、claim 版本
//   0、ready/cell_id 不配对）→ 多 World 同号 Space 复合键独立；
//   e2e（真链路）：login 注册面（968 受理随面装配）+ World 发布客户端（本
//   线程泵）——claim 投影落目录 → 新版本替换 → 墓碑抹除 → 客户端断链重启
//   （kUp 跃迁全量镜像重放）目录同版本幂等恢复。
// 断言事实源：PositionProjectionRegistry 状态 + 裁决计数（日志不算）。
// 缝工厂真实现直编（login_service_register_tests 惯例）；服务端自泵
//（M1ServiceRegisterServer 内置 10ms 泵线程），本线程泵发布端。
// Windows 下 link TCP 底座不进本批，用例本体空转。
#include "login/m1_service_register_server.hpp"
#include "login/position_projection_registry.hpp"
#include "login/service_registry.hpp"
#include "world_app/m1_projection_client.hpp"

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

using login::PositionProjectionRegistry;
using login::ProjectionVerdict;
using login::SpacePositionProjection;
using world_app::M1PositionProjectionClient;
using world_app::M1PositionProjectionConfig;
using world_app::makeM1PositionProjectionClient;

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

std::uint64_t now_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

//==============================================================================
// Part A：registry 水位裁决（纯表，无网络）
//==============================================================================

// 首投影受理（版本前进，条目入表 + 水位记录）
bool test_registry_first_claim_applied() {
    PositionProjectionRegistry r;
    SpacePositionProjection p;
    p.world_id = 1;
    p.space_id = 501;
    p.cell_id = 21;
    p.assignment_version = 1;
    p.management_epoch = 3;
    p.ready = true;
    TEST_ASSERT(r.apply_projection(p, now_ms()) == ProjectionVerdict::kApplied,
                "first claim applied");
    const auto* e = r.find_projection(1, 501);
    TEST_ASSERT(e != nullptr, "entry recorded");
    TEST_ASSERT(e->cell_id == 21 && e->assignment_version == 1
                    && e->management_epoch == 3 && e->ready,
                "claim fields recorded");
    TEST_ASSERT(r.applied_total() == 1 && r.projection_count() == 1,
                "one applied one entry");
    return true;
}

// 重复投影（版本 == 水位）幂等忽略——条目不变
bool test_registry_repeat_idempotent() {
    PositionProjectionRegistry r;
    SpacePositionProjection p;
    p.world_id = 1;
    p.space_id = 501;
    p.cell_id = 21;
    p.assignment_version = 1;
    p.ready = true;
    TEST_ASSERT(r.apply_projection(p, now_ms()) == ProjectionVerdict::kApplied,
                "first claim applied");
    TEST_ASSERT(r.apply_projection(p, now_ms()) ==
                    ProjectionVerdict::kIdempotentIgnored,
                "repeat ignored");
    const auto* e = r.find_projection(1, 501);
    TEST_ASSERT(e != nullptr && e->cell_id == 21, "entry unchanged");
    TEST_ASSERT(r.idempotent_total() == 1 && r.applied_total() == 1,
                "counters split applied vs idempotent");
    return true;
}

// 乱序旧投影（版本 < 水位）拒绝——不覆盖新归属
bool test_registry_stale_claim_rejected() {
    PositionProjectionRegistry r;
    SpacePositionProjection v1;
    v1.world_id = 1;
    v1.space_id = 501;
    v1.cell_id = 21;
    v1.assignment_version = 1;
    v1.ready = true;
    TEST_ASSERT(r.apply_projection(v1, now_ms()) == ProjectionVerdict::kApplied,
                "v1 applied");
    SpacePositionProjection v2 = v1;
    v2.cell_id = 22;
    v2.assignment_version = 2;
    TEST_ASSERT(r.apply_projection(v2, now_ms()) == ProjectionVerdict::kApplied,
                "v2 (re-claim) applied");
    TEST_ASSERT(r.apply_projection(v1, now_ms()) == ProjectionVerdict::kStaleRejected,
                "stale v1 rejected");
    const auto* e = r.find_projection(1, 501);
    TEST_ASSERT(e != nullptr && e->cell_id == 22 && e->assignment_version == 2,
                "v2 ownership kept");
    TEST_ASSERT(r.stale_total() == 1, "stale counted");
    return true;
}

// 释放墓碑命中水位抹除；重复墓碑幂等；乱序墓碑不抹新归属
bool test_registry_tombstone_rules() {
    PositionProjectionRegistry r;
    SpacePositionProjection claim;
    claim.world_id = 1;
    claim.space_id = 501;
    claim.cell_id = 21;
    claim.assignment_version = 1;
    claim.ready = true;
    TEST_ASSERT(r.apply_projection(claim, now_ms()) == ProjectionVerdict::kApplied,
                "claim applied");
    SpacePositionProjection tomb = claim;
    tomb.cell_id = 0;
    tomb.ready = false;
    TEST_ASSERT(r.apply_projection(tomb, now_ms()) == ProjectionVerdict::kTombstoned,
                "tombstone at watermark erases");
    TEST_ASSERT(r.find_projection(1, 501) == nullptr, "entry erased");
    TEST_ASSERT(r.apply_projection(tomb, now_ms()) == ProjectionVerdict::kTombstoned,
                "duplicate tombstone idempotent");
    TEST_ASSERT(r.tombstoned_total() == 2, "both tombstones counted");
    // 乱序墓碑：重认领（v2）后旧墓碑（v1）不得抹新归属
    SpacePositionProjection reclaim = claim;
    reclaim.cell_id = 22;
    reclaim.assignment_version = 2;
    TEST_ASSERT(r.apply_projection(reclaim, now_ms()) == ProjectionVerdict::kApplied,
                "re-claim applied");
    TEST_ASSERT(r.apply_projection(tomb, now_ms()) == ProjectionVerdict::kStaleRejected,
                "stale tombstone rejected");
    const auto* e = r.find_projection(1, 501);
    TEST_ASSERT(e != nullptr && e->cell_id == 22, "new claim intact");
    // 无认领墓碑（水位 0）拒绝——未见认领的释放 = 重放分歧，拒不猜
    SpacePositionProjection ghost = claim;
    ghost.space_id = 999;
    ghost.cell_id = 0;
    ghost.ready = false;
    TEST_ASSERT(r.apply_projection(ghost, now_ms()) == ProjectionVerdict::kStaleRejected,
                "tombstone without claim rejected");
    TEST_ASSERT(r.find_projection(1, 999) == nullptr, "no ghost entry");
    return true;
}

// 非法帧：world/space 缺位、claim 版本 0、ready/cell_id 不配对
bool test_registry_invalid_rejected() {
    PositionProjectionRegistry r;
    SpacePositionProjection p;
    p.world_id = 1;
    p.space_id = 501;
    p.cell_id = 21;
    p.assignment_version = 1;
    p.ready = true;
    p.world_id = 0;
    TEST_ASSERT(r.apply_projection(p, now_ms()) == ProjectionVerdict::kInvalidRejected,
                "world_id 0 rejected");
    p.world_id = 1;
    p.space_id = 0;
    TEST_ASSERT(r.apply_projection(p, now_ms()) == ProjectionVerdict::kInvalidRejected,
                "space_id 0 rejected");
    p.space_id = 501;
    p.assignment_version = 0;
    TEST_ASSERT(r.apply_projection(p, now_ms()) == ProjectionVerdict::kInvalidRejected,
                "claim version 0 rejected");
    p.assignment_version = 1;
    p.cell_id = 0;
    TEST_ASSERT(r.apply_projection(p, now_ms()) == ProjectionVerdict::kInvalidRejected,
                "cell 0 with ready 1 rejected");
    p.cell_id = 21;
    p.ready = false;
    TEST_ASSERT(r.apply_projection(p, now_ms()) == ProjectionVerdict::kInvalidRejected,
                "cell nonzero with ready 0 rejected");
    TEST_ASSERT(r.invalid_total() == 5 && r.projection_count() == 0,
                "nothing recorded");
    return true;
}

// 多 World 同号 Space 复合键独立
bool test_registry_multi_world_keys() {
    PositionProjectionRegistry r;
    SpacePositionProjection a;
    a.world_id = 1;
    a.space_id = 501;
    a.cell_id = 21;
    a.assignment_version = 1;
    a.ready = true;
    TEST_ASSERT(r.apply_projection(a, now_ms()) == ProjectionVerdict::kApplied,
                "world 1 applied");
    SpacePositionProjection b = a;
    b.world_id = 2;
    b.cell_id = 22;
    TEST_ASSERT(r.apply_projection(b, now_ms()) == ProjectionVerdict::kApplied,
                "world 2 applied");
    TEST_ASSERT(r.projection_count() == 2, "two entries");
    const auto* ea = r.find_projection(1, 501);
    const auto* eb = r.find_projection(2, 501);
    TEST_ASSERT(ea && eb && ea->cell_id == 21 && eb->cell_id == 22,
                "keys independent");
    return true;
}

//==============================================================================
// Part B：真链路 e2e（login 注册面 968 受理 + World 发布客户端）
//==============================================================================

struct LoginEnd {
    login::ServiceRegistry registry;
    PositionProjectionRegistry projections;
    std::unique_ptr<login::M1ServiceRegisterServer> server;

    bool start() {
        login::M1ServiceRegisterConfig config;
        config.host = "127.0.0.1";
        config.port = 0;
        config.route_json = APOLLO_TEST_ROUTE_JSON;
        server = login::makeM1ServiceRegisterServer(std::move(config), &registry,
                                                    &projections);
        return server && server->start() && server->listenPort() != 0;
    }
};

std::unique_ptr<M1PositionProjectionClient> make_client(std::uint16_t port,
                                                        std::int32_t world_id) {
    M1PositionProjectionConfig config;
    config.login_host = "127.0.0.1";
    config.login_port = port;
    config.world_id = world_id;
    config.route_json = APOLLO_TEST_ROUTE_JSON;
    config.self_name = "world-proj-" + std::to_string(world_id);
    auto client = makeM1PositionProjectionClient(std::move(config));
    return client->start() ? std::move(client) : nullptr;
}

// 发布端泵至谓词成立（服务端自泵；本线程 2ms 拍冲发布端——968 帧由客户端
// pump 冲线，目录受理在服务端泵拍）。
template <typename Pred>
bool wait_for(M1PositionProjectionClient& client, Pred pred,
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

// claim 投影落目录 → 新版本替换 → 墓碑抹除
bool test_e2e_projection_lifecycle() {
    LoginEnd login_end;
    TEST_ASSERT(login_end.start(), "login face starts");
    auto client = make_client(login_end.server->listenPort(), 1);
    TEST_ASSERT(client != nullptr, "publisher starts");
    TEST_ASSERT(wait_for(*client, [&] { return client->linkUp(); }),
                "publisher link up");

    // claim：journal A 行提交态推目录
    client->publish(501, 21, 1, 3, true);
    TEST_ASSERT(wait_for(*client, [&] {
                    const auto* e = login_end.projections.find_projection(1, 501);
                    return e != nullptr && e->cell_id == 21;
                }),
                "claim projected into directory");
    const auto* e = login_end.projections.find_projection(1, 501);
    TEST_ASSERT(e->assignment_version == 1 && e->management_epoch == 3 && e->ready,
                "claim fields land");
    TEST_ASSERT(login_end.projections.applied_total() == 1, "one applied");

    // 重认领：版本前进替换
    client->publish(501, 22, 2, 3, true);
    TEST_ASSERT(wait_for(*client, [&] {
                    const auto* f = login_end.projections.find_projection(1, 501);
                    return f != nullptr && f->cell_id == 22;
                }),
                "re-claim replaces entry");

    // 释放墓碑：目录抹除
    client->publish(501, 0, 2, 3, false);
    TEST_ASSERT(wait_for(*client,
                         [&] { return login_end.projections.find_projection(1, 501) == nullptr; }),
                "tombstone erases directory entry");
    TEST_ASSERT(login_end.projections.tombstoned_total() == 1,
                "tombstone counted");
    client->stop();
    return true;
}

// 断链重启：kUp 跃迁全量镜像重放——目录同版本幂等恢复条目
bool test_e2e_reconnect_replays_mirror() {
    LoginEnd login_end;
    TEST_ASSERT(login_end.start(), "login face starts");
    auto client = make_client(login_end.server->listenPort(), 7);
    TEST_ASSERT(client != nullptr, "publisher starts");
    TEST_ASSERT(wait_for(*client, [&] { return client->linkUp(); }),
                "first link up");
    client->publish(502, 31, 1, 5, true);
    TEST_ASSERT(wait_for(*client, [&] {
                    const auto* e = login_end.projections.find_projection(7, 502);
                    return e != nullptr && e->cell_id == 31;
                }),
                "claim lands");
    const auto applied_before = login_end.projections.applied_total();

    // 客户端断链重启（新 LinkManager，镜像保留）：kUp 重放全量视图——目录
    // 同版本幂等忽略（条目原样保持，不重复记账 applied）。
    client->stop();
    TEST_ASSERT(client->start(), "publisher restarts");
    TEST_ASSERT(wait_for(*client, [&] { return client->linkUp(); }),
                "reconnect up");
    TEST_ASSERT(wait_for(*client, [&] { return client->state().replays >= 2; }),
                "mirror replayed on kUp");
    const auto* e = login_end.projections.find_projection(7, 502);
    TEST_ASSERT(e != nullptr && e->cell_id == 31 && e->assignment_version == 1,
                "entry survives replay");
    TEST_ASSERT(login_end.projections.applied_total() == applied_before,
                "replay idempotent (no double bookkeeping)");
    client->stop();
    return true;
}

#endif // !_WIN32

} // namespace

int main() {
    int passed = 0, total = 0;
    auto run = [&](const char* name, bool (*fn)()) {
        std::cout << "== " << name << " ==" << std::endl;
        ++total;
        if (fn()) { ++passed; std::cout << "  PASSED" << std::endl; }
    };
#ifndef _WIN32
    run("registry_first_claim_applied", test_registry_first_claim_applied);
    run("registry_repeat_idempotent", test_registry_repeat_idempotent);
    run("registry_stale_claim_rejected", test_registry_stale_claim_rejected);
    run("registry_tombstone_rules", test_registry_tombstone_rules);
    run("registry_invalid_rejected", test_registry_invalid_rejected);
    run("registry_multi_world_keys", test_registry_multi_world_keys);
    run("e2e_projection_lifecycle", test_e2e_projection_lifecycle);
    run("e2e_reconnect_replays_mirror", test_e2e_reconnect_replays_mirror);
#endif
    std::cout << "=== Results: " << passed << "/" << total << " passed ==="
              << std::endl;
    return g_failures == 0 ? 0 : 1;
}
