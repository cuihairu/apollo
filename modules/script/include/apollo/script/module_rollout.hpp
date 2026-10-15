#pragma once

// §3.2 Phase A 工程面投递编排（灰度 per 场景线程）：一次预备 → 多目标投递 →
// 灰度观察判定 → 全量推广 / 回滚。
//
// 为什么是「分步 + pending 队列」而非同步遍历：灰度粒度 = per 场景线程
// （scripting-lua §3.2），一状态一线程（§2）——Phase B 的 commit 必须在目标
// owning 线程的 tick 边界执行，不可能由一个循环跨线程同步调用。因此编排器
// 只负责预备产物与判定推进，控制面按 pending_* 队列把每一步投进对应目标
// 场景线程的任务队列（§7.1 attach 同型投递），再回喂结果。单 gameThread
// 宿主可直接顺序直驱（等价同步形态）。
//
// 两段式：prepare() 在独立预备沙盒（本编排器自持 ScriptRuntime）跑编译/求值/
// 冒烟，一次预备服务全部目标（同一份 PreparedModule 交接，同 Phase A 的
// 进程内字节码纪律）；commit_target() 走既有 ScriptRuntime::commit_module
// 面（tick 边界 O(1) 换表）。本面不持有第二套换表路径。
//
// 判定：每 (灰度目标, 模块) 一个 ModuleCanary（Phase C 原语），观察窗以
// 「灰度面全部提交完成」为 T0（各目标窗起点 = 各自 runtime 当前 tick）。
// 任一 canary 超阈 = 早失败判坏（不等窗满）→ 回滚已提交的灰度目标；全部
// 窗满健康 = 判好 → 推广到余下目标。promote 失败是 per-target 错误
// （判好结论不改，目标停留在补丁前版本，重试面交宿主）。
//
// 复用：同一编排器实例可串行跑多轮（start 前置：终态才允许再次 prepare）。
// 预备沙盒随实例保留，已备依赖跨轮可见（同 prepare_module 预备面累积纪律）。
//
// 线程纪律：本面不持锁、不做跨线程调度；各方法按其契约面在对应线程调用。

#include "apollo/script/module_canary.hpp"
#include "apollo/script/script_runtime.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace apollo::script {

// 待投递模块（patch_loader 校验过的源码 + 控制面自带的版本/依赖声明）。
struct RolloutModule {
    std::string name;
    std::uint64_t version = 0;
    std::vector<std::string> deps;
    std::string source;  // prepare 输入
};

// 投递目标：per 场景线程的 runtime（Scene::attach_script 的桥后形态）。
// 编排器不持有 runtime 所有权，key 供宿主与场景/线程身份对齐。
struct RolloutTarget {
    std::string key;
    ScriptRuntime* rt = nullptr;
};

enum class RolloutState {
    kIdle,        // 未预备
    kFailed,      // 预备/参数失败（终态）——零目标被触碰
    kCanaryPending,   // 灰度目标待 commit
    kObserving,       // 灰度面已全部提交，观察窗在跑
    kPromoting,       // 判好，非灰度目标待 commit
    kRollingBack,     // 判坏，已提交灰度目标待 rollback
    kPromoted,    // 终态：全量推广完成
    kRolledBack,  // 终态：灰度已回滚
};

// 目标状态快照（观察面呈现用；key 序 = 登记序）。
struct TargetStatus {
    std::string key;
    bool canary = false;
    bool committed = false;       // 本轮补丁整集已在该目标激活
    bool rolled_back = false;     // 已执行回滚步骤
    std::uint64_t window_faults = 0;  // 观察窗内归因 fault（各模块合计）
    std::vector<std::string> errors;  // 该目标的投递/回滚错误行
};

class ModuleRollout {
public:
    ModuleRollout(std::vector<RolloutTarget> targets,
                  ScriptConfig preparer_config = {});

    // Phase A：拓扑序预备整集（依赖先行）。参数非法/依赖环/预备失败 →
    // false + errors()，状态 kFailed，任何目标未被触碰。终态（含 kFailed）
    // 才允许再次调用；在途状态调用同样拒绝。
    bool prepare(std::vector<RolloutModule> modules,
                 const std::vector<std::string>& canary_keys,
                 CanaryParams params);

    // pending 队列 = 控制面拆投面（每行一个待处理目标 key，登记序）。
    [[nodiscard]] std::vector<std::string> pending_canary() const;
    [[nodiscard]] std::vector<std::string> pending_promote() const;
    [[nodiscard]] std::vector<std::string> pending_rollback() const;

    // Phase B：目标 owning 线程 tick 边界投递整集（拓扑序，中途失败即止）。
    // 相位/角色不匹配时静默空转（宿主按帧统一驱动是合法用法）。
    void commit_target(const std::string& key);
    // Phase C：推进该灰度目标全部模块的观察窗；灰度面整体推进时由本编排器
    // 评估判定（任一窗判坏 = 早失败转回滚；全窗满健康 = 转推广）。
    void observe_target(const std::string& key);
    // 判坏相位：逆拓扑序回滚整集（rollback_module 是既有面；无冷冻区 = 拒
    // 回滚，按 per-target 错误记录——新装模块无从回退，既有面语义）。
    void rollback_target(const std::string& key);

    [[nodiscard]] RolloutState state() const { return state_; }
    [[nodiscard]] const std::vector<std::string>& errors() const {
        return errors_;
    }
    [[nodiscard]] std::vector<TargetStatus> statuses() const;

private:
    struct Entry {
        RolloutTarget target;
        bool canary = false;
        bool commit_attempted = false;
        bool committed = false;
        bool rollback_attempted = false;
        std::vector<ModuleCanary> canaries;
        std::uint64_t window_faults = 0;
        std::vector<std::string> errors;
    };

    // 拓扑序预备整集（失败即止，errors_ 记原因）。成功返回 true。
    bool prepare_set_(const std::vector<RolloutModule>& modules);
    // Kahn 拓扑排序：ready 按输入序取（确定性）。环/缺条目返回 false。
    static bool topological_order_(const std::vector<RolloutModule>& modules,
                                   std::vector<std::size_t>& out);
    Entry* find_(const std::string& key);
    void begin_canaries_();
    void evaluate_();
    bool promote_all_attempted_() const;
    bool rollback_all_attempted_() const;

    ScriptConfig preparer_config_;
    std::unique_ptr<ScriptRuntime> preparer_;  // 懒建，跨轮保留
    std::vector<PreparedModule> prepared_;     // 拓扑序产物
    std::vector<Entry> entries_;
    CanaryParams params_;
    RolloutState state_ = RolloutState::kIdle;
    std::vector<std::string> errors_;
};

}  // namespace apollo::script
