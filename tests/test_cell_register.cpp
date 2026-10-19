// cell 执行进程注册端到端（L101 注册首切片；契约 960/961）：
//   lifecycle：首注册受理（回带管理代次 + 确认 cell_id，自报面入表）→
//   同 ID 同代次重注册幂等刷新（自报面更新、登记数不变）→ 新代次替换
//   （重启换代）→ 旧代次拒绝（stale 不可覆盖，登记保持新代次）→ 第二
//   cell 并存；
//   invalid：cell_id=0 / start_epoch=0 显式驳回（code 4，不入表），
//   request_id=0 保留（无回执无登记）；
//   malformed：残帧拒收计数（注册表不触碰，拒收不堵链路）。
// 断言事实源：961 回执内容 + WorldApp 注册表观测面（日志不算）。
// 服务端 gameLoop 自泵，本线程泵客户端（同 test_world_drain 惯例）。
// 无 GTest，plain-assert + main 汇总。
#include "world_app/world_app.hpp"
#include "world_app/cell_register_client.hpp"
#include "world_app/m1_link_server.hpp"
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
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
namespace link = apollo::net::link;
namespace wire = apollo::net::wire;
using namespace std::chrono_literals;
using world_app::WorldApp;
using world_app::WorldAppConfig;
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

constexpr std::uint16_t id_of(MsgId id) { return static_cast<std::uint16_t>(id); }
constexpr std::uint16_t kRegisterRequestId = id_of(MsgId::cell_register_request);
constexpr std::uint16_t kRegisterReplyId = id_of(MsgId::cell_register_reply);
const char* kRouteJson = APOLLO_TEST_ROUTE_JSON;

std::uint64_t now_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

// 961 回执账（cell dispatcher 写；register_roundtrip 读取；测试串行，指针注入）。
struct RegisterReply {
    bool accepted = false;
    std::int32_t code = 0;
    std::uint64_t management_epoch = 0;
    std::uint64_t assigned_cell_id = 0;
    std::string detail;
};

std::mutex g_replies_mutex;
std::map<std::uint64_t, RegisterReply>* g_replies = nullptr;

// cell 侧投影：cell→World M1 控制受理面（裸 960 上行，961 按 request_id
// 收账）。peer 自报名 = 链路名（服务端按名回链路与记入注册表）。
struct CellEnd {
    std::unique_ptr<link::RouteTable> table;
    std::unique_ptr<link::LinkManager> manager;

    bool start(std::uint16_t port) {
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
        mc.self_name = "cell-x";
        mc.route_version = link::route_version_from_hash(table->schema_hash());
        manager = std::make_unique<link::LinkManager>(mc);

        link::EndpointConfig ep;
        ep.name = "world";
        ep.host = "127.0.0.1";
        ep.port = port;
        ep.role = link::EndpointRole::kActive;
        if (!manager->add_endpoint(ep)) {
            return false;
        }
        auto& dispatcher = manager->link("world")->dispatcher();
        dispatcher.set_route_table(table.get());
        dispatcher.allow_domain("internal");
        dispatcher.set_handler(kRegisterReplyId,
                               [](std::uint16_t, const std::uint8_t* data,
                                  std::size_t size) {
                                   std::vector<wire::FieldValue> fields;
                                   if (!wire::decode_payload(kRegisterReplyId, data, size,
                                                             fields)
                                       || fields.size() != 6) {
                                       return;
                                   }
                                   RegisterReply r;
                                   r.accepted = fields[1].integer != 0;
                                   r.code = static_cast<std::int32_t>(fields[2].integer);
                                   r.management_epoch =
                                       static_cast<std::uint64_t>(fields[3].integer);
                                   r.assigned_cell_id =
                                       static_cast<std::uint64_t>(fields[4].integer);
                                   r.detail = fields[5].text;
                                   const auto request_id =
                                       static_cast<std::uint64_t>(fields[0].integer);
                                   std::lock_guard<std::mutex> lock(g_replies_mutex);
                                   if (g_replies != nullptr) {
                                       (*g_replies)[request_id] = std::move(r);
                                   }
                               });
        return true;
    }

    void pump() { manager->pump(now_ms()); }

    bool up() { return manager->link("world")->state() == link::LinkState::kUp; }

    bool send_register(std::uint64_t request_id, std::uint64_t cell_id,
                       std::uint64_t start_epoch, const std::string& host,
                       std::int32_t port, std::int32_t capabilities, std::int32_t load,
                       std::int32_t drain_state, bool ready) {
        std::vector<wire::FieldValue> f(9);
        f[0].integer = static_cast<std::int64_t>(request_id);
        f[1].integer = static_cast<std::int64_t>(cell_id);
        f[2].integer = static_cast<std::int64_t>(start_epoch);
        f[3].text = host;
        f[4].integer = port;
        f[5].integer = capabilities;
        f[6].integer = load;
        f[7].integer = drain_state;
        f[8].integer = ready ? 1 : 0;
        std::vector<std::uint8_t> payload;
        if (!wire::encode_payload(kRegisterRequestId, f, payload)) {
            return false;
        }
        return manager->link("world")
                   ->send(kRegisterRequestId, payload.data(), payload.size(),
                          link::SendClass::kReliable)
               == link::SendCode::kAccepted;
    }

    // 残帧：合法 960 载荷截断（decode 必败 → 服务端拒收计数）。
    bool send_truncated_register(std::uint64_t request_id) {
        std::vector<wire::FieldValue> f(9);
        f[0].integer = static_cast<std::int64_t>(request_id);
        f[1].integer = 5;
        f[2].integer = 1;
        f[3].text = "127.0.0.1";
        f[4].integer = 9201;
        f[5].integer = 1;
        f[6].integer = 10;
        f[7].integer = 0;
        f[8].integer = 1;
        std::vector<std::uint8_t> payload;
        if (!wire::encode_payload(kRegisterRequestId, f, payload)) {
            return false;
        }
        payload.resize(payload.size() / 2);  // 截断 → decode 失败
        return manager->link("world")
                   ->send(kRegisterRequestId, payload.data(), payload.size(),
                          link::SendClass::kReliable)
               == link::SendCode::kAccepted;
    }
};

// 960 单发 + 961 等（request_id 关联；服务端 gameLoop 自泵，本线程泵客户端）。
// 期望无回执的用例（request_id=0 保留）靠 timeout 判负。
bool register_roundtrip(CellEnd& cell, std::uint64_t request_id, std::uint64_t cell_id,
                        std::uint64_t start_epoch, const std::string& host,
                        std::int32_t port, std::int32_t capabilities, std::int32_t load,
                        std::int32_t drain_state, bool ready, RegisterReply& out,
                        int timeout_ms = 5000) {
    if (!cell.send_register(request_id, cell_id, start_epoch, host, port, capabilities,
                            load, drain_state, ready)) {
        return false;
    }
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        cell.pump();
        {
            std::lock_guard<std::mutex> lock(g_replies_mutex);
            const auto it = g_replies->find(request_id);
            if (it != g_replies->end()) {
                out = it->second;
                return true;
            }
        }
        std::this_thread::sleep_for(10ms);
    }
    return false;
}

// 服务端壳：WorldApp（m1Port 0 内核分配；档案宿主已迁 base）。
// m1AuthorityEpoch = 管理代次注入（§5.7 透传；961 回带断言面）。
struct WorldEnd {
    WorldAppConfig config;
    std::unique_ptr<WorldApp> world;

    bool start(std::uint32_t world_id) {
        config.worldId = world_id;
        config.host = "127.0.0.1";
        config.m1Port = 0;
        config.m1RouteJson = kRouteJson;
        config.m1AuthorityEpoch = 77;
        config.tickRateMs = 20;
        try {
            world = std::make_unique<WorldApp>(config);
            world->start();
        } catch (const std::exception& e) {
            std::cout << "  world start failed: " << e.what() << std::endl;
            return false;
        }
        return world->m1Link().listenPort() != 0;
    }

    void stop() {
        if (world) {
            world->stop();
            world.reset();
        }
    }
};

// 拨装：cell（960 控制面）拨 WorldApp M1 端点并等待链路 up。
bool dial_cell(WorldEnd& server, CellEnd& cell) {
    if (!cell.start(server.world->m1Link().listenPort())) {
        return false;
    }
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (!cell.up()) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        cell.pump();
        std::this_thread::sleep_for(10ms);
    }
    return true;
}

// ---------- lifecycle：代次规则（同代次幂等 / 新代次替换 / 旧代次拒）----------

bool test_cell_register_lifecycle() {
    std::cout << "Running: test_cell_register_lifecycle..." << std::endl;
    WorldEnd server;
    TEST_ASSERT(server.start(111), "world started with M1 port");
    std::map<std::uint64_t, RegisterReply> replies;
    g_replies = &replies;
    CellEnd cell;
    TEST_ASSERT(dial_cell(server, cell), "cell dialed");

    // 首注册：受理 + 回带管理代次 + 确认 cell_id
    RegisterReply r;
    TEST_ASSERT(register_roundtrip(cell, 9001, 7, 1, "127.0.0.1", 9201, 1, 30, 0, true,
                                   r),
                "first register reply arrived");
    TEST_ASSERT(r.accepted, "first accepted");
    TEST_ASSERT(r.code == world_app::kCellRegisterAccepted, "first code accepted");
    TEST_ASSERT(r.management_epoch == 77, "management epoch echoed");
    TEST_ASSERT(r.assigned_cell_id == 7, "cell id confirmed");
    TEST_ASSERT(server.world->registered_cell_count() == 1, "one registration");
    const auto* reg = server.world->find_cell_registration(7);
    TEST_ASSERT(reg != nullptr, "registration recorded");
    TEST_ASSERT(reg->start_epoch == 1, "epoch recorded");
    TEST_ASSERT(reg->host == "127.0.0.1", "host recorded");
    TEST_ASSERT(reg->port == 9201, "port recorded");
    TEST_ASSERT(reg->capabilities == 1, "capabilities recorded");
    TEST_ASSERT(reg->load == 30, "load recorded");
    TEST_ASSERT(reg->drain_state == 0, "drain state recorded");
    TEST_ASSERT(reg->ready, "ready recorded");
    TEST_ASSERT(reg->registered_at_ms != 0, "registered_at recorded");
    TEST_ASSERT(reg->peer == "cell-x", "peer recorded");

    // 同代次重注册：幂等刷新（自报面更新，登记数不变，代次不变）
    TEST_ASSERT(register_roundtrip(cell, 9002, 7, 1, "127.0.0.1", 9201, 1, 55, 1, false,
                                   r),
                "same-epoch reply arrived");
    TEST_ASSERT(r.accepted, "same-epoch accepted");
    TEST_ASSERT(r.code == world_app::kCellRegisterAcceptedIdempotent,
                "same-epoch code idempotent");
    TEST_ASSERT(server.world->registered_cell_count() == 1, "count unchanged");
    reg = server.world->find_cell_registration(7);
    TEST_ASSERT(reg != nullptr, "registration still present");
    TEST_ASSERT(reg->start_epoch == 1, "epoch unchanged");
    TEST_ASSERT(reg->load == 55, "load refreshed");
    TEST_ASSERT(reg->drain_state == 1, "drain refreshed");
    TEST_ASSERT(!reg->ready, "ready refreshed");

    // 新代次替换（重启换代）
    TEST_ASSERT(register_roundtrip(cell, 9003, 7, 3, "127.0.0.1", 9201, 1, 10, 0, true,
                                   r),
                "new-epoch reply arrived");
    TEST_ASSERT(r.accepted && r.code == world_app::kCellRegisterAccepted,
                "new epoch accepted");
    reg = server.world->find_cell_registration(7);
    TEST_ASSERT(reg != nullptr && reg->start_epoch == 3, "epoch replaced");

    // 旧代次拒绝：不可覆盖新登记（代次与自报面均保持）
    TEST_ASSERT(register_roundtrip(cell, 9004, 7, 2, "10.0.0.9", 9999, 9, 99, 2, false,
                                   r),
                "stale reply arrived");
    TEST_ASSERT(!r.accepted, "stale rejected");
    TEST_ASSERT(r.code == world_app::kCellRegisterRejectedStaleEpoch, "stale code");
    TEST_ASSERT(r.detail.find("stale") != std::string::npos, "stale detail");
    reg = server.world->find_cell_registration(7);
    TEST_ASSERT(reg != nullptr && reg->start_epoch == 3, "epoch not downgraded");
    TEST_ASSERT(reg != nullptr && reg->load == 10, "load not overwritten");
    TEST_ASSERT(reg != nullptr && reg->host == "127.0.0.1", "host not overwritten");

    // 第二 cell 并存
    TEST_ASSERT(register_roundtrip(cell, 9005, 8, 1, "127.0.0.1", 9202, 2, 0, 0, false,
                                   r),
                "second cell reply arrived");
    TEST_ASSERT(r.accepted && r.code == world_app::kCellRegisterAccepted,
                "second accepted");
    TEST_ASSERT(server.world->registered_cell_count() == 2, "two registrations");
    TEST_ASSERT(server.world->find_cell_registration(8) != nullptr, "second recorded");

    g_replies = nullptr;
    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- invalid：身份/代次非法显式驳回；request_id=0 保留 ----------

bool test_cell_register_invalid_and_reserved() {
    std::cout << "Running: test_cell_register_invalid_and_reserved..." << std::endl;
    WorldEnd server;
    TEST_ASSERT(server.start(112), "world started with M1 port");
    std::map<std::uint64_t, RegisterReply> replies;
    g_replies = &replies;
    CellEnd cell;
    TEST_ASSERT(dial_cell(server, cell), "cell dialed");

    // request_id = 0 保留：无回执、无登记（500ms 窗口判负）
    RegisterReply r;
    TEST_ASSERT(!register_roundtrip(cell, 0, 9, 1, "127.0.0.1", 9201, 1, 10, 0, true, r,
                                    500),
                "request_id 0 silent");
    TEST_ASSERT(server.world->registered_cell_count() == 0, "no registration");

    // cell_id = 0：显式驳回（逻辑 ID 缺位——PID/IP 不当身份）
    TEST_ASSERT(register_roundtrip(cell, 9006, 0, 1, "127.0.0.1", 9201, 1, 10, 0, true,
                                   r),
                "invalid cell_id reply arrived");
    TEST_ASSERT(!r.accepted, "invalid rejected");
    TEST_ASSERT(r.code == world_app::kCellRegisterRejectedInvalid, "invalid code");
    TEST_ASSERT(r.detail.find("invalid") != std::string::npos, "invalid detail");

    // start_epoch = 0：显式驳回（代次缺位）
    TEST_ASSERT(register_roundtrip(cell, 9007, 9, 0, "127.0.0.1", 9201, 1, 10, 0, true,
                                   r),
                "invalid epoch reply arrived");
    TEST_ASSERT(!r.accepted, "invalid epoch rejected");
    TEST_ASSERT(r.code == world_app::kCellRegisterRejectedInvalid, "invalid epoch code");

    TEST_ASSERT(server.world->registered_cell_count() == 0, "registry untouched");
    TEST_ASSERT(server.world->find_cell_registration(9) == nullptr, "no cell 9");

    g_replies = nullptr;
    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- malformed：残帧拒收计数（注册表不触碰，拒收不堵链路）----------

bool test_cell_register_malformed_rejected() {
    std::cout << "Running: test_cell_register_malformed_rejected..." << std::endl;
    WorldEnd server;
    TEST_ASSERT(server.start(113), "world started with M1 port");
    std::map<std::uint64_t, RegisterReply> replies;
    g_replies = &replies;
    CellEnd cell;
    TEST_ASSERT(dial_cell(server, cell), "cell dialed");

    const auto baseline = server.world->m1Link().rejectedFrames();
    TEST_ASSERT(cell.send_truncated_register(9500), "truncated frame sent");
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (server.world->m1Link().rejectedFrames() == baseline) {
        cell.pump();
        if (std::chrono::steady_clock::now() > deadline) {
            break;
        }
        std::this_thread::sleep_for(10ms);
    }
    TEST_ASSERT(server.world->m1Link().rejectedFrames() == baseline + 1,
                "malformed counted once");
    TEST_ASSERT(server.world->registered_cell_count() == 0, "registry untouched");
    TEST_ASSERT(server.world->find_cell_registration(5) == nullptr, "no ghost cell");

    // 拒收后续照常受理（拒收不堵链路）
    RegisterReply r;
    TEST_ASSERT(register_roundtrip(cell, 9501, 6, 1, "127.0.0.1", 9201, 1, 5, 0, true,
                                   r),
                "register still works after reject");
    TEST_ASSERT(r.accepted, "accepted after reject");

    g_replies = nullptr;
    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- production client：真 CellRegisterClient 拨装注册 + 重拨幂等 ----------

template <typename Pred>
bool wait_until(Pred&& done, int timeout_ms) {
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (done()) return true;
        std::this_thread::sleep_for(10ms);
    }
    return done();
}

bool test_production_client_registers_and_redials() {
    std::cout << "Running: test_production_client_registers_and_redials..." << std::endl;
    WorldEnd server;
    TEST_ASSERT(server.start(114), "world started with M1 port");

    world_app::CellRegisterConfig cfg;
    cfg.world_host = "127.0.0.1";
    cfg.world_port = server.world->m1Link().listenPort();
    cfg.cell_id = 42;
    cfg.advertise_host = "127.0.0.1";
    cfg.advertise_port = 8801;
    cfg.start_epoch = 5;
    cfg.route_json = kRouteJson;
    auto client = world_app::makeCellRegisterClient(cfg);
    TEST_ASSERT(client->start(), "client started");
    // 服务端 gameLoop 自泵受理，本线程泵客户端（同 test_world_drain 惯例）。
    TEST_ASSERT(wait_until([&] {
                    client->pump(now_ms());
                    return server.world->find_cell_registration(42) != nullptr
                           && client->state().replies >= 1;
                }, 5000),
                "first registration roundtrip");
    TEST_ASSERT(client->linkUp(), "client link up");

    // 回执记账：受理裁决原样显形（码/管理代次/确认 ID）
    const auto st = client->state();
    TEST_ASSERT(st.sent, "request sent");
    TEST_ASSERT(st.accepted, "accepted");
    TEST_ASSERT(st.code == world_app::kCellRegisterAccepted, "code accepted");
    TEST_ASSERT(st.management_epoch == 77, "management epoch echoed");
    TEST_ASSERT(st.assigned_cell_id == 42, "cell id confirmed");
    TEST_ASSERT(st.attempts == 1 && st.replies == 1, "attempt/reply counted once");

    // World 端注册表：自报面 + peer 自报名入表
    const auto* reg = server.world->find_cell_registration(42);
    TEST_ASSERT(reg != nullptr, "registry entry");
    TEST_ASSERT(reg->start_epoch == 5, "epoch recorded");
    TEST_ASSERT(reg->host == "127.0.0.1", "advertise host recorded");
    TEST_ASSERT(reg->port == 8801, "advertise port recorded");
    TEST_ASSERT(reg->drain_state == 0 && reg->ready, "serving and ready recorded");
    TEST_ASSERT(reg->peer == "cell-42", "peer self name recorded");
    TEST_ASSERT(server.world->registered_cell_count() == 1, "one registration");

    // 重拨（断链重建，同代次）→ 幂等刷新 code 2；自报排空态随行更新
    client->setReportedDrainState(1);
    client->stop();
    TEST_ASSERT(client->start(), "client re-dialed");
    TEST_ASSERT(wait_until([&] {
                    client->pump(now_ms());
                    return client->state().replies >= 2;
                }, 5000),
                "second registration roundtrip");
    const auto st2 = client->state();
    TEST_ASSERT(st2.code == world_app::kCellRegisterAcceptedIdempotent,
                "same epoch idempotent");
    TEST_ASSERT(st2.attempts == 2 && st2.replies == 2, "attempt/reply accumulated");
    reg = server.world->find_cell_registration(42);
    TEST_ASSERT(reg != nullptr && reg->drain_state == 1, "drain refreshed on redial");
    TEST_ASSERT(reg != nullptr && reg->start_epoch == 5, "epoch unchanged");
    TEST_ASSERT(server.world->registered_cell_count() == 1, "count unchanged");

    // 新代次重拨（重启换代语义：旧进程先退，新进程换代注册）→ 替换 code 1。
    // 旧客户端必须先 stop——双活同名 peer（同 cell-42 两链）不是本例语义，
    // 同名链路替换竞态归 L101 旧执行端隔离批。
    client->stop();
    cfg.start_epoch = 6;
    auto client2 = world_app::makeCellRegisterClient(cfg);
    TEST_ASSERT(client2->start(), "second client started");
    TEST_ASSERT(wait_until([&] {
                    client2->pump(now_ms());
                    return client2->state().replies >= 1;
                }, 5000),
                "third registration roundtrip");
    TEST_ASSERT(client2->state().code == world_app::kCellRegisterAccepted,
                "new epoch replaced");
    reg = server.world->find_cell_registration(42);
    TEST_ASSERT(reg != nullptr && reg->start_epoch == 6, "epoch replaced in registry");
    TEST_ASSERT(server.world->registered_cell_count() == 1, "still one cell");

    client2->stop();
    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_production_client_invalid_config() {
    std::cout << "Running: test_production_client_invalid_config..." << std::endl;
    // cell_id 缺位 = 装配拒绝
    world_app::CellRegisterConfig bad;
    bad.world_host = "127.0.0.1";
    bad.world_port = 9;
    bad.cell_id = 0;
    bad.route_json = kRouteJson;
    auto client = world_app::makeCellRegisterClient(bad);
    TEST_ASSERT(!client->start(), "cell id 0 rejected");
    TEST_ASSERT(!client->linkUp(), "no link");
    const auto st = client->state();
    TEST_ASSERT(!st.sent && st.attempts == 0, "nothing sent");

    // 路由表缺位 = 装配拒绝
    world_app::CellRegisterConfig bad2 = bad;
    bad2.cell_id = 1;
    bad2.route_json.clear();
    auto client2 = world_app::makeCellRegisterClient(bad2);
    TEST_ASSERT(!client2->start(), "empty route json rejected");

    std::cout << "  PASSED" << std::endl;
    return true;
}

} // namespace

int main() {
#if defined(_WIN32)
    std::cout << "SKIP: cell register e2e requires POSIX TCP\n";
    return 0;
#else
    const bool ok = test_cell_register_lifecycle()
                    && test_cell_register_invalid_and_reserved()
                    && test_cell_register_malformed_rejected()
                    && test_production_client_registers_and_redials()
                    && test_production_client_invalid_config();
    if (!ok || g_failures != 0) {
        std::cout << "CELL REGISTER TESTS FAILED (" << g_failures << ")" << std::endl;
        return 1;
    }
    std::cout << "ALL CELL REGISTER TESTS PASSED" << std::endl;
    return 0;
#endif
}
