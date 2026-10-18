// World 排空端到端（P4 更新排空批；契约 956/957）：
//   956 控制面（裸帧，真 TCP LinkManager 拨 WorldApp M1 端点）+ 真实小队
//   上行（gateway M1Egress 意图族 → SquadInstanceService）——排空语义：
//   lifecycle：begin 闭排空闸（新 launch 拒 1118）→ 既有副本照常 enter/
//   start/结算/销毁 → active 归零落 drained（自然线，detail「all instances
//   settled」）→ 终态仍拒新（drained 不反闸）；双对端同面（gateway 意图
//   链 + fleetctl 控制链同拨 9101——fan-out 按名回执消歧回归面）。
//   deadline_force：期限到未清强停（abort 取消线收尾——detail「force-
//   aborted」，容量归还）；
//   unknown_op：显式拒点名（state 不被触碰，begin 后续照常）。
// 断言事实源：957 回执内容 + 客户端投递口收到的协议帧（日志不算）。
// 无 GTest，plain-assert + main 汇总。
#include "world_app/world_app.hpp"
#include "world_app/m1_link_server.hpp"
#include "gateway/m1_egress.hpp"
#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/wire/contract_wire.hpp"
#include "apollo_contract.h"
#include "apollo/protocol/codec.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {
namespace link = apollo::net::link;
namespace wire = apollo::net::wire;
namespace protocol = apollo::protocol;
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
constexpr std::uint16_t kDrainRequestId = id_of(MsgId::world_drain_request);
constexpr std::uint16_t kDrainReplyId = id_of(MsgId::world_drain_reply);
const char* kRouteJson = APOLLO_TEST_ROUTE_JSON;

std::uint64_t now_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

struct TemporaryDirectory {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("apollo-world-drain-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ~TemporaryDirectory() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

// 收到的客户端帧（会话号 + 协议类型 + 原始 body），下行断言事实源。
struct DeliveredFrame {
    std::uint64_t session_id;
    std::uint16_t type;
    std::vector<std::uint8_t> body;
};

class ClientFace {
public:
    void accept(std::uint64_t session_id, const std::vector<std::uint8_t>& payload) {
        const auto header = protocol::MessageCodec::parseHeader(payload);
        frames_.push_back({session_id, header.type,
                           std::vector<std::uint8_t>(payload.begin() +
                                                     sizeof(protocol::MessageHeader),
                                                     payload.end())});
    }

    std::vector<DeliveredFrame> take(std::uint16_t type) {
        std::vector<DeliveredFrame> out;
        for (auto& frame : frames_) {
            if (frame.type == type) out.push_back(frame);
        }
        return out;
    }

private:
    std::vector<DeliveredFrame> frames_;
};

// pump_until：泵 egress 一拍 + 轮询谓词（tag 只为超时定位）。
template <typename Pred>
void pump_until(gateway::M1Egress& egress, Pred&& done, int timeout_ms,
                const char* tag) {
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(timeout_ms);
    while (!done()) {
        if (std::chrono::steady_clock::now() > deadline) {
            throw std::runtime_error(std::string("pump_until deadline exceeded: ")
                                     + tag);
        }
        egress.pump(now_ms());
        std::this_thread::sleep_for(10ms);
    }
    egress.pump(now_ms());
}

// 取指定会话指定 op 的最近一条 SQUAD_RESULT code（未见过 = -1）。
std::int32_t last_squad_result_code(ClientFace& clients, std::uint64_t session_id,
                                    std::int32_t op) {
    std::int32_t code = -1;
    for (const auto& frame : clients.take(
                 static_cast<std::uint16_t>(protocol::MessageType::SQUAD_RESULT))) {
        if (frame.session_id != session_id) continue;
        const auto decoded =
            protocol::MessageCodec::decodeBody<protocol::SquadResult>(frame.body);
        if (decoded.op == op) code = decoded.code;
    }
    return code;
}

protocol::CellCreateEntity make_player_spawn(std::uint64_t player_id) {
    protocol::CellCreateEntity msg;
    msg.entityId = player_id;  // P0-2：player 身份 = 创建请求实体号
    msg.entityType = protocol::EntityType::PLAYER;
    msg.spaceId = 0;  // 默认 scene
    return msg;
}

// 会话建图 + 建队 + 入队 + 开副本（队长 = roster 首位）；返回 party_id。
std::uint64_t spawn_and_launch(
    gateway::M1Egress& egress, ClientFace& clients,
    const std::vector<std::pair<std::uint64_t, std::uint64_t>>& roster) {
    for (const auto [session_id, player_id] : roster) {
        const auto frame =
            protocol::MessageCodec::encode(make_player_spawn(player_id), session_id);
        if (!egress.forwardIntent(session_id, frame)) {
            throw std::runtime_error("spawn intent refused");
        }
    }
    const auto leader_session = roster.front().first;
    if (!egress.forwardIntent(leader_session,
                              protocol::MessageCodec::encode(protocol::PartyCreateRequest{},
                                                             leader_session))) {
        throw std::runtime_error("party create refused");
    }
    protocol::PartyResult created{};
    pump_until(egress,
               [&] {
                   for (const auto& frame : clients.take(static_cast<std::uint16_t>(
                            protocol::MessageType::PARTY_RESULT))) {
                       if (frame.session_id != leader_session) continue;
                       created = protocol::MessageCodec::decodeBody<protocol::PartyResult>(
                           frame.body);
                   }
                   return created.op ==
                              static_cast<std::int32_t>(protocol::SquadOp::PARTY_CREATE) &&
                          created.code == 0;
               },
               5000, "party-create-result");
    if (created.partyId == 0) throw std::runtime_error("party id not allocated");
    for (std::size_t i = 1; i < roster.size(); ++i) {
        protocol::PartyJoinRequest join;
        join.partyId = created.partyId;
        if (!egress.forwardIntent(roster[i].first,
                                  protocol::MessageCodec::encode(join,
                                                                 roster[i].first))) {
            throw std::runtime_error("party join refused");
        }
    }
    protocol::SquadLaunch launch;
    launch.partyId = created.partyId;
    if (!egress.forwardIntent(leader_session,
                              protocol::MessageCodec::encode(launch, leader_session))) {
        throw std::runtime_error("squad launch refused");
    }
    return created.partyId;
}

// 副本推进到 Running（roster 三连号全员 enter + 队长 start），返回怪表。
std::vector<std::uint64_t> start_and_collect_monsters(gateway::M1Egress& egress,
                                                      ClientFace& clients,
                                                      std::uint64_t party_id,
                                                      std::uint64_t leader_session) {
    for (const auto session :
         {leader_session, leader_session + 1, leader_session + 2}) {
        protocol::SquadEnter enter;
        enter.partyId = party_id;
        if (!egress.forwardIntent(session,
                                  protocol::MessageCodec::encode(enter, session))) {
            throw std::runtime_error("squad enter refused");
        }
    }
    protocol::SquadStart start;
    start.partyId = party_id;
    if (!egress.forwardIntent(leader_session,
                              protocol::MessageCodec::encode(start, leader_session))) {
        throw std::runtime_error("squad start refused");
    }
    std::vector<DeliveredFrame> running;
    pump_until(egress,
               [&] {
                   running.clear();
                   for (const auto& frame : clients.take(static_cast<std::uint16_t>(
                            protocol::MessageType::SQUAD_STATE))) {
                       const auto decoded =
                           protocol::MessageCodec::decodeBody<protocol::SquadState>(
                               frame.body);
                       if (decoded.partyId == party_id && decoded.phase == 3 &&
                           !decoded.monsters.empty()) {
                           running.push_back(frame);
                       }
                   }
                   return !running.empty();
               },
               5000, "running-state");
    return protocol::MessageCodec::decodeBody<protocol::SquadState>(running.front().body)
        .monsters;
}

// 击杀一怪并等 SQUAD_KILL code 0 回执。
void kill_monster(gateway::M1Egress& egress, ClientFace& clients,
                  std::uint64_t party_id, std::uint64_t leader_session,
                  std::uint64_t monster_id) {
    protocol::SquadKill kill;
    kill.partyId = party_id;
    kill.monsterEntityId = monster_id;
    if (!egress.forwardIntent(leader_session,
                              protocol::MessageCodec::encode(kill, leader_session))) {
        throw std::runtime_error("squad kill refused");
    }
    pump_until(egress,
               [&] {
                   for (const auto& frame : clients.take(static_cast<std::uint16_t>(
                            protocol::MessageType::SQUAD_RESULT))) {
                       const auto decoded =
                           protocol::MessageCodec::decodeBody<protocol::SquadResult>(
                               frame.body);
                       if (decoded.op ==
                               static_cast<std::int32_t>(protocol::SquadOp::SQUAD_KILL) &&
                           decoded.code == 0 && decoded.partyId == party_id) {
                           return true;
                       }
                   }
                   return false;
               },
               5000, "kill-ack");
}

// 957 回执账（ops dispatcher 写；drain_roundtrip 读取；测试串行，指针注入）。
struct DrainReply {
    bool ok = false;
    std::int32_t state = -1;
    std::uint64_t active = 0;
    std::string detail;
};

std::mutex g_replies_mutex;
std::map<std::uint64_t, DrainReply>* g_replies = nullptr;

// ops 客户端（fleetctl 投影）：裸 956 上行，957 按 request_id 收账。
struct OpsEnd {
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
        mc.self_name = "fleetctl";
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
        dispatcher.set_handler(kDrainReplyId,
                               [](std::uint16_t, const std::uint8_t* data,
                                  std::size_t size) {
                                   std::vector<wire::FieldValue> fields;
                                   if (!wire::decode_payload(kDrainReplyId, data, size,
                                                             fields)
                                       || fields.size() != 6) {
                                       return;
                                   }
                                   DrainReply r;
                                   r.ok = fields[2].integer != 0;
                                   r.state =
                                       static_cast<std::int32_t>(fields[3].integer);
                                   r.active =
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

    bool send_drain(std::uint64_t request_id, std::int32_t op, std::int64_t deadline_s) {
        std::vector<wire::FieldValue> f(3);
        f[0].integer = static_cast<std::int64_t>(request_id);
        f[1].integer = op;
        f[2].integer = deadline_s;
        std::vector<std::uint8_t> payload;
        if (!wire::encode_payload(kDrainRequestId, f, payload)) {
            return false;
        }
        return manager->link("world")
                   ->send(kDrainRequestId, payload.data(), payload.size(),
                          link::SendClass::kReliable)
               == link::SendCode::kAccepted;
    }
};

// 956 单发 + 957 等（request_id 关联；服务端 gameLoop 自泵，本线程泵客户端）。
bool drain_roundtrip(OpsEnd& ops, std::uint64_t request_id, std::int32_t op,
                     std::int64_t deadline_s, DrainReply& out, int timeout_ms = 5000) {
    if (!ops.send_drain(request_id, op, deadline_s)) {
        return false;
    }
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        ops.pump();
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

// 轮询 status 到 drained（2）终态。
bool poll_drained(OpsEnd& ops, std::uint64_t request_id, DrainReply& out,
                  int timeout_ms = 8000) {
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (!drain_roundtrip(ops, request_id, 2, 0, out)) {
            return false;
        }
        if (out.state == 2) {
            return true;
        }
        std::this_thread::sleep_for(50ms);
    }
    return false;
}

// 服务端壳：WorldApp（m1Port 0 内核分配；档案宿主已迁 baseServer）。
struct WorldEnd {
    TemporaryDirectory files;
    WorldAppConfig config;
    std::unique_ptr<WorldApp> world;

    bool start(std::uint32_t world_id) {
        config.worldId = world_id;
        config.host = "127.0.0.1";
        config.m1Port = 0;
        config.m1RouteJson = kRouteJson;
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

// 拨装：ops（956 控制面）拨 WorldApp M1 端点并等待链路 up（egress 由调用方
// 先行拨装——控制链与意图链同面双对端，fan-out 消歧回归面）。
bool dial_ops(WorldEnd& server, OpsEnd& ops) {
    if (!ops.start(server.world->m1Link().listenPort())) {
        return false;
    }
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (!ops.up()) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        ops.pump();
        std::this_thread::sleep_for(10ms);
    }
    return true;
}

// ---------- lifecycle：闭闸停分新副本，既有副本服务至结算销毁 ----------

bool test_world_drain_lifecycle() {
    std::cout << "Running: test_world_drain_lifecycle..." << std::endl;
    WorldEnd server;
    TEST_ASSERT(server.start(44), "world started with M1 port");
    gateway::M1EgressConfig egress_config;
    egress_config.host = "127.0.0.1";
    egress_config.port = server.world->m1Link().listenPort();
    egress_config.route_json = kRouteJson;
    auto egress = gateway::makeM1Egress(std::move(egress_config));
    TEST_ASSERT(egress->start(), "egress started");
    ClientFace clients;
    egress->setClientDeliver(
        [&clients](std::uint64_t session_id, std::uint32_t,
                   const std::vector<std::uint8_t>& payload) {
            clients.accept(session_id, payload);
        });
    std::map<std::uint64_t, DrainReply> replies;
    g_replies = &replies;
    OpsEnd ops;
    TEST_ASSERT(dial_ops(server, ops), "ops dialed");

    // serving 快照：state 0 / active 0 / detail serving
    DrainReply r;
    TEST_ASSERT(drain_roundtrip(ops, 101, 2, 0, r), "status reply arrived");
    TEST_ASSERT(r.ok && r.state == 0 && r.active == 0 && r.detail == "serving",
                "serving snapshot");

    // A 队（9001-9003）开启副本到 Running（控制链已在线——意图链回执照常）
    auto* squads = server.world->squadInstanceService();
    TEST_ASSERT(squads != nullptr, "squad service hosted");
    const auto party_a = spawn_and_launch(*egress, clients,
                                          {{9001, 100}, {9002, 101}, {9003, 102}});
    const auto monsters = start_and_collect_monsters(*egress, clients, party_a, 9001);
    TEST_ASSERT(monsters.size() == 3, "host goal wave spawned");
    TEST_ASSERT(squads->active_count() == 1, "one active binding");

    // begin（无期限）：闭排空闸 → draining
    TEST_ASSERT(drain_roundtrip(ops, 102, 1, 0, r), "begin reply arrived");
    TEST_ASSERT(r.ok && r.state == 1 && r.active == 1, "begin draining with active 1");
    TEST_ASSERT(r.detail.find("draining") != std::string::npos, "draining detail");
    TEST_ASSERT(squads->draining(), "service drain gate closed");

    // 闭闸期新分配被拒（错误码 1118 契约显形）：B 队（9101/200 单人）launch
    const auto party_b = spawn_and_launch(*egress, clients, {{9101, 200}});
    protocol::SquadLaunch launch_b;
    launch_b.partyId = party_b;
    TEST_ASSERT(egress->forwardIntent(9101,
                                      protocol::MessageCodec::encode(launch_b, 9101)),
                "launch B intent accepted");
    pump_until(*egress,
               [&] {
                   return last_squad_result_code(clients, 9101,
                                                 static_cast<std::int32_t>(
                                                     protocol::SquadOp::SQUAD_LAUNCH))
                       >= 0;
               },
               5000, "launch-b-reject");
    TEST_ASSERT(last_squad_result_code(clients, 9101,
                                       static_cast<std::int32_t>(
                                           protocol::SquadOp::SQUAD_LAUNCH)) == 1118,
                "launch rejected 1118 while draining");

    // 既有副本照常服务：击杀三怪 → 结算 → Destroyed 回执到全员
    for (const auto monster : monsters) {
        kill_monster(*egress, clients, party_a, 9001, monster);
    }
    pump_until(*egress,
               [&] {
                   std::set<std::uint64_t> reached;
                   for (const auto& frame : clients.take(static_cast<std::uint16_t>(
                            protocol::MessageType::SQUAD_STATE))) {
                       const auto decoded =
                           protocol::MessageCodec::decodeBody<protocol::SquadState>(
                               frame.body);
                       if (decoded.partyId == party_a && decoded.phase == 7) {
                           reached.insert(frame.session_id);
                       }
                   }
                   return reached.size() == 3;
               },
               5000, "destroyed-broadcast");

    // active 归零 → drained（自然线）
    TEST_ASSERT(poll_drained(ops, 103, r), "drained terminal reached");
    TEST_ASSERT(r.active == 0, "no active bindings at drained");
    TEST_ASSERT(r.detail.find("all instances settled") != std::string::npos,
                "natural drained detail");
    TEST_ASSERT(squads->used_capacity(44) == 0, "capacity returned");

    // 终态不反闸：drained 后新分配仍拒（两次 launch 均被同一闸拒，取末次）
    protocol::SquadLaunch relaunch;
    relaunch.partyId = party_b;
    TEST_ASSERT(egress->forwardIntent(9101,
                                      protocol::MessageCodec::encode(relaunch, 9101)),
                "relaunch intent accepted");
    pump_until(*egress,
               [&] {
                   return last_squad_result_code(clients, 9101,
                                                 static_cast<std::int32_t>(
                                                     protocol::SquadOp::SQUAD_LAUNCH))
                       >= 0;
               },
               5000, "relaunch-reject");
    TEST_ASSERT(last_squad_result_code(clients, 9101,
                                       static_cast<std::int32_t>(
                                           protocol::SquadOp::SQUAD_LAUNCH)) == 1118,
                "post-drained launch still rejected 1118");

    g_replies = nullptr;
    egress->stop();
    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- deadline_force：期限到未清强停（abort 取消线） ----------

bool test_world_drain_deadline_force() {
    std::cout << "Running: test_world_drain_deadline_force..." << std::endl;
    WorldEnd server;
    TEST_ASSERT(server.start(45), "world started with M1 port");
    gateway::M1EgressConfig egress_config;
    egress_config.host = "127.0.0.1";
    egress_config.port = server.world->m1Link().listenPort();
    egress_config.route_json = kRouteJson;
    auto egress = gateway::makeM1Egress(std::move(egress_config));
    TEST_ASSERT(egress->start(), "egress started");
    ClientFace clients;
    egress->setClientDeliver(
        [&clients](std::uint64_t session_id, std::uint32_t,
                   const std::vector<std::uint8_t>& payload) {
            clients.accept(session_id, payload);
        });
    std::map<std::uint64_t, DrainReply> replies;
    g_replies = &replies;
    OpsEnd ops;
    TEST_ASSERT(dial_ops(server, ops), "ops dialed");

    // C 队（9201/300 单人）开副本但无人进入（Create 相位悬挂——永不自然结算）
    const auto party_c = spawn_and_launch(*egress, clients, {{9201, 300}});
    pump_until(*egress,
               [&] {
                   return last_squad_result_code(clients, 9201,
                                                 static_cast<std::int32_t>(
                                                     protocol::SquadOp::SQUAD_LAUNCH)) == 0;
               },
               5000, "launch-c-ok");
    auto* squads = server.world->squadInstanceService();
    TEST_ASSERT(squads->active_count() == 1, "one active binding");

    // begin deadline_s = 1 → 期限到强停
    DrainReply r;
    TEST_ASSERT(drain_roundtrip(ops, 201, 1, 1, r), "begin reply arrived");
    TEST_ASSERT(r.ok && r.state == 1 && r.active == 1, "draining with active 1");

    TEST_ASSERT(poll_drained(ops, 202, r), "drained terminal reached by deadline");
    TEST_ASSERT(r.active == 0, "force-stop cleared bindings");
    TEST_ASSERT(r.detail.find("force-aborted") != std::string::npos,
                "force-stop visible in detail");
    TEST_ASSERT(squads->used_capacity(45) == 0, "capacity returned after force-stop");
    TEST_ASSERT(squads->find_by_party(party_c) == nullptr, "binding released");

    g_replies = nullptr;
    egress->stop();
    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- unknown_op：显式拒点名，状态机不被触碰 ----------

bool test_world_drain_unknown_op() {
    std::cout << "Running: test_world_drain_unknown_op..." << std::endl;
    WorldEnd server;
    TEST_ASSERT(server.start(46), "world started with M1 port");
    gateway::M1EgressConfig egress_config;
    egress_config.host = "127.0.0.1";
    egress_config.port = server.world->m1Link().listenPort();
    egress_config.route_json = kRouteJson;
    auto egress = gateway::makeM1Egress(std::move(egress_config));
    TEST_ASSERT(egress->start(), "egress started");
    ClientFace clients;
    egress->setClientDeliver(
        [&clients](std::uint64_t, std::uint32_t, const std::vector<std::uint8_t>&) {});
    std::map<std::uint64_t, DrainReply> replies;
    g_replies = &replies;
    OpsEnd ops;
    TEST_ASSERT(dial_ops(server, ops), "ops dialed");

    DrainReply r;
    TEST_ASSERT(drain_roundtrip(ops, 301, 9, 0, r), "unknown op reply arrived");
    TEST_ASSERT(!r.ok, "unknown op rejected");
    TEST_ASSERT(r.detail.find("unknown op 9") != std::string::npos,
                "rejection names the op");
    TEST_ASSERT(r.state == 0, "state machine untouched");

    // 后续 begin 照常（空世界 → 下拍即 drained）
    TEST_ASSERT(drain_roundtrip(ops, 302, 1, 0, r), "begin after bogus op");
    TEST_ASSERT(r.ok && r.state == 1, "begin accepted");
    TEST_ASSERT(poll_drained(ops, 303, r), "empty world drains next tick");
    TEST_ASSERT(r.detail.find("all instances settled") != std::string::npos,
                "natural drained detail");

    g_replies = nullptr;
    egress->stop();
    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

} // namespace

int main() {
#if defined(_WIN32)
    std::cout << "SKIP: world drain e2e requires POSIX TCP\n";
    return 0;
#else
    std::map<std::uint64_t, DrainReply> replies;
    g_replies = &replies;
    const bool ok = test_world_drain_lifecycle() && test_world_drain_deadline_force()
                    && test_world_drain_unknown_op();
    g_replies = nullptr;
    if (!ok || g_failures != 0) {
        std::cout << "WORLD DRAIN TESTS FAILED (" << g_failures << ")" << std::endl;
        return 1;
    }
    std::cout << "ALL WORLD DRAIN TESTS PASSED" << std::endl;
    return 0;
#endif
}
