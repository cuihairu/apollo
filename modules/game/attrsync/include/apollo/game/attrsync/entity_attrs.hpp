#pragma once
//
// EntityAttrs — 每实体属性存储（attribute-sync §3.1）。
//
// 写路径：set(attr, v) → 更新 values → 追加 history → 置 dirty。
// 单写者：归 scene 线程，无内锁。
//

#include <cstdint>
#include <unordered_map>

#include "apollo/game/attrsync/change_history.hpp"

namespace apollo::game::attrsync {

class EntityAttrs {
public:
    explicit EntityAttrs(std::uint64_t entity_id, std::size_t history_capacity = 1024);

    // 设置属性值（追加 history + 置 dirty）
    void set(std::uint32_t attr_id, std::int64_t value);

    // 获取属性值（未设置返回 0）
    [[nodiscard]] std::int64_t get(std::uint32_t attr_id) const;

    // 批量设置
    void set_all(const std::vector<std::pair<std::uint32_t, std::int64_t>>& attrs);

    // dirty 位操作
    [[nodiscard]] bool is_dirty(std::uint32_t attr_id) const;
    void clear_dirty();
    [[nodiscard]] std::size_t dirty_count() const;

    // 只读访问 history（DeltaBuilder 用）
    [[nodiscard]] const ChangeHistory& history() const noexcept { return history_; }

    // 全量当前值（快照负载组装用，§3.3）
    [[nodiscard]] const std::unordered_map<std::uint32_t, std::int64_t>& values() const noexcept {
        return values_;
    }

    // 实体 ID
    [[nodiscard]] std::uint64_t entity_id() const noexcept { return entity_id_; }

private:
    std::uint64_t entity_id_;
    ChangeHistory history_;
    std::unordered_map<std::uint32_t, std::int64_t> values_;
    std::unordered_map<std::uint32_t, bool> dirty_;
};

}  // namespace apollo::game::attrsync
