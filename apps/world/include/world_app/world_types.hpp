#pragma once

#include <cmath>
#include <cstdint>

namespace world_app {

using EntityID = uint64_t;
using SpaceID = uint32_t;

// L108 ID 域切分：Cell 是空间分区，执行进程是承载进程——两域不混用。
// 当前实现仍是「每 Space 一个全幅 Cell」，所以一个执行进程可承载多个
// Cell 分区（每个 Space 一个）；executor_id 对应 CellRegistration::cell_id。
using CellPartitionId = uint64_t;
using CellExecutorId = uint64_t;

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

// P2-1b：WorldApp 侧 Entity/PlayerEntity/EntityManager 已删——实体集合归 Scene
// （scene.hpp:33 既有裁决「承接 EntityManager 职责，行为等价迁移」）：
// 玩家实体走 attach 路径入 avatars_，非玩家实体经 spawn_entity 入 entities_。
// 本头仅保留消息面共用的枚举与位置类型。

} // namespace world_app
