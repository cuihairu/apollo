#pragma once
//
// ViewerAttrState — per-viewer 属性同步水位（attribute-sync §3.1）。
//
// 每个 (viewer, entity) 二元组维护 acked_seq：客户端已确认收到的实体 seq。
// delta = history[(acked_seq, world_seq]]，快照切换判定依据。
//
// 单写者约定：归 scene 线程，无内锁。
//

#include <cstdint>
#include <unordered_map>

namespace apollo::game::attrsync {

class ViewerAttrState {
public:
    // 设置某 viewer 对某实体的 acked_seq
    void set_acked(std::uint64_t entity_id, std::uint64_t seq);

    // 获取 acked_seq（未设置返回 0）
    [[nodiscard]] std::uint64_t acked_seq(std::uint64_t entity_id) const;

    // 移除某实体的记录
    void remove(std::uint64_t entity_id);

    // 在册实体数
    [[nodiscard]] std::size_t size() const noexcept { return acked_.size(); }

private:
    std::unordered_map<std::uint64_t, std::uint64_t> acked_;
};

}  // namespace apollo::game::attrsync
