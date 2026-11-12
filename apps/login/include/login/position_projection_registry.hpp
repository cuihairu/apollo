#pragma once

// Space 执行位置投影登记表（L115 位置投影首切片，契约 968）：loginServer
// 目录持有 World 已提交的执行位置事实（journal A 行已落盘才发布的投影），
// 供目录侧查址消费（L118 网关路由批接入）。
//
// 裁决口径（D05 §1.2，docs/review/2026-10-30-actions/03）：按 (world_id,
// space_id) 复合键以 assignment_version 水位幂等受理——
//   claim（cell_id≠0 且 ready）：
//     版本 > 水位  → 应用（首投影/重认领，条目替换、水位前进）
//     版本 == 水位 → 幂等忽略（重复投影不重复记账）
//     版本 < 水位  → 乱序拒绝（旧投影不覆盖新归属）
//   墓碑（cell_id=0 且 !ready = 释放）：
//     版本 == 水位 → 抹除条目（释放无执行位置可指；无条目可抹 = 幂等无操作）
//     其余版本     → 乱序拒绝（低于水位不抹新归属；高于水位 = 未见认领的
//                     释放——重放分歧，拒不猜）
// 目录不另行分配 Cell（L123/L115 行内拍板）：本表只登记 World 已提交事实，
// 分配事实源是 World 管理面（962/journal）；目录重启后由 World kUp 跃迁
// 重放全量视图重建（968 发布面内置重放，同版本幂等）。
//
// 线程纪律：同 ServiceRegistry——apply_projection / 观测面只在 M1 服务注册
// 面泵线程调用（受理点同步裁决，单写者上下文），类不加锁。

#include <cstdint>
#include <string>
#include <unordered_map>

namespace login {

// 968 受理裁决（观测/测试面原样显形，不重裁）
enum class ProjectionVerdict {
    kApplied = 0,        // 版本前进：条目入表（首投影 / 重认领替换）
    kIdempotentIgnored,  // 版本 == 水位：重复投影忽略
    kStaleRejected,      // 乱序（旧版本 claim / 非当前版本墓碑）：拒绝
    kTombstoned,         // 墓碑命中当前水位：条目抹除
    kInvalidRejected,    // 身份缺位（world_id/space_id=0）或 ready/cell_id 不配对
};

// 一条执行位置投影（968 六字段 + 受理面回填）
struct SpacePositionProjection {
    std::int32_t world_id = 0;             // 投影源 World 号（复合键）
    std::uint64_t space_id = 0;            // 被投影 Space
    std::uint64_t cell_id = 0;             // 已提交归属 cell（0 = 墓碑）
    std::uint64_t assignment_version = 0;  // 归属版本水位（与 962 同源）
    std::uint64_t management_epoch = 0;    // World 管理代次（与 962 同源）
    bool ready = false;                    // 1=归属可服务；0=墓碑
    std::uint64_t received_at_ms = 0;      // steady 毫秒（last-heard 语义）
};

class PositionProjectionRegistry {
public:
    // 受理点（M1 泵线程上下文）：968 无回执，裁决即落地。
    ProjectionVerdict apply_projection(const SpacePositionProjection& p,
                                       std::uint64_t now_ms) {
        const bool tombstone = p.cell_id == 0;
        if (p.world_id == 0 || p.space_id == 0 || tombstone == p.ready ||
            (!tombstone && p.assignment_version == 0)) {
            // ready 与 cell_id 同进退：claim = cell≠0 且 ready；墓碑 = cell=0
            // 且 !ready。claim 版本 0 = 未分配语义（962 首分配版本 1）——显式
            // 拒不落表。其余组合是解耦面违例（962 发布纪律不产出），同拒。
            ++invalid_total_;
            return ProjectionVerdict::kInvalidRejected;
        }
        const auto key = key_of(p.world_id, p.space_id);
        const auto it = watermarks_.find(key);
        const std::uint64_t watermark =
            it == watermarks_.end() ? 0 : it->second;
        if (tombstone) {
            if (watermark != 0 && p.assignment_version == watermark) {
                entries_.erase(key);  // 无条目可抹 = 幂等无操作
                ++tombstoned_total_;
                return ProjectionVerdict::kTombstoned;
            }
            ++stale_total_;
            return ProjectionVerdict::kStaleRejected;
        }
        if (p.assignment_version > watermark) {
            SpacePositionProjection& entry = entries_[key];
            entry = p;
            entry.received_at_ms = now_ms;
            watermarks_[key] = p.assignment_version;
            ++applied_total_;
            return ProjectionVerdict::kApplied;
        }
        if (p.assignment_version == watermark) {
            ++idempotent_total_;
            return ProjectionVerdict::kIdempotentIgnored;
        }
        ++stale_total_;
        return ProjectionVerdict::kStaleRejected;
    }

    // 查址面（L118 网关路由批消费入口；本切片为目录观测/测试面）
    const SpacePositionProjection* find_projection(std::int32_t world_id,
                                                   std::uint64_t space_id) const {
        const auto it = entries_.find(key_of(world_id, space_id));
        return it == entries_.end() ? nullptr : &it->second;
    }

    // 条目数（观测面；按 (world, space) 复合键计）
    std::size_t projection_count() const { return entries_.size(); }

    // 裁决计数（观测面：e2e/运维断言事实源，日志不算）
    std::uint64_t applied_total() const { return applied_total_; }
    std::uint64_t idempotent_total() const { return idempotent_total_; }
    std::uint64_t stale_total() const { return stale_total_; }
    std::uint64_t tombstoned_total() const { return tombstoned_total_; }
    std::uint64_t invalid_total() const { return invalid_total_; }

private:
    static std::uint64_t key_of(std::int32_t world_id, std::uint64_t space_id) {
        // world 与 space 复合键：两域数值空间独立（同号 World/Space 不串键）
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(world_id))
                << 56) |
               (space_id & ((static_cast<std::uint64_t>(1) << 56) - 1));
    }

    std::unordered_map<std::uint64_t, SpacePositionProjection> entries_;
    std::unordered_map<std::uint64_t, std::uint64_t> watermarks_;
    std::uint64_t applied_total_ = 0;
    std::uint64_t idempotent_total_ = 0;
    std::uint64_t stale_total_ = 0;
    std::uint64_t tombstoned_total_ = 0;
    std::uint64_t invalid_total_ = 0;
};

} // namespace login
