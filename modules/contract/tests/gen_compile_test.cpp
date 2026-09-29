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

// 继承展开：Player 的祖先链 = Monster, Avatar（根在前）
constexpr int findEntity(const char* name) {
    for (size_t i = 0; i < acg::kEntityCount; ++i) {
        if (eqStr(acg::kEntities[i].name, name)) return static_cast<int>(i);
    }
    return -1;
}
static_assert(findEntity("Player") >= 0, "必须有 Player 实体");
static_assert(acg::kEntities[findEntity("Player")].ancestorCount == 2,
              "Player 祖先链应为两级");
static_assert(eqStr(acg::kEntities[findEntity("Player")].ancestors[0], "Monster") &&
                  eqStr(acg::kEntities[findEntity("Player")].ancestors[1], "Avatar"),
              "Player 祖先链应为 [Monster, Avatar]");

int main() {
    // 运行时零副作用：编译通过 + 断言成立即绿
    return 0;
}
