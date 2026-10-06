/**
 * @file gen_compile_test.cpp
 * @brief 第 ③ 层编译期闸（xml-generation §5）：生成产物必须可编译，
 * 且关键不变量在 static_assert 层面成立。不产生运行时副作用。
 */

#include "apollo_contract.h"  // sdks/cpp/generated/apollo_contract.h（gen 产物）

#include <cstddef>
#include <cstdint>

namespace acg = apollo::contract::gen;

// 常量期字符串工具（std::strcmp/std::strlen 非 constexpr）
constexpr bool eqStr(const char* a, const char* b) {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) return false;
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

constexpr size_t strLen(const char* s) {
    size_t n = 0;
    while (s[n] != '\0') ++n;
    return n;
}

// 生成表完整性与基本不变量
static_assert(acg::kContractVersion >= 1, "契约版本必须 >= 1");
static_assert(acg::kAttrCount == sizeof(acg::kAttrs) / sizeof(acg::kAttrs[0]),
              "kAttrCount 必须与 kAttrs 表长一致");
static_assert(strLen(acg::kSchemaHash) == 64, "schema hash 应为 64 位十六进制串");

// 按 name 常量查找（返回下标，-1 = 不存在；顺带锁住「表非空且按 id 升序」）
constexpr int findAttr(const char* name) {
    for (size_t i = 0; i < acg::kAttrCount; ++i) {
        if (eqStr(acg::kAttrs[i].name, name)) return static_cast<int>(i);
    }
    return -1;
}
static_assert(findAttr("hp") >= 0, "必须有 hp 属性");
static_assert(static_cast<uint16_t>(acg::kAttrs[findAttr("hp")].id) == 1, "hp 的 id 应为 1");
static_assert((acg::kAttrs[findAttr("hp")].sync & acg::kSyncProp) != 0,
              "hp 应带 PROP 同步位");
static_assert((acg::kAttrs[findAttr("hp")].sync & acg::kSyncDbBanned) == 0,
              "生成物不得含 SYNC_DB 位（决策 #5：存储语义在 storage.xml）");
static_assert(findAttr("move_speed") >= 0 &&
                  (acg::kAttrs[findAttr("move_speed")].sync & acg::kSyncImmediate) == 0,
              "move_speed 不应带 IMMEDIATE");
static_assert(findAttr("level") >= 0 &&
                  (acg::kAttrs[findAttr("level")].sync & acg::kSyncWorld) != 0 &&
                  (acg::kAttrs[findAttr("level")].sync & acg::kSyncTeam) != 0,
              "level 应带 TEAM|WORLD（docs/05 §6.3 示例）");
static_assert(findAttr("weapon_appearance") >= 0 &&
                  (acg::kAttrs[findAttr("weapon_appearance")].sync & acg::kSyncAppr) != 0 &&
                  (acg::kAttrs[findAttr("weapon_appearance")].sync & acg::kSyncImmediate) != 0,
              "weapon_appearance 应带 APPR|IMMEDIATE");

// 消息
static_assert(static_cast<uint16_t>(acg::MsgId::move) == 10, "move id 应为 10");
static_assert(static_cast<uint16_t>(acg::MsgId::attr_batch) == 11, "attr_batch id 应为 11");

// 分域与绑定（sdk-contract §11.3 / §10.6 v3）：按 name 常量查找后锁住
// domain/binding 的解析结果——框架固定消息族四条全为 client 域 native 绑定。
constexpr int findMsg(const char* name) {
    for (size_t i = 0; i < acg::kMsgCount; ++i) {
        if (eqStr(acg::kMsgs[i].name, name)) return static_cast<int>(i);
    }
    return -1;
}
static_assert(findMsg("move") >= 0 && acg::kMsgs[findMsg("move")].domain == acg::MsgDomain::Client,
              "move 应为 client 域");
static_assert(findMsg("move") >= 0 &&
                  acg::kMsgs[findMsg("move")].binding == acg::MsgBinding::Native,
              "movement 通道框架族缺省 native 绑定");
static_assert(findMsg("heartbeat") >= 0 &&
                  acg::kMsgs[findMsg("heartbeat")].binding == acg::MsgBinding::Native,
              "control 通道框架族缺省 native 绑定");

// 双域 hash（sdk-contract §11.3 ②）：握手对象 kClientHash 与服务端断言锚点
// kInternalHash 都必须是 64 位十六进制，且与全量 kSchemaHash 三者互异——
// 生成器漏算任一份在此层即红。
static_assert(strLen(acg::kClientHash) == 64, "client_hash 应为 64 位十六进制串");
static_assert(strLen(acg::kInternalHash) == 64, "internal_hash 应为 64 位十六进制串");
static_assert(!eqStr(acg::kSchemaHash, acg::kClientHash) &&
                  !eqStr(acg::kSchemaHash, acg::kInternalHash) &&
                  !eqStr(acg::kClientHash, acg::kInternalHash),
              "三个 hash 输入不同必互异（域标签行参与 bundle）");

// 继承展开不变量（P2-3 实体链修文：Player 已废删除、Avatar 摘脱怪物族为根、
// Monster←NPC 单继承保留）
constexpr int findEntity(const char* name) {
    for (size_t i = 0; i < acg::kEntityCount; ++i) {
        if (eqStr(acg::kEntities[i].name, name)) return static_cast<int>(i);
    }
    return -1;
}
static_assert(findEntity("Player") < 0, "Player 已废（term-contract v1.0），不得复活");
static_assert(findEntity("Monster") >= 0 && findEntity("NPC") >= 0 &&
                  findEntity("Avatar") >= 0,
              "场景实体族三实体齐备（Monster/NPC/Avatar）");
static_assert(acg::kEntities[findEntity("NPC")].ancestorCount == 1 &&
                  eqStr(acg::kEntities[findEntity("NPC")].ancestors[0], "Monster"),
              "NPC 祖先链 = [Monster]（场景实体单继承保留）");
static_assert(acg::kEntities[findEntity("Avatar")].ancestorCount == 0,
              "Avatar 为根实体（玩家化身不入怪物族）");

int main() {
    // 运行时零副作用：编译通过 + 断言成立即绿
    return 0;
}
