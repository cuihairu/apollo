#pragma once

#include <cstdint>

namespace apollo::game::base {

struct WorldAssignment {
    std::uint32_t world_id = 0;
    std::uint64_t map_id = 0;
    std::uint64_t instance_id = 0;
    std::uint64_t space_id = 0;
    std::uint64_t route_version = 0;
    // 拓扑版本（D07 版本域分立，915/918 topology_version 同源）：布局代次
    // ——布局演进时前进，与路由版本（位置路由水位）分域互不推演；0 = 未声明。
    std::uint64_t topology_version = 0;
    // 执行进程归属 cell 号（L116 字段分型首切片，915/918 cell_id 同源）：
    // 0 = 执行进程未指派——落点先行，模拟域接线随多 cell 批。
    std::uint64_t cell_id = 0;
    // 模拟代次（执行域生命周期水位，915/918 simulation_epoch 同源）；
    // 0 = 未声明。与 968 management_epoch（管理代次）分域——管理面代次不
    // 代指模拟域生命周期。
    std::uint64_t simulation_epoch = 0;

    [[nodiscard]] bool is_assigned() const noexcept {
        return world_id != 0 || instance_id != 0 || space_id != 0;
    }

    // 值语义比较（镜像 wire 往返断言/落点迁移测试复用）
    [[nodiscard]] bool operator==(const WorldAssignment&) const noexcept = default;
};

} // namespace apollo::game::base
