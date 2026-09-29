/**
 * @file contract_model.cpp
 * @brief 模型助手：类型/方向命名、sync 掩码 ↔ token、相等比较（往返测试用）。
 */

#include "apollo/contract/contract_model.hpp"

namespace apollo::contract {

const char* wireTypeToString(WireType t) {
    switch (t) {
        case WireType::Bool: return "bool";
        case WireType::Int32: return "int32";
        case WireType::Int64: return "int64";
        case WireType::Float: return "float";
        case WireType::Double: return "double";
        case WireType::String: return "string";
        case WireType::Bytes: return "bytes";
        case WireType::StringList: return "string[]";
        case WireType::Int64List: return "int64[]";
    }
    return "?";
}

bool parseWireType(const std::string& name, WireType& out) {
    const std::pair<const char*, WireType> table[] = {
        {"bool", WireType::Bool},     {"int32", WireType::Int32},
        {"int64", WireType::Int64},   {"float", WireType::Float},
        {"double", WireType::Double}, {"string", WireType::String},
        {"bytes", WireType::Bytes},   {"string[]", WireType::StringList},
        {"int64[]", WireType::Int64List},
    };
    for (const auto& [n, t] : table) {
        if (name == n) {
            out = t;
            return true;
        }
    }
    return false;
}

const char* directionToString(Direction d) {
    switch (d) {
        case Direction::C2S: return "C2S";
        case Direction::S2C: return "S2C";
        case Direction::P2P: return "P2P";
    }
    return "?";
}

const char* msgDomainToString(MsgDomain d) {
    switch (d) {
        case MsgDomain::Client: return "client";
        case MsgDomain::Internal: return "internal";
    }
    return "?";
}

const char* msgBindingToString(MsgBinding b) {
    switch (b) {
        case MsgBinding::Native: return "native";
        case MsgBinding::Reflect: return "reflect";
    }
    return "?";
}

MsgBinding defaultBindingForChannel(const std::string& channel) {
    // events 通道缺省 reflect（业务消息 → bin 反射 + Lua handler）；
    // movement/attributes/control 缺省 native（框架固定消息族）。通道非法值
    // 已在解析层报错，此处对未知通道退 native 不掩盖诊断。
    return channel == "events" ? MsgBinding::Reflect : MsgBinding::Native;
}

uint8_t syncTokensToMask(const std::vector<std::string>& tokens) {
    uint8_t mask = kSyncNone;
    for (const auto& tok : tokens) {
        if (tok == "APPR") mask |= kSyncAppr;
        else if (tok == "PROP") mask |= kSyncProp;
        else if (tok == "SELF") mask |= kSyncSelf;
        else if (tok == "TEAM") mask |= kSyncTeam;
        else if (tok == "GUILD") mask |= kSyncGuild;
        else if (tok == "WORLD") mask |= kSyncWorld;
        else if (tok == "IMMEDIATE") mask |= kSyncImmediate;
    }
    return mask;
}

std::vector<std::string> syncMaskToTokens(uint8_t mask) {
    std::vector<std::string> out;
    if (mask & kSyncAppr) out.push_back("APPR");
    if (mask & kSyncProp) out.push_back("PROP");
    if (mask & kSyncSelf) out.push_back("SELF");
    if (mask & kSyncTeam) out.push_back("TEAM");
    if (mask & kSyncGuild) out.push_back("GUILD");
    if (mask & kSyncWorld) out.push_back("WORLD");
    if (mask & kSyncImmediate) out.push_back("IMMEDIATE");
    return out;
}

bool operator==(const TypeAlias& a, const TypeAlias& b) {
    return a.name == b.name && a.underlying == b.underlying && a.desc == b.desc;
}

bool operator==(const AttrDef& a, const AttrDef& b) {
    return a.id == b.id && a.name == b.name && a.typeName == b.typeName &&
           a.type == b.type && a.syncMask == b.syncMask && a.channel == b.channel &&
           a.predict == b.predict && a.defaultValue == b.defaultValue && a.desc == b.desc;
    // sourceFile 不参与语义相等（同一文件名下往返）
}

bool operator==(const FieldDef& a, const FieldDef& b) {
    return a.name == b.name && a.typeName == b.typeName && a.type == b.type && a.desc == b.desc;
}

bool operator==(const MsgDef& a, const MsgDef& b) {
    return a.id == b.id && a.name == b.name && a.dir == b.dir && a.channel == b.channel &&
           a.domain == b.domain && a.binding == b.binding && a.fields == b.fields &&
           a.desc == b.desc;
}

bool operator==(const EntityDef& a, const EntityDef& b) {
    return a.name == b.name && a.parent == b.parent && a.desc == b.desc &&
           a.flattenedAttrs == b.flattenedAttrs;
    // ancestors 由语义层推导，同样不进结构相等
}

bool operator==(const ErrorDef& a, const ErrorDef& b) {
    return a.code == b.code && a.name == b.name && a.desc == b.desc;
}

bool operator==(const Contract& a, const Contract& b) {
    return a.version == b.version && a.aliases == b.aliases && a.attrs == b.attrs &&
           a.msgs == b.msgs && a.entities == b.entities && a.errors == b.errors;
}

}  // namespace apollo::contract
