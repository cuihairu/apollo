#pragma once

#include "apollo/game/core/entity.hpp"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace apollo::game::world {

// ViewerState（P1-3，契约 viewer set 逐 viewer 水位）：
//   每个观察者一份「当前可见主体集」；AOI 事件分发的差集基准——
//   add 返回真 = 主体新进视野（ENTER），remove 返回真 = 主体出视野
//   （LEAVE），visible 为真且非新进 = 位置更新（SYNC）。AOI 实现只负责
//   算半径内集合，「谁看见什么」的权威账本在此。
//
// 单写者约定：归 scene 线程（与 SceneAoi 同生命周期），无内锁。
class ViewerState {
public:
    // 登记 observer（空视野起步；重复登记清空重置）
    void reset(apollo::game::core::EntityId observer);

    // 注销 observer（连同其视野账本）
    void drop(apollo::game::core::EntityId observer);

    // observer 是否登记在册
    [[nodiscard]] bool tracked(apollo::game::core::EntityId observer) const noexcept;

    // subject 当前是否在 observer 视野内（未登记 observer → false）
    [[nodiscard]] bool visible(apollo::game::core::EntityId observer,
                               apollo::game::core::EntityId subject) const noexcept;

    // observer 视野内主体集快照
    [[nodiscard]] std::vector<apollo::game::core::EntityId> visible_set(
        apollo::game::core::EntityId observer) const;

    // subject 记入 observer 视野；返回 true = 原不可见（新进，ENTER）
    bool add(apollo::game::core::EntityId observer, apollo::game::core::EntityId subject);

    // subject 移出 observer 视野；返回 true = 原可见（出视野，LEAVE）
    bool remove(apollo::game::core::EntityId observer, apollo::game::core::EntityId subject);

    // subject 从全部 observer 视野移除；返回受影响 observer 列表（LEAVE 面）
    std::vector<apollo::game::core::EntityId> remove_everywhere(
        apollo::game::core::EntityId subject);

    // 在册 observer 数（可观察）
    [[nodiscard]] std::size_t size() const noexcept { return views_.size(); }

private:
    std::unordered_map<std::uint64_t, std::unordered_set<std::uint64_t>> views_;
};

} // namespace apollo::game::world
