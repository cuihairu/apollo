// 更新连续性验收：真实 World 与控制链、Gateway 出口和真实客户端 TCP。
// Lua 用例验证健康模块替换期间副本继续运行及原连接/会话续用；认证仅使用
// Gateway 原型入口，不覆盖生产 Login 或跨 Gateway 恢复。
// C++ 用例验证排空、内部连接重建、有界未发送队列及新副本分配恢复；不声称
// 旧 Instance 跨进程保留。断言依据为协议回执与客户端实收帧。
#include "world_app/world_app.hpp"
#include "world_app/m1_link_server.hpp"
#include "gateway/m1_egress.hpp"
#include "gateway/gateway_server.hpp"
#include "apollo/net/link/frame.hpp"
#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/wire/contract_wire.hpp"
#include "apollo_contract.h"
#include "apollo/protocol/codec.hpp"
#include "player_store/database_service.hpp"

#include <atomic>
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

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#endif

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
constexpr std::uint16_t kRolloutRequestId = id_of(MsgId::module_rollout_request);
constexpr std::uint16_t kRolloutReplyId = id_of(MsgId::module_rollout_reply);
constexpr std::uint16_t kDrainRequestId = id_of(MsgId::world_drain_request);
constexpr std::uint16_t kDrainReplyId = id_of(MsgId::world_drain_reply);
const char* kRouteJson = APOLLO_TEST_ROUTE_JSON;
// 健康模块（观察窗零 fault）：onTick 空转 + smoke 真。
const char* kModuleGood =
    "return { onTick = function() end, smoke = function() return true end }";

std::uint64_t now_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

struct TemporaryDirectory {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("apollo-update-continuity-" +
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

// ---------- ops 端（fleetctl 投影）：952/953 模块投递 + 956/957 排空 ----------

struct RolloutReply {
    bool ok = false;
    std::int32_t state = -1;
    std::uint64_t version = 0;
    std::string detail;
};

struct DrainReply {
    bool ok = false;
    std::int32_t state = -1;
    std::uint64_t active = 0;
    std::string detail;
};

std::mutex g_replies_mutex;
std::map<std::uint64_t, RolloutReply>* g_rollout_replies = nullptr;
std::map<std::uint64_t, DrainReply>* g_drain_replies = nullptr;

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
        dispatcher.set_handler(kRolloutReplyId,
                               [](std::uint16_t, const std::uint8_t* data,
                                  std::size_t size) {
                                   std::vector<wire::FieldValue> fields;
                                   if (!wire::decode_payload(kRolloutReplyId, data,
                                                             size, fields)
                                       || fields.size() != 7) {
                                       return;
                                   }
                                   RolloutReply r;
                                   r.ok = fields[2].integer != 0;
                                   r.state =
                                       static_cast<std::int32_t>(fields[3].integer);
                                   r.version =
                                       static_cast<std::uint64_t>(fields[5].integer);
                                   r.detail = fields[6].text;
                                   const auto request_id =
                                       static_cast<std::uint64_t>(fields[0].integer);
                                   std::lock_guard<std::mutex> lock(g_replies_mutex);
                                   if (g_rollout_replies != nullptr) {
                                       (*g_rollout_replies)[request_id] = std::move(r);
                                   }
                               });
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
                                   if (g_drain_replies != nullptr) {
                                       (*g_drain_replies)[request_id] = std::move(r);
                                   }
                               });
        return true;
    }

    void pump() { manager->pump(now_ms()); }

    bool up() { return manager->link("world")->state() == link::LinkState::kUp; }

    // 952 裸帧上行（8 字段序：request_id/op/name/version/deps/source/
    // observe_ticks/fault_threshold）。
    bool send_rollout(std::uint64_t request_id, std::int32_t op,
                      const std::string& name, std::uint64_t version,
                      const std::string& source, std::uint64_t observe_ticks,
                      std::uint64_t fault_threshold) {
        std::vector<wire::FieldValue> f(8);
        f[0].integer = static_cast<std::int64_t>(request_id);
        f[1].integer = op;
        f[2].text = name;
        f[3].integer = static_cast<std::int64_t>(version);
        f[4].text = "";
        f[5].text = source;
        f[6].integer = static_cast<std::int64_t>(observe_ticks);
        f[7].integer = static_cast<std::int64_t>(fault_threshold);
        std::vector<std::uint8_t> payload;
        if (!wire::encode_payload(kRolloutRequestId, f, payload)) {
            return false;
        }
        return manager->link("world")
                   ->send(kRolloutRequestId, payload.data(), payload.size(),
                          link::SendClass::kReliable)
               == link::SendCode::kAccepted;
    }

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

// 953 等（意图链与控制链双泵：952 受理回执走 ops 控制链，egress 泵不能少——
// egress 重拨/心跳与 ops 派发各吃各的拍）。
bool wait_rollout_reply(gateway::M1Egress& egress, OpsEnd& ops,
                        std::uint64_t request_id, RolloutReply& out,
                        int timeout_ms = 5000) {
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        egress.pump(now_ms());
        ops.pump();
        {
            std::lock_guard<std::mutex> lock(g_replies_mutex);
            const auto it = g_rollout_replies->find(request_id);
            if (it != g_rollout_replies->end()) {
                out = it->second;
                return true;
            }
        }
        std::this_thread::sleep_for(10ms);
    }
    return false;
}

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
            const auto it = g_drain_replies->find(request_id);
            if (it != g_drain_replies->end()) {
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

// 轮询 rollout status 到期望相位（ModuleRollout 原值）。
bool poll_rollout_state(OpsEnd& ops, std::int32_t want_state,
                        std::uint64_t want_version, RolloutReply& out,
                        int timeout_ms = 10000) {
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(timeout_ms);
    std::uint64_t id = 900;
    while (std::chrono::steady_clock::now() < deadline) {
        ++id;
        if (!ops.send_rollout(id, 4, "greet", 0, "", 0, 0)) {
            return false;
        }
        const auto wait = std::chrono::steady_clock::now() + 300ms;
        while (std::chrono::steady_clock::now() < wait) {
            ops.pump();
            {
                std::lock_guard<std::mutex> lock(g_replies_mutex);
                const auto it = g_rollout_replies->find(id);
                if (it != g_rollout_replies->end()) {
                    out = it->second;
                    if (out.state == want_state && out.version == want_version) {
                        return true;
                    }
                    break;
                }
            }
            std::this_thread::sleep_for(5ms);
        }
        std::this_thread::sleep_for(20ms);
    }
    return false;
}

// 服务端壳：WorldApp（port 0 = 内核分配；替换路径给固定口重听）+ playerStore
// 落临时目录（seed=false 复用既有档——进程替换后档案仍在）。
struct WorldEnd {
    TemporaryDirectory files;
    WorldAppConfig config;
    std::unique_ptr<WorldApp> world;

    bool start(std::uint32_t world_id, std::uint16_t port = 0, bool seed = true,
               std::uint64_t epoch = 0) {
        config.worldId = world_id;
        config.host = "127.0.0.1";
        config.m1Port = port;
        config.m1RouteJson = kRouteJson;
        config.m1AuthorityEpoch = epoch;
        config.tickRateMs = 20;
        config.playerStoreEnabled = true;
        config.playerStore.m1Enabled = true;
        config.playerStore.host = "127.0.0.1";
        config.playerStore.dataDir = (files.path / "players").string();
        config.playerStore.journalPath = (files.path / "journal/persist.log").string();
        if (seed) {
            player_store::DatabaseService database(config.playerStore);
            if (!database.initialize()) {
                return false;
            }
            for (const auto id : {100u, 101u, 102u, 200u, 300u}) {
                player_store::PlayerData profile{};
                profile.playerId = id;
                profile.username = "p" + std::to_string(id);
                if (!database.createPlayer(profile)) {
                    return false;
                }
            }
            database.shutdown();
        }
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

// 拨装：ops 拨 WorldApp M1 端点并等链路 up。
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

// ---------- Lua 更新：出口和副本协议链路 ----------

bool test_update_continuity_lua_module() {
    std::cout << "Running: test_update_continuity_lua_module..." << std::endl;
    WorldEnd server;
    TEST_ASSERT(server.start(52), "world started with M1 port");
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
    std::map<std::uint64_t, DrainReply> drain_replies;
    std::map<std::uint64_t, RolloutReply> rollout_replies;
    g_drain_replies = &drain_replies;
    g_rollout_replies = &rollout_replies;
    OpsEnd ops;
    TEST_ASSERT(dial_ops(server, ops), "ops dialed");

    // A 队开启副本到 Running，建立活动副本基线。
    auto* squads = server.world->squadInstanceService();
    TEST_ASSERT(squads != nullptr, "squad service hosted");
    const auto party_a = spawn_and_launch(*egress, clients,
                                          {{9001, 100}, {9002, 101}, {9003, 102}});
    const auto monsters = start_and_collect_monsters(*egress, clients, party_a, 9001);
    TEST_ASSERT(monsters.size() == 3, "host goal wave spawned");
    TEST_ASSERT(egress->worldConnected(), "M1 link up at baseline");

    // prepare：预备沙盒（不激活）——副本存活不受触碰
    RolloutReply r;
    TEST_ASSERT(ops.send_rollout(11, 1, "greet", 7, kModuleGood, 50, 0),
                "prepare sent");
    TEST_ASSERT(wait_rollout_reply(*egress, ops, 11, r), "prepare reply arrived");
    TEST_ASSERT(r.ok && r.state == 2, "prepare accepted kCanaryPending");

    // commit：观察窗开启 + 模块激活——副本跨热替换存活（活动副本连续）
    TEST_ASSERT(ops.send_rollout(12, 2, "greet", 7, "", 0, 0), "commit sent");
    TEST_ASSERT(wait_rollout_reply(*egress, ops, 12, r), "commit reply arrived");
    TEST_ASSERT(r.ok && r.state == 3 && r.version == 7,
                "commit accepted kObserving v7");

    // 观察窗内击杀一怪（副本照常推进——连续不是只活着，还要能打）
    kill_monster(*egress, clients, party_a, 9001, monsters.front());

    // 观察窗内新队伍受理（服务持续接入：B 队 9004/300 单人建队+开副本）
    const auto party_b = spawn_and_launch(*egress, clients, {{9004, 300}});
    pump_until(*egress,
               [&] {
                   return last_squad_result_code(clients, 9004,
                                                 static_cast<std::int32_t>(
                                                     protocol::SquadOp::SQUAD_LAUNCH)) == 0;
               },
               5000, "launch-b-mid-window");

    // A 队余怪清完 → 结算 → Destroyed 回执到全员（副本完整走完生命周期）
    for (std::size_t i = 1; i < monsters.size(); ++i) {
        kill_monster(*egress, clients, party_a, 9001, monsters[i]);
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

    // 观察窗自驱到 kPromoted（健康窗零 fault——热替换收敛）
    RolloutReply st;
    TEST_ASSERT(poll_rollout_state(ops, 6, 7, st), "rollout promoted v7");

    // 原会话免重登续用：会话 9001 不重进图直接再建队（会话→玩家绑定未动）
    TEST_ASSERT(egress->forwardIntent(
                    9001, protocol::MessageCodec::encode(protocol::PartyCreateRequest{},
                                                         9001)),
                "party re-create intent accepted");
    protocol::PartyResult recreated{};
    pump_until(*egress,
               [&] {
                   for (const auto& frame : clients.take(static_cast<std::uint16_t>(
                            protocol::MessageType::PARTY_RESULT))) {
                       if (frame.session_id != 9001) continue;
                       const auto decoded =
                           protocol::MessageCodec::decodeBody<protocol::PartyResult>(
                               frame.body);
                       if (decoded.op ==
                           static_cast<std::int32_t>(protocol::SquadOp::PARTY_CREATE)) {
                           recreated = decoded;
                       }
                   }
                   return recreated.code == 0 && recreated.partyId != party_a;
               },
               5000, "party-recreate-same-session");
    TEST_ASSERT(recreated.code == 0, "same session served without re-login");

    // 更新后链路仍可用；本用例未建立真实客户端 TCP，不能证明客户端连接连续。
    TEST_ASSERT(egress->worldConnected(), "M1 link up after rollout");

    g_drain_replies = nullptr;
    g_rollout_replies = nullptr;
    egress->stop();
    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---------- process_replace：C++ 更新路径（排空 + 进程替换） ----------

bool test_update_continuity_process_replace() {
    std::cout << "Running: test_update_continuity_process_replace..." << std::endl;
    WorldEnd server;
    TEST_ASSERT(server.start(53), "world #1 started with M1 port");
    const auto port = server.world->m1Link().listenPort();
    gateway::M1EgressConfig egress_config;
    egress_config.host = "127.0.0.1";
    egress_config.port = port;
    egress_config.route_json = kRouteJson;
    auto egress = gateway::makeM1Egress(std::move(egress_config));
    TEST_ASSERT(egress->start(), "egress started");
    ClientFace clients;
    egress->setClientDeliver(
        [&clients](std::uint64_t session_id, std::uint32_t,
                   const std::vector<std::uint8_t>& payload) {
            clients.accept(session_id, payload);
        });
    std::map<std::uint64_t, DrainReply> drain_replies;
    std::map<std::uint64_t, RolloutReply> rollout_replies;
    g_drain_replies = &drain_replies;
    g_rollout_replies = &rollout_replies;
    OpsEnd ops1;
    TEST_ASSERT(dial_ops(server, ops1), "ops #1 dialed");

    // C 队（9001/300）开副本挂起（Create 相位悬挂——永不自然结算）
    const auto party_c = spawn_and_launch(*egress, clients, {{9001, 300}});
    pump_until(*egress,
               [&] {
                   return last_squad_result_code(clients, 9001,
                                                 static_cast<std::int32_t>(
                                                     protocol::SquadOp::SQUAD_LAUNCH)) == 0;
               },
               5000, "launch-c-ok");
    auto* squads = server.world->squadInstanceService();

    // 排空（期限 1s）：副本按设计收线——强停不是连续，是显形销毁
    DrainReply r;
    TEST_ASSERT(drain_roundtrip(ops1, 101, 1, 1, r), "begin reply arrived");
    TEST_ASSERT(r.ok && r.state == 1, "draining begun");
    TEST_ASSERT(poll_drained(ops1, 102, r), "drained terminal reached");
    TEST_ASSERT(r.detail.find("force-aborted") != std::string::npos,
                "deadline force-stop detail");

    // 进程替换：WorldApp #1 退场（控制链先拆，意图链留 egress 持续重拨）
    g_drain_replies = nullptr;
    g_rollout_replies = nullptr;
    server.stop();
    std::this_thread::sleep_for(100ms);
    egress->pump(now_ms());
    TEST_ASSERT(!egress->worldConnected(), "M1 link down after replacement gap");

    // 替换窗内未发送意图入箱。此处为注入会话，不证明真实登录恢复。
    TEST_ASSERT(egress->forwardIntent(
                    9101, protocol::MessageCodec::encode(make_player_spawn(200), 9101)),
                "spawn intent queued while world down");
    TEST_ASSERT(egress->forwardIntent(
                    9101,
                    protocol::MessageCodec::encode(protocol::PartyCreateRequest{}, 9101)),
                "party create intent queued while world down");
    TEST_ASSERT(!egress->worldConnected(), "still down at queue time");

    // World #2 同口重听（进程替换语义：全状态随 #1 消亡，档案落盘仍在；
    // 代次显式换代 = 真进程各自自铸的模拟——同进程 e2e 共享自铸值，须注入）
    TEST_ASSERT(server.start(53, port, false, 0xA17E0002),
                "world #2 restarted on same port");
    TEST_ASSERT(server.world->m1Link().listenPort() == port, "same M1 port re-listen");

    // egress 自拨回连；仅尚未发送的排队意图在新代次投递。
    // 已发但结果不明的请求不能自动重放。
    pump_until(*egress, [&] { return egress->worldConnected(); }, 8000,
               "egress-redial");
    protocol::PartyResult created{};
    pump_until(*egress,
               [&] {
                   for (const auto& frame : clients.take(static_cast<std::uint16_t>(
                            protocol::MessageType::PARTY_RESULT))) {
                       if (frame.session_id != 9101) continue;
                       created = protocol::MessageCodec::decodeBody<protocol::PartyResult>(
                           frame.body);
                   }
                   return created.op ==
                              static_cast<std::int32_t>(protocol::SquadOp::PARTY_CREATE) &&
                          created.code == 0;
               },
               8000, "queued-party-create-reply");
    TEST_ASSERT(created.partyId != 0, "queued intent flushed and served");

    // 排空态不跨进程：新 World serving（恢复 = 替换的另一证据）
    g_drain_replies = &drain_replies;
    OpsEnd ops2;
    TEST_ASSERT(dial_ops(server, ops2), "ops #2 dialed");
    DrainReply s;
    TEST_ASSERT(drain_roundtrip(ops2, 201, 2, 0, s), "status reply #2 arrived");
    TEST_ASSERT(s.ok && s.state == 0, "fresh process is serving again");
    TEST_ASSERT(s.detail == "serving", "serving detail on fresh process");

    // 持续接入恢复：新副本分配照常（D 队 launch code 0）
    protocol::SquadLaunch launch_d;
    launch_d.partyId = created.partyId;
    TEST_ASSERT(egress->forwardIntent(
                    9101, protocol::MessageCodec::encode(launch_d, 9101)),
                "launch D intent accepted");
    pump_until(*egress,
               [&] {
                   return last_squad_result_code(clients, 9101,
                                                 static_cast<std::int32_t>(
                                                     protocol::SquadOp::SQUAD_LAUNCH)) == 0;
               },
               5000, "launch-d-ok");

    g_drain_replies = nullptr;
    g_rollout_replies = nullptr;
    egress->stop();
    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

#if !defined(_WIN32)
// 独立客户端 socket；断线直接失败，不执行自动重连或再次认证。
class TcpClient {
public:
    ~TcpClient() { if (fd_ >= 0) ::close(fd_); }

    void connect(std::uint16_t port) {
        fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);
        if (fd_ < 0 || ::connect(fd_, reinterpret_cast<sockaddr*>(&address),
                                  sizeof(address)) != 0) {
            throw std::runtime_error("client connect failed");
        }
    }

    template <typename Message>
    void send(const Message& message, std::uint64_t session = 0) {
        std::vector<std::uint8_t> bytes;
        link::append_frame(bytes, ++sequence_, 0,
                           protocol::MessageCodec::encode(message, session));
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const auto n = ::send(fd_, bytes.data() + offset, bytes.size() - offset,
                                  MSG_NOSIGNAL);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) throw std::runtime_error("existing client socket send failed");
            offset += static_cast<std::size_t>(n);
        }
    }

    template <typename Message, typename Predicate>
    Message wait(protocol::MessageType type, Predicate predicate) {
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        while (std::chrono::steady_clock::now() < deadline) {
            std::uint8_t bytes[8192];
            const auto n = ::recv(fd_, bytes, sizeof(bytes), MSG_DONTWAIT);
            if (n == 0) throw std::runtime_error("existing client TCP connection closed");
            if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                throw std::runtime_error("client receive failed");
            }
            if (n > 0) {
                auto outcome = parser_.feed(bytes, static_cast<std::size_t>(n));
                if (outcome.fatal) throw std::runtime_error("invalid client frame");
                for (auto& frame : outcome.frames) frames_.push_back(std::move(frame));
            }
            for (auto it = frames_.begin(); it != frames_.end();) {
                const auto header = protocol::MessageCodec::parseHeader(it->payload);
                if (header.type != static_cast<std::uint16_t>(type)) { ++it; continue; }
                const std::vector<std::uint8_t> body(
                    it->payload.begin() + sizeof(protocol::MessageHeader), it->payload.end());
                auto message = protocol::MessageCodec::decodeBody<Message>(body);
                it = frames_.erase(it);
                if (predicate(message)) return message;
            }
            std::this_thread::sleep_for(5ms);
        }
        throw std::runtime_error("client response deadline exceeded: " +
                                 std::to_string(static_cast<int>(type)));
    }

private:
    int fd_ = -1;
    std::uint32_t sequence_ = 0;
    link::FrameParser parser_;
    std::vector<link::Frame> frames_;
};

bool test_lua_update_over_existing_client_tcp() {
    std::cout << "Running: test_lua_update_over_existing_client_tcp..." << std::endl;
    WorldEnd server;
    TEST_ASSERT(server.start(54), "world started");
    gateway::GatewayConfig config;
    config.host = "127.0.0.1";
    config.port = 0;
    config.worldM1Port = server.world->m1Link().listenPort();
    config.worldM1RouteJson = kRouteJson;
    config.sessionTimeoutMs = 60000;
    gateway::GatewayServer gateway(config);
    gateway.start();
    TcpClient client;
    client.connect(gateway.listenPort());
    gateway::SessionID session = 0;
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (!session && std::chrono::steady_clock::now() < deadline) {
        for (gateway::SessionID id = 1; id <= 16; ++id) {
            if (gateway.findClientEndpoint(id)) { session = id; break; }
        }
        std::this_thread::sleep_for(5ms);
    }
    TEST_ASSERT(session != 0, "real socket registered");
    // 本用例只初始化一次 Gateway 原型认证；不声称已验证 Login 凭证恢复。
    TEST_ASSERT(gateway.authenticateSession(session, 100, "test-ticket"),
                "initial gateway authentication");
    client.send(make_player_spawn(100), session);
    client.send(protocol::PartyCreateRequest{}, session);
    const auto party = client.wait<protocol::PartyResult>(
        protocol::MessageType::PARTY_RESULT, [](const auto& r) { return r.code == 0; });
    protocol::SquadLaunch launch;
    launch.partyId = party.partyId;
    client.send(launch, session);
    client.wait<protocol::SquadResult>(protocol::MessageType::SQUAD_RESULT,
        [](const auto& r) { return r.op == static_cast<int>(protocol::SquadOp::SQUAD_LAUNCH)
                                   && r.code == 0; });
    protocol::SquadEnter enter;
    enter.partyId = party.partyId;
    client.send(enter, session);
    protocol::SquadStart start;
    start.partyId = party.partyId;
    client.send(start, session);
    const auto running = client.wait<protocol::SquadState>(protocol::MessageType::SQUAD_STATE,
        [](const auto& state) { return state.phase == 3 && !state.monsters.empty(); });

    std::map<std::uint64_t, RolloutReply> replies;
    g_rollout_replies = &replies;
    OpsEnd ops;
    TEST_ASSERT(dial_ops(server, ops), "ops connected");
    TEST_ASSERT(ops.send_rollout(301, 1, "greet", 7, kModuleGood, 2, 0), "prepare sent");
    auto wait_reply = [&](std::uint64_t id) {
        const auto until = std::chrono::steady_clock::now() + 5s;
        while (std::chrono::steady_clock::now() < until) {
            ops.pump();
            const auto it = replies.find(id);
            if (it != replies.end()) return it->second.ok;
            std::this_thread::sleep_for(5ms);
        }
        return false;
    };
    TEST_ASSERT(wait_reply(301), "prepare succeeded");
    TEST_ASSERT(ops.send_rollout(302, 2, "greet", 7, "", 0, 0), "commit sent");
    TEST_ASSERT(wait_reply(302), "commit succeeded");
    RolloutReply baseline;
    TEST_ASSERT(poll_rollout_state(ops, 6, 7, baseline), "baseline v7 promoted");
    const char* updated = "return { revision = 8, onTick = function() end, "
                          "smoke = function() return true end }";
    TEST_ASSERT(ops.send_rollout(303, 1, "greet", 8, updated, 50, 0),
                "replacement prepared over active baseline");
    TEST_ASSERT(wait_reply(303), "replacement preparation succeeded");
    TEST_ASSERT(ops.send_rollout(304, 2, "greet", 8, "", 0, 0), "replacement commit sent");
    TEST_ASSERT(wait_reply(304), "replacement committed");

    // 更新期间新 TCP 连接可接入；旧 socket 不重连、不重新认证继续完成副本。
    TcpClient newcomer;
    newcomer.connect(gateway.listenPort());
    protocol::Ping ping;
    ping.timestamp = 123456;
    newcomer.send(ping);
    newcomer.wait<protocol::Pong>(protocol::MessageType::PONG,
        [](const auto& pong) { return pong.timestamp == 123456; });
    for (const auto monster : running.monsters) {
        protocol::SquadKill kill;
        kill.partyId = party.partyId;
        kill.monsterEntityId = monster;
        client.send(kill, session);
        client.wait<protocol::SquadResult>(protocol::MessageType::SQUAD_RESULT,
            [](const auto& r) { return r.op == static_cast<int>(protocol::SquadOp::SQUAD_KILL)
                                       && r.code == 0; });
    }
    client.wait<protocol::SquadState>(protocol::MessageType::SQUAD_STATE,
        [&](const auto& state) { return state.partyId == party.partyId && state.phase == 7; });
    client.send(ping, session);
    client.wait<protocol::Pong>(protocol::MessageType::PONG,
        [](const auto& pong) { return pong.timestamp == 123456; });
    RolloutReply promoted;
    TEST_ASSERT(poll_rollout_state(ops, 6, 8, promoted), "replacement v8 promoted");
    TEST_ASSERT(gateway.findClientEndpoint(session).has_value(), "original session retained");
    g_rollout_replies = nullptr;
    gateway.stop();
    server.stop();
    return true;
}
#endif

}  // namespace

#if !defined(_WIN32)
int main() {
    int failed = 0;
    if (!test_update_continuity_lua_module()) ++failed;
    if (!test_update_continuity_process_replace()) ++failed;
    if (!test_lua_update_over_existing_client_tcp()) ++failed;
    if (failed == 0) {
        std::cout << "ALL UPDATE CONTINUITY TESTS PASSED" << std::endl;
        return 0;
    }
    std::cerr << failed << " update continuity test(s) failed" << std::endl;
    return 1;
}
#else
int main() {
    std::cout << "update continuity tests: POSIX-only (link TCP base)" << std::endl;
    return 0;
}
#endif
