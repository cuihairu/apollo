#pragma once

#include <cstdint>
#include <string>

namespace apollo::game::world {

// SceneDescriptor（P0-3 拆层产物）：Map 资产引用（term-contract §1.1「地图资产」
// ——地形/碰撞/寻路的离线烘焙资产引用），与 Scene（运行时）分离。
//
// 三层空壳（MapInstance→WorldSpace→Scene）拆除后：
//   地图资产（Descriptor，静态声明）──create_scene──▶ Scene（运行时容器）
// Descriptor 是值类型：不含运行时状态，不参与 tick。
struct SceneDescriptor {
    std::uint64_t map_id = 0;      // 地图资产 id（离线烘焙资产引用；0 = 未绑定资产）
    std::string map_name;          // 地图资产名（如 "default-world"）
    float width = 0.0f;            // 地图尺寸（AOI 网格划分输入）
    float height = 0.0f;
    float grid_size = 1.0f;        // AOI 网格边长
    float view_radius = 20.0f;     // AOI 视野半径

    [[nodiscard]] bool is_valid() const noexcept {
        return map_id != 0 && !map_name.empty();
    }
};

} // namespace apollo::game::world