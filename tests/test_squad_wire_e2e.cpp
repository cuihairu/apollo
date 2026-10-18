// 小队副本真实链路端到端（P2 真实上行/下行）：
//   客户端帧（协议 native）→ 真实 gateway M1Egress（契约翻译 + 932 信封）
//   → 真实 WorldApp M1 接收端（gameLoop 意图消费）→ SquadInstanceService
//   执行面 → 结果/状态帧（协议 native）→ 契约 939 client_deliver
//   → gateway client_deliver 出口 → 客户端会话。
//
// 断言口径：下行完成证据 = egress 客户端投递口收到的协议帧内容
//（broadcastToViewers 日志不算），上行完成证据 = 服务受理产生的结果帧回带。
// 全 TCP：WorldApp 自己的 gameLoop 泵链路；测试线程只泵 egress 并轮询。

#include "world_app/world_app.hpp"
#include "gateway/m1_egress.hpp"
#include "player_store/player_store.hpp"
#include "store_host.hpp"
#include "apollo/protocol/codec.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace std::chrono_literals;
namespace protocol = apollo::protocol;

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

std::uint64_t now_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

struct TemporaryDirectory {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("apollo-squad-e2e-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ~TemporaryDirectory() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

// 收到的客户端帧（会话号 + 协议类型 + 原始 body），下行断言的事实源。
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

    std::size_t total() const { return frames_.size(); }

private:
    std::vector<DeliveredFrame> frames_;
};

// pump_until：泵 egress 一拍 + 轮询谓词（上行受理/下行回带全异步）。
template <typename Pred>
void pump_until(gateway::M1Egress& egress, Pred&& done, int timeout_ms) {
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(timeout_ms);
    while (!done()) {
        if (std::chrono::steady_clock::now() > deadline) {
            throw std::runtime_error("pump_until deadline exceeded");
        }
        egress.pump(now_ms());
        std::this_thread::sleep_for(10ms);
    }
    egress.pump(now_ms());
}

protocol::CellCreateEntity make_player_spawn(std::uint64_t player_id) {
    protocol::CellCreateEntity msg;
    msg.entityId = player_id;  // P0-2：player 身份 = 创建请求实体号（契约缺陷期）
    msg.entityType = protocol::EntityType::PLAYER;
    msg.spaceId = 0;  // 默认 scene
    return msg;
}

void squad_wire_e2e() {
    TemporaryDirectory files;

    // Base 宿主（L19 资料归属切换）：档案锚点在 baseServer；结算落账 =
    // World 业务受理面拨本面（958/959 返回业务请求——P1 基础业务与结算迁入）。
    player_store::PlayerStoreConfig base_config;
    base_config.host = "127.0.0.1";
    base_config.m1Enabled = true;
    base_config.m1Port = 0;  // 内核分配：测试经 m1_listen_port() 读回
    base_config.m1SelfName = "baseServer";
    base_config.m1RouteJson = APOLLO_TEST_ROUTE_JSON;
    base_config.dataDir = (files.path / "base/players").string();
    base_config.journalPath = (files.path / "base/journal/persist.log").string();
    StoreHost base(base_config, 0, 0, 0, 0);  // 纯 store 位（无报告面）
    require(base.store->m1_listen_port() != 0, "base store did not listen");

    world_app::WorldAppConfig world_config;
    world_config.worldId = 44;
    world_config.host = "127.0.0.1";
    world_config.m1Port = 0;  // 内核分配：测试经 listenPort() 读回
    world_config.m1RouteJson = APOLLO_TEST_ROUTE_JSON;
    // 结算落账业务受理面拨 baseServer（0 = 挂账降级态；本 e2e 全链接线）
    world_config.baseBusinessPort = base.store->m1_listen_port();

    world_app::WorldApp world(world_config);
    world.start();
    require(world.m1Link().listenPort() != 0, "World M1 link did not listen");
    auto* squads = world.squadInstanceService();
    require(squads != nullptr, "Squad service not hosted");

    gateway::M1EgressConfig egress_config;
    egress_config.host = "127.0.0.1";
    egress_config.port = world.m1Link().listenPort();
    egress_config.route_json = APOLLO_TEST_ROUTE_JSON;
    auto egress = gateway::makeM1Egress(std::move(egress_config));
    require(egress->start(), "Gateway M1 egress did not start");

    ClientFace clients;
    egress->setClientDeliver(
        [&clients](std::uint64_t session_id, std::uint32_t /*gateway_id*/,
                   const std::vector<std::uint8_t>& payload) {
            clients.accept(session_id, payload);
        });

    // 会话进图（真实上行：CELL_CREATE_ENTITY → WorldApp 会话绑定面）。
    // 会话 9001/9002/9003 ↔ 玩家 100/101/102。
    const std::vector<std::pair<std::uint64_t, std::uint64_t>> roster{
        {9001, 100}, {9002, 101}, {9003, 102}};
    // 名册成员档案接纳（Base 宿主真实接纳路径：会话/网关双锚 + 承载域 44）
    for (const auto [session_id, player_id] : roster) {
        require(base.store->adoptPlayer(player_id, "", session_id, 12, 44),
                "base adopt failed");
    }
    for (const auto [session_id, player_id] : roster) {
        const auto frame = protocol::MessageCodec::encode(make_player_spawn(player_id),
                                                          session_id);
        require(egress->forwardIntent(session_id, frame), "spawn intent refused");
    }

    // ---- 建队（session 9001 / 玩家 100 为队长）----
    pump_until(*egress, [&] { return egress->worldConnected(); }, 5000);
    require(egress->forwardIntent(
                9001, protocol::MessageCodec::encode(protocol::PartyCreateRequest{}, 9001)),
            "party create refused");
    protocol::PartyResult created{};
    pump_until(*egress,
               [&] {
                   for (const auto& frame : clients.take(
                            static_cast<std::uint16_t>(protocol::MessageType::PARTY_RESULT))) {
                       if (frame.session_id != 9001) continue;
                       created = protocol::MessageCodec::decodeBody<protocol::PartyResult>(
                           frame.body);
                   }
                   return created.op ==
                              static_cast<std::int32_t>(protocol::SquadOp::PARTY_CREATE) &&
                          created.code == 0;
               },
               5000)
        ;
    const auto party_id = created.partyId;
    require(party_id > 0, "server did not allocate party id");

    // ---- 两名队员入队 ----
    for (const auto [session_id, player_id] :
         {std::pair{9002u, 101u}, std::pair{9003u, 102u}}) {
        (void)player_id;
        protocol::PartyJoinRequest join;
        join.partyId = party_id;
        require(egress->forwardIntent(session_id, protocol::MessageCodec::encode(join, session_id)),
                "party join refused");
    }
    pump_until(*egress,
               [&] {
                   return clients.take(static_cast<std::uint16_t>(
                                              protocol::MessageType::PARTY_RESULT))
                              .size() >= 3;
               },
               5000);

    // ---- 副本开启（仅队长）----
    protocol::SquadLaunch launch;
    launch.partyId = party_id;
    launch.mapId = 0;  // 服务端默认
    require(egress->forwardIntent(
                9001, protocol::MessageCodec::encode(launch, 9001)),
            "squad launch refused");
    protocol::SquadResult launch_result{};
    pump_until(*egress,
               [&] {
                   for (const auto& frame : clients.take(
                            static_cast<std::uint16_t>(protocol::MessageType::SQUAD_RESULT))) {
                       if (frame.session_id != 9001) continue;
                       const auto decoded =
                           protocol::MessageCodec::decodeBody<protocol::SquadResult>(frame.body);
                       if (decoded.op == static_cast<std::int32_t>(protocol::SquadOp::SQUAD_LAUNCH))
                           launch_result = decoded;
                   }
                   return launch_result.code != 0 ||
                          (launch_result.partyId == party_id &&
                           launch_result.instanceId != 0 && launch_result.worldId == 44 &&
                           launch_result.sceneId != 0);
               },
               5000);
    require(launch_result.code == 0, "squad launch rejected");

    // ---- 全员进入 ----
    for (const auto session_id : {9001u, 9002u, 9003u}) {
        protocol::SquadEnter enter;
        enter.partyId = party_id;
        require(egress->forwardIntent(session_id, protocol::MessageCodec::encode(enter, session_id)),
                "squad enter refused");
    }
    pump_until(*egress,
               [&] {
                   std::size_t ok = 0;
                   for (const auto& frame : clients.take(static_cast<std::uint16_t>(
                            protocol::MessageType::SQUAD_RESULT))) {
                       const auto decoded =
                           protocol::MessageCodec::decodeBody<protocol::SquadResult>(frame.body);
                       if (decoded.op == static_cast<std::int32_t>(protocol::SquadOp::SQUAD_ENTER) &&
                           decoded.code == 0)
                           ++ok;
                   }
                   return ok >= 3;
               },
               5000);

    // ---- 队长启动玩法：宿主设目标 + 刷怪（骨架玩法内容）----
    protocol::SquadStart start;
    start.partyId = party_id;
    require(egress->forwardIntent(9001, protocol::MessageCodec::encode(start, 9001)),
            "squad start refused");
    std::vector<DeliveredFrame> running_states;
    pump_until(*egress,
               [&] {
                   running_states.clear();
                   for (const auto& frame : clients.take(static_cast<std::uint16_t>(
                            protocol::MessageType::SQUAD_STATE))) {
                       const auto decoded =
                           protocol::MessageCodec::decodeBody<protocol::SquadState>(frame.body);
                       if (decoded.partyId == party_id && decoded.phase == 3 &&  // Running
                           !decoded.monsters.empty())
                           running_states.push_back(frame);
                   }
                   return !running_states.empty();
               },
               5000);
    // 状态帧广播到全员会话（9001/9002/9003 都该收到 Running 态）
    {
        std::set<std::uint64_t> reached;
        for (const auto& frame : running_states) reached.insert(frame.session_id);
        require(reached.size() == 3, "Running state did not reach every roster session");
    }

    // ---- 击杀上报（队长逐个上报宿主刷出的怪）----
    const auto monsters = protocol::MessageCodec::decodeBody<protocol::SquadState>(
                              running_states.front().body)
                              .monsters;
    require(monsters.size() == 3, "host goal wave missing");
    for (const auto monster : monsters) {
        protocol::SquadKill kill;
        kill.partyId = party_id;
        kill.monsterEntityId = monster;
        require(egress->forwardIntent(9001, protocol::MessageCodec::encode(kill, 9001)),
                "squad kill refused");
        pump_until(*egress,
                   [&] {
                       for (const auto& frame : clients.take(static_cast<std::uint16_t>(
                                protocol::MessageType::SQUAD_RESULT))) {
                           const auto decoded = protocol::MessageCodec::decodeBody<
                               protocol::SquadResult>(frame.body);
                           if (decoded.op ==
                                   static_cast<std::int32_t>(protocol::SquadOp::SQUAD_KILL) &&
                               decoded.code == 0 && decoded.partyId == party_id)
                               return true;
                       }
                       return false;
                   },
                   5000);
    }

    // ---- 结算 → 收尾：终态回执（Destroyed）广播全员 + 分值经 battle 落锚 ----
    std::set<std::uint64_t> destroyed_reached;
    pump_until(*egress,
               [&] {
                   destroyed_reached.clear();
                   for (const auto& frame : clients.take(static_cast<std::uint16_t>(
                            protocol::MessageType::SQUAD_STATE))) {
                       const auto decoded =
                           protocol::MessageCodec::decodeBody<protocol::SquadState>(frame.body);
                       if (decoded.partyId == party_id && decoded.phase == 7)  // Destroyed
                           destroyed_reached.insert(frame.session_id);
                   }
                   return destroyed_reached.size() == 3;
               },
               5000);
    require(destroyed_reached.size() == 3, "terminal state did not reach every roster session");
    // 结算落账（P1 基础业务与结算迁入）：返回业务请求（958/959）真拨 Base
    // ——锚点 exp 在 Base 侧读数（挂账面已接线，分值不丢）；收尾三件套
    // （终态回收 + 容量归还 + 名册锚点 Online 在 Base）。
    require(squads->active_count() == 0, "instance binding not reclaimed");
    require(squads->used_capacity(44) == 0, "world capacity not returned");
    for (const auto [session_id, player_id] : roster) {
        const auto anchor = base.store->findAnchor(player_id);
        require(anchor != nullptr, "roster anchor missing on base after settlement");
        require(anchor->state() == apollo::game::session::AnchorState::Online,
                "roster anchor not Online after settlement");
    }
    {
        const auto anchor = base.store->findAnchor(100);
        require(anchor != nullptr && anchor->progress("exp") > 0,
                "killer settlement score did not reach base anchor");
    }

    egress->stop();
    world.stop();
}

} // namespace

int main() {
#if defined(_WIN32)
    std::cout << "SKIP: squad wire e2e requires POSIX TCP\n";
    return 0;
#else
    try {
        squad_wire_e2e();
    } catch (const std::exception& ex) {
        std::cerr << "FAIL: " << ex.what() << '\n';
        return 1;
    }
    std::cout << "Squad wire e2e passed\n";
    return 0;
#endif
}
