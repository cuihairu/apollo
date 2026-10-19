#include "world_app/world_app.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/script/module_rollout.hpp"
#include "apollo/script/script_runtime.hpp"
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <vector>

namespace world_app {

namespace {
// 库面日志出口：cat=world_app（§5.1 cat= 检索键；主程已接文件面）
apollo::core::log::Logger& worldLog() {
    return *apollo::core::log::global_log_manager().createLogger("world_app");
}

// 逗号分隔依赖名 → 列表（952 编排请求；空段跳过，全空 = 无依赖）。
std::vector<std::string> split_dep_list(const std::string& text) {
    std::vector<std::string> out;
    std::string token;
    auto flush = [&] {
        if (!token.empty()) {
            out.push_back(std::move(token));
        }
        token.clear();
    };
    for (const char c : text) {
        if (c == ',') {
            flush();
        } else if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            token.push_back(c);
        }
    }
    flush();
    return out;
}

// 错误行拼合（953 detail 多行面）。
std::string join_lines(const std::vector<std::string>& lines) {
    std::string out;
    for (const auto& l : lines) {
        if (!out.empty()) {
            out.push_back(';');
            out.push_back(' ');
        }
        out += l;
    }
    return out;
}

// M1 eval 在途关联账上限（host 级背压——防丢最旧语义下的无界挂账；
// 对齐 Scene 队列深度的量级放大，超过即时回执拒收）。
constexpr std::size_t kM1EvalBacklogMax = 64;
} // namespace

//==============================================================================
// WorldApp 实现（manager 角色壳：M1 传输端点 + 管理编排；执行内核 CellApp）
//==============================================================================

WorldApp::WorldApp(const WorldAppConfig& config)
    : config_(config)
    , cell_(std::make_unique<CellApp>(config)) {
    // 注入缝装配（start 前即位）：出口缝 + 拍首传输泵 + 拍后管理推进。
    // 全部钩子在内核线程内执行——管理受理点（eval/编排/排空）与驱动同线程，
    // 单写者纪律不破（ADR-015(a)）。
    cell_->set_egress(&egressAdapter_);
    cell_->set_pre_tick([this](std::uint64_t now_ms) {
        if (m1Link_) {
            m1Link_->pump(now_ms);
        }
    });
    cell_->set_post_tick([this](double) {
        // World 排空推进（P4 更新排空批；driveSquadGameplay 后拍——本拍结算
        // 收尾先推进，归零判定用新值）
        driveWorldDrain();
        // §7.1 eval 回执收账（tick 内固定调试点已执行在途 eval）
        drainDebugEvalResults();
        // 952 编排推进（P4 仅代码更新批）：在装模块 onTick + 观察窗/推广/
        // 回滚队列（owning 线程纪律）
        driveModuleRollout();
    });
}

WorldApp::~WorldApp() {
    stop();
}

void WorldApp::start() {
    if (isRunning()) return;

    // net M1 接收端（2b-ii，2b-iii 起 WorldApp 唯一受理入口——legacy RepSocket
    // 已退役）：InterServerLink passive 端点装配（先建对象不监听——内核
    // pre-tick 泵引用它在位；bind/listen 在闸开之后）
    m1Link_ = makeM1LinkServer(
        M1LinkConfig{config_.host, config_.m1Port, config_.m1RouteJson,
                     config_.m1AuthorityEpoch},
        [this](IntentEnvelope intent) { cell_->onIntent(std::move(intent)); },
        [this](const std::string& peer, const DebugEvalRequest& request) {
            onM1DebugEval(peer, request);
        },
        [this](const std::string& peer, const ModuleRolloutRequest& request) {
            onM1ModuleRollout(peer, request);
        },
        [this](const std::string& peer, const WorldDrainRequest& request) {
            onM1WorldDrain(peer, request);
        },
        [this](const std::string& peer, const CellRegisterRequest& request) {
            onCellRegister(peer, request);
        });
    egressAdapter_.link = m1Link_.get();

    // 执行内核启动：闸开（受理面就绪）+ 内核循环线程起
    try {
        cell_->start();
    } catch (...) {
        egressAdapter_.link = nullptr;
        m1Link_.reset();
        throw;
    }

    // 监听后置（开闸先于监听不变式）：bind 失败同契约显形，回滚内核再抛
    if (!m1Link_->start()) {
        m1Link_->stop();
        egressAdapter_.link = nullptr;
        m1Link_.reset();
        cell_->stop();
        throw std::runtime_error("failed to start WorldApp M1 link endpoint (route table load or bind failed)");
    }

    worldLog().info("WorldApp M1 link listening on " + config_.host + ":"
                + std::to_string(m1Link_->listenPort()));
}

void WorldApp::stop() {
    // 停机 G-3 镜像顺序（ADR-016）：内核停（关闸 → 队列尾扫 → join → 拨号
    // 收口 → worldHost stop；尾扫的延时应答经出口缝时 M1 端点仍在）→ M1
    // 端点停
    if (cell_) {
        cell_->stop();
    }

    if (m1Link_) {
        m1Link_->stop();
        m1Link_.reset();
    }
    egressAdapter_.link = nullptr;
}

void WorldApp::onM1ModuleRollout(const std::string& peer,
                                 const ModuleRolloutRequest& request) {
    // 952 受理（P4 仅代码更新批）：本回调在内核线程泵上下文 = ModuleRollout
    // 契约线程（owning 线程直驱，无 tick 排队）。request_id = 0 保留；受理
    // 点同步回 953（观察窗推进随拍自驱，控制面按 status 轮询聚合）。
    if (request.request_id == 0) {
        return;
    }
    auto* script = cell_ ? cell_->script_runtime() : nullptr;
    auto reply = [&](bool ok, std::int32_t state, std::uint64_t faults,
                     std::uint64_t version, const std::string& detail) {
        if (m1Link_) {
            m1Link_->sendModuleRolloutReply(peer, request.request_id, request.op,
                                            ok, state, faults, version, detail);
        }
    };
    if (script == nullptr) {
        reply(false, -1, 0, 0, "script runtime not ready");
        return;
    }
    const auto state_of = [&]() -> std::int32_t {
        return static_cast<std::int32_t>(moduleRollout_->state());
    };
    const auto detail_of = [&]() -> std::string {
        const auto errs = moduleRollout_->errors();
        return errs.empty() ? std::string("no errors") : join_lines(errs);
    };
    switch (request.op) {
        case 1: {  // prepare：拓扑预备（独立预备沙盒），成功 → kCanaryPending
            if (moduleRollout_ == nullptr) {
                // 单目标编排器（懒建，仅 prepare 造对象）：target = 内核
                // runtime，canary = 自身（跨 World 灰度 = 控制面按世界分相位
                // 投递，世界内不再细分）。commit/rollback/status 不复活——
                // 无在途轮次即显式拒（半开状态不可默默重建）。
                moduleRollout_ = std::make_unique<apollo::script::ModuleRollout>(
                    std::vector<apollo::script::RolloutTarget>{{"world", script}});
            }
            std::vector<std::string> deps = split_dep_list(request.deps);
            apollo::script::RolloutModule mod;
            mod.name = request.name;
            mod.version = request.version;
            mod.deps = std::move(deps);
            mod.source = request.source;
            const bool ok = moduleRollout_->prepare(
                {mod}, {"world"},
                apollo::script::CanaryParams{request.observe_ticks,
                                             request.fault_threshold});
            if (ok) {
                lastRolloutName_ = request.name;
                worldLog().info("module rollout prepared: " + request.name + " v"
                                + std::to_string(request.version));
            }
            reply(ok, state_of(), 0, script->module_version(request.name),
                  detail_of());
            return;
        }
        case 2: {  // commit：kCanaryPending → 提交本目标（单目标即入观察窗）
            if (moduleRollout_ == nullptr) {
                reply(false, 0, 0, script->module_version(request.name),
                      "no rollout in flight");
                return;
            }
            if (moduleRollout_->state() ==
                apollo::script::RolloutState::kCanaryPending) {
                moduleRollout_->commit_target("world");
            }
            const auto st = moduleRollout_->state();
            const bool ok = st == apollo::script::RolloutState::kObserving;
            if (ok) {
                // 受理名登记（onTick 驱动面；commit 即激活——驱动只对 active
                // 版本，prepare 期不投——重名不重复登记）。
                if (std::find(rolloutModuleNames_.begin(), rolloutModuleNames_.end(),
                              request.name) == rolloutModuleNames_.end()) {
                    rolloutModuleNames_.push_back(request.name);
                }
                worldLog().info("module rollout committed: " + request.name + " v"
                                + std::to_string(request.version));
            }
            reply(ok, state_of(), 0, script->module_version(request.name),
                  detail_of());
            return;
        }
        case 3: {  // rollback / abort：kRollingBack = 回滚已提交集；
                   // kCanaryPending = 灰度未提交 → 丢弃预备轮复位（编排器已随
                   // 早失败即止，活跃模块表未被触碰——下轮 prepare 从零重来）
            if (moduleRollout_ == nullptr) {
                // 无在途轮次：幂等空操作（编排器清尾可能追到已复位的世界）
                reply(true, 0, 0, script->module_version(request.name),
                      "no rollout in flight");
                return;
            }
            if (moduleRollout_->state() ==
                apollo::script::RolloutState::kCanaryPending) {
                moduleRollout_.reset();
                worldLog().info("module rollout aborted: " + request.name);
                reply(true, 0, 0, script->module_version(request.name),
                      "aborted: nothing committed");
                return;
            }
            moduleRollout_->rollback_target("world");
            const bool ok = moduleRollout_->state() ==
                            apollo::script::RolloutState::kRolledBack;
            if (ok) {
                worldLog().info("module rollout rolled back: " + request.name);
            }
            reply(ok, state_of(), 0, script->module_version(request.name),
                  ok ? std::string("rolled back") : detail_of());
            return;
        }
        case 4: {  // status：控制面聚合观察（state/faults/version）
            std::uint64_t faults = 0;
            const auto st = moduleRollout_ ? moduleRollout_->statuses()
                                            : std::vector<apollo::script::TargetStatus>{};
            if (!st.empty()) {
                faults = st[0].window_faults;
            }
            reply(true, moduleRollout_ ? state_of() : 0, faults,
                  script->module_version(request.name),
                  moduleRollout_ ? detail_of() : std::string("no rollout"));
            return;
        }
        default:
            reply(false, state_of(), 0, script->module_version(request.name),
                  "unknown op " + std::to_string(request.op));
            return;
    }
}

void WorldApp::driveModuleRollout() {
    // post-tick 拍（drainDebugEvalResults 同址）：先驱在装模块 onTick（fault
    // 归因源——canary 窗计账靠模块函数真被调用，runtime 按 active 版本解
    // 析），再推观察窗/推广/回滚队列（frame 序 = rollout 单测同型）。
    auto* script = cell_ ? cell_->script_runtime() : nullptr;
    if (script == nullptr) {
        return;
    }
    for (const auto& name : rolloutModuleNames_) {
        (void)script->call_module(name, "onTick");
    }
    if (moduleRollout_ == nullptr) {
        return;
    }
    const auto st = moduleRollout_->state();
    if (st == apollo::script::RolloutState::kObserving) {
        moduleRollout_->observe_target("world");
    } else if (st == apollo::script::RolloutState::kPromoting) {
        for (const auto& k : moduleRollout_->pending_promote()) {
            moduleRollout_->commit_target(k);
        }
    } else if (st == apollo::script::RolloutState::kRollingBack) {
        for (const auto& k : moduleRollout_->pending_rollback()) {
            moduleRollout_->rollback_target(k);
        }
    }
    const auto after = moduleRollout_->state();
    if (st != after && (after == apollo::script::RolloutState::kPromoted ||
                        after == apollo::script::RolloutState::kRolledBack)) {
        // 自驱跃迁（观察窗判好/判坏）——控制面只拉 status，日志是唯一落证面
        //（drill 断言 + 运维排障）。
        worldLog().info(std::string("module rollout ")
                         + (after == apollo::script::RolloutState::kPromoted ? "promoted"
                                                                              : "rolled back")
                         + ": " + lastRolloutName_);
    }
}

void WorldApp::onM1WorldDrain(const std::string& peer, const WorldDrainRequest& request) {
    // 956 受理（P4 更新排空批）：内核线程泵上下文 = launch/玩法推进同线程
    // ——排空状态机单线程独占无锁，受理点同步回 957。request_id = 0 保留。
    if (request.request_id == 0) {
        return;
    }
    auto* squadInstanceService = cell_ ? cell_->squadInstanceService() : nullptr;
    const auto now_ms = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
    const auto active_of = [&]() -> std::uint64_t {
        return squadInstanceService ? squadInstanceService->active_count() : 0;
    };
    const auto detail_of = [&]() -> std::string {
        switch (worldDrainState_) {
            case kWorldDrainDrained:
                return worldDrainDetail_;
            case kWorldDrainDraining:
                return "draining: " + std::to_string(active_of())
                       + " active instances settling";
            default:
                return "serving";
        }
    };
    auto reply = [&](bool ok, const std::string& detail) {
        if (m1Link_) {
            m1Link_->sendWorldDrainReply(peer, request.request_id, request.op, ok,
                                         worldDrainState_, active_of(), detail);
        }
    };
    if (squadInstanceService == nullptr) {
        reply(false, "squad service not ready");
        return;
    }
    switch (request.op) {
        case 1: {  // begin：排空闸闭合 + 期限登记（重复 begin 幂等，期限只紧不松）
            if (worldDrainState_ == kWorldDrainServing) {
                squadInstanceService->set_drain(true);
                worldDrainState_ = kWorldDrainDraining;
                worldDrainDeadlineMs_ =
                    request.deadline_s > 0
                        ? now_ms + static_cast<std::uint64_t>(request.deadline_s) * 1000U
                        : 0;
                worldLog().info("world drain begun");
            } else if (worldDrainState_ == kWorldDrainDraining && request.deadline_s > 0) {
                // 已在排空：期限取更紧者（0 = 无期限不覆盖既有期限）
                const auto deadline =
                    now_ms + static_cast<std::uint64_t>(request.deadline_s) * 1000U;
                if (worldDrainDeadlineMs_ == 0 || deadline < worldDrainDeadlineMs_) {
                    worldDrainDeadlineMs_ = deadline;
                }
            }
            reply(true, detail_of());
            return;
        }
        case 2: {  // status：控制面聚合观察（state/active/detail）
            reply(true, detail_of());
            return;
        }
        default:
            reply(false, "unknown op " + std::to_string(request.op));
            return;
    }
}

void WorldApp::driveWorldDrain() {
    // post-tick 拍（本拍结算收尾已推进）：归零即落 drained；期限到未清强停
    // 剩余副本——Create..Running 走 abort（取消线，不发成功奖励），已入结算
    // 走 dismiss（收尾推进），两原语覆盖全部活动相位。drained 终态不反闸
    //（恢复 = 进程替换）。
    auto* squadInstanceService = cell_ ? cell_->squadInstanceService() : nullptr;
    if (worldDrainState_ != kWorldDrainDraining || squadInstanceService == nullptr) {
        return;
    }
    auto active = squadInstanceService->active_count();
    if (active == 0) {
        worldDrainState_ = kWorldDrainDrained;
        worldDrainDetail_ = "drained: all instances settled";
        worldLog().info("world " + worldDrainDetail_);
        return;
    }
    if (worldDrainDeadlineMs_ == 0) {
        return;  // 无期限：既有副本服务至自然结算
    }
    const auto now_ms = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
    if (now_ms < worldDrainDeadlineMs_) {
        return;
    }
    // 强停：快照遍历（abort/dismiss 同步释放绑定，快照期内安全）
    std::size_t force_stopped = 0;
    for (const auto& binding : squadInstanceService->activeBindings()) {
        if (squadInstanceService->abort(binding.party_id)
            || squadInstanceService->dismiss(binding.party_id)) {
            ++force_stopped;
        }
    }
    active = squadInstanceService->active_count();
    if (active == 0) {
        worldDrainState_ = kWorldDrainDrained;
        worldDrainDetail_ = "drained: deadline reached, " + std::to_string(force_stopped)
                            + " instances force-aborted";
        worldLog().info("world " + worldDrainDetail_);
    } else {
        worldLog().warning("world drain force-stop left " + std::to_string(active)
                           + " instances; retrying next tick");
    }
}

void WorldApp::onCellRegister(const std::string& peer,
                              const CellRegisterRequest& request) {
    // 960 受理（L101 注册首切片；本回调在内核线程泵上下文——注册表单线程
    // 独占无锁，受理点同步回 961）。request_id = 0 保留（无回执无登记）。
    // 认证凭据面按 world-cell-management 差距表另行挂账（裸帧受理同
    // 944/952/956 惯例）。
    if (request.request_id == 0) {
        return;
    }
    const auto reply = [&](bool accepted, std::int32_t code, const std::string& detail) {
        if (m1Link_) {
            m1Link_->sendCellRegisterReply(peer, request.request_id, accepted, code,
                                           config_.m1AuthorityEpoch, request.cell_id,
                                           detail);
        }
    };
    if (request.cell_id == 0 || request.start_epoch == 0) {
        // 差距 ①：身份/代次非法显式驳回——逻辑 ID 与启动代次是登记前提
        //（PID/IP 不当身份，0 号即缺位）
        reply(false, kCellRegisterRejectedInvalid, "invalid cell_id or start_epoch");
        return;
    }
    const auto now_ms = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
    auto it = cellRegistrations_.find(request.cell_id);
    if (it == cellRegistrations_.end()) {
        CellRegistration reg;
        reg.cell_id = request.cell_id;
        reg.start_epoch = request.start_epoch;
        reg.host = request.host;
        reg.port = request.port;
        reg.capabilities = request.capabilities;
        reg.load = request.load;
        reg.drain_state = request.drain_state;
        reg.ready = request.ready;
        reg.registered_at_ms = now_ms;
        reg.peer = peer;
        cellRegistrations_.emplace(request.cell_id, std::move(reg));
        worldLog().info("cell registered: id=" + std::to_string(request.cell_id)
                        + " epoch=" + std::to_string(request.start_epoch)
                        + " peer=" + peer);
        reply(true, kCellRegisterAccepted, "registered");
        return;
    }
    if (request.start_epoch < it->second.start_epoch) {
        // §4.4：旧代次不可覆盖新登记（重启获新代次——旧连接回拨拒绝，
        // 登记保持新代次；旧执行端隔离随 L101 后续切片）
        reply(false, kCellRegisterRejectedStaleEpoch,
              "stale epoch: registered=" + std::to_string(it->second.start_epoch));
        return;
    }
    const bool idempotent = request.start_epoch == it->second.start_epoch;
    // 同代次幂等刷新 / 新代次替换：自报面整体刷新（peer 也换——重连 =
    // 同名或新名链路，回执按名寻路）
    it->second.start_epoch = request.start_epoch;
    it->second.host = request.host;
    it->second.port = request.port;
    it->second.capabilities = request.capabilities;
    it->second.load = request.load;
    it->second.drain_state = request.drain_state;
    it->second.ready = request.ready;
    it->second.registered_at_ms = now_ms;
    it->second.peer = peer;
    worldLog().info(std::string("cell re-") + (idempotent ? "registered: " : "placed (new epoch): ")
                    + "id=" + std::to_string(request.cell_id)
                    + " epoch=" + std::to_string(request.start_epoch)
                    + " peer=" + peer);
    reply(true, idempotent ? kCellRegisterAcceptedIdempotent : kCellRegisterAccepted,
          idempotent ? "re-registered (same epoch, refreshed)"
                     : "replaced (new epoch)");
}

const WorldApp::CellRegistration* WorldApp::find_cell_registration(
    std::uint64_t cell_id) const {
    // 泵上下文只读观测面（注册表单写者 = M1 泵上下文受理点）
    const auto it = cellRegistrations_.find(cell_id);
    return it != cellRegistrations_.end() ? &it->second : nullptr;
}

void WorldApp::onM1DebugEval(const std::string& peer, const DebugEvalRequest& request) {
    // 944 受理（§7.1 attach transport；本回调在内核线程泵上下文）：语法面已
    // 在 M1LinkServer 解码，此处只做定位与背压判定——失败即时回 945（不进
    // tick 窗口），成功挂账待 Scene tick 执行后回传。
    if (request.eval_id == 0) {
        return;  // 0 保留（admin 关联键缺位，拒收面都不给）
    }
    if (evalReplyPeers_.size() >= kM1EvalBacklogMax) {
        sendM1EvalReply(peer, request.eval_id, false, "", "eval backlog full");
        return;
    }
    const std::uint64_t scene_id =
        request.scene_id != 0 ? request.scene_id : cell_->default_scene_id();
    auto* scene = find_scene(scene_id);
    if (scene == nullptr || !scene->script_attached()) {
        sendM1EvalReply(peer, request.eval_id, false, "", "scene not attached");
        return;
    }
    if (!scene->enqueue_debug_eval(request.eval_id, request.source)) {
        // 队列满（有界 FIFO）或未挂——即时回执，admin 超时面裁决重发
        sendM1EvalReply(peer, request.eval_id, false, "", "eval queue full");
        return;
    }
    evalReplyPeers_[request.eval_id] = peer;
    scenePendingEvals_[scene_id].insert(request.eval_id);
}

void WorldApp::sendM1EvalReply(const std::string& peer, std::uint64_t eval_id, bool ok,
                                 const std::string& result, const std::string& error) {
    if (m1Link_) {
        m1Link_->sendEvalResult(peer, eval_id, ok, result, error);
    }
}

void WorldApp::drainDebugEvalResults() {
    // post-tick 收账：Scene::tick 的 §7.1 固定调试点已执行在途 eval，此处取
    // 回传结果按 eval_id 回 945。仅在途才 drain（直接 enqueue 面不挤占）；
    // 同拍丢最旧的结果无回执（有界 FIFO 语义——admin 超时面重发裁决）。
    if (scenePendingEvals_.empty()) {
        return;
    }
    for (auto it = scenePendingEvals_.begin(); it != scenePendingEvals_.end();) {
        auto* scene = find_scene(it->first);
        if (scene == nullptr) {
            // scene 已不存在（销毁/停机路径）：挂账清空，无回执（尽力面）
            for (auto eval_id : it->second) {
                evalReplyPeers_.erase(eval_id);
            }
            it = scenePendingEvals_.erase(it);
            continue;
        }
        for (auto& r : scene->drain_debug_eval_results()) {
            const auto peer = evalReplyPeers_.find(r.id);
            if (peer == evalReplyPeers_.end()) {
                continue;  // 非本面投递的结果（直接 enqueue 面）——留过即弃
            }
            sendM1EvalReply(peer->second, r.id, r.ok, r.ok ? r.result : "",
                            r.ok ? "" : r.result);
            evalReplyPeers_.erase(peer);
            it->second.erase(r.id);
        }
        if (it->second.empty()) {
            it = scenePendingEvals_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace world_app
