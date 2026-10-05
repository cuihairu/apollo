#pragma once

#include <cmath>
#include <cstdint>

namespace cell {

using EntityID = uint64_t;
using SpaceID = uint32_t;

// 实体类型
enum class EntityType : uint8_t {
    UNKNOWN = 0,
    PLAYER = 1,
    NPC = 2,
    MONSTER = 3,
    PET = 4,
};

// 位置
struct Position {
    float x, y, z;
    Position() : x(0), y(0), z(0) {}
    Position(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    float distanceTo(const Position& other) const {
        float dx = x - other.x;
        float dy = y - other.y;
        float dz = z - other.z;
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    }
};

// P2-1b：cell 侧 Entity/PlayerEntity/EntityManager 已删——实体集合归 Scene
// （scene.hpp:33 既有裁决「承接 EntityManager 职责，行为等价迁移」）：
// 玩家实体走 attach 路径入 avatars_，非玩家实体经 spawn_entity 入 entities_。
// 本头仅保留消息面共用的枚举与位置类型。

} // namespace cell
