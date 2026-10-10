#pragma once

// Recovery（P1-5；lifecycle §3 矩阵 + 任务书 §20 三档清单）：
//   - recovery_manifest()：「必须持久化 / 可重算 / 可丢」三档清单，以代码
//     承载 lifecycle §3 矩阵，逐项标注恢复动作；
//   - RecoveryCoordinator：进程启动序列 restore→admission→ready（缺一进
//     Failed，拒绝服务）；
//   - restore_anchors：落盘玩家按需重建锚点（恢复态 Offline——档案在、
//     会话无；登录路径 activatePlayer 再转 Online）。
//
// journal 消费侧经 RestoreStep 注入（本模块不依赖 data 栈；zone-app 以
// PersistJournal::replay 充当 restore_step）。

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace apollo::game::session {

class AnchorManager;

enum class RecoveryTier : std::uint8_t {
    MustPersist = 0,  // 必须持久化
    Recomputable,     // 可以重新计算
    Lossy,            // 可以丢失
};

const char* to_string(RecoveryTier tier);

struct RecoveryItem {
    const char* object;  // 恢复对象
    RecoveryTier tier;
    const char* action;  // 承载机制 / 恢复动作
};

// 三档清单（lifecycle §3 恢复矩阵的代码形态）
const std::vector<RecoveryItem>& recovery_manifest();

enum class RecoveryPhase : std::uint8_t {
    Init = 0,
    Restoring,
    Admitting,
    Ready,
    Failed,
};

const char* to_string(RecoveryPhase phase);

class RecoveryCoordinator {
public:
    // 回放/恢复一步，返回恢复条数（如 journal replay 应用数）
    using RestoreStep = std::function<std::size_t()>;
    // 准入检查：数据面/场景面可服务才放行
    using AdmissionCheck = std::function<bool()>;

    RecoveryCoordinator(std::string component, RestoreStep restore_step,
                        AdmissionCheck admission);

    // 完整序列：restore→admission→ready；任一步失败落 Failed 并返回 false
    bool run();

    bool restore();
    bool admit();

    RecoveryPhase phase() const noexcept { return phase_; }
    bool ready() const noexcept { return phase_ == RecoveryPhase::Ready; }
    std::size_t replayed() const noexcept { return replayed_; }
    const std::string& component() const noexcept { return component_; }

private:
    std::string component_;
    RestoreStep restore_step_;
    AdmissionCheck admission_;
    RecoveryPhase phase_ = RecoveryPhase::Init;
    std::size_t replayed_ = 0;
};

// Anchor 恢复：为落盘玩家重建锚点并置 Offline（会话未至）。
// 返回恢复锚点数。
std::size_t restore_anchors(AnchorManager& manager,
                            const std::vector<std::uint64_t>& player_ids);

} // namespace apollo::game::session
