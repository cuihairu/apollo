#pragma once
//
// EntityAttrs — 每实体属性存储（attribute-sync §3.1）。
//
// 写路径：set(attr, v) → 更新 values → 追加 history → 置 dirty。
// 单写者：归 scene 线程，无内锁。
//

#include <cstdint>
#include <optional>
#include <unordered_map>

#include "apollo/game/attrsync/attr_domain.hpp"
#include "apollo/game/attrsync/change_history.hpp"

namespace apollo::game::attrsync {

class EntityAttrs {
public:
    // ── slice 复合类型（attribute-sync §2.2 机制层）──
    //
    // 复合属性 = slice 容器：每 slot 独立同步单元（脏标记、序号、下发都以
    // slot 为粒度）。组合 id 纪律（§7.1 wire：attr_id 高 16 位为 slot）：
    //   组合 id = (slot << 16) | base；slot 0 = 容器长度格（即 base 本体），
    //   数据 slot 1..max_slots。
    //
    // 组合 id 直接走既有 values/dirty/history 面——per-slot 合并、ACK 水位、
    // 阈值与快照组装全部免费获得；attr_id 升序输出天然满足「先扩容后写
    // slot」（长度格 = base 排最前）。
    //
    // 纪律：slice 基 id ≤ 0xFFFF 且其 slot 空间 [base, (max<<16)|base] 独占
    // （契约面保证，声明面 slice_count/slice_type 归契约批——declare_slice
    // 为机制层登记）；收缩即终态（越界格值擦除）；base 直写拒绝（长度正门
    // = set_slice_len，直写会绕过收缩联动）。

    static constexpr std::uint16_t kSliceLenSlot = 0;

    // 组合 id → 容器坐标（slot 0 = 长度格）；不落在任何已声明容器空间返回 nullopt
    struct SliceCoord {
        std::uint32_t base;
        std::uint16_t slot;
    };

    explicit EntityAttrs(std::uint64_t entity_id, std::size_t history_capacity = 1024);

    // 设置属性值（追加 history + 置 dirty）；已声明 slice 容器的 base 直写拒绝
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

    // 属性可见域（§2.2；未声明 = kPublic）。slice 组合 id 继承 base 域
    // （域按属性声明，slot 随 base）。
    void set_domain(std::uint32_t attr_id, AttrDomain domain) { domains_[attr_id] = domain; }
    [[nodiscard]] AttrDomain domain(std::uint32_t attr_id) const {
        const auto coord = slice_coord(attr_id);
        auto it = domains_.find(coord ? coord->base : attr_id);
        return (it != domains_.end()) ? it->second : AttrDomain::kPublic;
    }

    // 实体 ID
    [[nodiscard]] std::uint64_t entity_id() const noexcept { return entity_id_; }

    // ── slice 容器面 ──

    // 机制层登记：base_attr 声明为 slice 容器，数据 slot 1..max_slots（重
    // 声明 = 更新上限）。不产生 history 条目（声明非变更）。
    void declare_slice(std::uint32_t base_attr, std::uint16_t max_slots) {
        slices_[base_attr] = max_slots;
    }
    [[nodiscard]] bool is_slice(std::uint32_t base_attr) const {
        return slices_.count(base_attr) != 0;
    }
    [[nodiscard]] std::uint16_t slice_max_slots(std::uint32_t base_attr) const {
        auto it = slices_.find(base_attr);
        return (it != slices_.end()) ? it->second : 0;
    }

    // 容器长度变更（同步 op，追加 history；超 max / 未声明拒绝）。收缩即
    // 终态：越界格值连脏标记一并擦除——快照面不携带死格，delta 面由
    // DeltaBuilder 丢弃同批死格条目（§3.4 保序例外的服务端半边）。
    void set_slice_len(std::uint32_t base_attr, std::uint16_t len);

    // 容器长度（未声明 = 0；声明未设长 = 0）
    [[nodiscard]] std::uint16_t slice_len(std::uint32_t base_attr) const;

    // slot 写（校验：已声明、slot ≥ 1 且 ≤ 当前长度；越界拒绝，不触 history）
    void set_slot(std::uint32_t base_attr, std::uint16_t slot, std::int64_t value);

    // slot 读（未设置返回 0）
    [[nodiscard]] std::int64_t get_slot(std::uint32_t base_attr, std::uint16_t slot) const {
        return get(slice_slot_id(base_attr, slot));
    }

    // 组合 id 拆解：落在任一已声明容器空间（slot 0..max）→ 坐标
    [[nodiscard]] std::optional<SliceCoord> slice_coord(std::uint32_t attr_id) const;

    // 组合 id 构造（slot 0 = base 本体 = 长度格）
    [[nodiscard]] static std::uint32_t slice_slot_id(std::uint32_t base_attr,
                                                     std::uint16_t slot) {
        return (static_cast<std::uint32_t>(slot) << 16) | base_attr;
    }

private:
    // 原始写（追加 history + 置 dirty；无 base 直写守卫）——set 与
    // set_slice_len 共用
    void set_raw_(std::uint32_t attr_id, std::int64_t value);

    std::uint64_t entity_id_;
    ChangeHistory history_;
    std::unordered_map<std::uint32_t, std::int64_t> values_;
    std::unordered_map<std::uint32_t, bool> dirty_;
    std::unordered_map<std::uint32_t, AttrDomain> domains_;
    std::unordered_map<std::uint32_t, std::uint16_t> slices_;  // base → max_slots
};

}  // namespace apollo::game::attrsync
