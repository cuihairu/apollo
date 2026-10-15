#pragma once

// §3.2 Phase C 灰度观察期原语（canary）：换表提交后在观察窗内数该模块的
// fault，超阈即提前判坏。promote（全量推广）与 rollback（换回旧表）由控制面
// 按 verdict 驱动既有面（commit_module / rollback_module）——本面不持有第二套
// 换表路径，也不做线程调度（tick() 由控制面在 owning 线程逐拍调用，同 §7.1
// tick 窗口纪律）。
//
// fault 归因：ScriptRuntime 内 per-module fault 流水（call_module 的
// breaker_on_fault 是模块级 fault 的唯一归因点）；观察窗以 runtime tick 计
// （宿主 set_tick 喂入），begin 前的既有 fault 不入窗。

#include <cstdint>
#include <string>

namespace apollo::script {

class ScriptRuntime;

struct CanaryParams {
    std::uint64_t observe_ticks = 100;  // 观察窗长度（tick 计）
    std::uint64_t fault_threshold = 0;  // 窗内该模块 fault 上限；超 = unhealthy
};

class ModuleCanary {
public:
    ModuleCanary(ScriptRuntime& rt, std::string module, CanaryParams params);

    // 提交（prepare+commit 或 swap）后调用：以 runtime 当前 tick 为窗起点。
    void begin();

    // 控制面每 tick 调用（owning 线程）。超阈提前结束观察（早失败省窗）。
    void tick();

    // 窗满或超阈即 true；未 begin / 窗未满为 false。
    [[nodiscard]] bool finished() const { return finished_; }

    // finished 后可查：窗内 fault ≤ 阈值（begin 前 fault 不计）。
    [[nodiscard]] bool healthy() const;

    // 窗内（[begin_tick, now]）该模块归因 fault 次数。
    [[nodiscard]] std::uint64_t window_faults() const;

    [[nodiscard]] const std::string& module() const { return module_; }

private:
    ScriptRuntime& rt_;
    std::string module_;
    CanaryParams params_;
    std::uint64_t begin_tick_ = 0;
    std::uint64_t ticks_left_ = 0;
    bool begun_ = false;
    bool finished_ = false;
};

}  // namespace apollo::script
