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
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
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
constexpr std::uint16_t kSpaceAssignId = id_of(MsgId::cell_space_assign);
constexpr std::uint16_t kRetireRequestId = id_of(MsgId::cell_retire_request);
constexpr std::uint16_t kRetireReplyId = id_of(MsgId::cell_retire_reply);
constexpr std::uint16_t kSpaceReportId = id_of(MsgId::cell_space_report);
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

// 962 分配账（同上惯例：dispatcher 写 / 断言读，指针注入 + 串行）。
struct SpaceAssignFrame {
    std::uint64_t version = 0;
    std::uint64_t management_epoch = 0;
    std::uint64_t assigned_cell_id = 0;
    bool ready = false;
};
std::mutex g_assigns_mutex;
std::map<std::uint64_t, SpaceAssignFrame>* g_assigns = nullptr;  // space_id → frame

// 966 缩容回执账（同上惯例：dispatcher 写 / 断言读，指针注入 + 串行）。
struct RetireReply {
    bool accepted = false;
    std::int32_t code = 0;
    std::uint64_t cell_id = 0;
    std::uint64_t management_epoch = 0;
    std::string detail;
};
std::mutex g_retire_replies_mutex;
std::map<std::uint64_t, RetireReply>* g_retire_replies = nullptr;

// cell 侧投影：cell→World M1 控制受理面（裸 960 上行，961 按 request_id
// 收账）。peer 自报名 = 链路名（服务端按名回链路与记入注册表）。
struct CellEnd {
    std::unique_ptr<link::RouteTable> table;
    std::unique_ptr<link::LinkManager> manager;

    bool start(std::uint16_t port, const char* self_name = "cell-x") {
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
        // 962 分配（World 推；test_cell_space_assignment 断言面）
        dispatcher.set_handler(kSpaceAssignId,
                               [](std::uint16_t, const std::uint8_t* data,
                                  std::size_t size) {
                                   std::vector<wire::FieldValue> fields;
                                   if (!wire::decode_payload(kSpaceAssignId, data, size,
                                                             fields)
                                       || fields.size() != 5) {
                                       return;
                                   }
                                   SpaceAssignFrame f;
                                   f.version = static_cast<std::uint64_t>(fields[0].integer);
                                   f.management_epoch =
                                       static_cast<std::uint64_t>(fields[1].integer);
                                   f.assigned_cell_id =
                                       static_cast<std::uint64_t>(fields[3].integer);
                                   f.ready = fields[4].integer != 0;
                                   const auto space_id =
                                       static_cast<std::uint64_t>(fields[2].integer);
                                   std::lock_guard<std::mutex> lock(g_assigns_mutex);
                                   if (g_assigns != nullptr) {
                                       (*g_assigns)[space_id] = f;
                                   }
                               });
        // 966 缩容回执（World 回；test_cell_retire_management_command 断言面）
        dispatcher.set_handler(kRetireReplyId,
                               [](std::uint16_t, const std::uint8_t* data,
                                  std::size_t size) {
                                   std::vector<wire::FieldValue> fields;
                                   if (!wire::decode_payload(kRetireReplyId, data, size,
                                                             fields)
                                       || fields.size() != 6) {
                                       return;
                                   }
                                   RetireReply r;
                                   r.accepted = fields[1].integer != 0;
                                   r.code = static_cast<std::int32_t>(fields[2].integer);
                                   r.management_epoch =
                                       static_cast<std::uint64_t>(fields[3].integer);
                                   r.cell_id =
                                       static_cast<std::uint64_t>(fields[4].integer);
                                   r.detail = fields[5].text;
                                   const auto request_id =
                                       static_cast<std::uint64_t>(fields[0].integer);
                                   std::lock_guard<std::mutex> lock(
                                       g_retire_replies_mutex);
                                   if (g_retire_replies != nullptr) {
                                       (*g_retire_replies)[request_id] = std::move(r);
                                   }
                               });
        return true;
    }

    void pump() { manager->pump(now_ms()); }

    bool up() { return manager->link("world")->state() == link::LinkState::kUp; }

    bool send_register(std::uint64_t request_id, std::uint64_t cell_id,
                       std::uint64_t start_epoch, const std::string& host,
                       std::int32_t port, std::int32_t capabilities,
                       std::int64_t compat, std::int32_t load,
                       std::int32_t drain_state, bool ready) {
        std::vector<wire::FieldValue> f(10);
        f[0].integer = static_cast<std::int64_t>(request_id);
        f[1].integer = static_cast<std::int64_t>(cell_id);
        f[2].integer = static_cast<std::int64_t>(start_epoch);
        f[3].text = host;
        f[4].integer = port;
        f[5].integer = capabilities;
        f[6].integer = compat;
        f[7].integer = load;
        f[8].integer = drain_state;
        f[9].integer = ready ? 1 : 0;
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
        std::vector<wire::FieldValue> f(10);
        f[0].integer = static_cast<std::int64_t>(request_id);
        f[1].integer = 5;
        f[2].integer = 1;
        f[3].text = "127.0.0.1";
        f[4].integer = 9201;
        f[5].integer = 1;
        f[6].integer = world_app::contract_compat_projection();
        f[7].integer = 10;
        f[8].integer = 0;
        f[9].integer = 1;
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

    // 963 缩容命令（fleetctl 同口；op 字面量 = 963 契约词汇 1=begin 2=cancel）。
    bool send_retire(std::uint64_t request_id, std::int32_t op,
                     std::uint64_t cell_id) {
        std::vector<wire::FieldValue> f(3);
        f[0].integer = static_cast<std::int64_t>(request_id);
        f[1].integer = op;
        f[2].integer = static_cast<std::int64_t>(cell_id);
        std::vector<std::uint8_t> payload;
        if (!wire::encode_payload(kRetireRequestId, f, payload)) {
            return false;
        }
        return manager->link("world")
                   ->send(kRetireRequestId, payload.data(), payload.size(),
                          link::SendClass::kReliable)
               == link::SendCode::kAccepted;
    }

    // 残帧：合法 963 载荷截断（decode 必败 → 服务端拒收计数）。
    bool send_truncated_retire(std::uint64_t request_id) {
        std::vector<wire::FieldValue> f(3);
        f[0].integer = static_cast<std::int64_t>(request_id);
        f[1].integer = 1;
        f[2].integer = 51;
        std::vector<std::uint8_t> payload;
        if (!wire::encode_payload(kRetireRequestId, f, payload)) {
            return false;
        }
        payload.resize(payload.size() / 2);  // 截断 → decode 失败
        return manager->link("world")
                   ->send(kRetireRequestId, payload.data(), payload.size(),
                          link::SendClass::kReliable)
               == link::SendCode::kAccepted;
    }

    // 967 自报（对账批B）：已应用归属视图逐 Space 一帧。
    bool send_space_report(std::uint64_t request_id, std::uint64_t cell_id,
                           std::uint64_t start_epoch, std::uint64_t space_id,
                           std::uint64_t applied_version,
                           std::uint64_t applied_mgmt_epoch) {
        std::vector<wire::FieldValue> f(6);
        f[0].integer = static_cast<std::int64_t>(request_id);
        f[1].integer = static_cast<std::int64_t>(cell_id);
        f[2].integer = static_cast<std::int64_t>(start_epoch);
        f[3].integer = static_cast<std::int64_t>(space_id);
        f[4].integer = static_cast<std::int64_t>(applied_version);
        f[5].integer = static_cast<std::int64_t>(applied_mgmt_epoch);
        std::vector<std::uint8_t> payload;
        if (!wire::encode_payload(kSpaceReportId, f, payload)) {
            return false;
        }
        return manager->link("world")
                   ->send(kSpaceReportId, payload.data(), payload.size(),
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
                        std::int64_t compat = world_app::contract_compat_projection(),
                        int timeout_ms = 5000) {
    if (!cell.send_register(request_id, cell_id, start_epoch, host, port, capabilities,
                            compat, load, drain_state, ready)) {
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

// 963 单发 + 966 等（request_id 关联；服务端 gameLoop 自泵，本线程泵客户端）。
// 期望无回执的用例（request_id=0 保留）靠 timeout 判负。
bool retire_roundtrip(CellEnd& cell, std::uint64_t request_id, std::int32_t op,
                      std::uint64_t cell_id, RetireReply& out, int timeout_ms = 5000) {
    if (!cell.send_retire(request_id, op, cell_id)) {
        return false;
    }
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        cell.pump();
        {
            std::lock_guard<std::mutex> lock(g_retire_replies_mutex);
            const auto it = g_retire_replies->find(request_id);
            if (it != g_retire_replies->end()) {
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
// fixed_m1_port 非 0 = 绑定该端口（重启同址——失联冻结/重拨例用；0 = 内核分配）。
struct WorldEnd {
    WorldAppConfig config;
    std::unique_ptr<WorldApp> world;
    std::uint16_t fixed_m1_port = 0;

    bool start(std::uint32_t world_id) {
        config.worldId = world_id;
        config.host = "127.0.0.1";
        config.m1Port = fixed_m1_port;
        config.m1RouteJson = kRouteJson;
        if (config.m1AuthorityEpoch == 0) {
            config.m1AuthorityEpoch = 77;
        }
        std::cout << "  shell boot epoch=" << config.m1AuthorityEpoch
                  << " journal=" << config.worldJournalPath << std::endl;
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

// 拨装：cell（960 控制面）拨 WorldApp M1 端点并等待链路 up。自报名可配
//（多 cell 并存用例必配不同名——同名双活竞态归 L101 旧执行端隔离批）。
bool dial_cell(WorldEnd& server, CellEnd& cell, const char* self_name = "cell-x") {
    if (!cell.start(server.world->m1Link().listenPort(), self_name)) {
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
                                    world_app::contract_compat_projection(), 500),
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
    cfg.heartbeat_ms = 0;  // 0 = 关（仅 kUp 跃迁单发形态由本例钉住；心跳归下一例）
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

// ---------- heartbeat + expiry：心跳续活 / 停发过期摘除 / 过期后重注册 ----------
// World 过期窗 cellExpiryMs = 服务端单方裁决注册表存活（last-heard = 每次
// 受理刷新的 registered_at_ms）；心跳 = 周期 960 走幂等刷新路径（兼作 961
// 回执丢失重试）。过期 = 整条移除；代次水位（本进程曾受理的最高代次）
// 保留——旧代次复活拒 code 3、同代次复活仍首注册（隔离切片见
// test_stale_epoch_watermark_isolation）；持久墓碑随管理日志批。

bool test_cell_heartbeat_and_expiry() {
    std::cout << "Running: test_cell_heartbeat_and_expiry..." << std::endl;
    WorldEnd server;
    server.config.cellExpiryMs = 150;  // 测试窗（生产默认 30000）
    TEST_ASSERT(server.start(115), "world started with M1 port");
    std::map<std::uint64_t, RegisterReply> replies;
    g_replies = &replies;

    // 阶段 1：裸 960 注册后停发 → 过期窗内整条摘除
    CellEnd cell;
    TEST_ASSERT(dial_cell(server, cell), "cell dialed");
    RegisterReply r;
    TEST_ASSERT(register_roundtrip(cell, 9601, 7, 1, "127.0.0.1", 9201, 1, 10, 0, true, r),
                "register accepted");
    TEST_ASSERT(r.code == world_app::kCellRegisterAccepted, "code accepted");
    TEST_ASSERT(server.world->registered_cell_count() == 1, "one registration");
    // 停发（不再泵不再发 960），等过期窗 + 清扫拍（服务端 gameLoop 自泵）
    TEST_ASSERT(wait_until([&] {
                    return server.world->find_cell_registration(7) == nullptr;
                }, 3000),
                "expired entry removed");
    TEST_ASSERT(server.world->registered_cell_count() == 0, "registry empty");

    // 过期后同代次重注册 → 首注册受理（登记忘却；同代次=水位不拦，见隔离切片）
    TEST_ASSERT(register_roundtrip(cell, 9602, 7, 1, "127.0.0.1", 9201, 1, 10, 0, true, r),
                "re-register after expiry reply arrived");
    TEST_ASSERT(r.accepted && r.code == world_app::kCellRegisterAccepted,
                "re-register after expiry = fresh admission");
    TEST_ASSERT(server.world->registered_cell_count() == 1, "re-registered");

    // 阶段 2：生产客户端周期心跳 > 过期窗持续续活
    world_app::CellRegisterConfig cfg;
    cfg.world_host = "127.0.0.1";
    cfg.world_port = server.world->m1Link().listenPort();
    cfg.cell_id = 42;
    cfg.advertise_host = "127.0.0.1";
    cfg.advertise_port = 8801;
    cfg.start_epoch = 5;
    cfg.route_json = kRouteJson;
    cfg.heartbeat_ms = 40;  // 测试窗（生产默认 5000；0 = 关 = 仅跃迁单发形态）
    auto client = world_app::makeCellRegisterClient(cfg);
    TEST_ASSERT(client->start(), "heartbeat client started");
    TEST_ASSERT(wait_until([&] {
                    client->pump(now_ms());
                    return client->state().replies >= 1;
                }, 5000),
                "first heartbeat roundtrip");

    // 持续跑 > 3×过期窗：心跳刷新 last-heard → 条目不摘除
    const auto keep_deadline = std::chrono::steady_clock::now() + 600ms;
    while (std::chrono::steady_clock::now() < keep_deadline) {
        client->pump(now_ms());
        std::this_thread::sleep_for(10ms);
    }
    const auto st = client->state();
    TEST_ASSERT(st.attempts >= 3, "periodic heartbeats sent");
    TEST_ASSERT(st.replies >= 3, "heartbeat replies recorded");
    TEST_ASSERT(server.world->find_cell_registration(42) != nullptr,
                "entry kept alive by heartbeats");

    // 停心跳 → 过期摘除；cell 7（阶段 1 重注册后无心跳）同样过期——
    // 停发/未发两条路径同归过期窗裁决
    client->stop();
    TEST_ASSERT(wait_until([&] {
                    return server.world->registered_cell_count() == 0;
                }, 3000),
                "all entries expired after heartbeats cease");
    TEST_ASSERT(server.world->find_cell_registration(7) == nullptr, "cell 7 expired");
    TEST_ASSERT(server.world->find_cell_registration(42) == nullptr, "cell 42 expired");

    g_replies = nullptr;
    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- space assignment：World 分配面（962 推送 + 认领独占 + 客户端去重）----------
// 池 = WorldAppConfig.assignSpaces；策略 = 首个就绪 cell 独占认领全池未分配
// Space（code 1/2 受理即推当前分配），后续注册不重分——重平衡/回收/拓扑版本
// 随 L108。断言事实源：962 帧内容 + World 分配表观测面 + 客户端监听去重。

bool test_cell_space_assignment() {
    std::cout << "Running: test_cell_space_assignment..." << std::endl;
    WorldEnd server;
    server.config.assignSpaces = {{7, 0}, {8, 0}};
    TEST_ASSERT(server.start(116), "world started with M1 port");
    std::map<std::uint64_t, RegisterReply> replies;
    g_replies = &replies;
    std::map<std::uint64_t, SpaceAssignFrame> assigns;
    g_assigns = &assigns;

    // 首注册：受理即推当前分配（池 7/8 → 本 cell），帧内容逐字段断言
    CellEnd cell;
    TEST_ASSERT(dial_cell(server, cell), "cell dialed");
    RegisterReply r;
    TEST_ASSERT(register_roundtrip(cell, 9701, 21, 1, "127.0.0.1", 9201, 1, 10, 0, true, r),
                "register accepted");
    TEST_ASSERT(r.accepted && r.code == world_app::kCellRegisterAccepted,
                "first registration accepted");
    TEST_ASSERT(wait_until([&] {
                    std::lock_guard<std::mutex> lock(g_assigns_mutex);
                    return assigns.size() >= 2;
                }, 3000),
                "both pool spaces pushed");
    {
        std::lock_guard<std::mutex> lock(g_assigns_mutex);
        const auto it7 = assigns.find(7);
        const auto it8 = assigns.find(8);
        TEST_ASSERT(it7 != assigns.end() && it8 != assigns.end(),
                    "spaces 7 and 8 assigned");
        TEST_ASSERT(it7 != assigns.end() && it7->second.version == 1,
                    "assignment version 1");
        TEST_ASSERT(it7 != assigns.end() && it7->second.assigned_cell_id == 21,
                    "assigned to registering cell");
        TEST_ASSERT(it7 != assigns.end() && it7->second.management_epoch == 77,
                    "management epoch echoed");
        TEST_ASSERT(it7 != assigns.end() && it7->second.ready, "ready flagged");
    }
    TEST_ASSERT(server.world->assigned_space_count() == 2, "world assignment table");
    const auto* a7 = server.world->find_space_assignment(7);
    TEST_ASSERT(a7 != nullptr && a7->cell_id == 21 && a7->version == 1,
                "world assignment recorded");

    // 后续注册不重分（认领独占）：962 = 权威归属视图（L108 回收批精化）——
    // 第二 cell 收到的是非归属标记（ready=0、owner 仍是 21、版本 1），而非
    // ready=1 分配；World 侧归属不变
    CellEnd cell2;
    TEST_ASSERT(dial_cell(server, cell2, "cell-y"), "second cell dialed");
    TEST_ASSERT(register_roundtrip(cell2, 9702, 22, 1, "127.0.0.1", 9202, 1, 10, 0, true, r),
                "second register accepted");
    TEST_ASSERT(r.accepted && r.code == world_app::kCellRegisterAccepted,
                "second accepted (no spaces left)");
    TEST_ASSERT(wait_until([&] {
                    cell2.pump();
                    std::lock_guard<std::mutex> lock(g_assigns_mutex);
                    const auto it = assigns.find(7);
                    return it != assigns.end() && !it->second.ready
                           && it->second.assigned_cell_id == 21;
                }, 3000),
                "second cell sees not-owner marker");
    {
        std::lock_guard<std::mutex> lock(g_assigns_mutex);
        const auto it7 = assigns.find(7);
        const auto it8 = assigns.find(8);
        TEST_ASSERT(assigns.size() == 2, "no new assignments (pool fixed)");
        TEST_ASSERT(it7 != assigns.end() && !it7->second.ready
                        && it7->second.assigned_cell_id == 21,
                    "space 7 marker: not owner, owner 21");
        TEST_ASSERT(it8 != assigns.end() && !it8->second.ready,
                    "space 8 marker: not owner");
        TEST_ASSERT(it7 != assigns.end() && it7->second.version == 1,
                    "marker carries current version");
    }
    TEST_ASSERT(server.world->assigned_space_count() == 2, "table unchanged");
    a7 = server.world->find_space_assignment(7);
    TEST_ASSERT(a7 != nullptr && a7->cell_id == 21 && a7->version == 1,
                "ownership unchanged by second registration");

    // 同代次刷新（心跳路径）→ 重推当前分配、版本不变（客户端按版本去重）
    TEST_ASSERT(register_roundtrip(cell, 9703, 21, 1, "127.0.0.1", 9201, 1, 10, 0, true, r),
                "refresh roundtrip");
    TEST_ASSERT(r.code == world_app::kCellRegisterAcceptedIdempotent,
                "refresh idempotent");
    {
        std::lock_guard<std::mutex> lock(g_assigns_mutex);
        const auto it7 = assigns.find(7);
        TEST_ASSERT(it7 != assigns.end() && it7->second.version == 1,
                    "refresh re-push keeps version");
    }

    g_assigns = nullptr;
    g_replies = nullptr;
    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- 就绪/排空裁决：不认领新 Space，既有归属保留 ----------

bool test_space_assignment_readiness() {
    std::cout << "Running: test_space_assignment_readiness..." << std::endl;
    WorldEnd server;
    server.config.assignSpaces = {{7, 0}, {8, 0}};
    TEST_ASSERT(server.start(119), "world started with M1 port");
    std::map<std::uint64_t, RegisterReply> replies;
    g_replies = &replies;
    std::map<std::uint64_t, SpaceAssignFrame> assigns;
    g_assigns = &assigns;

    RegisterReply r;

    // 未就绪 cell：进程存活不等于可分配——不认领新 Space，也不发归属视图
    CellEnd cell;
    TEST_ASSERT(dial_cell(server, cell, "cell-a"), "cell a dialed");
    TEST_ASSERT(
        register_roundtrip(cell, 9901, 41, 1, "127.0.0.1", 9207, 0, 5, 0, false, r),
        "unready register accepted");
    TEST_ASSERT(r.accepted, "unready cell accepted");
    std::this_thread::sleep_for(200ms);  // 拍足：无 962 才谈不认领
    {
        std::lock_guard<std::mutex> lock(g_assigns_mutex);
        TEST_ASSERT(assigns.empty(), "unready cell claims nothing");
    }
    TEST_ASSERT(server.world->assigned_space_count() == 0, "no claim without ready");

    // 转为就绪（同代次刷新 = 心跳面）：才首次认领
    TEST_ASSERT(
        register_roundtrip(cell, 9902, 41, 1, "127.0.0.1", 9207, 0, 5, 0, true, r),
        "ready refresh accepted");
    TEST_ASSERT(r.code == world_app::kCellRegisterAcceptedIdempotent,
                "ready refresh idempotent");
    TEST_ASSERT(wait_until([&] {
                    const auto* a7 = server.world->find_space_assignment(7);
                    return a7 != nullptr && a7->cell_id == 41 && a7->version == 1;
                }, 3000),
                "ready cell claims at version 1");
    TEST_ASSERT(wait_until([&] {
                    cell.pump();
                    std::lock_guard<std::mutex> lock(g_assigns_mutex);
                    const auto it = assigns.find(7);
                    return it != assigns.end() && it->second.ready
                           && it->second.assigned_cell_id == 41;
                }, 3000),
                "ready cell receives ownership push");
    {
        std::lock_guard<std::mutex> lock(g_assigns_mutex);
        TEST_ASSERT(assigns.size() == 2, "pool claimed at ready");
    }
    TEST_ASSERT(server.world->assigned_space_count() == 2, "world assignment table");
    const auto* a7 = server.world->find_space_assignment(7);
    TEST_ASSERT(a7 != nullptr && a7->cell_id == 41 && a7->version == 1
                    && a7->space_id == 7,
                "world assignment recorded");

    // 排空端：不认领新 Space（既有归属仍刷新，撤空不撤权）
    TEST_ASSERT(
        register_roundtrip(cell, 9903, 41, 1, "127.0.0.1", 9207, 0, 5, 1, true, r),
        "draining refresh accepted");
    const auto* a8 = server.world->find_space_assignment(8);
    TEST_ASSERT(a8 != nullptr && a8->cell_id == 41 && a8->version == 1,
                "draining keeps ownership");

    // 高负载新 cell 无可认领者（池已满 = 只收撤销视图）
    CellEnd cell2;
    TEST_ASSERT(dial_cell(server, cell2, "cell-b"), "cell b dialed");
    TEST_ASSERT(
        register_roundtrip(cell2, 9904, 42, 1, "127.0.0.1", 9208, 0, 90, 0, true, r),
        "high-load register accepted");
    TEST_ASSERT(wait_until([&] {
                    cell2.pump();
                    std::lock_guard<std::mutex> lock(g_assigns_mutex);
                    const auto it = assigns.find(7);
                    return it != assigns.end() && !it->second.ready
                           && it->second.assigned_cell_id == 41;
                }, 3000),
                "cell b sees retraction view");
    TEST_ASSERT(server.world->assigned_space_count() == 2, "table unchanged");

    // A 断线失联（停发心跳）后 B 仍无可认领者——资格以自报面为准，不以
    // World 侧是否怀疑为准（保守面：宁可不分，不抢分给失控端）
    const auto* a8b = server.world->find_space_assignment(8);
    TEST_ASSERT(a8b != nullptr && a8b->cell_id == 41, "no reclamation after silence");

    // 归属它方者只收撤销标记，不迁移不重分
    CellEnd cell3;
    TEST_ASSERT(dial_cell(server, cell3, "cell-c"), "cell c dialed");
    TEST_ASSERT(
        register_roundtrip(cell3, 9905, 43, 1, "127.0.0.1", 9209, 0, 1, 0, true, r),
        "low-load newcomer accepted");
    TEST_ASSERT(wait_until([&] {
                    cell3.pump();
                    std::lock_guard<std::mutex> lock(g_assigns_mutex);
                    const auto it = assigns.find(7);
                    return it != assigns.end() && !it->second.ready
                           && it->second.assigned_cell_id == 41
                           && it->second.version == 1;
                }, 3000),
                "retraction carries owner and version");
    const auto* a7b = server.world->find_space_assignment(7);
    TEST_ASSERT(a7b != nullptr && a7b->cell_id == 41 && a7b->version == 1,
                "ownership not migrated by low-load newcomer");

    g_assigns = nullptr;
    g_replies = nullptr;
    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- 负载裁决：释放面最小负载优先，高负载让位 ----------

bool test_space_assignment_load_balance() {
    std::cout << "Running: test_space_assignment_load_balance..." << std::endl;
    WorldEnd server;
    server.config.assignSpaces = {{7, 0}, {8, 0}};
    // 过期窗须远大于阶段 1 序列（A 认领 → B/C 拨号+注册+收撤销帧）：窗内
    // A 若先过期释放，最低负载的 C 会重认领 Space 7，B 收到的视图被覆盖成
    // owner=C/ready=1，撤销断言永不成立（ASan 下已见假红）。同时须小于下文
    // 各 3000ms 过期等待。
    server.config.cellExpiryMs = 1500;
    TEST_ASSERT(server.start(120), "world started with M1 port");
    std::map<std::uint64_t, RegisterReply> replies;
    g_replies = &replies;
    std::map<std::uint64_t, SpaceAssignFrame> assigns;
    g_assigns = &assigns;

    RegisterReply r;

    // A 先受理 → 独占全池（无人可让位）
    CellEnd cell_a;
    TEST_ASSERT(dial_cell(server, cell_a, "cell-a"), "cell a dialed");
    TEST_ASSERT(
        register_roundtrip(cell_a, 9911, 43, 1, "127.0.0.1", 9207, 0, 10, 0, true, r),
        "cell a registered");
    TEST_ASSERT(wait_until([&] {
                    const auto* a7 = server.world->find_space_assignment(7);
                    return a7 != nullptr && a7->cell_id == 43 && a7->version == 1;
                }, 3000),
                "cell a claims space 7");
    TEST_ASSERT(server.world->assigned_space_count() == 2, "cell a claims pool");

    // B / C 后到：他人已归属不迁移，只收撤销视图
    CellEnd cell_b;
    TEST_ASSERT(dial_cell(server, cell_b, "cell-b"), "cell b dialed");
    TEST_ASSERT(
        register_roundtrip(cell_b, 9912, 44, 1, "127.0.0.1", 9208, 0, 90, 0, true, r),
        "cell b registered");
    CellEnd cell_c;
    TEST_ASSERT(dial_cell(server, cell_c, "cell-c"), "cell c dialed");
    TEST_ASSERT(
        register_roundtrip(cell_c, 9913, 45, 1, "127.0.0.1", 9209, 0, 20, 0, true, r),
        "cell c registered");
    TEST_ASSERT(wait_until([&] {
                    cell_b.pump();
                    std::lock_guard<std::mutex> lock(g_assigns_mutex);
                    const auto it = assigns.find(7);
                    return it != assigns.end() && !it->second.ready
                           && it->second.assigned_cell_id == 43;
                }, 3000),
                "cell b sees retraction view");

    // A 停发心跳 → 过期释放（owner 置 0，版本不变）
    TEST_ASSERT(wait_until([&] {
                    const auto* a7 = server.world->find_space_assignment(7);
                    return a7 != nullptr && a7->cell_id == 0;
                }, 3000),
                "cell a releases ownership");

    // B（负载 90）刷新：同窗存在更低负载候选 C（20）→ 让位不认领
    TEST_ASSERT(
        register_roundtrip(cell_b, 9914, 44, 1, "127.0.0.1", 9208, 0, 90, 0, true, r),
        "cell b refresh accepted");
    TEST_ASSERT(r.code == world_app::kCellRegisterAcceptedIdempotent,
                "cell b still registered");
    const auto* a7b = server.world->find_space_assignment(7);
    TEST_ASSERT(a7b != nullptr && a7b->cell_id == 0, "cell b yields to lighter peer");
    const auto* a8b = server.world->find_space_assignment(8);
    TEST_ASSERT(a8b != nullptr && a8b->cell_id == 0, "pool still released");

    // C（负载 20）刷新：同窗无更低负载候选 → 最小负载方重认领（版本前进防
    // 离线旧主 ABA）
    TEST_ASSERT(
        register_roundtrip(cell_c, 9915, 45, 1, "127.0.0.1", 9209, 0, 20, 0, true, r),
        "cell c refresh accepted");
    TEST_ASSERT(wait_until([&] {
                    const auto* a7c = server.world->find_space_assignment(7);
                    return a7c != nullptr && a7c->cell_id == 45 && a7c->version == 2;
                }, 3000),
                "lightest peer reclaims at version 2");
    TEST_ASSERT(wait_until([&] {
                    cell_c.pump();
                    std::lock_guard<std::mutex> lock(g_assigns_mutex);
                    const auto it = assigns.find(7);
                    return it != assigns.end() && it->second.ready
                           && it->second.assigned_cell_id == 45
                           && it->second.version == 2;
                }, 3000),
                "cell c receives ownership push at v2");
    TEST_ASSERT(server.world->assigned_space_count() == 2, "pool fully assigned");

    g_assigns = nullptr;
    g_replies = nullptr;
    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- 容量申报裁决：能力位要求过滤认领资格，无合格候选留池不认领 ----------

bool test_space_assignment_capability_filter() {
    std::cout << "Running: test_space_assignment_capability_filter..." << std::endl;
    WorldEnd server;
    // 池：Space 7 要求能力位 1、Space 8 要求位 2、Space 9 要求位 4（无任何
    // cell 声明位 4 = 全池无合格候选，不得凑数落到不合格 cell 上）
    server.config.assignSpaces = {{7, 1}, {8, 2}, {9, 4}};
    // 过期窗须远大于阶段 1 序列（A/B 依次认领、C 注册期间 A 必须仍持有
    // Space 7）：窗内 A 若先过期释放，持有稳定断言永不成立（ASan 下已见
    // 假红）。同时须小于下文 3000ms 过期等待。
    server.config.cellExpiryMs = 1500;
    TEST_ASSERT(server.start(120), "world started with M1 port");
    std::map<std::uint64_t, RegisterReply> replies;
    g_replies = &replies;

    RegisterReply r;

    // A（能力 1，负载 50）先受理：只认领覆盖得了的 Space 7
    CellEnd cell_a;
    TEST_ASSERT(dial_cell(server, cell_a, "cell-a"), "cell a dialed");
    TEST_ASSERT(
        register_roundtrip(cell_a, 9921, 51, 1, "127.0.0.1", 9211, 1, 50, 0, true, r),
        "cell a registered");
    TEST_ASSERT(wait_until([&] {
                    const auto* a7 = server.world->find_space_assignment(7);
                    return a7 != nullptr && a7->cell_id == 51 && a7->version == 1;
                }, 3000),
                "cell a claims its only capable space");
    const auto* a8 = server.world->find_space_assignment(8);
    TEST_ASSERT(a8 == nullptr || a8->cell_id == 0,
                "incapable cell does not claim space 8");
    TEST_ASSERT(server.world->find_space_assignment(9) == nullptr,
                "space 9 unclaimed while nobody declares capability 4");

    // B（能力 2，负载 10）：认领 Space 8；更低负载不构成抢 A 的 7（无重分）
    CellEnd cell_b;
    TEST_ASSERT(dial_cell(server, cell_b, "cell-b"), "cell b dialed");
    TEST_ASSERT(
        register_roundtrip(cell_b, 9922, 52, 1, "127.0.0.1", 9212, 2, 10, 0, true, r),
        "cell b registered");
    TEST_ASSERT(wait_until([&] {
                    const auto* b8 = server.world->find_space_assignment(8);
                    return b8 != nullptr && b8->cell_id == 52 && b8->version == 1;
                }, 3000),
                "cell b claims its capable space");
    const auto* a7b = server.world->find_space_assignment(7);
    TEST_ASSERT(a7b != nullptr && a7b->cell_id == 51,
                "lighter peer does not reassign an owned space");

    // C（能力 0 = 未声明，负载 0）：任何非零要求都覆盖不了 → 一格不领
    CellEnd cell_c;
    TEST_ASSERT(dial_cell(server, cell_c, "cell-c"), "cell c dialed");
    TEST_ASSERT(
        register_roundtrip(cell_c, 9923, 53, 1, "127.0.0.1", 9213, 0, 0, 0, true, r),
        "cell c registered");
    TEST_ASSERT(wait_until([&] {
                    const auto* a7c = server.world->find_space_assignment(7);
                    return a7c != nullptr && a7c->cell_id == 51;
                }, 3000),
                "owner stable while an undeclared cell waits");
    TEST_ASSERT(server.world->assigned_space_count() == 2,
                "only capable spaces got table entries");

    // A 停发心跳 → Space 7 过期释放；B（能力 2，负载 10）刷新：更低负载但
    // 能力不足 → 不抢。让位目标也须合格，否则合格方把归属让给无资格方、
    // Space 永久搁置
    TEST_ASSERT(wait_until([&] {
                    const auto* a7r = server.world->find_space_assignment(7);
                    return a7r != nullptr && a7r->cell_id == 0;
                }, 3000),
                "cell a releases space 7");
    TEST_ASSERT(
        register_roundtrip(cell_b, 9924, 52, 1, "127.0.0.1", 9212, 2, 10, 0, true, r),
        "cell b refresh accepted");
    const auto* a7r2 = server.world->find_space_assignment(7);
    TEST_ASSERT(a7r2 != nullptr && a7r2->cell_id == 0,
                "lighter but incapable peer does not steal space 7");

    // D（能力 3 = 位 1|2，负载 1）：7 的唯一合格候选 → 重认领（版本前进）
    CellEnd cell_d;
    TEST_ASSERT(dial_cell(server, cell_d, "cell-d"), "cell d dialed");
    TEST_ASSERT(
        register_roundtrip(cell_d, 9925, 54, 1, "127.0.0.1", 9214, 3, 1, 0, true, r),
        "cell d registered");
    TEST_ASSERT(wait_until([&] {
                    const auto* d7 = server.world->find_space_assignment(7);
                    return d7 != nullptr && d7->cell_id == 54 && d7->version == 2;
                }, 3000),
                "only capable peer reclaims at version 2");
    const auto* d8 = server.world->find_space_assignment(8);
    TEST_ASSERT(d8 != nullptr && d8->cell_id == 52 && d8->version == 1,
                "space 8 stays with b across d's registration");
    TEST_ASSERT(server.world->find_space_assignment(9) == nullptr,
                "space 9 still unclaimed by anyone");

    g_replies = nullptr;
    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- 版本/兼容裁决：compat 投影不符或未声明（0）拒 code 5，不登记 ----------

bool test_cell_register_compat_adjudication() {
    std::cout << "Running: test_cell_register_compat_adjudication..." << std::endl;
    WorldEnd server;
    TEST_ASSERT(server.start(129), "world started with M1 port");
    std::map<std::uint64_t, RegisterReply> replies;
    g_replies = &replies;
    CellEnd cell;
    TEST_ASSERT(dial_cell(server, cell), "cell dialed");

    const std::int64_t want = world_app::contract_compat_projection();
    RegisterReply r;

    // 投影不符（旧/异版进程）：显式驳回 code 5，不登记
    TEST_ASSERT(
        register_roundtrip(cell, 9941, 61, 1, "127.0.0.1", 9215, 1, 10, 0, true, r,
                           want + 1),
        "wrong-compat reply arrived");
    TEST_ASSERT(!r.accepted, "wrong compat rejected");
    TEST_ASSERT(r.code == world_app::kCellRegisterRejectedIncompatible,
                "wrong compat code 5");
    TEST_ASSERT(r.detail.find("incompatible") != std::string::npos,
                "incompatible detail");
    TEST_ASSERT(server.world->registered_cell_count() == 0, "no registration");

    // 未声明（0 = 不可验证）：同拒 code 5（裸帧/异实现不得凭空受理）
    TEST_ASSERT(
        register_roundtrip(cell, 9942, 61, 1, "127.0.0.1", 9215, 1, 10, 0, true, r, 0),
        "undeclared-compat reply arrived");
    TEST_ASSERT(!r.accepted, "undeclared compat rejected");
    TEST_ASSERT(r.code == world_app::kCellRegisterRejectedIncompatible,
                "undeclared compat code 5");
    TEST_ASSERT(server.world->registered_cell_count() == 0, "still no registration");

    // 投影同值：受理 + 登记保留声明值（观测面）
    TEST_ASSERT(
        register_roundtrip(cell, 9943, 61, 1, "127.0.0.1", 9215, 1, 10, 0, true, r),
        "matching-compat reply arrived");
    TEST_ASSERT(r.accepted && r.code == world_app::kCellRegisterAccepted,
                "matching compat accepted");
    const auto* reg = server.world->find_cell_registration(61);
    TEST_ASSERT(reg != nullptr, "registration recorded");
    TEST_ASSERT(reg != nullptr && reg->compat == want, "compat declared value kept");

    // 同代次幂等刷新（同投影）：code 2，登记兼容投影不变
    TEST_ASSERT(
        register_roundtrip(cell, 9944, 61, 1, "127.0.0.1", 9215, 1, 55, 1, false, r),
        "idempotent reply arrived");
    TEST_ASSERT(r.accepted && r.code == world_app::kCellRegisterAcceptedIdempotent,
                "same-epoch idempotent");
    reg = server.world->find_cell_registration(61);
    TEST_ASSERT(reg != nullptr && reg->compat == want, "compat unchanged on refresh");

    // 代次裁决先于兼容裁决：旧代次（code 3）凭自冻结语义不被 code 5 抢答
    TEST_ASSERT(
        register_roundtrip(cell, 9945, 62, 5, "127.0.0.1", 9216, 1, 10, 0, true, r),
        "cell 62 first epoch accepted");
    TEST_ASSERT(r.accepted && r.code == world_app::kCellRegisterAccepted,
                "cell 62 registered at epoch 5");
    TEST_ASSERT(
        register_roundtrip(cell, 9946, 62, 3, "127.0.0.1", 9216, 1, 10, 0, true, r,
                           want + 1),
        "stale wrong-compat reply arrived");
    TEST_ASSERT(!r.accepted, "stale epoch rejected even with wrong compat");
    TEST_ASSERT(r.code == world_app::kCellRegisterRejectedStaleEpoch,
                "stale epoch preempts compat code 5");
    reg = server.world->find_cell_registration(62);
    TEST_ASSERT(reg != nullptr && reg->start_epoch == 5,
                "registration not downgraded by stale wrong-compat request");

    g_replies = nullptr;
    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- 缩容编排：World 单方缩容意图停止认领并回池归属，可取消 ----------

bool test_cell_retire_and_reclaim() {
    std::cout << "Running: test_cell_retire_and_reclaim..." << std::endl;
    WorldEnd server;
    server.config.assignSpaces = {{7, 0}, {8, 0}};
    TEST_ASSERT(server.start(121), "world started with M1 port");
    std::map<std::uint64_t, RegisterReply> replies;
    g_replies = &replies;
    std::map<std::uint64_t, SpaceAssignFrame> assigns;
    g_assigns = &assigns;

    RegisterReply r;

    // A（cell 51，负载 10）先受理独占全池
    CellEnd cell_a;
    TEST_ASSERT(dial_cell(server, cell_a, "cell-a"), "cell a dialed");
    TEST_ASSERT(
        register_roundtrip(cell_a, 9931, 51, 1, "127.0.0.1", 9207, 0, 10, 0, true, r),
        "cell a registered");
    TEST_ASSERT(wait_until([&] {
                    const auto* a7 = server.world->find_space_assignment(7);
                    return a7 != nullptr && a7->cell_id == 51 && a7->version == 1;
                }, 3000),
                "cell a claims space 7");
    TEST_ASSERT(server.world->assigned_space_count() == 2, "cell a claims pool");

    // 未知 cell 缩容 = 未命中（管理面显形，不静默成功）
    TEST_ASSERT(!server.world->beginCellRetire(999), "unknown retire refused");

    // 缩容 A：置 World 单方意图 + 名下归属回池（owner 归零、版本不变）
    TEST_ASSERT(server.world->beginCellRetire(51), "retire hit");
    const auto* reg51 = server.world->find_cell_registration(51);
    TEST_ASSERT(reg51 != nullptr && reg51->retire, "retire flag recorded");
    TEST_ASSERT(server.world->retired_cell_count() == 1, "one retired cell");
    const auto* a7 = server.world->find_space_assignment(7);
    TEST_ASSERT(a7 != nullptr && a7->cell_id == 0 && a7->version == 1,
                "retire releases ownership, version kept");
    const auto* a8 = server.world->find_space_assignment(8);
    TEST_ASSERT(a8 != nullptr && a8->cell_id == 0, "whole pool released");

    // B（cell 52，负载 50）重认领：A 负载更低但已缩容，不参与让位裁决
    //（让位给永不安驻的缩容端会永久搁置该 Space）
    CellEnd cell_b;
    TEST_ASSERT(dial_cell(server, cell_b, "cell-b"), "cell b dialed");
    TEST_ASSERT(
        register_roundtrip(cell_b, 9932, 52, 1, "127.0.0.1", 9208, 0, 50, 0, true, r),
        "cell b registered");
    TEST_ASSERT(wait_until([&] {
                    const auto* a7b = server.world->find_space_assignment(7);
                    return a7b != nullptr && a7b->cell_id == 52
                           && a7b->version == 2;
                }, 3000),
                "higher-load peer reclaims from retired peer");
    TEST_ASSERT(server.world->assigned_space_count() == 2, "pool re-assigned");
    TEST_ASSERT(wait_until([&] {
                    cell_b.pump();
                    std::lock_guard<std::mutex> lock(g_assigns_mutex);
                    const auto it = assigns.find(7);
                    return it != assigns.end() && it->second.ready
                           && it->second.assigned_cell_id == 52
                           && it->second.version == 2;
                }, 3000),
                "cell b receives ownership push at v2");

    // A 仍在册且照常心跳：收撤销视图（owner B、版本前进），不回抢已重认领
    TEST_ASSERT(
        register_roundtrip(cell_a, 9933, 51, 1, "127.0.0.1", 9207, 0, 10, 0, true, r),
        "retired cell heartbeat accepted");
    TEST_ASSERT(r.code == world_app::kCellRegisterAcceptedIdempotent,
                "retired heartbeat idempotent");
    TEST_ASSERT(wait_until([&] {
                    cell_a.pump();
                    std::lock_guard<std::mutex> lock(g_assigns_mutex);
                    const auto it = assigns.find(7);
                    return it != assigns.end() && !it->second.ready
                           && it->second.assigned_cell_id == 52
                           && it->second.version == 2;
                }, 3000),
                "retired cell converges to revocation view");
    const auto* a7c = server.world->find_space_assignment(7);
    TEST_ASSERT(a7c != nullptr && a7c->cell_id == 52 && a7c->version == 2,
                "retired cell did not reclaim");

    // 缩容 B 后取消：归属维持释放态，意图一清即恢复资格（版本继续前进）
    TEST_ASSERT(server.world->beginCellRetire(52), "cell b retired");
    const auto* a7d = server.world->find_space_assignment(7);
    TEST_ASSERT(a7d != nullptr && a7d->cell_id == 0, "cell b spaces released");
    TEST_ASSERT(server.world->cancelCellRetire(52), "cell b retire cancelled");
    const auto* reg52 = server.world->find_cell_registration(52);
    TEST_ASSERT(reg52 != nullptr && !reg52->retire, "retire flag cleared");
    TEST_ASSERT(server.world->retired_cell_count() == 1, "only cell a retired");
    TEST_ASSERT(wait_until([&] {
                    const auto* a7e = server.world->find_space_assignment(7);
                    return a7e != nullptr && a7e->cell_id == 52
                           && a7e->version == 3;
                }, 3000),
                "cancelled cell reclaims at v3");

    // A 也取消缩容恢复资格：他人已归属的不迁移不重分
    TEST_ASSERT(server.world->cancelCellRetire(51), "cell a retire cancelled");
    TEST_ASSERT(server.world->retired_cell_count() == 0, "no retired cells left");
    TEST_ASSERT(
        register_roundtrip(cell_a, 9934, 51, 1, "127.0.0.1", 9207, 0, 10, 0, true, r),
        "restored cell heartbeat accepted");
    const auto* a7f = server.world->find_space_assignment(7);
    TEST_ASSERT(a7f != nullptr && a7f->cell_id == 52 && a7f->version == 3,
                "no migration of others' ownership");
    TEST_ASSERT(server.world->assigned_space_count() == 2, "pool intact");

    g_assigns = nullptr;
    g_replies = nullptr;
    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- 缩容管理命令面（963→966）：命令受理/驳回与意图撤销 ----------

bool test_cell_retire_management_command() {
    std::cout << "Running: test_cell_retire_management_command..." << std::endl;
    WorldEnd server;
    server.config.assignSpaces = {{7, 0}, {8, 0}};
    TEST_ASSERT(server.start(122), "world started with M1 port");
    std::map<std::uint64_t, RegisterReply> replies;
    g_replies = &replies;
    std::map<std::uint64_t, RetireReply> retire_replies;
    g_retire_replies = &retire_replies;

    RegisterReply reg;
    RetireReply r;
    CellEnd cell;
    TEST_ASSERT(dial_cell(server, cell, "cell-rt"), "retire cell dialed");
    TEST_ASSERT(
        register_roundtrip(cell, 9951, 51, 1, "127.0.0.1", 9207, 0, 10, 0, true, reg),
        "cell registered");

    // 未命中先验：目标未登记 = 显式 miss（运维据此判命令目标是否打错）
    TEST_ASSERT(retire_roundtrip(cell, 9952, 1, 999, r), "miss reply delivered");
    TEST_ASSERT(!r.accepted && r.code == world_app::kCellRetireRejectedUnknownCell,
                "miss code");
    TEST_ASSERT(r.detail.find("not registered") != std::string::npos, "miss detail");

    // 请求合法性先看：cell_id = 0 与 op 未知均驳回，且不动表
    TEST_ASSERT(retire_roundtrip(cell, 9953, 1, 0, r), "invalid cell_id reply");
    TEST_ASSERT(!r.accepted && r.code == world_app::kCellRetireRejectedInvalid,
                "invalid cell_id code");
    TEST_ASSERT(retire_roundtrip(cell, 9954, 7, 51, r), "unknown op reply");
    TEST_ASSERT(!r.accepted && r.code == world_app::kCellRetireRejectedInvalid,
                "unknown op code");
    TEST_ASSERT(r.detail.find("unknown op") != std::string::npos, "unknown op detail");
    TEST_ASSERT(server.world->retired_cell_count() == 0,
                "no intent set by rejections");

    // begin 命中：意图置 + 名下归属回池；966 回带确认事实（代次/目标）
    TEST_ASSERT(retire_roundtrip(cell, 9955, 1, 51, r), "begin reply delivered");
    TEST_ASSERT(r.accepted, "begin accepted");
    TEST_ASSERT(r.code == world_app::kCellRetireBegun, "begin code");
    TEST_ASSERT(r.cell_id == 51, "begin cell id echoed");
    TEST_ASSERT(r.management_epoch == 77, "begin management epoch echoed");
    TEST_ASSERT(r.detail.find("released") != std::string::npos, "begin detail");
    const auto* reg51 = server.world->find_cell_registration(51);
    TEST_ASSERT(reg51 != nullptr && reg51->retire, "retire intent recorded");
    const auto* a7 = server.world->find_space_assignment(7);
    TEST_ASSERT(a7 != nullptr && a7->cell_id == 0 && a7->version == 1,
                "begin releases ownership, version kept");

    // cancel 命中：只清意图，不取回已被他人接管者——本例无人接管，恢复资格
    // 的本人随本次推送重认领（版本 +1）；取消本身不迁移不重分。
    TEST_ASSERT(retire_roundtrip(cell, 9956, 2, 51, r), "cancel reply delivered");
    TEST_ASSERT(r.accepted && r.code == world_app::kCellRetireCancelled, "cancel code");
    TEST_ASSERT(r.cell_id == 51, "cancel cell id echoed");
    TEST_ASSERT(reg51 != nullptr && !reg51->retire, "retire intent cleared");
    TEST_ASSERT(server.world->retired_cell_count() == 0, "no retired cells");
    TEST_ASSERT(
        wait_until([&] {
            const auto* a7x = server.world->find_space_assignment(7);
            return a7x != nullptr && a7x->cell_id == 51 && a7x->version == 2;
        }, 3000),
        "cancelled cell reclaims own pool at v2");
    TEST_ASSERT(retire_roundtrip(cell, 9957, 2, 999, r), "cancel miss reply");
    TEST_ASSERT(!r.accepted && r.code == world_app::kCellRetireRejectedUnknownCell,
                "cancel miss code");

    // request_id = 0 保留：对端不关联静默不回（超时判负）
    TEST_ASSERT(cell.send_retire(0, 1, 51), "reserved request id sent");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (std::chrono::steady_clock::now() < deadline) {
        cell.pump();
        std::this_thread::sleep_for(10ms);
    }
    {
        std::lock_guard<std::mutex> lock(g_retire_replies_mutex);
        TEST_ASSERT(g_retire_replies->find(0) == g_retire_replies->end(),
                    "reserved request id not echoed");
    }

    // 残帧 = 拒收（不受理不算失败面，仅计数显形）
    const auto rejected_before = server.world->m1Link().rejectedFrames();
    TEST_ASSERT(cell.send_truncated_retire(9958), "truncated sent");
    TEST_ASSERT(
        wait_until([&] {
            cell.pump();
            return server.world->m1Link().rejectedFrames() > rejected_before;
        }, 3000),
        "truncated counted as rejected");

    g_retire_replies = nullptr;
    g_replies = nullptr;
    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- production client：962 监听去重（版本不前进不重复触发）----------

bool test_production_client_space_assign_listener() {
    std::cout << "Running: test_production_client_space_assign_listener..." << std::endl;
    WorldEnd server;
    server.config.assignSpaces = {{7, 0}, {8, 0}};
    TEST_ASSERT(server.start(117), "world started with M1 port");

    world_app::CellRegisterConfig cfg;
    cfg.world_host = "127.0.0.1";
    cfg.world_port = server.world->m1Link().listenPort();
    cfg.cell_id = 31;
    cfg.advertise_host = "127.0.0.1";
    cfg.advertise_port = 8802;
    cfg.start_epoch = 9;
    cfg.route_json = kRouteJson;
    cfg.heartbeat_ms = 40;
    auto client = world_app::makeCellRegisterClient(cfg);

    std::mutex m;
    std::vector<world_app::SpaceAssignRecord> fired;
    client->setSpaceAssignListener([&](const world_app::SpaceAssignRecord& rec) {
        std::lock_guard<std::mutex> lock(m);
        fired.push_back(rec);
    });
    TEST_ASSERT(client->start(), "client started");
    TEST_ASSERT(wait_until([&] {
                    client->pump(now_ms());
                    std::lock_guard<std::mutex> lock(m);
                    return fired.size() >= 2 && client->state().replies >= 1;
                }, 5000),
                "assignment listener fired for pool");
    {
        std::lock_guard<std::mutex> lock(m);
        TEST_ASSERT(fired.size() == 2, "one fire per space");
        TEST_ASSERT(fired[0].space_id == 7 && fired[1].space_id == 8,
                    "pool spaces in push order");
        TEST_ASSERT(fired[0].version == 1 && fired[1].version == 1, "versions");
        TEST_ASSERT(fired[0].assigned_cell_id == 31, "assigned cell recorded");
        TEST_ASSERT(fired[0].ready, "ready recorded");
    }
    TEST_ASSERT(client->space_assign_count() == 2, "client assignment face");

    // 心跳重推（版本不前进）→ 监听不重复触发（去重面）
    const auto keep_deadline = std::chrono::steady_clock::now() + 200ms;
    while (std::chrono::steady_clock::now() < keep_deadline) {
        client->pump(now_ms());
        std::this_thread::sleep_for(10ms);
    }
    {
        std::lock_guard<std::mutex> lock(m);
        TEST_ASSERT(fired.size() == 2, "no duplicate fires on heartbeat re-push");
        TEST_ASSERT(client->state().replies >= 3, "heartbeats flowed");
    }

    client->stop();
    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- 失联宽限冻结 + 复核解冻（L101 失联策略切片）----------
// cell 侧控制面失联：链路 down 超宽限窗（link_grace_ms）→ 冻结监听器触发
// 一次（宿主停模拟 + 关闸）；重连后 World 重新受理（961 accepted）才解冻
// ——链路恢复本身不解冻（§4：不能自行解冻，复核以 961 裁决为准）。
// World 端固定端口重启（同址起新 World），客户端端点配置不变即可重拨。

bool test_grace_freeze_and_readmit() {
    std::cout << "Running: test_grace_freeze_and_readmit..." << std::endl;
    WorldEnd server;
    server.fixed_m1_port = 18877;
    TEST_ASSERT(server.start(116), "world started on fixed port");

    world_app::CellRegisterConfig cfg;
    cfg.world_host = "127.0.0.1";
    cfg.world_port = 18877;
    cfg.cell_id = 61;
    cfg.advertise_host = "127.0.0.1";
    cfg.advertise_port = 8805;
    cfg.start_epoch = 9;
    cfg.route_json = kRouteJson;
    cfg.heartbeat_ms = 40;
    cfg.link_grace_ms = 150;  // 测试窗（生产默认 15000）
    std::atomic<int> freeze_fires = 0;
    std::atomic<int> grace_fires = 0;
    std::atomic<int> readmit_fires = 0;
    auto client = world_app::makeCellRegisterClient(cfg);
    client->set_link_freeze_listener([&](world_app::LinkFreezeReason reason) {
        ++freeze_fires;
        if (reason == world_app::LinkFreezeReason::kGraceExpired) {
            ++grace_fires;
        }
    });
    client->set_readmit_listener([&] { ++readmit_fires; });
    TEST_ASSERT(client->start(), "client started");
    TEST_ASSERT(wait_until([&] {
                    client->pump(now_ms());
                    return client->state().replies >= 1;
                }, 5000),
                "first registration");
    TEST_ASSERT(client->state().accepted, "accepted before outage");
    TEST_ASSERT(freeze_fires == 0 && readmit_fires == 0, "no freeze while link up");

    // 控制面失联：World 全停 → 链路 down → 宽限窗到期冻结（触发一次）
    server.stop();
    TEST_ASSERT(wait_until([&] {
                    client->pump(now_ms());
                    return freeze_fires.load() >= 1;
                }, 5000),
                "freeze fired after grace expiry");
    TEST_ASSERT(grace_fires == 1, "reason = grace expired");
    TEST_ASSERT(client->state().frozen, "frozen state visible");
    const auto frozen_at = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - frozen_at < std::chrono::milliseconds(250)) {
        client->pump(now_ms());
        std::this_thread::sleep_for(10ms);
    }
    TEST_ASSERT(freeze_fires == 1, "freeze fires once per outage");

    // 同址起新 World → 客户端重拨 → 新 World 重新受理（961 code 1）→ 解冻
    WorldEnd world2;
    world2.fixed_m1_port = 18877;
    TEST_ASSERT(world2.start(116), "world restarted on same port");
    TEST_ASSERT(wait_until([&] {
                    client->pump(now_ms());
                    return readmit_fires.load() >= 1;
                }, 5000),
                "re-admitted after fresh acceptance");
    TEST_ASSERT(freeze_fires == 1 && grace_fires == 1, "no re-freeze on recovery");
    TEST_ASSERT(!client->state().frozen, "frozen cleared on re-admission");
    TEST_ASSERT(client->state().accepted, "re-admission accepted");
    // 重拨首受理 code 1，随后 40ms 心跳幂等刷新 code 2——读面取末次回执，
    // 断言受理码族（1/2 均 = World 重新认可本端）
    const auto re_code = client->state().code;
    TEST_ASSERT(re_code == world_app::kCellRegisterAccepted
                    || re_code == world_app::kCellRegisterAcceptedIdempotent,
                "re-admission adjudicated accepted (fresh or idempotent)");

    client->stop();
    world2.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- 旧执行端隔离：代次水位（L101 隔离切片）----------
// 过期摘除忘却登记，但本进程曾受理的最高代次保留（内存水位）：摘除后旧
// 代次（< 水位）复活被拒（code 3，登记不复活）——旧执行端不能借摘除冒认
// 首注册；同代次（= 水位，同一化身断链回拨）仍按首注册受理（批㉔ 语义保
// 留）。持久墓碑（跨 World 重启）随管理日志批。

bool test_stale_epoch_watermark_isolation() {
    std::cout << "Running: test_stale_epoch_watermark_isolation..." << std::endl;
    WorldEnd server;
    server.config.cellExpiryMs = 150;  // 测试窗（生产默认 30000）
    TEST_ASSERT(server.start(117), "world started with M1 port");
    std::map<std::uint64_t, RegisterReply> replies;
    g_replies = &replies;

    CellEnd old_end;
    TEST_ASSERT(dial_cell(server, old_end, "cell-old"), "old epoch end dialed");
    CellEnd new_end;
    TEST_ASSERT(dial_cell(server, new_end, "cell-new"), "new epoch end dialed");
    RegisterReply r;
    TEST_ASSERT(
        register_roundtrip(old_end, 7101, 51, 100, "127.0.0.1", 9205, 0, 0, 0, true, r),
        "old epoch first registration");
    TEST_ASSERT(r.code == world_app::kCellRegisterAccepted, "code accepted");
    TEST_ASSERT(
        register_roundtrip(new_end, 7102, 51, 200, "127.0.0.1", 9206, 0, 0, 0, true, r),
        "new epoch replaced");
    TEST_ASSERT(r.code == world_app::kCellRegisterAccepted, "replace accepted");
    const auto* reg = server.world->find_cell_registration(51);
    TEST_ASSERT(reg != nullptr && reg->start_epoch == 200, "registry holds new epoch");

    // 双端停发 → 过期摘除（登记忘却，水位保留）
    TEST_ASSERT(wait_until([&] { return server.world->registered_cell_count() == 0; },
                           3000),
                "entry expired and removed");

    // 旧代次复活：水位 200 拒 100 → code 3，登记不复活
    TEST_ASSERT(
        register_roundtrip(old_end, 7103, 51, 100, "127.0.0.1", 9205, 0, 0, 0, true, r),
        "stale resurrection roundtrip");
    TEST_ASSERT(!r.accepted, "stale resurrection rejected");
    TEST_ASSERT(r.code == world_app::kCellRegisterRejectedStaleEpoch, "code stale epoch");
    TEST_ASSERT(server.world->registered_cell_count() == 0, "no resurrection entry");

    // 同代次（= 水位）复活：同一化身回拨 → 首注册受理（批㉔ 语义保留）
    TEST_ASSERT(
        register_roundtrip(new_end, 7104, 51, 200, "127.0.0.1", 9206, 0, 0, 0, true, r),
        "same epoch resurrection roundtrip");
    TEST_ASSERT(r.accepted, "same epoch admitted fresh");
    TEST_ASSERT(r.code == world_app::kCellRegisterAccepted, "code fresh accepted");
    reg = server.world->find_cell_registration(51);
    TEST_ASSERT(reg != nullptr && reg->start_epoch == 200, "re-entry holds same epoch");

    g_replies = nullptr;
    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- Space 回收与重分配（L108 回收首切片）----------
// 962 = 每 Space 权威归属视图（版本单调）：过期释放 owner→0（版本不变——
// 无认领发生不推进），再认领版本 +1（连续——防离线旧主 ABA 回放），旧主重
// 注册（同代次水位放行）即收全池视图（含失去 Space 的 ready=0 撤销标记）
// ——离线期缺口靠重注册收敛，不再有永劫 stale 路由。

bool test_space_release_and_reclaim() {
    std::cout << "Running: test_space_release_and_reclaim..." << std::endl;
    WorldEnd server;
    server.config.assignSpaces = {{7, 0}, {8, 0}};
    server.config.cellExpiryMs = 150;  // 测试窗（生产默认 30000）
    TEST_ASSERT(server.start(118), "world started with M1 port");
    std::map<std::uint64_t, RegisterReply> replies;
    g_replies = &replies;
    std::map<std::uint64_t, SpaceAssignFrame> assigns;
    g_assigns = &assigns;

    // A（cell 31）认领 7/8 v1
    CellEnd cell_a;
    TEST_ASSERT(dial_cell(server, cell_a, "cell-a"), "cell a dialed");
    RegisterReply r;
    TEST_ASSERT(
        register_roundtrip(cell_a, 9801, 31, 1, "127.0.0.1", 9207, 0, 0, 0, true, r),
        "cell a registered");
    TEST_ASSERT(wait_until([&] {
                    std::lock_guard<std::mutex> lock(g_assigns_mutex);
                    const auto it = assigns.find(7);
                    return it != assigns.end() && it->second.ready;
                }, 3000),
                "cell a claimed space 7");

    // A 停发（链路保持）→ 过期释放：owner→0，版本不变（无认领不推进）
    TEST_ASSERT(wait_until([&] {
                    const auto* a7 = server.world->find_space_assignment(7);
                    return a7 != nullptr && a7->cell_id == 0;
                }, 3000),
                "expired cell releases ownership");
    const auto* a7 = server.world->find_space_assignment(7);
    TEST_ASSERT(a7 != nullptr && a7->version == 1, "release keeps version");
    TEST_ASSERT(server.world->registered_cell_count() == 0, "registry emptied");

    // B（cell 32）注册认领 → v2（版本连续，防离线旧主 ABA）
    CellEnd cell_b;
    TEST_ASSERT(dial_cell(server, cell_b, "cell-b"), "cell b dialed");
    TEST_ASSERT(
        register_roundtrip(cell_b, 9802, 32, 1, "127.0.0.1", 9208, 0, 0, 0, true, r),
        "cell b registered");
    TEST_ASSERT(wait_until([&] {
                    const auto* a7b = server.world->find_space_assignment(7);
                    return a7b != nullptr && a7b->cell_id == 32 && a7b->version == 2;
                }, 3000),
                "cell b reclaims at version 2");
    TEST_ASSERT(wait_until([&] {
                    cell_b.pump();
                    std::lock_guard<std::mutex> lock(g_assigns_mutex);
                    const auto it = assigns.find(7);
                    return it != assigns.end() && it->second.ready
                           && it->second.assigned_cell_id == 32
                           && it->second.version == 2;
                }, 3000),
                "cell b receives ownership push at v2");

    // A 重注册（同代次，水位放行走首注册）→ 收 7/8 撤销标记 v2（owner B）
    TEST_ASSERT(
        register_roundtrip(cell_a, 9803, 31, 1, "127.0.0.1", 9207, 0, 0, 0, true, r),
        "cell a re-admitted fresh");
    TEST_ASSERT(r.code == world_app::kCellRegisterAccepted, "fresh admission");
    TEST_ASSERT(wait_until([&] {
                    cell_a.pump();
                    std::lock_guard<std::mutex> lock(g_assigns_mutex);
                    const auto it = assigns.find(7);
                    return it != assigns.end() && !it->second.ready
                           && it->second.assigned_cell_id == 32
                           && it->second.version == 2;
                }, 3000),
                "old owner converges to revocation view");

    g_assigns = nullptr;
    g_replies = nullptr;
    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- 持久墓碑（L101 管理日志切片）：代次水位跨 World 重启 ----------
// 批㉗ 水位是内存面（重启忘却）——墓碑日志把「本进程曾受理的最高启动代次」
// 落盘，旧执行端隔离跨 World 重启成立。同代次回拨仍放行（批㉔ 语义跨重启
// 保留）；不配墓碑 = 现状内存水位（不假装记得重启前的事）。
bool test_watermark_journal_persists_across_restart() {
    std::cout << "Running: test_watermark_journal_persists_across_restart..." << std::endl;
    namespace fs = std::filesystem;
    const fs::path journal_dir = fs::temp_directory_path()
        / ("apollo-restart-e2e-" + std::to_string(::getpid()));
    fs::create_directories(journal_dir);  // 临时目录不自动建坑账：落盘前显式建
    const fs::path journal = journal_dir / "tombstone-wm.txt";
    fs::remove(journal);

    std::map<std::uint64_t, RegisterReply> replies;
    g_replies = &replies;

    // W1：受理 cell 21 epoch 5（首注册）→ 停机。水位 5 进墓碑。
    WorldEnd w1;
    w1.config.worldJournalPath = journal.string();
    TEST_ASSERT(w1.start(121), "w1 started with journal");
    CellEnd c1;
    TEST_ASSERT(dial_cell(w1, c1, "cell-w1"), "cell dialed w1");
    RegisterReply r;
    TEST_ASSERT(register_roundtrip(c1, 4101, 21, 5, "127.0.0.1", 9301, 1, 10, 0, true, r),
                "w1 fresh admission epoch 5");
    TEST_ASSERT(r.code == world_app::kCellRegisterAccepted, "accepted code 1");
    w1.stop();

    // W2：同墓碑重启——epoch 4 复活拒 code 3（水位来自日志，内存已空）；
    // epoch 5 同代次仍首注册受理；epoch 6 替换受理且水位推进 6 落盘。
    WorldEnd w2;
    w2.config.worldJournalPath = journal.string();
    TEST_ASSERT(w2.start(122), "w2 restarted with journal");
    CellEnd c2;
    TEST_ASSERT(dial_cell(w2, c2, "cell-w2"), "cell dialed w2");
    TEST_ASSERT(register_roundtrip(c2, 4102, 21, 4, "127.0.0.1", 9301, 1, 10, 0, true, r),
                "stale epoch 4 roundtrip");
    TEST_ASSERT(r.code == world_app::kCellRegisterRejectedStaleEpoch,
                "journal watermark rejects 4");
    TEST_ASSERT(register_roundtrip(c2, 4103, 21, 5, "127.0.0.1", 9301, 1, 10, 0, true, r),
                "same-epoch 5 roundtrip");
    TEST_ASSERT(r.code == world_app::kCellRegisterAccepted,
                "same epoch 5 admitted fresh across restart");
    TEST_ASSERT(register_roundtrip(c2, 4104, 21, 6, "127.0.0.1", 9301, 1, 10, 0, true, r),
                "new epoch 6 roundtrip");
    TEST_ASSERT(r.code == world_app::kCellRegisterAccepted, "epoch 6 replaced");
    w2.stop();

    // W3：epoch 5 再复活仍拒（日志水位 6）；epoch 6 同代次受理。
    WorldEnd w3;
    w3.config.worldJournalPath = journal.string();
    TEST_ASSERT(w3.start(123), "w3 restarted with journal");
    CellEnd c3;
    TEST_ASSERT(dial_cell(w3, c3, "cell-w3"), "cell dialed w3");
    TEST_ASSERT(register_roundtrip(c3, 4105, 21, 5, "127.0.0.1", 9301, 1, 10, 0, true, r),
                "stale epoch 5 roundtrip");
    TEST_ASSERT(r.code == world_app::kCellRegisterRejectedStaleEpoch,
                "journal watermark rejects 5");
    TEST_ASSERT(register_roundtrip(c3, 4106, 21, 6, "127.0.0.1", 9301, 1, 10, 0, true, r),
                "same-epoch 6 roundtrip");
    TEST_ASSERT(r.code == world_app::kCellRegisterAccepted, "same epoch 6 admitted");
    w3.stop();

    // 对照组：不配墓碑 = 现状内存水位——重启忘却，epoch 7 仍受理。
    WorldEnd w4;
    TEST_ASSERT(w4.start(124), "w4 started without journal");
    CellEnd c4;
    TEST_ASSERT(dial_cell(w4, c4, "cell-w4"), "cell dialed w4");
    TEST_ASSERT(register_roundtrip(c4, 4107, 23, 8, "127.0.0.1", 9304, 1, 10, 0, true, r),
                "w4 fresh admission epoch 8");
    TEST_ASSERT(r.code == world_app::kCellRegisterAccepted, "accepted");
    w4.stop();
    WorldEnd w5;
    TEST_ASSERT(w5.start(125), "w5 restarted without journal");
    CellEnd c5;
    TEST_ASSERT(dial_cell(w5, c5, "cell-w5"), "cell dialed w5");
    TEST_ASSERT(register_roundtrip(c5, 4108, 23, 7, "127.0.0.1", 9304, 1, 10, 0, true, r),
                "epoch 7 roundtrip without journal");
    TEST_ASSERT(r.code == world_app::kCellRegisterAccepted,
                "no journal: watermark forgotten on restart");
    g_replies = nullptr;
    w5.stop();

    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- 管理日志 v2（断连对账批A 切片）：分配终态跨重启 ----------
// A/R/B 行（设计稿 world-reconciliation §2）：认领转移记 A（write-ahead——
// A 行 append+flush 成功才推 962，持久决定先于对外声明）、释放记 R（owner
// 归零版本不变）、启动记 B（重启代次 = 最大 B + 1，先记后开受理面）。装载
// 终态重建 = 每 space 最后一条 A/R 为归属；v1 裸行仍按 T 解析（不迁移）。
// 重启后旧主未归队前 Space 不另端重建（设计稿 §4「过期窗不来」行）——装载
// 主进 pending 过期窗（cellExpiryMs），窗内不归队释放回池（R 行），重认领
// 版本 +1 依赖装载版本（池条目重建保留历史版本防 ABA 倒退）。
bool test_journal_v2_assignment_state_across_restart() {
    std::cout << "Running: test_journal_v2_assignment_state_across_restart..." << std::endl;
    namespace fs = std::filesystem;
    const fs::path journal_dir = "/tmp/apollo-cell-register-tests";
    fs::create_directories(journal_dir);  // 临时目录不自动建坑账：落盘前显式建
    const fs::path journal = journal_dir / "journal-v2.txt";
    fs::remove(journal);

    // v1 兼容预置：裸行 `81|9` = 旧格式水位，装载后 epoch 8 复活拒 code 3。
    {
        std::ofstream seed(journal, std::ios::binary);
        seed << "81|9\n";
    }

    std::map<std::uint64_t, RegisterReply> replies;
    g_replies = &replies;
    std::map<std::uint64_t, SpaceAssignFrame> assigns;
    g_assigns = &assigns;
    RegisterReply r;

    // W1：认领 61/62 → A 行落盘；B 行首启铸初始代次（配置 77）。
    WorldEnd w1;
    w1.config.worldJournalPath = journal.string();
    w1.config.assignSpaces = {{61, 0}, {62, 0}};
    w1.config.cellExpiryMs = 5000;  // 本段无关过期（停机即收）
    TEST_ASSERT(w1.start(121), "w1 started with journal");
    CellEnd cx;
    TEST_ASSERT(dial_cell(w1, cx, "cell-compat"), "compat cell dialed");
    TEST_ASSERT(register_roundtrip(cx, 4201, 81, 8, "127.0.0.1", 9350, 1, 10, 0, true, r),
                "compat stale epoch 8 roundtrip");
    TEST_ASSERT(r.code == world_app::kCellRegisterRejectedStaleEpoch,
                "v1 bare line seeds watermark");
    CellEnd c1;
    TEST_ASSERT(dial_cell(w1, c1, "cell-w1"), "cell 71 dialed w1");
    TEST_ASSERT(register_roundtrip(c1, 4202, 71, 1, "127.0.0.1", 9301, 1, 10, 0, true, r),
                "cell 71 fresh admission");
    TEST_ASSERT(wait_until([&] {
                    std::lock_guard<std::mutex> lock(g_assigns_mutex);
                    return assigns.count(61) && assigns.count(62)
                           && assigns[61].ready && assigns[62].ready;
                }, 3000),
                "cell 71 claimed 61/62");
    // write-ahead 旁证：声明已发出 = A/B 行已在盘上。
    {
        std::ifstream in(journal, std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
        TEST_ASSERT(text.find("\nB 77\n") != std::string::npos,
                    "B row cast at first boot");
        TEST_ASSERT(text.find("\nA 61|71|1\n") != std::string::npos,
                    "A row 61 on disk");
        TEST_ASSERT(text.find("\nA 62|71|1\n") != std::string::npos,
                    "A row 62 on disk");
    }
    w1.stop();

    // W2 重启：装载终态 = 61/62 归 71 v1（任何登记前即重建）；B 铸代次 78。
    WorldEnd w2;
    w2.config.worldJournalPath = journal.string();
    w2.config.assignSpaces = {{61, 0}, {62, 0}};
    w2.config.cellExpiryMs = 5000;  // 71 归队例不受 pending 过期窗干扰
    TEST_ASSERT(w2.start(122), "w2 restarted with journal");
    const auto* s61 = w2.world->find_space_assignment(61);
    TEST_ASSERT(s61 != nullptr && s61->cell_id == 71 && s61->version == 1,
                "journal replays ownership before any registration");
    const auto* s62 = w2.world->find_space_assignment(62);
    TEST_ASSERT(s62 != nullptr && s62->cell_id == 71 && s62->version == 1,
                "journal replays space 62 ownership");

    // 旁证 cell 72 ready 登记：不抢 61/62（不立即另端重建），收 owner 71
    // 撤销标记；961 回带管理代次 = 重启新铸 78。
    CellEnd c2;
    TEST_ASSERT(dial_cell(w2, c2, "cell-w2"), "cell 72 dialed w2");
    TEST_ASSERT(register_roundtrip(c2, 4203, 72, 1, "127.0.0.1", 9302, 1, 10, 0, true, r),
                "cell 72 registered");
    TEST_ASSERT(r.management_epoch == 78, "restart recasts management epoch max B + 1");
    TEST_ASSERT(wait_until([&] {
                    c2.pump();
                    std::lock_guard<std::mutex> lock(g_assigns_mutex);
                    return assigns.count(61) && !assigns[61].ready
                           && assigns[61].assigned_cell_id == 71;
                }, 3000),
                "absent owner keeps spaces; other cell gets revoke marker");
    const auto* after = w2.world->find_space_assignment(61);
    TEST_ASSERT(after != nullptr && after->cell_id == 71 && after->version == 1,
                "not rebuilt on other side");

    // 旧主 71 归队（同代次复核受理）→ 归属保持版本不变（刷新非转移）。
    CellEnd c1b;
    TEST_ASSERT(dial_cell(w2, c1b, "cell-w1b"), "cell 71 redialed w2");
    TEST_ASSERT(register_roundtrip(c1b, 4204, 71, 1, "127.0.0.1", 9301, 1, 10, 0, true, r),
                "cell 71 re-admitted after restart");
    TEST_ASSERT(r.code == world_app::kCellRegisterAccepted, "same epoch fresh admission");
    TEST_ASSERT(wait_until([&] {
                    c1b.pump();
                    std::lock_guard<std::mutex> lock(g_assigns_mutex);
                    return assigns[61].ready && assigns[61].assigned_cell_id == 71
                           && assigns[61].version == 1;
                }, 3000),
                "owner retains spaces at same version after restart");
    w2.stop();

    // W3：owner 71 不归队 → 一个过期窗后 pending 释放回池（R 行落盘），
    // 随后 cell 73 首注册重认领 v2（装载版本保留 → 重认领 +1 不倒退）。
    WorldEnd w3;
    w3.config.worldJournalPath = journal.string();
    w3.config.assignSpaces = {{61, 0}, {62, 0}};
    w3.config.cellExpiryMs = 250;  // pending 过期窗（测试窗）
    TEST_ASSERT(w3.start(123), "w3 restarted with journal");
    TEST_ASSERT(wait_until([&] {
                    const auto* p = w3.world->find_space_assignment(61);
                    return p != nullptr && p->cell_id == 0;
                }, 3000),
                "absent journal owner expires and releases");
    {
        std::ifstream in(journal, std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
        TEST_ASSERT(text.find("\nR 61\n") != std::string::npos,
                    "R row on disk for 61");
    }
    CellEnd c3;
    TEST_ASSERT(dial_cell(w3, c3, "cell-w3"), "cell 73 dialed w3");
    TEST_ASSERT(register_roundtrip(c3, 4205, 73, 1, "127.0.0.1", 9303, 1, 10, 0, true, r),
                "cell 73 registered on w3");
    TEST_ASSERT(wait_until([&] {
                    c3.pump();
                    std::lock_guard<std::mutex> lock(g_assigns_mutex);
                    return assigns.count(61) && assigns[61].ready
                           && assigns[61].assigned_cell_id == 73
                           && assigns[61].version == 2;
                }, 3000),
                "released space reclaimed at version 2");
    w3.stop();

    // 损坏行拒启回归（v2 行格式）：A 行缺版本字段 = 损坏，显式拒启。
    {
        std::ofstream out(journal, std::ios::app | std::ios::binary);
        out << "A 61|71\n";
    }
    WorldEnd w4;
    w4.config.worldJournalPath = journal.string();
    TEST_ASSERT(!w4.start(124), "corrupt v2 row refuses boot");

    g_assigns = nullptr;
    g_replies = nullptr;
    std::cout << "  PASSED" << std::endl;
    return true;
}

// 重启 pending 归属窗单调基准：journal 不携带历史时间戳时以注入 bootAtMs
// 为锚，与 registered_at_ms 共用 steady 毫秒和 cellExpiryMs 同一口径。
bool test_journal_owner_boot_basis_window() {
    std::cout << "Running: test_journal_owner_boot_basis_window..." << std::endl;
    namespace fs = std::filesystem;
    const fs::path journal_dir = "/tmp/apollo-cell-register-tests";
    fs::create_directories(journal_dir);
    const fs::path journal = journal_dir / "boot-basis-window.txt";
    fs::remove_all(journal_dir);
    fs::create_directories(journal_dir);
    std::map<std::uint64_t, RegisterReply> replies;
    g_replies = &replies;
    std::map<std::uint64_t, SpaceAssignFrame> boot_assigns;
    g_assigns = &boot_assigns;

    // 源世界先持有既有管理关系证据（T 行），否则启动即按缺失日志冻结分配。
    {
        std::ofstream evidence(journal, std::ios::binary | std::ios::trunc);
        evidence << "80|8\n";
    }
    WorldEnd source;
    source.config.worldJournalPath = journal.string();
    source.config.assignSpaces = {{7, 0}, {8, 0}};
    TEST_ASSERT(source.start(140), "evidence source world started");
    CellEnd evidence_cell;
    RegisterReply r;
    TEST_ASSERT(dial_cell(source, evidence_cell, "cell-evidence"),
                "evidence cell dialed");
    TEST_ASSERT(register_roundtrip(evidence_cell, 4299, 81, 9, "127.0.0.1", 9400,
                                   1, 10, 0, true, r),
                "evidence cell registered");
    source.stop();

    // 首启生成 A 终态；v2 行只有归属/版本，重启时必须补时间基准。
    WorldEnd w1;
    w1.config.worldJournalPath = journal.string();
    w1.config.assignSpaces = {{7, 0}, {8, 0}};
    w1.config.cellExpiryMs = 0;
    TEST_ASSERT(w1.start(141), "journal source world started");
    CellEnd c1;
    TEST_ASSERT(dial_cell(w1, c1, "cell-w1"), "journal owner dialed");
    TEST_ASSERT(register_roundtrip(c1, 4301, 81, 9, "127.0.0.1", 9401, 1, 10, 0,
                                   true, r),
                "journal owner registered");
    TEST_ASSERT(wait_until([&] {
                    c1.pump();
                    std::lock_guard<std::mutex> lock(g_assigns_mutex);
                    return boot_assigns.count(7) > 0 && boot_assigns.count(8) > 0
                           && boot_assigns[7].ready && boot_assigns[8].ready
                           && boot_assigns[7].assigned_cell_id == 81
                           && boot_assigns[8].assigned_cell_id == 81;
                }, 3000),
                "journal source claimed both spaces");
    {
        std::lock_guard<std::mutex> lock(g_assigns_mutex);
        TEST_ASSERT(boot_assigns[7].assigned_cell_id == 81
                        && boot_assigns[7].version == 1,
                    "journal source assigned space 7");
        TEST_ASSERT(boot_assigns[8].assigned_cell_id == 81
                        && boot_assigns[8].version == 1,
                    "journal source assigned space 8");
    }
    w1.stop();
    g_assigns = nullptr;

    const auto write_journal = [&](std::uint64_t boot_basis_row) {
        std::ofstream out(journal, std::ios::binary | std::ios::trunc);
        out << "T 81|" << boot_basis_row << "\nB 77\n"
            << "A 7|81|1\nA 8|81|1\n";
    };

    // 历史锚已超窗：不依赖当前钟，W2 启动后第一拍即释放回池。
    const std::uint64_t expired_boot = world_app::steady_now_ms() - 100;
    write_journal(expired_boot);
    WorldEnd w2;
    w2.config.worldJournalPath = journal.string();
    w2.config.assignSpaces = {{7, 0}, {8, 0}};
    w2.config.bootAtMs = expired_boot;
    w2.config.cellExpiryMs = 40;
    TEST_ASSERT(w2.start(142), "expired injected basis booted");
    TEST_ASSERT(wait_until([&] {
                    const auto* first = w2.world->find_space_assignment(7);
                    const auto* second = w2.world->find_space_assignment(8);
                    return first != nullptr && first->cell_id == 0
                           && second != nullptr && second->cell_id == 0;
                }, 2000),
                "injected past boot basis releases both owners");
    w2.stop();

    // 历史锚远未到期：真身重注册清除 pending，按登记语义重新认领并版本 +1。
    const std::uint64_t future_boot = world_app::steady_now_ms() + 1000000;
    write_journal(future_boot);
    WorldEnd w3;
    w3.config.worldJournalPath = journal.string();
    w3.config.assignSpaces = {{7, 0}, {8, 0}};
    w3.config.bootAtMs = future_boot;
    w3.config.cellExpiryMs = 2000;
    TEST_ASSERT(w3.start(143), "future injected basis booted");
    CellEnd c2;
    TEST_ASSERT(dial_cell(w3, c2, "cell-w3"), "journal owner redialed");
    TEST_ASSERT(register_roundtrip(c2, 4302, 81, future_boot + 1, "127.0.0.1", 9401, 1, 10, 0,
                                   true, r),
                "journal owner re-admitted");
    TEST_ASSERT(r.code == world_app::kCellRegisterAccepted,
                "restart owner re-admitted");
    {
        const auto* first = w3.world->find_space_assignment(7);
        const auto* second = w3.world->find_space_assignment(8);
        TEST_ASSERT(first != nullptr && first->cell_id == 81 && first->version == 2,
                    "injected future basis retains space 7");
        TEST_ASSERT(second != nullptr && second->cell_id == 81 && second->version == 2,
                    "injected future basis retains space 8");
    }
    g_replies = nullptr;
    g_assigns = nullptr;
    w3.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// write-ahead 声明闸（批A）：A 行 append 失败 = 不对外声明——认领内存回滚
//（Space 留池待下次受理重试）、962 不发；受理面照常（R/B/T 行失败沿用
// 批㉚ 告警降级，可用性优先）。锁写入失败拒启单列；A 行失败用启动后摘卷
// 复现。
bool test_journal_v2_write_ahead_gates_declaration() {
    std::cout << "Running: test_journal_v2_write_ahead_gates_declaration..." << std::endl;
    namespace fs = std::filesystem;
    const fs::path missing_dir = "/tmp/apollo-cell-register-tests/missing-dir";
    fs::remove_all(missing_dir);
    {
        WorldEnd lock_probe;
        lock_probe.config.worldJournalPath = (missing_dir / "journal.txt").string();
        TEST_ASSERT(!lock_probe.start(136),
                    "world refuses boot when journal lock cannot be written");
    }
    fs::create_directories(missing_dir);
    WorldEnd server;
    server.config.worldJournalPath = (missing_dir / "journal.txt").string();
    server.config.assignSpaces = {{65, 0}, {66, 0}};
    TEST_ASSERT(server.start(126), "world started for write-ahead gate");
    fs::remove_all(missing_dir);  // 启动后摘卷 = A 行 append 确定性失败
    std::map<std::uint64_t, RegisterReply> replies;
    g_replies = &replies;
    std::map<std::uint64_t, SpaceAssignFrame> assigns;
    g_assigns = &assigns;
    CellEnd cell;
    TEST_ASSERT(dial_cell(server, cell, "cell-wa"), "cell dialed");
    RegisterReply r;
    TEST_ASSERT(register_roundtrip(cell, 4301, 74, 1, "127.0.0.1", 9306, 1, 10, 0, true, r),
                "registration accepted despite journal failure");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));  // 数拍确认无滞后声明
    TEST_ASSERT(server.world->assigned_space_count() == 0,
                "claim reverted when A row append failed");
    {
        std::lock_guard<std::mutex> lock(g_assigns_mutex);
        TEST_ASSERT(assigns.empty(), "no 962 ownership declaration sent");
    }
    g_assigns = nullptr;
    g_replies = nullptr;
    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- 对账自报与逐项裁决（断连对账批B 切片）----------
// 967 cell_space_report（设计稿 world-reconciliation §3/§4）：cell 在 961 受理
// 后逐 Space 自报本地已应用归属视图（每条目一帧——生成链无重复组）；World
// 逐项核对权威归属表：匹配保持 / 版本冲突该 Space 冻结（不认领不释放、冲突
// 集观测面）/ 未登记权威（池外 Space）冻结 / 释放态上报 = 收敛滞后不冲突 /
// 未登记或旧化身上报丢弃。无专用回执——962 全池权威视图即裁决载体。
bool test_cell_space_report_adjudication() {
    std::cout << "Running: test_cell_space_report_adjudication..." << std::endl;
    WorldEnd server;
    server.config.assignSpaces = {{71, 0}, {72, 0}};
    server.config.cellExpiryMs = 2500;  // 测试窗（生产默认 30000）
    TEST_ASSERT(server.start(127), "world started with M1 port");
    std::map<std::uint64_t, RegisterReply> replies;
    g_replies = &replies;
    std::map<std::uint64_t, SpaceAssignFrame> assigns;
    g_assigns = &assigns;
    RegisterReply r;

    // cell 81 认领 71/72 v1。
    CellEnd cell_a;
    TEST_ASSERT(dial_cell(server, cell_a, "cell-report-a"), "cell a dialed");
    TEST_ASSERT(register_roundtrip(cell_a, 4401, 81, 1, "127.0.0.1", 9401, 1, 10, 0, true, r),
                "cell 81 registered");
    TEST_ASSERT(wait_until([&] {
                    std::lock_guard<std::mutex> lock(g_assigns_mutex);
                    return assigns.count(71) && assigns.count(72)
                           && assigns[71].ready && assigns[72].ready;
                }, 3000),
                "cell 81 claimed 71/72");

    // 匹配自报：两 Space 均按权威视图上报 → 无冲突，幂等重复不重裁决。
    TEST_ASSERT(cell_a.send_space_report(1, 81, 1, 71, 1, 77), "report 71 v1");
    TEST_ASSERT(cell_a.send_space_report(2, 81, 1, 72, 1, 77), "report 72 v1");
    TEST_ASSERT(wait_until([&] {
                    cell_a.pump();  // 出队发送（kAccepted = 入队，泵才上线）
                    return server.world->space_report_count() >= 2;
                }, 3000),
                "match reports adjudicated");
    TEST_ASSERT(server.world->conflicted_space_count() == 0, "matching view: no conflict");
    TEST_ASSERT(cell_a.send_space_report(3, 81, 1, 71, 1, 77), "idempotent report");
    TEST_ASSERT(wait_until([&] {
                    cell_a.pump();
                    return server.world->space_report_count() >= 3;
                }, 3000),
                "idempotent report adjudicated");
    TEST_ASSERT(server.world->conflicted_space_count() == 0, "repeat keeps no conflict");

    // 版本冲突自报：72 报 v9（权威 v1）→ 72 冻结；71 报文不变不受累。
    TEST_ASSERT(cell_a.send_space_report(4, 81, 1, 72, 9, 77), "conflict report 72 v9");
    TEST_ASSERT(wait_until([&] {
                    cell_a.pump();
                    return server.world->conflicted_space_count() == 1;
                }, 3000),
                "version conflict freezes space 72");

    // 旁证 cell 82：71/72 均归属 81——71 收撤销标记、72 冻结完全静默（不推
    // 新分配也不推撤销标记）。账清零重记：后续断言只看 cell 82 收到的帧
    //（全局账含 cell 81 既有收帧，不能作旁证）。
    CellEnd cell_b;
    {
        std::lock_guard<std::mutex> lock(g_assigns_mutex);
        assigns.clear();
    }
    TEST_ASSERT(dial_cell(server, cell_b, "cell-report-b"), "cell b dialed");
    TEST_ASSERT(register_roundtrip(cell_b, 4402, 82, 1, "127.0.0.1", 9402, 1, 10, 0, true, r),
                "cell 82 registered");
    TEST_ASSERT(wait_until([&] {
                    cell_b.pump();
                    std::lock_guard<std::mutex> lock(g_assigns_mutex);
                    return assigns.count(71) && !assigns[71].ready
                           && assigns[71].assigned_cell_id == 81;
                }, 3000),
                "cell 82 gets revoke marker for 71");
    {
        std::lock_guard<std::mutex> lock(g_assigns_mutex);
        TEST_ASSERT(!assigns.count(72), "frozen space 72 not pushed at all");
    }
    const auto* frozen = server.world->find_space_assignment(72);
    TEST_ASSERT(frozen != nullptr && frozen->cell_id == 81, "frozen space keeps owner");

    // 未登记权威：上报池外 Space 999 → 冲突集 +1。
    TEST_ASSERT(cell_b.send_space_report(5, 82, 1, 999, 1, 77), "out-of-pool report");
    TEST_ASSERT(wait_until([&] {
                    cell_b.pump();
                    return server.world->conflicted_space_count() == 2;
                }, 3000),
                "unregistered authority freezes 999");

    // 过期摘除 cell 81：71 正常释放回池（owner→0），冻结的 72 不释放。
    TEST_ASSERT(wait_until([&] {
                    const auto* a71 = server.world->find_space_assignment(71);
                    return a71 != nullptr && a71->cell_id == 0;
                }, 4000),
                "expired owner releases 71");
    const auto* held = server.world->find_space_assignment(72);
    TEST_ASSERT(held != nullptr && held->cell_id == 81, "conflict freeze blocks release");

    // 释放态上报 = 收敛滞后：cell 82 报 71（owner 已 0）→ 不入冲突集。
    TEST_ASSERT(cell_b.send_space_report(6, 82, 1, 71, 1, 77), "released-space report");
    TEST_ASSERT(wait_until([&] {
                    cell_b.pump();
                    return server.world->space_report_count() >= 5;
                }, 3000),
                "released-space report adjudicated");
    TEST_ASSERT(server.world->conflicted_space_count() == 2,
                "released-space report is convergence lag, not conflict");

    // 释放的 71 可被重认领（冻结不外溢）：cell 83 首注册 → 71 v2，72 仍冻结。
    CellEnd cell_c;
    TEST_ASSERT(dial_cell(server, cell_c, "cell-report-c"), "cell c dialed");
    TEST_ASSERT(register_roundtrip(cell_c, 4403, 83, 1, "127.0.0.1", 9403, 1, 10, 0, true, r),
                "cell 83 registered");
    TEST_ASSERT(wait_until([&] {
                    const auto* a71 = server.world->find_space_assignment(71);
                    return a71 != nullptr && a71->cell_id == 83 && a71->version == 2;
                }, 3000),
                "released 71 reclaimed at v2");
    const auto* still = server.world->find_space_assignment(72);
    TEST_ASSERT(still != nullptr && still->cell_id == 81 && server.world->conflicted_space_count() == 2,
                "72 stays frozen for claims and reports");

    // 未登记/旧化身上报：丢弃不裁决（计数不前进、冲突集不动；泵上下行走完
    // 发送链——确证丢弃发生在受理面裁决，而非报文滞留队列）。
    TEST_ASSERT(cell_b.send_space_report(7, 77, 1, 71, 1, 77), "unregistered report");
    TEST_ASSERT(cell_b.send_space_report(8, 82, 9999, 71, 1, 77), "stale epoch report");
    {
        const auto deadline = std::chrono::steady_clock::now()
            + std::chrono::milliseconds(300);
        while (std::chrono::steady_clock::now() < deadline) {
            cell_b.pump();
            std::this_thread::sleep_for(10ms);
        }
    }
    TEST_ASSERT(server.world->space_report_count() == 6, "dropped reports not adjudicated");
    TEST_ASSERT(server.world->conflicted_space_count() == 2, "no conflict from dropped reports");

    g_assigns = nullptr;
    g_replies = nullptr;
    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// 生产客户端自报接线（批B）：CellRegisterClient 在 961 受理后自动逐 Space
// 上发 967（已应用归属视图）——World 侧匹配裁决无冲突。
bool test_production_client_space_report() {
    std::cout << "Running: test_production_client_space_report..." << std::endl;
    WorldEnd server;
    server.config.assignSpaces = {{7, 0}};
    TEST_ASSERT(server.start(128), "world started with M1 port");

    world_app::CellRegisterConfig cfg;
    cfg.world_host = "127.0.0.1";
    cfg.world_port = server.world->m1Link().listenPort();
    cfg.cell_id = 31;
    cfg.advertise_host = "127.0.0.1";
    cfg.advertise_port = 8803;
    cfg.start_epoch = 9;
    cfg.route_json = kRouteJson;
    cfg.heartbeat_ms = 40;
    auto client = world_app::makeCellRegisterClient(cfg);
    TEST_ASSERT(client->start(), "client started");
    TEST_ASSERT(wait_until([&] {
                    client->pump(now_ms());
                    return server.world->space_report_count() >= 1
                           && server.world->conflicted_space_count() == 0;
                }, 5000),
                "client self-reported applied view after admission");
    const auto* a7 = server.world->find_space_assignment(7);
    TEST_ASSERT(a7 != nullptr && a7->cell_id == 31 && a7->version == 1,
                "assignment matches reported view");
    client->stop();
    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// 重启 e2e（world-reconciliation §5 批C）：WorldApp 进程内真链路重启——
// W1 认领/自报 → stop → W2 同 journal 重启铸 B 新代次；cell 重拨受理后
// 967 匹配，无冲突时 962 闸开。真端点重启，但 World 与 cell 仍同测试进程，
// 不引入外部监督器。
bool test_world_restart_e2e_and_negative_cases() {
    std::cout << "Running: test_world_restart_e2e_and_negative_cases..." << std::endl;
    namespace fs = std::filesystem;
    const fs::path journal_dir = "/tmp/apollo-cell-register-tests";
    fs::create_directories(journal_dir);
    const fs::path journal = journal_dir / "restart-e2e.txt";
    const fs::path lock = journal.string() + ".lock";
    fs::remove(journal);
    fs::remove(lock);
    {
        std::ofstream seed(journal, std::ios::binary);
        seed << "7|7\nA 7|7|1\nR 7\n";
    }
    WorldEnd w1;
    w1.config.worldJournalPath = journal.string();
    w1.config.assignSpaces = {{7, 0}};
    w1.config.m1AuthorityEpoch = 91;
    w1.config.cellExpiryMs = 200;
    w1.fixed_m1_port = 19877;
    TEST_ASSERT(w1.start(131), "w1 started");
    std::map<std::uint64_t, RegisterReply> replies;
    g_replies = &replies;
    std::map<std::uint64_t, SpaceAssignFrame> assigns;
    g_assigns = &assigns;
    world_app::CellRegisterConfig cell_cfg;
    cell_cfg.world_host = "127.0.0.1";
    cell_cfg.world_port = w1.world->m1Link().listenPort();
    cell_cfg.cell_id = 7;
    cell_cfg.advertise_host = "127.0.0.1";
    cell_cfg.advertise_port = 8803;
    cell_cfg.start_epoch = 7;
    cell_cfg.route_json = kRouteJson;
    cell_cfg.heartbeat_ms = 20;
    auto cell = world_app::makeCellRegisterClient(cell_cfg);
    std::map<std::uint64_t, std::array<std::uint64_t, 3>> applied_assigns;
    std::mutex assigns_mutex;
    cell->setSpaceAssignListener([&applied_assigns, &assigns_mutex](
                                     const world_app::SpaceAssignRecord& rec) {
        std::lock_guard<std::mutex> lock(assigns_mutex);
        applied_assigns[rec.space_id] = {rec.version, rec.management_epoch,
                                          rec.assigned_cell_id};
    });
    TEST_ASSERT(cell->start(), "cell client dialed w1");
    RegisterReply r;
    TEST_ASSERT(wait_until([&] {
                    cell->pump(now_ms());
                    const auto state = cell->state();
                    r.accepted = state.accepted;
                    r.management_epoch = state.management_epoch;
                    return state.replies >= 1;
                }, 5000), "w1 admission");
    TEST_ASSERT(wait_until([&] {
                    cell->pump(now_ms());
                    std::lock_guard<std::mutex> lock(assigns_mutex);
                    return w1.world->space_report_count() >= 1
                           && w1.world->conflicted_space_count() == 0
                           && applied_assigns.count(7)
                           && applied_assigns[7][1] == 91;
                }, 5000), "w1 matched report and assignment gate opened");
    std::cout << "  w1 state: accepted=" << r.accepted
              << " epoch=" << r.management_epoch << std::endl;
    cell->stop();
    w1.stop();

    // W2 固定同一控制面端口，客户端断链端点不变即可真重拨。
    WorldEnd w2;
    w2.config = w1.config;
    w2.config.worldJournalPath = journal.string();
    w2.config.assignSpaces = {{7, 0}};
    w2.config.m1AuthorityEpoch = 92;
    w2.config.cellExpiryMs = 200;
    w2.fixed_m1_port = 19877;
    TEST_ASSERT(w2.start(132), "w2 restarted from journal");
    TEST_ASSERT(w2.world->m1Link().listenPort() == 19877, "restart listened");
    cell_cfg.world_port = 19877;
    auto redial = world_app::makeCellRegisterClient(cell_cfg);
    redial->setSpaceAssignListener([&applied_assigns, &assigns_mutex](
                                     const world_app::SpaceAssignRecord& rec) {
        std::lock_guard<std::mutex> lock(assigns_mutex);
        applied_assigns[rec.space_id] = {rec.version, rec.management_epoch,
                                          rec.assigned_cell_id};
    });
    TEST_ASSERT(redial->start(), "cell client re-dialed w2");
    TEST_ASSERT(wait_until([&] {
                    redial->pump(now_ms());
                    const auto state = redial->state();
                    r.accepted = state.accepted;
                    r.management_epoch = state.management_epoch;
                    return state.replies >= 1;
                }, 5000), "redial admission");
    TEST_ASSERT(r.accepted && r.management_epoch == 92,
                "fresh B generation returned on redial");
    applied_assigns.clear();
    TEST_ASSERT(wait_until([&] {
                    redial->pump(now_ms());
                    return redial->linkUp();
                }, 5000), "redial link up before recovery assertions");
    TEST_ASSERT(wait_until([&] {
                    redial->pump(now_ms());
                    std::lock_guard<std::mutex> lock(assigns_mutex);
                    return w2.world->space_report_count() >= 1
                           && w2.world->conflicted_space_count() == 0
                           && redial->space_assign_count() == 1
                           && redial->state().management_epoch == 92;
                }, 10000), "967 matched and recovery gate opened");
    w2.stop();
    redial->stop();

    // 陈旧锁破锁：W2 正常释放后预置他人死 pid 锁，启动重写为本进程 owner。
    WorldEnd live_lock;
    live_lock.config.worldJournalPath = journal.string();
    {
        std::ofstream out(lock, std::ios::trunc);
        out << "999999:2020-01-01T00:00:00Z\n";
    }
    TEST_ASSERT(live_lock.start(133), "stale foreign lock is broken and boot continues");
    {
        std::ifstream in(lock);
        std::string owner;
        TEST_ASSERT(std::getline(in, owner), "stale lock owner replaced");
        TEST_ASSERT(owner.rfind(std::to_string(::getpid()) + ":", 0) == 0,
                    "boot records current owner after stale break");
    }
    live_lock.stop();

    // 活锁拒启：预置可读测试进程自身 pid 锁；当前进程确为持锁活进程。
    {
        std::ofstream out(lock, std::ios::trunc);
        out << ::getpid() << ":2020-01-01T00:00:00Z\n";
    }
    WorldEnd active_lock;
    active_lock.config.worldJournalPath = journal.string();
    TEST_ASSERT(!active_lock.start(135), "live owner lock refuses replacement boot");
    fs::remove(lock);
    g_assigns = nullptr;

    // 空日志负例：journal 缺失但 T 水位证明 cell 曾拨入 → 全池冻结。
    fs::remove(journal);
    WorldEnd missing_journal;
    missing_journal.config.worldJournalPath = journal.string();
    missing_journal.config.assignSpaces = {{7, 0}};
    TEST_ASSERT(missing_journal.start(134), "missing journal with tombstone boots");
    std::map<std::uint64_t, SpaceAssignFrame> frozen_assigns;
    g_assigns = &frozen_assigns;
    CellEnd walk_in;
    TEST_ASSERT(dial_cell(missing_journal, walk_in, "cell-missing-journal"),
                "cell walked into missing journal world");
    TEST_ASSERT(register_roundtrip(walk_in, 5203, 82, 7, "127.0.0.1", 9402, 1, 10, 0,
                                   true, r), "walk-in registration roundtrip");
    TEST_ASSERT(r.accepted, "walk-in admission does not violate log rule");
    const auto* frozen = missing_journal.world->find_space_assignment(7);
    TEST_ASSERT(frozen == nullptr || frozen->cell_id == 0,
                "missing journal freezes all new assignment");
    TEST_ASSERT(wait_until([&] {
                    walk_in.pump();
                    std::lock_guard<std::mutex> lock(g_assigns_mutex);
                    return frozen_assigns.empty();
                }, 300), "no 962 declarations while evidence missing");

    g_assigns = nullptr;
    g_replies = nullptr;
    cell->stop();
    redial->stop();
    w1.stop();
    w2.stop();
    missing_journal.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// 断连存疑标记（L101 断连存疑切片）：控制链路断（对端 TCP 关）→ 注册表
// 条目置存疑（gap 表 §line71：断开后先标记存疑对账，不立即另端重建——
// 本切片只落标记与清除，对账/重建约束随既有过期释放面）；同 ID 同代次
// 重注册（961 复核受理）清存疑。观测面 suspect_cell_count（轮询口径同
// registered_cell_count）。
bool test_cell_link_down_marks_suspect() {
    std::cout << "Running: test_cell_link_down_marks_suspect..." << std::endl;
    WorldEnd server;
    TEST_ASSERT(server.start(115), "world started with M1 port");
    std::map<std::uint64_t, RegisterReply> replies;
    g_replies = &replies;

    // 注册：cell 31 epoch 5 受理，无存疑
    CellEnd c1;
    TEST_ASSERT(dial_cell(server, c1, "cell-suspect-1"), "cell dialed");
    RegisterReply r;
    TEST_ASSERT(register_roundtrip(c1, 6101, 31, 5, "127.0.0.1", 9311, 1, 10, 0,
                                   true, r),
                "register reply arrived");
    TEST_ASSERT(r.accepted, "registered");
    TEST_ASSERT(server.world->registered_cell_count() == 1, "one registration");
    TEST_ASSERT(server.world->suspect_cell_count() == 0, "fresh entry not suspect");

    // 断链：LinkManager 析构 → TCP 关 → 服务端泵读 EOF → on_down → 存疑
    c1.manager.reset();
    TEST_ASSERT(wait_until([&] { return server.world->suspect_cell_count() == 1; },
                           3000),
                "link down marks suspect");
    const auto* reg = server.world->find_cell_registration(31);
    TEST_ASSERT(reg != nullptr && reg->suspect, "suspect visible on entry");

    // 同 ID 同代次重注册（断链重拨 walk-in）：复核受理清存疑
    CellEnd c2;
    TEST_ASSERT(dial_cell(server, c2, "cell-suspect-2"), "cell re-dialed");
    TEST_ASSERT(register_roundtrip(c2, 6102, 31, 5, "127.0.0.1", 9311, 1, 10, 0,
                                   true, r),
                "re-register reply arrived");
    TEST_ASSERT(r.accepted, "same-epoch walk-in accepted");
    TEST_ASSERT(wait_until([&] { return server.world->suspect_cell_count() == 0; },
                           3000),
                "verified re-admission clears suspect");
    reg = server.world->find_cell_registration(31);
    TEST_ASSERT(reg != nullptr && !reg->suspect, "entry clear after re-admit");
    TEST_ASSERT(reg->peer == "cell-suspect-2", "peer updated to new link");

    g_replies = nullptr;
    server.stop();
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
                    && test_production_client_invalid_config()
                    && test_cell_heartbeat_and_expiry()
                    && test_cell_space_assignment()
                    && test_space_assignment_readiness()
                    && test_space_assignment_load_balance()
                    && test_space_assignment_capability_filter()
                    && test_cell_register_compat_adjudication()
                    && test_cell_retire_and_reclaim()
                    && test_cell_retire_management_command()
                    && test_production_client_space_assign_listener()
                    && test_grace_freeze_and_readmit()
                    && test_stale_epoch_watermark_isolation()
                    && test_space_release_and_reclaim()
                    && test_watermark_journal_persists_across_restart()
                    && test_journal_v2_assignment_state_across_restart()
                    && test_journal_owner_boot_basis_window()
                    && test_journal_v2_write_ahead_gates_declaration()
                    && test_cell_space_report_adjudication()
                    && test_production_client_space_report()
                    && test_world_restart_e2e_and_negative_cases()
                    && test_cell_link_down_marks_suspect();
    if (!ok || g_failures != 0) {
        std::cout << "CELL REGISTER TESTS FAILED (" << g_failures << ")" << std::endl;
        return 1;
    }
    std::cout << "ALL CELL REGISTER TESTS PASSED" << std::endl;
    return 0;
#endif
}
