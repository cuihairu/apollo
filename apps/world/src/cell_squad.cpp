#include "world_app/cell_app.hpp"
#include "player_store/business_wire.hpp"
#include "player_store/store_dial.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/net/linktrace/span_emitter.hpp"
#include "apollo/protocol/messages.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/game/world/instance.hpp"
#include "apollo/game/world/scene.hpp"
#include "apollo/game/world/scene_transfer.hpp"
#include "apollo/script/script_runtime.hpp"
#include "apollo_contract.h"
#include <algorithm>
#include <fstream>
#include <optional>
#include <stdexcept>

#include "cell_app_detail.hpp"

namespace world_app {

namespace protocol = apollo::protocol;

namespace {

// 本 TU 沿用的日志出口别名（LAY-001 拆分面；单一定义见 cell_app.cpp）
apollo::core::log::Logger& worldLog() {
    return cell_detail::world_log();
}

} // namespace

std::uint64_t CellApp::resolveIntentPlayer(std::uint64_t session_id) const {
    if (!worldSessionManager_) return 0;
    const auto session = worldSessionManager_->find_session(session_id);
    if (!session) return 0;
    return session->player_id().value();
}

void CellApp::deliverPartyResult(std::uint64_t session_id, std::int32_t op,
                                 std::int32_t code, std::uint64_t party_id) {
    protocol::PartyResult msg;
    msg.op = op;
    msg.code = code;
    msg.partyId = party_id;
    deliverToClient(session_id, protocol::MessageCodec::encode(msg, session_id));
}

void CellApp::deliverSquadResult(std::uint64_t session_id, std::int32_t op,
                                 std::int32_t code,
                                 const squad_instance::ActiveSquadInstance* binding) {
    protocol::SquadResult msg;
    msg.op = op;
    msg.code = code;
    msg.partyId = binding ? binding->party_id : 0;
    msg.instanceId = binding ? binding->instance_id : 0;
    msg.worldId = binding ? binding->world_id : 0;
    msg.sceneId = binding ? binding->scene_id : 0;
    deliverToClient(session_id, protocol::MessageCodec::encode(msg, session_id));
}

void CellApp::deliverSquadStateToParty(std::uint64_t party_id) {
    const auto* binding = squadInstanceService_ ? squadInstanceService_->find_by_party(party_id)
                                                : nullptr;
    if (!binding || !world_ || !worldSessionManager_) return;
    const auto* instance = world_->find_instance(binding->instance_id);
    if (!instance) return;
    const auto progress = squadInstanceService_->gameplay(party_id);

    protocol::SquadState msg;
    msg.partyId = binding->party_id;
    msg.instanceId = binding->instance_id;
    msg.phase = static_cast<std::int32_t>(instance->state());
    msg.monstersDefeated = progress ? progress->monsters_defeated : 0;
    msg.ticks = static_cast<protocol::EntityID>(instance->tick_count());
    msg.goalSet = progress && progress->goal_set ? 1 : 0;
    if (progress) {
        msg.monsters.assign(progress->monsters_alive.begin(),
                            progress->monsters_alive.end());
    }

    // 名册全员状态广播：会话解析得到的成员才收（离线成员随恢复面补推）
    for (const auto member : binding->members) {
        const auto session = worldSessionManager_->find_by_player(
            apollo::game::core::PlayerId{member});
        if (!session) continue;
        deliverToClient(session->session_id(),
                        protocol::MessageCodec::encode(msg, session->session_id()));
    }
}

std::vector<uint8_t> CellApp::handlePartyCreate(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    const auto player_id = resolveIntentPlayer(header.sessionId);
    if (player_id == 0) {
        worldLog().warning("party create from unknown session "
                           + std::to_string(header.sessionId));
        return {};
    }
    // 服务端分配 party_id（契约 23 语义）；首位成员即队长
    const auto party_id = nextPartyId_++;
    const auto* party = squadParties_.create(party_id, player_id);
    deliverPartyResult(header.sessionId,
                       static_cast<std::int32_t>(protocol::SquadOp::PARTY_CREATE),
                       party != nullptr ? 0 : 2001, party_id);
    return {};
}

std::vector<uint8_t> CellApp::handlePartyJoin(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());
    auto msg = protocol::MessageCodec::decodeBody<protocol::PartyJoinRequest>(bodyData);

    const auto player_id = resolveIntentPlayer(header.sessionId);
    if (player_id == 0) {
        worldLog().warning("party join from unknown session "
                           + std::to_string(header.sessionId));
        return {};
    }
    auto* party = squadParties_.find(msg.partyId);
    std::int32_t code = 0;
    if (party == nullptr) {
        code = 1101;  // squad_party_not_found
    } else if (party->has_member(player_id)) {
        code = 1111;  // squad_duplicate_enter
    } else if (!party->add_member(player_id)) {
        code = 1103;  // squad_party_too_large
    }
    deliverPartyResult(header.sessionId,
                       static_cast<std::int32_t>(protocol::SquadOp::PARTY_JOIN), code,
                       msg.partyId);
    return {};
}

std::vector<uint8_t> CellApp::handleSquadLaunch(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());
    auto msg = protocol::MessageCodec::decodeBody<protocol::SquadLaunch>(bodyData);

    const auto player_id = resolveIntentPlayer(header.sessionId);
    if (player_id == 0) {
        worldLog().warning("squad launch from unknown session "
                           + std::to_string(header.sessionId));
        return {};
    }
    squad_instance::LaunchRequest launch;
    launch.party_id = msg.partyId;
    launch.requester_id = player_id;
    launch.map_id = msg.mapId != 0 ? msg.mapId : 1;

    const auto result = squadInstanceService_->launch(launch);
    std::int32_t code = 0;
    switch (result.error) {
        case squad_instance::LaunchError::Ok: break;
        case squad_instance::LaunchError::InvalidRequest: code = 1100; break;
        case squad_instance::LaunchError::PartyNotFound: code = 1101; break;
        case squad_instance::LaunchError::NotLeader: code = 1102; break;
        case squad_instance::LaunchError::PartyTooLarge: code = 1103; break;
        case squad_instance::LaunchError::MemberOffline: code = 1104; break;
        case squad_instance::LaunchError::MemberBusy: code = 1105; break;
        case squad_instance::LaunchError::NoWorldCapacity: code = 1106; break;
        case squad_instance::LaunchError::SpawnRejected: code = 1107; break;
        case squad_instance::LaunchError::Draining: code = 1118; break;
    }
    const auto* binding = squadInstanceService_->find_by_party(msg.partyId);
    deliverSquadResult(header.sessionId,
                       static_cast<std::int32_t>(protocol::SquadOp::SQUAD_LAUNCH), code, binding);
    if (code == 0) deliverSquadStateToParty(msg.partyId);
    return {};
}

std::vector<uint8_t> CellApp::handleSquadEnter(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());
    auto msg = protocol::MessageCodec::decodeBody<protocol::SquadEnter>(bodyData);

    const auto player_id = resolveIntentPlayer(header.sessionId);
    if (player_id == 0) {
        worldLog().warning("squad enter from unknown session "
                           + std::to_string(header.sessionId));
        return {};
    }
    std::int32_t code = 0;
    switch (squadInstanceService_->enter(msg.partyId, player_id)) {
        case squad_instance::SquadInstanceService::EnterError::Ok: break;
        case squad_instance::SquadInstanceService::EnterError::NoBinding: code = 1108; break;
        case squad_instance::SquadInstanceService::EnterError::NotLocal: code = 2001; break;
        case squad_instance::SquadInstanceService::EnterError::NotRoster: code = 1109; break;
        case squad_instance::SquadInstanceService::EnterError::StateRejected: code = 1110; break;
        case squad_instance::SquadInstanceService::EnterError::Duplicate: code = 1111; break;
        case squad_instance::SquadInstanceService::EnterError::NotLeader: code = 1102; break;
        case squad_instance::SquadInstanceService::EnterError::NotAllEntered: code = 1112; break;
    }
    const auto* binding = squadInstanceService_->find_by_party(msg.partyId);
    deliverSquadResult(header.sessionId,
                       static_cast<std::int32_t>(protocol::SquadOp::SQUAD_ENTER), code, binding);
    if (code == 0) deliverSquadStateToParty(msg.partyId);
    return {};
}

std::vector<uint8_t> CellApp::handleSquadStart(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());
    auto msg = protocol::MessageCodec::decodeBody<protocol::SquadStart>(bodyData);

    const auto player_id = resolveIntentPlayer(header.sessionId);
    if (player_id == 0) {
        worldLog().warning("squad start from unknown session "
                           + std::to_string(header.sessionId));
        return {};
    }
    std::int32_t code = 0;
    switch (squadInstanceService_->start_gameplay(msg.partyId, player_id)) {
        case squad_instance::SquadInstanceService::EnterError::Ok: break;
        case squad_instance::SquadInstanceService::EnterError::NoBinding: code = 1108; break;
        case squad_instance::SquadInstanceService::EnterError::NotLocal: code = 2001; break;
        case squad_instance::SquadInstanceService::EnterError::NotRoster: code = 1109; break;
        case squad_instance::SquadInstanceService::EnterError::StateRejected: code = 1110; break;
        case squad_instance::SquadInstanceService::EnterError::Duplicate: code = 1111; break;
        case squad_instance::SquadInstanceService::EnterError::NotLeader: code = 1102; break;
        case squad_instance::SquadInstanceService::EnterError::NotAllEntered: code = 1112; break;
    }
    if (code == 0) {
        // 宿主玩法内容（骨架）：启动即设默认目标 + 首波怪（击杀配额/波次
        // 数值随玩法批；本批打通击杀上报→battle 分值→结算全链）
        squad_instance::GameplayGoal goal;
        goal.monsters_to_defeat = kHostGoalMonsters;
        goal.timeout_ticks = kHostGoalTimeoutTicks;
        (void)squadInstanceService_->set_goal(msg.partyId, goal);
        (void)squadInstanceService_->spawn_wave(msg.partyId, kHostGoalMonsters);
    }
    const auto* binding = squadInstanceService_->find_by_party(msg.partyId);
    deliverSquadResult(header.sessionId,
                       static_cast<std::int32_t>(protocol::SquadOp::SQUAD_START), code, binding);
    if (code == 0) deliverSquadStateToParty(msg.partyId);
    return {};
}

std::vector<uint8_t> CellApp::handleSquadKill(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());
    auto msg = protocol::MessageCodec::decodeBody<protocol::SquadKill>(bodyData);

    const auto player_id = resolveIntentPlayer(header.sessionId);
    if (player_id == 0) {
        worldLog().warning("squad kill from unknown session "
                           + std::to_string(header.sessionId));
        return {};
    }
    std::int32_t code = 0;
    switch (squadInstanceService_->report_monster_defeated(
        msg.partyId, msg.monsterEntityId, player_id)) {
        case squad_instance::SquadInstanceService::GameplayError::Ok: break;
        case squad_instance::SquadInstanceService::GameplayError::NoBinding: code = 1108; break;
        case squad_instance::SquadInstanceService::GameplayError::NotLocal: code = 2001; break;
        case squad_instance::SquadInstanceService::GameplayError::NotRunning: code = 1114; break;
        case squad_instance::SquadInstanceService::GameplayError::InvalidRequest: code = 1100; break;
        case squad_instance::SquadInstanceService::GameplayError::InvalidGoal: code = 1113; break;
        case squad_instance::SquadInstanceService::GameplayError::AlreadySet: code = 1113; break;
        case squad_instance::SquadInstanceService::GameplayError::NoGoal: code = 1114; break;
        case squad_instance::SquadInstanceService::GameplayError::WaveTooLarge: code = 1115; break;
        case squad_instance::SquadInstanceService::GameplayError::UnknownMonster: code = 1116; break;
    }
    const auto* binding = squadInstanceService_->find_by_party(msg.partyId);
    deliverSquadResult(header.sessionId,
                       static_cast<std::int32_t>(protocol::SquadOp::SQUAD_KILL), code, binding);
    if (code == 0) deliverSquadStateToParty(msg.partyId);
    return {};
}

void CellApp::driveSquadGameplay(double delta_seconds) {
    if (!squadInstanceService_ || !world_) return;
    // 逐拍推进全部本地 Running 副本（远端绑定随跨进程条目）；结算期副本
    // 收尾（dismiss：移出玩家 → 终态链 → 双回收）。快照遍历——收尾会改
    // 绑定表。
    for (const auto& binding : squadInstanceService_->activeBindings()) {
        if (binding.world_id != worldId_) continue;
        auto* instance = world_->find_instance(binding.instance_id);
        if (instance == nullptr) continue;
        if (instance->state() == apollo::game::world::Instance::State::Running) {
            (void)squadInstanceService_->advance_tick(binding.party_id, 1, delta_seconds);
            instance = world_->find_instance(binding.instance_id);
            if (instance == nullptr) continue;  // 失败线收尾可能已回收
        }
        const auto state = instance->state();
        if (state >= apollo::game::world::Instance::State::Finishing &&
            state != apollo::game::world::Instance::State::Destroyed) {
            // 终态回执取数在 dismiss 前（binding/instance 随收尾释放）
            const auto party_id = binding.party_id;
            const auto instance_id = binding.instance_id;
            const auto members = binding.members;
            if (!squadInstanceService_->dismiss(party_id)) continue;
            protocol::SquadState final;
            final.partyId = party_id;
            final.instanceId = instance_id;
            final.phase = static_cast<std::int32_t>(
                apollo::game::world::Instance::State::Destroyed);
            final.monstersDefeated = 0;
            final.ticks = 0;
            final.goalSet = 0;
            for (const auto member : members) {
                const auto session = worldSessionManager_->find_by_player(
                    apollo::game::core::PlayerId{member});
                if (!session) continue;
                deliverToClient(session->session_id(),
                                protocol::MessageCodec::encode(
                                    final, session->session_id()));
            }
        }
    }
}

} // namespace world_app
