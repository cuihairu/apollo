// §3.2 Phase C canary 实现——观察窗倒计时 + per-module fault 窗查询。
// 全部经 ScriptRuntime 公有面（current_tick / module_faults_since），本文件
// 不触 Lua 状态。

#include "apollo/script/module_canary.hpp"

#include "apollo/script/script_runtime.hpp"

namespace apollo::script {

ModuleCanary::ModuleCanary(ScriptRuntime& rt, std::string module, CanaryParams params)
    : rt_(rt), module_(std::move(module)), params_(params) {}

void ModuleCanary::begin() {
    begin_tick_ = rt_.current_tick();
    ticks_left_ = params_.observe_ticks;
    begun_ = true;
    finished_ = false;
}

void ModuleCanary::tick() {
    if (!begun_ || finished_) {
        return;
    }
    if (params_.observe_ticks > 0) {
        --ticks_left_;
    }
    // 早失败：超阈立即判坏，不等窗满（§3.2「超阈值 → 换回旧表快照」）。
    if (window_faults() > params_.fault_threshold) {
        finished_ = true;
        return;
    }
    if (ticks_left_ == 0) {
        finished_ = true;
    }
}

bool ModuleCanary::healthy() const {
    return finished_ && window_faults() <= params_.fault_threshold;
}

std::uint64_t ModuleCanary::window_faults() const {
    if (!begun_) {
        return 0;
    }
    return rt_.module_faults_since(module_, begin_tick_);
}

}  // namespace apollo::script
