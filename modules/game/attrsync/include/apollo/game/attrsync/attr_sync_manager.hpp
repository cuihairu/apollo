#pragma once
//
// AttrSyncManager — per-viewer delta 管线集成（attribute-sync §3.2/§4.1）。
//
// 职责：
//   - 每实体 EntityAttrs（属性存储 + ChangeHistory）
//   - 每 (viewer, entity) ViewerAttrState（acked_seq 水位）
//   - AOI Enter → 建 ViewerState + 快照；AOI Leave → 回收
//   - Broadcast tick → 收集 per-viewer delta 帧
//   - ACK → 推进 acked_seq
//
// 单写者：归 scene 线程，无内锁。
//

#include <cstdint>
#include <functional>
#include <unordered_map>

#include "apollo/game/attrsync/delta_builder.hpp"
#include "apollo/game/attrsync/entity_attrs.hpp"

namespace apollo::game::attrsync {

class AttrSyncManager {
public:
    // ---- 实体注册 ----
    void register_entity(std::uint64_t entity_id);
    [[nodiscard]] bool has_entity(std::uint64_t entity_id) const;

    // ---- 属性写路径 ----
    void set_attribute(std::uint64_t entity_id, std::uint32_t attr_id, std::int64_t value);
    [[nodiscard]] std::int64_t get_attribute(std::uint64_t entity_id, std::uint32_t attr_id) const;

    // ---- §8.2 写后日志入口 ----
    // 属性变更回调（每次 set_attribute 成功后触发），供宿主接入 PersistJournal
    using ChangeSink = std::function<void(std::uint64_t entity_id,
                                          std::uint32_t attr_id,
                                          std::int64_t value)>;
    void set_change_sink(ChangeSink sink);
    void clear_change_sink() { change_sink_ = nullptr; }

    // ---- §5.1 属性钩子窗口（scripting-lua §5.1：dirty 固定后、collect 前）----
    // 注入式：attrsync 不依赖 script 模块——窗口实现（白名单/预算/错误隔离）
    // 由宿主适配注入（ScriptRuntime::run_attr_hook_window）。
    struct HookChange {
        std::uint64_t entity;
        std::uint32_t attr_id;
        std::int64_t old_value;
        std::int64_t new_value;
    };
    using AttrHook = std::function<void(std::vector<HookChange>&)>;

    void set_attr_hook(AttrHook hook);
    void clear_attr_hook() {
        attr_hook_ = nullptr;
        pending_order_.clear();
        pending_index_.clear();
    }

    // 窗口执行：本 tick 写入集（每 (entity,attr) 去重 first-old/last-new）→ 调注入
    // 钩子 → 修订值经正规写路径落存储+history+sink（不再进窗，防重入死循环）。
    // 无钩子空载零开销；同 tick 幂等（变更集取走即空）。
    void apply_attr_hook();

    // ---- 域投影（§2.2）----
    // 属性可见域（未注册实体静默忽略；未声明 = kPublic）
    void set_attr_domain(std::uint64_t entity_id, std::uint32_t attr_id, AttrDomain domain);
    // 队/会域成员判定（未设置 = 该两域不可见）
    void set_group_gate(DeltaBuilder::GroupGate gate);

    // ---- AOI 事件接线（§4.1）----
    // Viewer 进入实体视野：建 ViewerState（不组帧，快照由 collect 面出）
    void on_viewer_enter(std::uint64_t viewer, std::uint64_t entity_id);
    // Viewer 进入实体视野：建 ViewerState + 返回快照帧
    std::vector<AttrSyncFrame> on_viewer_enter_collect(std::uint64_t viewer, std::uint64_t entity_id);
    // Viewer 离开实体视野：回收 ViewerState
    void on_viewer_leave(std::uint64_t viewer, std::uint64_t entity_id);

    // ---- 查询 ----
    [[nodiscard]] bool has_viewer(std::uint64_t viewer, std::uint64_t entity_id) const;

    // ---- Broadcast tick 收集（§3.2）----
    // 返回 per-viewer delta 帧（空 = 无变更）
    [[nodiscard]] std::vector<AttrSyncFrame> collect(std::uint64_t viewer, std::uint64_t entity_id);
    // 只读收集：不推进 acked_seq、不清 dirty（供调度器预检用）
    [[nodiscard]] std::vector<AttrSyncFrame> peek(std::uint64_t viewer, std::uint64_t entity_id);

    // ---- ACK（§3.2 第 4 步）----
    void ack(std::uint64_t viewer, std::uint64_t entity_id, std::uint64_t seq);

    // ---- ACK 超时降级（§3.3）----
    // 配置 ACK 超时拍数（默认 3）
    void set_ack_timeout_ticks(std::uint64_t ticks) { ack_timeout_ticks_ = ticks; }
    // 查询 (viewer, entity) 是否处于降级态
    [[nodiscard]] bool is_degraded(std::uint64_t viewer, std::uint64_t entity_id) const;

    // ---- 内部访问（测试用）----
    [[nodiscard]] const EntityAttrs& entity_attrs(std::uint64_t entity_id) const;

private:
    static std::uint64_t pair_key(std::uint64_t viewer, std::uint64_t entity_id) {
        return (viewer << 32) | (entity_id & 0xFFFFFFFFull);
    }

    std::unordered_map<std::uint64_t, EntityAttrs> entities_;
    // (viewer, entity) → ViewerAttrState 展平为单键
    std::unordered_map<std::uint64_t, ViewerAttrState> viewers_;
    // (viewer, entity) → 自上次 ACK 起的 collect 拍数
    std::unordered_map<std::uint64_t, std::uint64_t> silent_ticks_;
    std::uint64_t ack_timeout_ticks_ = 3;
    ChangeSink change_sink_;
    DeltaBuilder::GroupGate group_gate_;

    // §5.1 钩子窗口：注入实现 + 待处理变更集（写序保持 + (entity,attr) 去重索引）
    AttrHook attr_hook_;
    std::vector<HookChange> pending_order_;
    std::unordered_map<std::uint64_t, std::size_t> pending_index_;
    static std::uint64_t pending_key(std::uint64_t entity, std::uint32_t attr) {
        return (entity << 32) | attr;
    }
    void record_pending(std::uint64_t entity, std::uint32_t attr,
                        std::int64_t old_value, std::int64_t new_value);
};

}  // namespace apollo::game::attrsync
