/**
 * @file contract_model.hpp
 * @brief Entity 契约的数据模型（docs/36 决策 #3/#5；sdk-contract §2；xml-generation §4）。
 *
 * 模型刻意不含任何持久化字段——契约只答「线上怎么传」；落库语义（SYNC_DB、列提升、
 * journal）全部在服务端私有 storage.xml（docs/18 §5）。AttrDef 没有 persist/column
 * 成员，存储词汇在解析层即被拒绝（tests 覆盖）。
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace apollo::contract {

/// 同步可见域 8 位掩码（docs/05 §6.3:698-706 的线上子集）。
/// SYNC_DB(0x04) 是存储语义，不进契约——解析器显式拒绝该 token（决策 #5）。
inline constexpr uint8_t kSyncAppr      = 0x01;  ///< 外观变化 → AOI 广播
inline constexpr uint8_t kSyncProp      = 0x02;  ///< 属性变更 → AOI 广播
inline constexpr uint8_t kSyncDbBanned  = 0x04;  ///< 占位：契约中禁用（属 storage.xml）
inline constexpr uint8_t kSyncSelf      = 0x08;  ///< 只同步给自己
inline constexpr uint8_t kSyncTeam      = 0x10;  ///< 同步给队伍
inline constexpr uint8_t kSyncGuild     = 0x20;  ///< 同步给公会
inline constexpr uint8_t kSyncWorld     = 0x40;  ///< 全服广播
inline constexpr uint8_t kSyncImmediate = 0x80;  ///< 立即同步（跳过批量）

inline constexpr uint8_t kSyncNone = 0x00;

/// 线上类型集（apollo.xsd PrimitiveType 枚举的一一对应）
enum class WireType : uint8_t {
    Bool = 0,
    Int32,
    Int64,
    Float,
    Double,
    String,
    Bytes,
    StringList,
    Int64List,
};

/// 类型别名（attrs.xml <alias>）：仅是基元的命名，不产生新线上类型。
struct TypeAlias {
    std::string name;
    std::string underlying;
    std::string desc;
};

/// 属性定义（attrs.xml <attr>）
struct AttrDef {
    uint16_t id = 0;
    std::string name;
    std::string typeName;   ///< 原文（基元名或别名）
    WireType type = WireType::Int64;
    uint8_t syncMask = kSyncNone;
    std::string channel = "attributes";
    std::vector<std::string> predict;  ///< 预测权限位 token 集
    std::string defaultValue;
    std::string desc;
    std::string sourceFile;  ///< 诊断用
};

/// 消息方向
enum class Direction : uint8_t { C2S = 0, S2C, P2P };

/// 消息域（sdk-contract §11.3 内外分域：域 = 消息上的逻辑属性，非物理文件）。
/// client 域对客户端可见（进客户端 bin/semantic 投影）；internal 域服务端私有。
enum class MsgDomain : uint8_t { Client = 0, Internal };

/// 消息绑定路线（sdk-contract §10.6 v3 消息两分法）：
/// native = 框架固定消息族（schema 不随契约变，内建强类型代码守热路径）；
/// reflect = 业务消息（bin 反射 + contract.lua 路由 → Lua handler）。
/// 「反射在哪解」是装配选择，故意不进契约（§10.6 附节：两解码案零契约分叉）。
enum class MsgBinding : uint8_t { Native = 0, Reflect };

/// 消息字段（messages.xml <field>）
struct FieldDef {
    std::string name;
    std::string typeName;
    WireType type = WireType::Int64;
    std::string desc;
};

/// 消息定义（messages.xml <msg>）
struct MsgDef {
    uint16_t id = 0;
    std::string name;
    Direction dir = Direction::C2S;
    std::string channel;
    MsgDomain domain = MsgDomain::Client;
    /// 缺省按通道解析（§10.6 v3）：movement/attributes/control=native，events=reflect。
    /// 解析层填充；规范序列化恒写出解析后的值（往返不动点的前提）。
    MsgBinding binding = MsgBinding::Native;
    std::vector<FieldDef> fields;
    std::string desc;
    std::string sourceFile;
};

/// 实体定义（entities.xml <entity>，单继承，生成期展开）
struct EntityDef {
    std::string name;
    std::string parent;  ///< 空 = 根
    std::string desc;
    /// 展开后的全量属性 id（含祖先链），解析器语义层填充，按 id 升序。
    /// 本批为空：逐实体 attr 绑定属后续批（见 entities.xml 注释）。
    std::vector<uint16_t> flattenedAttrs;
    /// 生成期展开的祖先链（根在前，不含自身）——§16.7.2「继承在生成期拍平」。
    std::vector<std::string> ancestors;
    std::string sourceFile;
};

/// 错误码（errors.xml <error>）
struct ErrorDef {
    int32_t code = 0;
    std::string name;
    std::string desc;
    std::string sourceFile;
};

/// 属性 ID 分段（docs/05 §6.1）。契约 attr.id 必须落在某一段内。
struct AttrSegment {
    uint16_t lo;
    uint16_t hi;
    const char* name;
};

inline constexpr AttrSegment kAttrSegments[] = {
    {1,   99,  "基础属性"},
    {100, 199, "战斗属性"},
    {200, 299, "元素属性"},
    {300, 399, "状态属性"},
    {400, 499, "特殊属性"},
    {500, 599, "PvP 属性"},
    {600, 699, "外观属性"},
    {700, 799, "社交属性"},
    {800, 899, "状态标记"},
};

/// 消息 ID 域分段（sdk-contract §11.3 ①）。原设计留白「段值落地时定」——
/// 本批（P2 反射后端首批）定值：client 1-899（对齐 attr 分段上限的惯例）、
/// internal 900+。段内自由增删，互不推动对方排布（对 KBE 单一分配表病根的反制）。
/// id 与 domain 的段约束是跨属性规则，XSD 表达不了——落在生成器第 ② 层。
struct MsgSegment {
    uint16_t lo;
    uint16_t hi;
    const char* name;
};

inline constexpr MsgSegment kMsgSegments[] = {
    {1,    899,   "client 域（对客户端可见）"},
    {900,  65535, "internal 域（服务端私有）"},
};

/// 整份契约（四个文件 + version 的聚合模型）
struct Contract {
    uint16_t version = 0;
    std::vector<TypeAlias> aliases;   ///< 按 name 升序
    std::vector<AttrDef> attrs;       ///< 按 id 升序
    std::vector<MsgDef> msgs;         ///< 按 id 升序
    std::vector<EntityDef> entities;  ///< 按 name 升序
    std::vector<ErrorDef> errors;     ///< 按 code 升序
    std::string sourceDir;            ///< 契约目录（诊断用）
};

const char* wireTypeToString(WireType t);
bool parseWireType(const std::string& name, WireType& out);
const char* directionToString(Direction d);
const char* msgDomainToString(MsgDomain d);
const char* msgBindingToString(MsgBinding b);

/// binding 缺省值按通道（sdk-contract §10.6 v3）：movement/attributes/control →
/// native（框架固定消息族），events → reflect（业务消息）。
MsgBinding defaultBindingForChannel(const std::string& channel);

/// 掩码 ↔ token 互转（canonical 顺序 APPR PROP SELF TEAM GUILD WORLD IMMEDIATE）
uint8_t syncTokensToMask(const std::vector<std::string>& tokens);
std::vector<std::string> syncMaskToTokens(uint8_t mask);

bool operator==(const TypeAlias& a, const TypeAlias& b);
bool operator==(const AttrDef& a, const AttrDef& b);
bool operator==(const FieldDef& a, const FieldDef& b);
bool operator==(const MsgDef& a, const MsgDef& b);
bool operator==(const EntityDef& a, const EntityDef& b);
bool operator==(const ErrorDef& a, const ErrorDef& b);
bool operator==(const Contract& a, const Contract& b);

}  // namespace apollo::contract
