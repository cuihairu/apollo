// m1_link_server 拆分单元（LAY-008）：932 意图翻译面——契约 msg id → legacy
// 帧镜像（build_legacy_intent，m1_link_detail 共享面）+ decodeEnvelopeIntent
// 对外转发（cell 执行端意图监听复用）。
#include "m1_link_server_impl.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"

#if !defined(_WIN32)

namespace world_app {

namespace m1_link_detail {


// 契约 msg id → legacy 类型 + 帧字节（字段序 = 契约表序，M1 契约批镜像定案）。
// false = id 不在本进程装配面（dispatcher 白名单已挡，此处兜底计数）。
bool build_legacy_intent(std::uint16_t msg_id, const std::vector<wire::FieldValue>& f,
                         IntentEnvelope& out) {
    out = IntentEnvelope{};
    // 五族意图共用头部两字段：request_id（应答关联键）+ session_id（源会话）。
    if (f.size() < 2) {
        return false;
    }
    out.request_id = static_cast<std::uint64_t>(f[0].integer);
    out.session_id = static_cast<protocol::SessionID>(f[1].integer);

    switch (msg_id) {
        case id_of(MsgId::cell_create_entity): {
            if (f.size() != 8) {
                return false;
            }
            protocol::CellCreateEntity msg;
            msg.entityId = static_cast<protocol::EntityID>(f[2].integer);
            msg.entityType = static_cast<protocol::EntityType>(
                static_cast<std::uint8_t>(f[3].integer));
            msg.spaceId = static_cast<protocol::SpaceID>(f[4].integer);
            msg.position = protocol::Position(static_cast<float>(f[5].real),
                                              static_cast<float>(f[6].real),
                                              static_cast<float>(f[7].real));
            out.type = protocol::MessageType::CELL_CREATE_ENTITY;
            out.frame = protocol::MessageCodec::encode(msg, out.session_id);
            return true;
        }
        case id_of(MsgId::cell_destroy_entity): {
            if (f.size() != 3) {
                return false;
            }
            protocol::CellDestroyEntity msg;
            msg.entityId = static_cast<protocol::EntityID>(f[2].integer);
            out.type = protocol::MessageType::CELL_DESTROY_ENTITY;
            out.frame = protocol::MessageCodec::encode(msg, out.session_id);
            return true;
        }
        case id_of(MsgId::cell_entity_move): {
            if (f.size() != 9) {
                return false;
            }
            protocol::CellEntityMove msg;
            msg.entityId = static_cast<protocol::EntityID>(f[2].integer);
            msg.oldPos = protocol::Position(static_cast<float>(f[3].real),
                                            static_cast<float>(f[4].real),
                                            static_cast<float>(f[5].real));
            msg.newPos = protocol::Position(static_cast<float>(f[6].real),
                                            static_cast<float>(f[7].real),
                                            static_cast<float>(f[8].real));
            out.type = protocol::MessageType::CELL_ENTITY_MOVE;
            out.frame = protocol::MessageCodec::encode(msg, out.session_id);
            return true;
        }
        case id_of(MsgId::cell_cross_border): {
            if (f.size() != 8) {
                return false;
            }
            protocol::CellCrossBorder msg;
            msg.entityId = static_cast<protocol::EntityID>(f[2].integer);
            msg.fromSpace = static_cast<protocol::SpaceID>(f[3].integer);
            msg.toSpace = static_cast<protocol::SpaceID>(f[4].integer);
            msg.position = protocol::Position(static_cast<float>(f[5].real),
                                              static_cast<float>(f[6].real),
                                              static_cast<float>(f[7].real));
            out.type = protocol::MessageType::CELL_CROSS_BORDER;
            out.frame = protocol::MessageCodec::encode(msg, out.session_id);
            return true;
        }
        case id_of(MsgId::cell_skill_cast): {
            if (f.size() != 11) {
                return false;
            }
            protocol::CombatSkillCast msg;
            msg.casterId = static_cast<protocol::EntityID>(f[2].integer);
            msg.targetId = static_cast<protocol::EntityID>(f[3].integer);
            msg.skillId = static_cast<std::uint32_t>(f[4].integer);
            msg.casterPos = protocol::Position(static_cast<float>(f[5].real),
                                               static_cast<float>(f[6].real),
                                               static_cast<float>(f[7].real));
            msg.targetPos = protocol::Position(static_cast<float>(f[8].real),
                                               static_cast<float>(f[9].real),
                                               static_cast<float>(f[10].real));
            out.type = protocol::MessageType::COMBAT_SKILL_CAST;
            out.frame = protocol::MessageCodec::encode(msg, out.session_id);
            return true;
        }
        // 队伍与小队副本意图族（P2 真实链路；契约 946-951）：头部两字段同
        // cell 族（request_id + session_id），业务字段 → 协议 native 帧。
        case id_of(MsgId::party_create_intent): {
            if (f.size() != 2) {
                return false;
            }
            protocol::PartyCreateRequest msg;
            out.type = protocol::MessageType::PARTY_CREATE_REQUEST;
            out.frame = protocol::MessageCodec::encode(msg, out.session_id);
            return true;
        }
        case id_of(MsgId::party_join_intent): {
            if (f.size() != 3) {
                return false;
            }
            protocol::PartyJoinRequest msg;
            msg.partyId = static_cast<protocol::EntityID>(f[2].integer);
            out.type = protocol::MessageType::PARTY_JOIN_REQUEST;
            out.frame = protocol::MessageCodec::encode(msg, out.session_id);
            return true;
        }
        case id_of(MsgId::squad_launch_intent): {
            if (f.size() != 4) {
                return false;
            }
            protocol::SquadLaunch msg;
            msg.partyId = static_cast<protocol::EntityID>(f[2].integer);
            msg.mapId = static_cast<protocol::EntityID>(f[3].integer);
            out.type = protocol::MessageType::SQUAD_LAUNCH;
            out.frame = protocol::MessageCodec::encode(msg, out.session_id);
            return true;
        }
        case id_of(MsgId::squad_enter_intent): {
            if (f.size() != 3) {
                return false;
            }
            protocol::SquadEnter msg;
            msg.partyId = static_cast<protocol::EntityID>(f[2].integer);
            out.type = protocol::MessageType::SQUAD_ENTER;
            out.frame = protocol::MessageCodec::encode(msg, out.session_id);
            return true;
        }
        case id_of(MsgId::squad_start_intent): {
            if (f.size() != 3) {
                return false;
            }
            protocol::SquadStart msg;
            msg.partyId = static_cast<protocol::EntityID>(f[2].integer);
            out.type = protocol::MessageType::SQUAD_START;
            out.frame = protocol::MessageCodec::encode(msg, out.session_id);
            return true;
        }
        case id_of(MsgId::squad_kill_intent): {
            if (f.size() != 4) {
                return false;
            }
            protocol::SquadKill msg;
            msg.partyId = static_cast<protocol::EntityID>(f[2].integer);
            msg.monsterEntityId = static_cast<protocol::EntityID>(f[3].integer);
            out.type = protocol::MessageType::SQUAD_KILL;
            out.frame = protocol::MessageCodec::encode(msg, out.session_id);
            return true;
        }
        default:
            return false;
    }
}


} // namespace m1_link_detail


bool decodeEnvelopeIntent(const std::vector<std::uint8_t>& payload,
                          IntentEnvelope& out) {
    // 932 internal_envelope 原始 payload 解包（cell 执行端意图监听复用）。
    // build_legacy_intent 与匿名命名空间符号在同一 TU 内可见（匿名命名空间
    // 本 TU 链接），此处仅做对外转发——cellServer 不编 WorldApp 管理壳，
    // 但 m1_link_server.cpp 同时编入两个目标。
    trace::EnvelopeFields env;
    if (!trace::unpack_envelope(payload.data(), payload.size(), env)) {
        return false;
    }
    std::uint16_t inner_id = 0;
    const std::uint8_t* body = nullptr;
    std::size_t body_size = 0;
    if (!trace::split_payload(env.payload, inner_id, body, body_size)) {
        return false;
    }
    std::vector<wire::FieldValue> fields;
    if (!wire::decode_payload(inner_id, body, body_size, fields)) {
        return false;
    }
    if (!build_legacy_intent(inner_id, fields, out)) {
        return false;
    }
    if (env.trace.has_trace()) {
        out.trace = env.trace;
    }
    out.received_ms = trace::now_ms();
    return true;
}


} // namespace world_app

#endif // !_WIN32

