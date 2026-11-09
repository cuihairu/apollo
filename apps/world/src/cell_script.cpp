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

// 消息型 → 契约意图 id（gateway m1_egress contract_id_for 同表镜像；
// 0 = 不在 Lua 分发面）。
std::uint16_t contract_id_for(apollo::protocol::MessageType type) {
    using MT = apollo::protocol::MessageType;
    using apollo::contract::gen::MsgId;
    switch (type) {
        case MT::CELL_CREATE_ENTITY: return static_cast<std::uint16_t>(MsgId::cell_create_entity);
        case MT::CELL_DESTROY_ENTITY: return static_cast<std::uint16_t>(MsgId::cell_destroy_entity);
        case MT::CELL_ENTITY_MOVE: return static_cast<std::uint16_t>(MsgId::cell_entity_move);
        case MT::CELL_CROSS_BORDER: return static_cast<std::uint16_t>(MsgId::cell_cross_border);
        case MT::COMBAT_SKILL_CAST: return static_cast<std::uint16_t>(MsgId::cell_skill_cast);
        case MT::PARTY_CREATE_REQUEST: return static_cast<std::uint16_t>(MsgId::party_create_intent);
        case MT::PARTY_JOIN_REQUEST: return static_cast<std::uint16_t>(MsgId::party_join_intent);
        case MT::SQUAD_LAUNCH: return static_cast<std::uint16_t>(MsgId::squad_launch_intent);
        case MT::SQUAD_ENTER: return static_cast<std::uint16_t>(MsgId::squad_enter_intent);
        case MT::SQUAD_START: return static_cast<std::uint16_t>(MsgId::squad_start_intent);
        case MT::SQUAD_KILL: return static_cast<std::uint16_t>(MsgId::squad_kill_intent);
        default: return 0;
    }
}

} // namespace

void CellApp::attach_script_to_scenes() {
    if (!script_) {
        return;
    }
    for (auto* scene : world_->scenes()) {
        if (!scene->script_attached()) {
            scene->attach_script(*script_);
        }
    }
}

void CellApp::register_world_module() {
    if (!script_) {
        return;
    }
    static const luaL_Reg kWorldFuncs[] = {
        {"broadcast_viewers", &CellApp::lua_world_broadcast_viewers},
        {"push_session", &CellApp::lua_world_push_session},
        {nullptr, nullptr},
    };
    script_->register_module_ctx("world", kWorldFuncs, this);
}

int CellApp::lua_world_broadcast_viewers(lua_State* L) {
    auto* self = static_cast<CellApp*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (self == nullptr) {
        return luaL_error(L, "world module: host missing");
    }
    const auto entity_id = static_cast<EntityID>(luaL_checkinteger(L, 1));
    const auto msg_type = static_cast<std::uint16_t>(luaL_checkinteger(L, 2));
    std::size_t body_size = 0;
    const char* body = luaL_checklstring(L, 3, &body_size);

    // 脚本源帧：头按编码器默认面组装，sessionId 置 0——真实寻址随
    // broadcastToViewers 逐 viewer 会话号重编（批⑩ 下行管线复用）。
    protocol::MessageHeader header;
    header.magic = protocol::MessageHeader::MAGIC;
    header.version = protocol::MessageHeader::CURRENT_VERSION;
    header.type = msg_type;
    header.length = static_cast<std::uint32_t>(body_size);
    header.sequence = 0;
    header.sessionId = 0;
    auto frame = protocol::MessageCodec::encodeHeader(header);
    frame.insert(frame.end(), body, body + body_size);
    self->broadcastToViewers(entity_id, frame);
    return 0;
}

int CellApp::lua_world_push_session(lua_State* L) {
    auto* self = static_cast<CellApp*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (self == nullptr) {
        return luaL_error(L, "world module: host missing");
    }
    const auto session_id = static_cast<std::uint64_t>(luaL_checkinteger(L, 1));
    const auto msg_type = static_cast<std::uint16_t>(luaL_checkinteger(L, 2));
    std::size_t body_size = 0;
    const char* body = luaL_checklstring(L, 3, &body_size);

    protocol::MessageHeader header;
    header.magic = protocol::MessageHeader::MAGIC;
    header.version = protocol::MessageHeader::CURRENT_VERSION;
    header.type = msg_type;
    header.length = static_cast<std::uint32_t>(body_size);
    header.sequence = 0;
    header.sessionId = session_id;
    auto frame = protocol::MessageCodec::encodeHeader(header);
    frame.insert(frame.end(), body, body + body_size);
    self->deliverToClient(session_id, frame);
    return 0;
}

void CellApp::dispatch_script_intent(const IntentEnvelope& intent) {
    // 客户端消息 → Lua（L51 消费端面）：C++ handler 已先行（权威状态变更），
    // 这里按契约路由带参分发；脚本未挂 / 契约未覆盖 = 静默跳过。
    if (!script_) {
        return;
    }
    const auto route_id = contract_id_for(intent.type);
    if (route_id == 0) {
        return;
    }
    if (script_->contract_routes().find(route_id) == script_->contract_routes().end()) {
        return;
    }
    namespace ls = apollo::script;
    std::vector<ls::LuaValue> args;
    const auto body = std::vector<std::uint8_t>(
        intent.frame.begin() + sizeof(protocol::MessageHeader), intent.frame.end());
    // 字段序 = gateway m1_egress translate_and_send 同序镜像（real = double 面
    // 经 LuaValue 数字通道；Lua 侧按位取参）。
    switch (intent.type) {
        case protocol::MessageType::CELL_CREATE_ENTITY: {
            const auto m = protocol::MessageCodec::decodeBody<protocol::CellCreateEntity>(body);
            args = {ls::LuaValue::of_int(static_cast<std::int64_t>(m.entityId)),
                    ls::LuaValue::of_int(static_cast<std::uint8_t>(m.entityType)),
                    ls::LuaValue::of_int(static_cast<std::int64_t>(m.spaceId)),
                    ls::LuaValue::of_double(m.position.x),
                    ls::LuaValue::of_double(m.position.y),
                    ls::LuaValue::of_double(m.position.z)};
            break;
        }
        case protocol::MessageType::CELL_DESTROY_ENTITY: {
            const auto m = protocol::MessageCodec::decodeBody<protocol::CellDestroyEntity>(body);
            args = {ls::LuaValue::of_int(static_cast<std::int64_t>(m.entityId))};
            break;
        }
        case protocol::MessageType::CELL_ENTITY_MOVE: {
            const auto m = protocol::MessageCodec::decodeBody<protocol::CellEntityMove>(body);
            args = {ls::LuaValue::of_int(static_cast<std::int64_t>(m.entityId)),
                    ls::LuaValue::of_double(m.oldPos.x), ls::LuaValue::of_double(m.oldPos.y),
                    ls::LuaValue::of_double(m.oldPos.z), ls::LuaValue::of_double(m.newPos.x),
                    ls::LuaValue::of_double(m.newPos.y), ls::LuaValue::of_double(m.newPos.z)};
            break;
        }
        case protocol::MessageType::CELL_CROSS_BORDER: {
            const auto m = protocol::MessageCodec::decodeBody<protocol::CellCrossBorder>(body);
            args = {ls::LuaValue::of_int(static_cast<std::int64_t>(m.entityId)),
                    ls::LuaValue::of_int(static_cast<std::int64_t>(m.fromSpace)),
                    ls::LuaValue::of_int(static_cast<std::int64_t>(m.toSpace)),
                    ls::LuaValue::of_double(m.position.x),
                    ls::LuaValue::of_double(m.position.y),
                    ls::LuaValue::of_double(m.position.z)};
            break;
        }
        case protocol::MessageType::COMBAT_SKILL_CAST: {
            const auto m = protocol::MessageCodec::decodeBody<protocol::CombatSkillCast>(body);
            args = {ls::LuaValue::of_int(static_cast<std::int64_t>(m.casterId)),
                    ls::LuaValue::of_int(static_cast<std::int64_t>(m.targetId)),
                    ls::LuaValue::of_int(static_cast<std::int64_t>(m.skillId)),
                    ls::LuaValue::of_double(m.casterPos.x), ls::LuaValue::of_double(m.casterPos.y),
                    ls::LuaValue::of_double(m.casterPos.z), ls::LuaValue::of_double(m.targetPos.x),
                    ls::LuaValue::of_double(m.targetPos.y), ls::LuaValue::of_double(m.targetPos.z)};
            break;
        }
        // 队伍/小队族：带源会话号单参（帧体族语义在 C++ 受理面，Lua 只观测）。
        case protocol::MessageType::PARTY_CREATE_REQUEST:
        case protocol::MessageType::PARTY_JOIN_REQUEST:
        case protocol::MessageType::SQUAD_LAUNCH:
        case protocol::MessageType::SQUAD_ENTER:
        case protocol::MessageType::SQUAD_START:
        case protocol::MessageType::SQUAD_KILL:
            args = {ls::LuaValue::of_int(static_cast<std::int64_t>(intent.session_id))};
            break;
        default:
            return;
    }
    if (!script_->call_route_args(route_id, args)) {
        // call_route_args 已发 runtime fault 事件面（handler 缺失/执行错）；
        // 这里补运维对账行，拍不停（同 boot 源纪律）。
        worldLog().warning("script route dispatch failed: route=" + std::to_string(route_id));
    }
}

} // namespace world_app
