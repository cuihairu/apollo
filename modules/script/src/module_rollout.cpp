// §3.2 Phase A 工程面投递编排实现。判定推进是纯状态机（不接触 Lua）；
// 真正的换表/回滚只经既有 ScriptRuntime 面（commit_module / rollback_module）。

#include "apollo/script/module_rollout.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace apollo::script {

ModuleRollout::ModuleRollout(std::vector<RolloutTarget> targets,
                             ScriptConfig preparer_config)
    : preparer_config_(std::move(preparer_config)),
      entries_(std::make_move_iterator(targets.begin()),
               std::make_move_iterator(targets.end())) {}

bool ModuleRollout::topological_order_(const std::vector<RolloutModule>& modules,
                                       std::vector<std::size_t>& out) {
    out.clear();
    out.reserve(modules.size());
    const std::size_t n = modules.size();
    std::map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < n; ++i) {
        index[modules[i].name] = i;
    }
    // 入度 = 集内依赖数（集外依赖不参与排序——由预备/投递面的依赖闸拦截）。
    std::vector<std::size_t> indeg(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        for (const auto& dep : modules[i].deps) {
            if (index.count(dep) != 0) {
                ++indeg[i];
            }
        }
    }
    std::vector<bool> placed(n, false);
    while (out.size() < n) {
        bool progressed = false;
        for (std::size_t i = 0; i < n && !progressed; ++i) {
            if (placed[i] || indeg[i] != 0) {
                continue;
            }
            placed[i] = true;
            out.push_back(i);
            for (std::size_t j = 0; j < n; ++j) {
                if (placed[j]) {
                    continue;
                }
                for (const auto& dep : modules[j].deps) {
                    if (dep == modules[i].name) {
                        --indeg[j];
                    }
                }
            }
            progressed = true;
        }
        if (!progressed) {
            return false;  // 依赖环
        }
    }
    return true;
}

bool ModuleRollout::prepare_set_(const std::vector<RolloutModule>& modules) {
    prepared_.clear();  // 一轮一份交接面（预备沙盒保留，产物不跨轮累积）
    std::vector<std::size_t> order;
    if (!topological_order_(modules, order)) {
        errors_.push_back("cyclic dependency in module set");
        return false;
    }
    if (!preparer_) {
        preparer_ = std::make_unique<ScriptRuntime>(preparer_config_);
    }
    for (const std::size_t i : order) {
        const auto& m = modules[i];
        const ModuleSpec spec{m.name, m.version, m.deps};
        PreparedModule p = preparer_->prepare_module(spec, m.source);
        if (!p.ok()) {
            errors_.push_back("prepare " + m.name + ": " + p.error);
            return false;
        }
        prepared_.push_back(std::move(p));
    }
    return true;
}

bool ModuleRollout::prepare(std::vector<RolloutModule> modules,
                            const std::vector<std::string>& canary_keys,
                            CanaryParams params) {
    auto fail = [this](const std::string& why) {
        errors_.push_back(why);
        state_ = RolloutState::kFailed;
        return false;
    };
    // 在途不允许重开（预备产物与目标面状态都按「一轮一次」推进）。
    if (state_ != RolloutState::kIdle && state_ != RolloutState::kFailed &&
        state_ != RolloutState::kPromoted &&
        state_ != RolloutState::kRolledBack) {
        return fail("rollout in flight (state " +
                    std::to_string(static_cast<int>(state_)) + ")");
    }
    if (modules.empty()) {
        return fail("empty module set");
    }
    if (entries_.empty()) {
        return fail("empty target set");
    }
    for (const auto& e : entries_) {
        if (e.target.rt == nullptr) {
            return fail(e.target.key + ": null runtime");
        }
    }
    if (canary_keys.empty()) {
        return fail("empty canary set");
    }
    for (const auto& key : canary_keys) {
        if (find_(key) == nullptr) {
            return fail("unknown canary target: " + key);
        }
    }
    for (std::size_t i = 0; i < modules.size(); ++i) {
        for (std::size_t j = i + 1; j < modules.size(); ++j) {
            if (modules[i].name == modules[j].name) {
                return fail("duplicate module in patch set: " + modules[i].name);
            }
        }
    }
    if (!prepare_set_(modules)) {
        return fail("prepare failed");
    }
    // 轮间重置编排态（预备沙盒与产物保留——已备依赖跨轮可见，同预备面累积
    // 纪律；prepared_ 即本轮交接面）。
    for (auto& e : entries_) {
        e.canary = std::find(canary_keys.begin(), canary_keys.end(),
                             e.target.key) != canary_keys.end();
        e.commit_attempted = false;
        e.committed = false;
        e.rollback_attempted = false;
        e.canaries.clear();
        e.window_faults = 0;
        e.errors.clear();
    }
    params_ = params;
    state_ = RolloutState::kCanaryPending;
    return true;
}

ModuleRollout::Entry* ModuleRollout::find_(const std::string& key) {
    for (auto& e : entries_) {
        if (e.target.key == key) {
            return &e;
        }
    }
    return nullptr;
}

void ModuleRollout::begin_canaries_() {
    for (auto& e : entries_) {
        if (!e.canary || !e.committed) {
            continue;
        }
        e.canaries.clear();
        for (const auto& p : prepared_) {
            e.canaries.emplace_back(*e.target.rt, p.name, params_);
        }
        for (auto& c : e.canaries) {
            c.begin();
        }
    }
}

void ModuleRollout::evaluate_() {
    bool any_unhealthy = false;
    bool all_done = true;
    for (const auto& e : entries_) {
        if (!e.canary || !e.committed) {
            continue;
        }
        for (const auto& c : e.canaries) {
            if (c.finished() && !c.healthy()) {
                any_unhealthy = true;
            }
            if (!c.finished()) {
                all_done = false;
            }
        }
    }
    if (any_unhealthy) {
        // 任一灰度窗判坏 = 早失败（不等其余窗满）→ 转回滚已提交的灰度目标。
        state_ = RolloutState::kRollingBack;
        return;
    }
    if (all_done) {
        bool has_promote = false;
        for (const auto& e : entries_) {
            if (!e.canary && !e.commit_attempted) {
                has_promote = true;
                break;
            }
        }
        state_ = has_promote ? RolloutState::kPromoting : RolloutState::kPromoted;
    }
}

bool ModuleRollout::promote_all_attempted_() const {
    for (const auto& e : entries_) {
        if (!e.canary && !e.commit_attempted) {
            return false;
        }
    }
    return true;
}

bool ModuleRollout::rollback_all_attempted_() const {
    for (const auto& e : entries_) {
        if (e.canary && e.commit_attempted && !e.rollback_attempted) {
            return false;
        }
    }
    return true;
}

void ModuleRollout::commit_target(const std::string& key) {
    Entry* e = find_(key);
    if (e == nullptr) {
        errors_.push_back(key + ": unknown target");
        return;
    }
    if (state_ == RolloutState::kCanaryPending) {
        if (!e->canary || e->commit_attempted) {
            return;  // 非灰度目标 / 已投递：空转
        }
        e->commit_attempted = true;
        bool all = true;
        for (const auto& p : prepared_) {
            if (!e->target.rt->commit_module(p)) {
                e->errors.push_back("commit failed: " + p.name);
                all = false;
                break;  // 中途失败即止（后续模块多半依赖失败者）
            }
        }
        e->committed = all;
        if (!all) {
            // 灰度面装载失败 = 判坏（冒烟/装载闸同族纪律）。
            state_ = RolloutState::kRollingBack;
            return;
        }
        bool rest = false;
        for (const auto& o : entries_) {
            if (o.canary && !o.commit_attempted) {
                rest = true;
                break;
            }
        }
        if (!rest) {
            // 灰度面全部提交完成 → 观察窗 T0（各目标窗起点 = 各自 runtime tick）。
            begin_canaries_();
            state_ = RolloutState::kObserving;
        }
        return;
    }
    if (state_ == RolloutState::kPromoting) {
        if (e->canary || e->commit_attempted) {
            return;  // 非推广目标 / 已投递：空转
        }
        e->commit_attempted = true;
        bool all = true;
        for (const auto& p : prepared_) {
            if (!e->target.rt->commit_module(p)) {
                e->errors.push_back("commit failed: " + p.name);
                all = false;
                break;
            }
        }
        e->committed = all;
        if (promote_all_attempted_()) {
            // 判好结论不改：推广失败是 per-target 错误（目标停留在补丁前版本）。
            state_ = RolloutState::kPromoted;
        }
    }
}

void ModuleRollout::observe_target(const std::string& key) {
    if (state_ != RolloutState::kObserving) {
        return;  // 非观察相位：空转
    }
    Entry* e = find_(key);
    if (e == nullptr || !e->canary || !e->committed) {
        return;  // 未知 / 非灰度 / 未投递目标：空转
    }
    for (auto& c : e->canaries) {
        c.tick();
    }
    e->window_faults = 0;
    for (const auto& c : e->canaries) {
        e->window_faults += c.window_faults();
    }
    evaluate_();
}

void ModuleRollout::rollback_target(const std::string& key) {
    if (state_ != RolloutState::kRollingBack) {
        return;  // 非回滚相位：空转
    }
    Entry* e = find_(key);
    if (e == nullptr || !e->canary || !e->commit_attempted ||
        e->rollback_attempted) {
        return;  // 未知 / 非灰度 / 未投递 / 已回滚：空转
    }
    e->rollback_attempted = true;
    // 逆拓扑序回滚（先撤依赖方）。无冷冻区 = 拒回滚（新装模块无从回退，
    // 既有面语义），按 per-target 错误记录。
    for (auto it = prepared_.rbegin(); it != prepared_.rend(); ++it) {
        if (!e->target.rt->rollback_module(it->name)) {
            e->errors.push_back("rollback failed: " + it->name);
        }
    }
    if (rollback_all_attempted_()) {
        state_ = RolloutState::kRolledBack;
    }
}

std::vector<std::string> ModuleRollout::pending_canary() const {
    std::vector<std::string> out;
    if (state_ != RolloutState::kCanaryPending) {
        return out;
    }
    for (const auto& e : entries_) {
        if (e.canary && !e.commit_attempted) {
            out.push_back(e.target.key);
        }
    }
    return out;
}

std::vector<std::string> ModuleRollout::pending_promote() const {
    std::vector<std::string> out;
    if (state_ != RolloutState::kPromoting) {
        return out;
    }
    for (const auto& e : entries_) {
        if (!e.canary && !e.commit_attempted) {
            out.push_back(e.target.key);
        }
    }
    return out;
}

std::vector<std::string> ModuleRollout::pending_rollback() const {
    std::vector<std::string> out;
    if (state_ != RolloutState::kRollingBack) {
        return out;
    }
    for (const auto& e : entries_) {
        if (e.canary && e.commit_attempted && !e.rollback_attempted) {
            out.push_back(e.target.key);
        }
    }
    return out;
}

std::vector<TargetStatus> ModuleRollout::statuses() const {
    std::vector<TargetStatus> out;
    out.reserve(entries_.size());
    for (const auto& e : entries_) {
        TargetStatus s;
        s.key = e.target.key;
        s.canary = e.canary;
        s.committed = e.committed;
        s.rolled_back = e.rollback_attempted;
        s.window_faults = e.window_faults;
        s.errors = e.errors;
        out.push_back(std::move(s));
    }
    return out;
}

}  // namespace apollo::script
