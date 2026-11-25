#pragma once

#include <cstdint>
#include <string>

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
    // 资料写权代次（L118 归属域，915/918 write_gen 同源）：Base 写权自持的
    // 代次（档案 writeGen 同源）。与 simulation_epoch（模拟域生命周期）分域
    // 互不推演——换 Base 承载不动模拟代次，模拟域接线不代写归属代次；
    // 0 = 未声明。
    std::uint64_t write_gen = 0;
    // 归属 cell 的 M1 监听端点（L122 网关直连端点发现面，915/918/968
    // cell_host/cell_port 同源）：网关据此直连归属 cell 送模拟输入，不经
    // World 管理端物理中继。host 空串 = 端点未声明（cell 未声明监听/未登记），
    // 网关回落 World 中继；port 0 = 未监听。承载域——落点行原样携带，网关
    // 不据此裁决（裁决源仍是 assignment_version 水位）。
    std::string cell_host;
    std::uint32_t cell_port = 0;

    // 归属 Base 的 M1 监听端点（D12 逐 Base 结算选路，915/918 base_host/
    // base_port 同源）：登录实拨归属 Base 端点随落点成行，cell 结算面据此
    // 逐玩家拨 958。host 空串 = 端点未声明（Base 面关/目录未承接）——结算
    // 面按单 Base 配置兜底（承载域，不幻值）。
    std::string base_host;
    std::uint32_t base_port = 0;

    [[nodiscard]] bool is_assigned() const noexcept {
        return world_id != 0 || instance_id != 0 || space_id != 0;
    }

    // 值语义比较（镜像 wire 往返断言/落点迁移测试复用）
    [[nodiscard]] bool operator==(const WorldAssignment&) const noexcept = default;
};

} // namespace apollo::game::base
