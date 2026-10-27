#include "world_app/world_app.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/script/module_rollout.hpp"
#include "apollo/script/script_runtime.hpp"
#include <algorithm>
#include <chrono>
// POSIX journal fsync（批A 决定收口）
#include <fcntl.h>
#include <fstream>
#include <limits>
#include <csignal>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <unistd.h>
#include <stdexcept>
#include <vector>

namespace world_app {

namespace {
// 库面日志出口：cat=world_app（§5.1 cat= 检索键；主程已接文件面）
apollo::core::log::Logger& worldLog() {
    return *apollo::core::log::global_log_manager().createLogger("world_app");
}

// 能力位覆盖判定（容量申报裁决）：cell 声明的 capabilities 须覆盖 Space
// 要求的 needs 全部位才有认领资格。needs=0 = 无要求——任何 cell 均
// 合格，含未声明（capabilities=0）的现状语义。
bool capability_fit(std::int32_t capabilities, std::int32_t needs) {
    return (capabilities & needs) == needs;
}

// 分配池成员判定（SpaceSlot 表口径，pool 复用两处：装载终态重建 + 观测）。
bool pool_has(const WorldAppConfig& config, std::uint64_t space_id) {
    for (const auto& slot : config.assignSpaces) {
        if (slot.space_id == space_id) {
            return true;
        }
    }
    return false;
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

class PersistentVolumeLock {
public:
    static bool acquire(const std::string& journal_path) {
        const std::string path = journal_path + ".lock";
        std::ifstream existing(path);
        if (existing) {
            std::string owner_line;
            std::getline(existing, owner_line);
            const auto sep = owner_line.find(':');
            const bool mine = owner_line.rfind(std::to_string(::getpid()) + ":", 0) == 0;
            if (sep != std::string::npos && pid_alive(owner_line.substr(0, sep))) {
                throw std::runtime_error("world journal volume is locked by live pid "
                                         + owner_line.substr(0, sep) + " at " + path);
            }
            if (sep == std::string::npos) {
                worldLog().warning(
                    "breaking malformed world journal volume lock at " + path);
            } else if (!pid_alive(owner_line.substr(0, sep))) {
                worldLog().warning(
                    "breaking stale world journal volume lock at " + path
                    + " stale_pid=" + owner_line.substr(0, sep));
            }
        }
        std::ofstream out(path, std::ios::trunc);
        if (!out) {
            throw std::runtime_error("failed to open world journal volume lock at "
                                     + path);
        }
        out << ::getpid() << ':' << started_at() << '\n';
        out.flush();
        if (!out) {
            throw std::runtime_error("failed to write world journal volume lock at "
                                     + path);
        }
        return true;
    }

private:
    static std::string started_at() {
        const auto now = std::chrono::system_clock::now();
        const auto seconds = std::chrono::system_clock::to_time_t(now);
        std::tm utc{};
        gmtime_r(&seconds, &utc);
        char buffer[32]{};
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
        return buffer;
    }

    static bool pid_alive(const std::string& pid_text) {
        char* end = nullptr;
        const long pid = std::strtol(pid_text.c_str(), &end, 10);
        return end != pid_text.c_str() && *end == '\0' && pid > 0
               && ::kill(static_cast<pid_t>(pid), 0) == 0;
    }

};
} // namespace

//==============================================================================
// WorldApp 实现（manager 角色壳：M1 传输端点 + 管理编排；执行内核 CellApp）
//==============================================================================

WorldApp::WorldApp(const WorldAppConfig& config)
    : config_(config)
    , cell_(std::make_unique<CellApp>(config)) {
    if (config_.bootAtMs == 0) {
        config_.bootAtMs = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());
    }
    // 注入缝装配（start 前即位）：出口缝 + 拍首传输泵 + 拍后管理推进。
    // 全部钩子在内核线程内执行——管理受理点（eval/编排/排空）与驱动同线程，
    // 单写者纪律不破（ADR-015(a)）。
    cell_->set_egress(&egressAdapter_);
    cell_->set_pre_tick([this](std::uint64_t now_ms) {
        if (m1Link_) {
            m1Link_->pump(now_ms);
        }
        if (managerPreTick_) {
            managerPreTick_(now_ms);  // 管理侧拨号面（L123 服务注册拨号）
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
        // cell 注册表过期清扫（960 心跳批）：last-heard 超窗摘除
        driveCellRegistry();
    });
}

WorldApp::~WorldApp() {
    stop();
}

void WorldApp::start() {
    if (isRunning()) return;

    if (!config_.worldJournalPath.empty()) {
        volumeLocked_ = PersistentVolumeLock::acquire(config_.worldJournalPath);
    }
    // 管理日志装载（L101 管理日志切片 → 批A journal v2）：闸开前——受理面
    // 未开，无并发写者（损坏行显式拒启，见 loadWorldJournal；B 行铸代次
    // 先记后开受理面——本调用先于 makeM1LinkServer 装配）
    loadWorldJournal();

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
        },
        [this](const std::string& peer, const CellRetireRequest& request) {
            onCellRetire(peer, request);
        },
        [this](const std::string& peer, const CellSpaceReport& report) {
            onCellSpaceReport(peer, report);
        });
    egressAdapter_.link = m1Link_.get();
    // 断连存疑标记接线（L101 断连存疑切片）：on_peer_down 在 M1 泵上下文
    //（pre-tick，内核线程）触发——注册表单写者上下文，直接改表。
    m1Link_->on_peer_down([this](const std::string& peer) { onPeerLinkDown(peer); });

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
    if (volumeLocked_ && !config_.worldJournalPath.empty()) {
        std::error_code ec;
        std::filesystem::remove(config_.worldJournalPath + ".lock", ec);
    }
    volumeLocked_ = false;
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
    // 代次裁决先于兼容裁决（code 3 先于 code 5）：旧代次复活凭 code 3 自冻
    // 结隔离（客户端语义）——若被 code 5 抢答，旧执行端失去自隔离信号
    if (it == cellRegistrations_.end()) {
        // 旧执行端隔离（L101 隔离切片）：代次水位——摘除忘却登记，但本进程
        // 曾受理的更高代次仍拒旧代次复活（旧端不能借摘除冒认首注册；同代次
        // = 同一化身断链回拨，放行走首注册。内存水位，持久墓碑随管理日志批）
        const auto wm = cellEpochWatermarks_.find(request.cell_id);
        if (wm != cellEpochWatermarks_.end() && request.start_epoch < wm->second) {
            reply(false, kCellRegisterRejectedStaleEpoch,
                  "stale epoch: watermark=" + std::to_string(wm->second));
            return;
        }
    } else if (request.start_epoch < it->second.start_epoch) {
        // §4.4：旧代次不可覆盖新登记（重启获新代次——旧连接回拨拒绝，
        // 登记保持新代次；旧执行端隔离随 L101 后续切片）
        reply(false, kCellRegisterRejectedStaleEpoch,
              "stale epoch: registered=" + std::to_string(it->second.start_epoch));
        return;
    }
    if (request.compat != contract_compat_projection()) {
        // 契约兼容裁决（960 版本/兼容切片，§4.3「World 验证所属关系、版本
        // 和身份」的版本面）：声明投影须与本进程同值才受理——未声明（0）
        // 即不可验证，混编落地的兼容范围白名单随协议更新批
        reply(false, kCellRegisterRejectedIncompatible,
              "incompatible contract compat: declared="
                  + std::to_string(request.compat) + " expected="
                  + std::to_string(contract_compat_projection()));
        return;
    }
    if (it == cellRegistrations_.end()) {
        CellRegistration reg;
        reg.cell_id = request.cell_id;
        reg.start_epoch = request.start_epoch;
        reg.host = request.host;
        reg.port = request.port;
        reg.capabilities = request.capabilities;
        reg.compat = request.compat;
        reg.load = request.load;
        reg.drain_state = request.drain_state;
        reg.ready = request.ready;
        reg.registered_at_ms = now_ms;
        reg.peer = peer;
        cellRegistrations_.emplace(request.cell_id, std::move(reg));
        recordEpochWatermark(request.cell_id, request.start_epoch);
        worldLog().info("cell registered: id=" + std::to_string(request.cell_id)
                        + " epoch=" + std::to_string(request.start_epoch)
                        + " peer=" + peer);
        reply(true, kCellRegisterAccepted, "registered");
        // 分配推送（962 分配面）：裁决随刚落表的自报面——ready/drain/load
        // 同拍生效，首注册即按资格认领池内未分配 Space
        pushSpaceAssignments(peer, cellRegistrations_.at(request.cell_id));
        return;
    }
    const bool idempotent = request.start_epoch == it->second.start_epoch;
    // 同代次幂等刷新 / 新代次替换：自报面整体刷新（peer 也换——重连 =
    // 同名或新名链路，回执按名寻路）
    it->second.start_epoch = request.start_epoch;
    it->second.host = request.host;
    it->second.port = request.port;
    it->second.capabilities = request.capabilities;
    it->second.compat = request.compat;
    it->second.load = request.load;
    it->second.drain_state = request.drain_state;
    it->second.ready = request.ready;
    it->second.registered_at_ms = now_ms;
    it->second.peer = peer;
    it->second.suspect = false;  // 断链复核归来（961 受理裁决通过）清存疑
    // 水位随受理推进 + 墓碑前进才落盘（recordEpochWatermark 统一判前进，
    // 这里不再直写 map——先写会骗过前进判定漏落盘）
    recordEpochWatermark(request.cell_id, request.start_epoch);
    worldLog().info(std::string("cell re-") + (idempotent ? "registered: " : "placed (new epoch): ")
                    + "id=" + std::to_string(request.cell_id)
                    + " epoch=" + std::to_string(request.start_epoch)
                    + " peer=" + peer);
    reply(true, idempotent ? kCellRegisterAcceptedIdempotent : kCellRegisterAccepted,
          idempotent ? "re-registered (same epoch, refreshed)"
                     : "replaced (new epoch)");
    // 分配推送（962 分配面；code 1/2 受理都推——断链重拨后分配不丢）
    pushSpaceAssignments(peer, it->second);
}

void WorldApp::onPeerLinkDown(const std::string& peer) {
    // 断连存疑标记（L101 断连存疑切片，on_down 泵上下文 = 注册表单写者
    // 线程）：peer 对上登记即置存疑——gap 表 §line71 断开后先标记，不立即
    // 另端重建权威（释放/重认领仍走既有过期窗裁决）。未知 peer（gateway/
    // admin 链路无登记）无操作；同名重连前旧标记不清除（清除只随 961
    // 复核受理，见 onCellRegister）。
    for (auto& entry : cellRegistrations_) {
        if (entry.second.peer == peer && !entry.second.suspect) {
            entry.second.suspect = true;
            worldLog().info("cell link down marked suspect: cell_id="
                            + std::to_string(entry.first) + " peer=" + peer);
        }
    }
}

std::size_t WorldApp::suspect_cell_count() const {
    std::size_t count = 0;
    for (const auto& entry : cellRegistrations_) {
        if (entry.second.suspect) {
            ++count;
        }
    }
    return count;
}

void WorldApp::pushSpaceAssignments(const std::string& peer,
                                    const CellRegistration& reg) {
    // 分配面（L101 首切片 + L108 回收切片 + L104 就绪/排空/负载裁决切片）：
    // 受理即推全池权威归属视图。未登记 Space 首认领（版本 1）；owner=0
    // （过期释放）重认领且版本 +1（认领转移面：离线旧主本地视图停在旧版本，
    // 须版本前进才收敛，防 ABA 重放；同主回拨也前进）；本 cell 已归属的
    // 刷新（版本不变幂等）；归属其它 cell 的推 ready=0 撤销标记（收帧方据
    // 此撤本地残留路由——离线期缺口经重注册收敛，见 L108 切片）。认领独占：
    // 不重分他人 Space（重平衡随 L108 拓扑批）。
    //
    // 认领资格（L104 负载裁决 + 容量申报裁决切片）：未就绪 / 自报排空中 /
    // World 单方缩容（retire）的 cell 不认领新 Space——进程存活不等于可分配，
    // 既有归属照常刷新（撤空不撤权）；声明能力未覆盖该 Space needs 的
    // cell 同样不认领——没有容量明确拒绝（world-cell-management §5），留池
    // 等合格候选，不落到不合格 cell 上凑数。已释放的 Space 走最小负载优先，
    // 存在负载严格更低且同样合格的候选则本 cell 退让（等负载面收敛后随下次
    // 受理/刷新认领），同负载让位给先受理者（不抖动重分）。
    if (config_.assignSpaces.empty()) {
        return;
    }
    if (journalMissingAtBoot_) {
        if (!missingJournal_) {
            missingJournal_ = true;
            worldLog().warning(
                "world journal missing evidence: cell=" + std::to_string(reg.cell_id)
                + " has no tombstone; all assignments frozen");
        }
        for (const auto& slot : config_.assignSpaces) {
            worldLog().warning(
                "space assignment blocked by missing journal evidence: space="
                + std::to_string(slot.space_id));
        }
        return;
    }
    const bool may_claim = reg.ready && reg.drain_state == 0 && !reg.retire;
    for (const auto& slot : config_.assignSpaces) {
        const std::uint64_t space_id = slot.space_id;
        if (missingJournal_) {
            worldLog().warning(
                "space assignment blocked by missing journal evidence: space="
                + std::to_string(space_id));
            continue;
        }
        // 冲突冻结（批B 967 裁决面）：冻结期不推新分配、不推刷新、不推撤销
        //（设计稿 §4——自报与权威表分歧未决前，任何一侧声明都可能固化错误
        // 归属；解冻随人工修复批挂账）。
        if (conflictedSpaces_.count(space_id) > 0) {
            continue;
        }
        auto it = spaceAssignments_.find(space_id);
        if (it == spaceAssignments_.end() || it->second.cell_id == 0) {
            // 未分配 / 已释放：无资格、无容量（能力未覆盖）或让位者不发 962
            //（无归属视图可推）
            if (!may_claim || !capability_fit(reg.capabilities, slot.needs) ||
                lower_load_peer(reg.cell_id, reg.load, slot.needs)) {
                continue;
            }
            const bool reclaim = it != spaceAssignments_.end();  // owner=0
            if (reclaim) {
                ++it->second.version;  // 重认领转移：版本前进（防 ABA）
            } else {
                SpaceAssignment a;
                a.space_id = space_id;
                a.cell_id = reg.cell_id;
                a.version = 1;
                it = spaceAssignments_.emplace(space_id, a).first;
            }
            it->second.cell_id = reg.cell_id;
            // write-ahead（批A journal v2）：A 行 append+flush 成功才推 962
            // ——持久决定先于对外声明。落盘失败回滚内存认领并跳过声明
            //（Space 留池待下次受理重试，告警 loudly；刷新/撤销标记不经此闸
            //——无转移发生，最后一条 A 行已覆盖该归属+版本）。
            if (!appendJournalRow("A " + std::to_string(space_id) + "|"
                                  + std::to_string(reg.cell_id) + "|"
                                  + std::to_string(it->second.version), true)) {
                worldLog().warning(
                    "world journal append failed: " + config_.worldJournalPath
                    + " (space claim withheld: space=" + std::to_string(space_id)
                    + " cell=" + std::to_string(reg.cell_id) + ")");
                if (reclaim) {
                    --it->second.version;  // 回滚版本前进（保持释放态）
                } else {
                    spaceAssignments_.erase(it);
                }
                continue;
            }
            worldLog().info(reclaim
                                ? "space re-claimed: space=" + std::to_string(space_id)
                                      + " cell=" + std::to_string(reg.cell_id)
                                      + " version=" + std::to_string(it->second.version)
                                : "space assigned: space=" + std::to_string(space_id)
                                      + " cell=" + std::to_string(reg.cell_id)
                                      + " version=1");
        } else if (it->second.cell_id != reg.cell_id) {
            if (m1Link_) {
                m1Link_->sendCellSpaceAssign(peer, it->second.version,
                                             config_.m1AuthorityEpoch, space_id,
                                             it->second.cell_id, false);
            }
            continue;  // 归属它方：撤销标记（owner/版本 = 当前归属视图）
        }
        if (m1Link_) {
            m1Link_->sendCellSpaceAssign(peer, it->second.version,
                                         config_.m1AuthorityEpoch, space_id,
                                         it->second.cell_id, true);
        }
    }
}

bool WorldApp::lower_load_peer(std::uint64_t cell_id, std::int32_t load,
                               std::int32_t needs) const {
    // 最小负载优先（L104 负载裁决 + 容量申报裁决切片）：登记表内任一合格
    // 候选负载严格低于本 cell 即让位——合格 = 就绪、不排空、未缩容且声明
    // 能力覆盖该 Space 的 needs（不合格的更低负载 peer 不参裁，否则合格
    // 方会把归属让给无资格方，Space 永久搁置）。同负载不退让（认领归先受理
    // 者，裁决对注册顺序稳定）；存疑（suspect）条目仍参裁——链路未决时归属
    // 不动比抢分安全。缩容意图（retire）条目排除在外：它永不认领，让位给
    // 它会永久搁置该 Space。
    for (const auto& entry : cellRegistrations_) {
        const auto& other = entry.second;
        if (other.cell_id != cell_id && other.ready && other.drain_state == 0 &&
            !other.retire && capability_fit(other.capabilities, needs) &&
            other.load < load) {
            return true;
        }
    }
    return false;
}

void WorldApp::releaseCellSpaces(std::uint64_t cell_id) {
    // 过期释放（L108 回收切片，driveCellRegistry 同上下文）：owner 置 0、
    // 版本不变——释放不是认领，无转移发生；重认领方经 pushSpaceAssignments
    // 版本 +1 收敛。条目保留：池成员身份不随心跳消失，Space 回池待认领。
    for (auto& entry : spaceAssignments_) {
        // 冲突冻结（批B 967 裁决面）：冻结期不释放（设计稿 §4「不推新分配、
        // 不释放」）——条目停尸在当前 owner，等待人工修复裁决解冻。
        if (conflictedSpaces_.count(entry.first) > 0) {
            continue;
        }
        if (entry.second.cell_id == cell_id) {
            entry.second.cell_id = 0;
            // R 行（批A journal v2）：释放是终态转移（owner 归零、版本不变），
            // 重放以每 space 最后一条 A/R 为准——缺席则重启误认旧主仍持有权。
            // 失败告警降级（批㉚ 可用性优先；装载面对账裁决随批B）。
            if (!appendJournalRow("R " + std::to_string(entry.first))) {
                worldLog().warning("world journal append failed: "
                                   + config_.worldJournalPath
                                   + " (release replays as owned: space="
                                   + std::to_string(entry.first) + ")");
            }
            worldLog().info("space released: space="
                            + std::to_string(entry.first)
                            + " cell=" + std::to_string(cell_id)
                            + " version=" + std::to_string(entry.second.version));
        }
    }
}

bool WorldApp::beginCellRetire(std::uint64_t cell_id) {
    // 缩容编排（L104 缩容切片，注册表单写者上下文）：置 World 单方缩容意图
    // ——与 cell 自报 drain_state 分面（自报是 cell 自己的活动状态，本位是
    // World 的调度决定），此后该 cell 停止认领新 Space；名下归属即刻回池
    // （owner 归零、版本不变——在途活动在 cell 本地自然排空，不强断），再
    // 向其推当前权威视图：它收到的自有 Space 现为撤销标记，962 应用面据此
    // 撤本地路由。撤路由受客户端版本去重约束（释放不推进版本），故实际撤
    // 除随下次合格 peer 重认领后版本前进的那一拍收敛——同 L108 离线期缺口
    // 的收敛口径，不为强收敛改写版本号语义。
    auto it = cellRegistrations_.find(cell_id);
    if (it == cellRegistrations_.end()) {
        return false;
    }
    it->second.retire = true;
    releaseCellSpaces(cell_id);
    pushSpaceAssignments(it->second.peer, it->second);
    worldLog().info("cell retire begun: id=" + std::to_string(cell_id)
                    + " epoch=" + std::to_string(it->second.start_epoch)
                    + " peer=" + it->second.peer);
    return true;
}

bool WorldApp::cancelCellRetire(std::uint64_t cell_id) {
    // 取消缩容（设计稿「尚未发出退出命令且重新缺容量时可取消排空」）：只清
    // 意图，归属维持现状——已被他人重认领的不取回（不迁移不重分），仍释放
    // （owner=0）的随本次推送由恢复资格的本人或最低负载 peer 重认领（版本 +1）。
    auto it = cellRegistrations_.find(cell_id);
    if (it == cellRegistrations_.end()) {
        return false;
    }
    it->second.retire = false;
    pushSpaceAssignments(it->second.peer, it->second);
    worldLog().info("cell retire cancelled: id=" + std::to_string(cell_id)
                    + " epoch=" + std::to_string(it->second.start_epoch)
                    + " peer=" + it->second.peer);
    return true;
}

void WorldApp::onCellRetire(const std::string& peer, const CellRetireRequest& request) {
    // 缩容管理命令受理（963 cell_retire_request → 966，M1 泵上下文 = 登记表
    // 单写者上下文）：与 onCellRegister 同口，请求/回执同步同线程回发端链路。
    // 请求合法性先看（op 未知/cell_id 非法 → 拒收，不动表），再查登记
    // （目标未登记 = 显式 miss——运维据此判命令目标是否打错）。
    if (m1Link_ == nullptr) {
        return;
    }
    if (request.cell_id == 0) {
        m1Link_->sendCellRetireReply(peer, request.request_id, false,
                                     kCellRetireRejectedInvalid,
                                     config_.m1AuthorityEpoch, request.cell_id,
                                     "invalid cell_id");
        return;
    }
    if (request.op == kCellRetireOpBegin) {
        if (!beginCellRetire(request.cell_id)) {
            m1Link_->sendCellRetireReply(peer, request.request_id, false,
                                         kCellRetireRejectedUnknownCell,
                                         config_.m1AuthorityEpoch, request.cell_id,
                                         "cell not registered");
            return;
        }
        m1Link_->sendCellRetireReply(peer, request.request_id, true, kCellRetireBegun,
                                     config_.m1AuthorityEpoch, request.cell_id,
                                     "retire intent set; assignments released to pool");
        return;
    }
    if (request.op == kCellRetireOpCancel) {
        if (!cancelCellRetire(request.cell_id)) {
            m1Link_->sendCellRetireReply(peer, request.request_id, false,
                                         kCellRetireRejectedUnknownCell,
                                         config_.m1AuthorityEpoch, request.cell_id,
                                         "cell not registered");
            return;
        }
        m1Link_->sendCellRetireReply(peer, request.request_id, true,
                                     kCellRetireCancelled, config_.m1AuthorityEpoch,
                                     request.cell_id,
                                     "retire intent cleared; ownership unchanged");
        return;
    }
    m1Link_->sendCellRetireReply(peer, request.request_id, false,
                                 kCellRetireRejectedInvalid,
                                 config_.m1AuthorityEpoch, request.cell_id,
                                 "unknown op");
}

void WorldApp::onCellSpaceReport(const std::string& peer, const CellSpaceReport& report) {
    // 对账自报受理（967，断连对账批B；M1 泵上下文 = 注册表单写者上下文，
    // 与 onCellRegister/onCellRetire 同口）。逐条目核对权威归属表（设计稿
    // §4）——裁决就地生效，无回执（962 全池权威视图即裁决载体，冻结不经
    // 协议下发）。request_id = 0 保留（静默弃，同 952/960 关联键纪律）。
    if (report.request_id == 0 || report.cell_id == 0 || report.space_id == 0) {
        return;
    }
    const auto reg = cellRegistrations_.find(report.cell_id);
    if (reg == cellRegistrations_.end()) {
        // 未登记来源：无裁决地位（对账表以登记为准），丢弃。
        worldLog().info("space report from unregistered cell dropped: cell="
                        + std::to_string(report.cell_id)
                        + " space=" + std::to_string(report.space_id));
        return;
    }
    if (report.start_epoch != reg->second.start_epoch) {
        // 旧化身上报：登记已是新代次（新化身），旧端视图无对账效力，丢弃。
        worldLog().info("space report from stale incarnation dropped: cell="
                        + std::to_string(report.cell_id)
                        + " reported_epoch=" + std::to_string(report.start_epoch));
        return;
    }
    if (reg->second.suspect) {
        // 控制面失联期隔离：961 复核归来前，同登记代次但走新/旧链路的
        // 旧化身或旧端自报不得参与对账，也不能借 967 证明旧归属。
        worldLog().warning("space report from suspect cell dropped: cell="
                           + std::to_string(report.cell_id)
                           + " space=" + std::to_string(report.space_id));
        return;
    }
    ++spaceReportCount_;
    const auto entry = spaceAssignments_.find(report.space_id);
    if (!config_.worldJournalPath.empty() && entry != spaceAssignments_.end() &&
        entry->second.cell_id == report.cell_id &&
        entry->second.version == report.applied_version) {
        appendJournalRow("C " + std::to_string(report.space_id) + '|'
                         + std::to_string(report.cell_id) + '|'
                         + std::to_string(report.applied_version));
        ++reconciledSpaceCount_;
    }
    if (entry == spaceAssignments_.end()) {
        // 未登记权威（§4「无 S 记录 | C 报 S」行）：权威归属表无此 Space
        //（池外 id，或池内从未认领/记忆已失）——源隔离不明，进冲突集
        //（观测面；池外 id 无分配语义可冻结，冻结面天然只作用于池内）。
        const bool fresh = conflictedSpaces_.insert(report.space_id).second;
        worldLog().warning("space report conflict (no authoritative record): space="
                           + std::to_string(report.space_id)
                           + " cell=" + std::to_string(report.cell_id)
                           + (fresh ? " (frozen)" : " (already frozen)"));
        return;
    }
    if (entry->second.cell_id == report.cell_id) {
        if (entry->second.version == report.applied_version) {
            // 匹配（§4 行1）：保持，无操作。
            return;
        }
        // 版本冲突（§4 行2）：权威仍归属本 cell 但应用版本对不上——journal
        // 落后或应用面偏离，该 Space 冻结（不认领不释放不推送），待对账修复。
        const bool fresh = conflictedSpaces_.insert(report.space_id).second;
        worldLog().warning("space report version conflict: space="
                           + std::to_string(report.space_id)
                           + " cell=" + std::to_string(report.cell_id)
                           + " authoritative=" + std::to_string(entry->second.version)
                           + " applied=" + std::to_string(report.applied_version)
                           + (fresh ? " (frozen)" : " (already frozen)"));
        return;
    }
    // 其余（owner=0 已释放 / 归属它方）：权威已前进——962 全池视图即裁决，
    // 上报端随撤销标记/重认领推送收敛（§4 行3 既有路径），收敛滞后不入冲突集。
    worldLog().info("space report lags authority: space="
                    + std::to_string(report.space_id)
                    + " cell=" + std::to_string(report.cell_id)
                    + " authoritative_owner=" + std::to_string(entry->second.cell_id)
                    + " applied=" + std::to_string(report.applied_version));
}

std::size_t WorldApp::retired_cell_count() const {
    std::size_t count = 0;
    for (const auto& entry : cellRegistrations_) {
        if (entry.second.retire) {
            ++count;
        }
    }
    return count;
}

const WorldApp::CellRegistration* WorldApp::find_cell_registration(
    std::uint64_t cell_id) const {
    // 泵上下文只读观测面（注册表单写者 = M1 泵上下文受理点）
    const auto it = cellRegistrations_.find(cell_id);
    return it != cellRegistrations_.end() ? &it->second : nullptr;
}

const WorldApp::SpaceAssignment* WorldApp::find_space_assignment(
    std::uint64_t space_id) const {
    const auto it = spaceAssignments_.find(space_id);
    return it != spaceAssignments_.end() ? &it->second : nullptr;
}

void WorldApp::loadWorldJournal() {
    // 管理日志装载（启动窗，M1 链路开闸前——受理面未开，无并发写者）。
    // v2 行型（world-reconciliation 设计稿 §2，前缀区型）：`[T] cell|epoch`
    // 代次墓碑（T 前缀可省——v1 裸行直接升级不迁移）、`A space|cell|version`
    // 认领转移、`R space` 释放（owner 归零版本不变）、`B epoch` 启动代次。
    // 装载终态 = 每 space 最后一条 A/R + 每 cell 最大 T + 最大 B。0 号/非
    // 数字/缺字段/多字段 = 损坏显式拒启（不静默部分回放——orders journal
    // 同口径）；行尾 \r 剥除（Windows 落盘坑账防御）。文件缺省 = 首启，
    // append 建档（空日志顶替禁令的判定随批C）。
    if (config_.worldJournalPath.empty()) {
        return;
    }
    std::ifstream in(config_.worldJournalPath, std::ios::binary);
    if (!in) {
        // 空日志判定（world-reconciliation §2）分两步：启动时文件缺失先按
        // 「无 cell 拨入的真空候选」放行；首个 961 拨入即证明曾存在旧管理
        // 关系，转记录缺失并全池冻结。装载面 T 水位也可直接证明证据缺失。
        journalMissingAtBoot_ = true;
        if (!cellEpochWatermarks_.empty()) {
            missingJournal_ = true;
            worldLog().warning(
                "world journal file missing: " + config_.worldJournalPath
                + " known_cells=" + std::to_string(cellEpochWatermarks_.size())
                + " (missing journal evidence frozen all assignments)");
            return;
        }
        return;  // 无记录 = 首启候选，受理后按拨入证据裁决
    }
    const auto bad_line = [&](std::uint64_t line_no, const char* why) {
        throw std::runtime_error(std::string("world journal corrupt (") + why
                                 + ") at " + config_.worldJournalPath + ":"
                                 + std::to_string(line_no));
    };
    const auto parse_u64 = [&](const std::string& field,
                               std::uint64_t line_no) -> std::uint64_t {
        std::size_t pos = 0;
        std::uint64_t value = 0;
        try {
            value = std::stoull(field, &pos);
        } catch (const std::exception&) {
            bad_line(line_no, "not a number");
        }
        if (pos != field.size() || field.empty()) {
            bad_line(line_no, "trailing garbage");
        }
        return value;
    };
    struct ReplaySpace {
        std::uint64_t cell_id = 0;
        std::uint64_t version = 0;
    };
    std::unordered_map<std::uint64_t, ReplaySpace> replay;
    std::uint64_t max_boot_epoch = 0;
    bool saw_boot_row = false;
    std::string line;
    std::uint64_t line_no = 0;
    while (std::getline(in, line)) {
        ++line_no;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        if (line.compare(0, 2, "A ") == 0) {
            const std::string body = line.substr(2);
            const auto b1 = body.find('|');
            if (b1 == std::string::npos) {
                bad_line(line_no, "missing '|'");
            }
            const auto b2 = body.find('|', b1 + 1);
            if (b2 == std::string::npos) {
                bad_line(line_no, "missing '|'");
            }
            if (body.find('|', b2 + 1) != std::string::npos) {
                bad_line(line_no, "extra field");
            }
            const auto space_id = parse_u64(body.substr(0, b1), line_no);
            const auto cell_id = parse_u64(body.substr(b1 + 1, b2 - b1 - 1), line_no);
            const auto version = parse_u64(body.substr(b2 + 1), line_no);
            if (space_id == 0 || cell_id == 0 || version == 0) {
                bad_line(line_no, "zero id or version");
            }
            replay[space_id] = ReplaySpace{cell_id, version};  // 最后一条 A 为准
            continue;
        }
        if (line.compare(0, 2, "R ") == 0) {
            const std::string body = line.substr(2);
            if (body.find('|') != std::string::npos) {
                bad_line(line_no, "extra field");
            }
            const auto space_id = parse_u64(body, line_no);
            if (space_id == 0) {
                bad_line(line_no, "zero space id");
            }
            const auto it = replay.find(space_id);
            if (it != replay.end()) {
                it->second.cell_id = 0;  // 终态 owner 归零（版本保留）
            }
            // 无 A 的 R = 无操作（释放等价于从未认领）
            continue;
        }
        if (line.compare(0, 2, "B ") == 0) {
            const std::string body = line.substr(2);
            if (body.find('|') != std::string::npos) {
                bad_line(line_no, "extra field");
            }
            const auto epoch = parse_u64(body, line_no);
            if (epoch == 0) {
                bad_line(line_no, "zero epoch");
            }
            max_boot_epoch = std::max(max_boot_epoch, epoch);
            saw_boot_row = true;
            continue;
        }
        if (line.compare(0, 2, "C ") == 0 || line.compare(0, 2, "S ") == 0) {
            continue;
        }
        // T 行（可省前缀）：v2 带 `T ` 前缀或 v1 裸行，同解析；其余无管道
        // 行型显式损坏（防新行型被 T 兼容面误吞）。
        if (line.compare(0, 2, "T ") != 0 && line.find('|') == std::string::npos) {
            bad_line(line_no, "unknown row type");
        }
        std::string body;
        if (line.compare(0, 2, "T ") == 0) {
            body = line.substr(2);
        } else {
            body = line;
        }
        const auto bar = body.find('|');
        if (bar == std::string::npos) {
            bad_line(line_no, "missing '|'");
        }
        if (body.find('|', bar + 1) != std::string::npos) {
            bad_line(line_no, "extra field");
        }
        const auto cell_id = parse_u64(body.substr(0, bar), line_no);
        const auto epoch = parse_u64(body.substr(bar + 1), line_no);
        if (cell_id == 0 || epoch == 0) {
            bad_line(line_no, "zero id or epoch");
        }
        auto& wm = cellEpochWatermarks_[cell_id];
        if (epoch > wm) {
            wm = epoch;
        }
    }
    // B 行铸代次（设计稿 §5 步骤3）：重启 = 最大 B + 1；首启无 B 行 = 配置
    // 代次即初始代次（写首个 B 行——先记后开受理面，本函数先于链路装配）。
    // 自铸代次（配置 0）且无 B 行 = 无从记，沿用进程自铸（每启动自然不同）。
    // 追加失败告警降级：内存代次已铸，两次启动可能同代次（loudly 可见；
    // flush/fsync 持久级别随设计稿待拍板项）。
    if (config_.m1AuthorityEpoch != 0 || saw_boot_row) {
        if (saw_boot_row && max_boot_epoch == std::numeric_limits<std::uint64_t>::max()) {
            throw std::runtime_error("world journal epoch exhausted at "
                                     + config_.worldJournalPath);
        }
        const std::uint64_t cast = saw_boot_row ? max_boot_epoch + 1
                                                : config_.m1AuthorityEpoch;
        if (!appendJournalRow("B " + std::to_string(cast))) {
            worldLog().warning("world journal B row append failed: "
                               + config_.worldJournalPath
                               + " (restart may reuse management epoch)");
        }
        config_.m1AuthorityEpoch = cast;  // 受理面统一用铸后代次（961/962 透传）
    }
    // 装载终态重建进池（批A）：仅池成员生效（非池行计数告警——池配置变更后
    // 旧记录无权威落点，逐项对账随批B）；已释放（owner=0）条目照旧重建——
    // 重认领版本 +1 依赖历史版本（新启 version 1 会倒退，ABA 回放面）。
    // 有主的条目进 pending 过期窗（driveCellRegistry 驱动释放）。
    std::size_t seeded = 0;
    std::size_t skipped = 0;
    for (const auto& entry : replay) {
        if (!pool_has(config_, entry.first)) {
            ++skipped;
            continue;
        }
        SpaceAssignment a;
        a.space_id = entry.first;
        a.cell_id = entry.second.cell_id;
        a.version = entry.second.version;
        spaceAssignments_[entry.first] = a;
        if (a.cell_id != 0) {
            pendingJournalOwners_[a.cell_id] = config_.bootAtMs;
        }
        ++seeded;
    }
    worldLog().info("world journal loaded: " + config_.worldJournalPath
                    + " watermarks=" + std::to_string(cellEpochWatermarks_.size())
                    + " spaces=" + std::to_string(seeded)
                    + (skipped != 0
                           ? " skipped_nonpool=" + std::to_string(skipped)
                           : "")
                    + " epoch=" + std::to_string(config_.m1AuthorityEpoch));
}

bool WorldApp::appendJournalRow(const std::string& row, bool durable) {
    // journal v2 追加统一入口（批A；批C 巡检代决：A 行 fsync，write-ahead
    // 决定必须先落介质；R/B/T 行 flush + 失败告警降级，沿可用性优先）。
    // journal 关 = 纯内存语义恒成功（批㉚ 已代决沿用）。调用点全在内核线程
    //（受理点/驱动钩子 = 注册表单写者上下文），无并发写者。
    if (config_.worldJournalPath.empty()) {
        return true;
    }
    std::ofstream out(config_.worldJournalPath, std::ios::app | std::ios::binary);
    if (!out) {
        return false;
    }
    out << row << '\n';
    out.flush();
    if (!out) return false;
    if (!durable) {
        return true;
    }
    const int fd = ::open(config_.worldJournalPath.c_str(), O_RDONLY);
    if (fd < 0) return false;
    const bool synced = ::fsync(fd) == 0;
    ::close(fd);
    return synced;
}

void WorldApp::recordEpochWatermark(std::uint64_t cell_id, std::uint64_t epoch) {
    // 水位记录 + 墓碑追加（L101 管理日志切片）：仅水位真实前进才落盘——
    // 同代次刷新/回拨不追加（行冗余无害但省 I/O；prev 已在日志或本进程
    // 追加过）。v1 裸行格式保留（装载面直接升级，不迁移）。追加失败只告
    // 警不拒受理：墓碑缺席时隔离退化回批㉗ 内存水位（可用性优先，loudly
    // 可见）。调用点 = onCellRegister 受理点（内核线程泵上下文，单写者）。
    const auto it = cellEpochWatermarks_.find(cell_id);
    const std::uint64_t prev = it != cellEpochWatermarks_.end() ? it->second : 0;
    cellEpochWatermarks_[cell_id] = std::max(prev, epoch);
    if (epoch <= prev) {
        return;
    }
    if (!appendJournalRow(std::to_string(cell_id) + '|' + std::to_string(epoch))) {
        worldLog().warning("world journal append failed: " + config_.worldJournalPath
                           + " (isolation degrades to in-memory watermark)");
    }
}

void WorldApp::driveCellRegistry() {
    // 注册表过期清扫（960 心跳批；post-tick 上下文 = 注册表单写者线程）：
    // last-heard（registered_at_ms，每次受理刷新）超 cellExpiryMs 未刷新 =
    // 心跳失效，整条摘除。摘除后该 cell 重注册走受理面首注册路径（登记
    // 忘却——代次墓碑随旧执行端隔离批）；cell 侧心跳 = 周期 960 重发。
    if (config_.cellExpiryMs == 0) {
        return;
    }
    const auto now_ms = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
    if (!cellRegistrations_.empty()) {
        for (auto it = cellRegistrations_.begin(); it != cellRegistrations_.end();) {
            if (now_ms - it->second.registered_at_ms > config_.cellExpiryMs) {
                worldLog().info("cell expired: id=" + std::to_string(it->first)
                                + " epoch=" + std::to_string(it->second.start_epoch)
                                + " peer=" + it->second.peer);
                releaseCellSpaces(it->first);  // 归属 Space 释放回池（L108 回收切片）
                it = cellRegistrations_.erase(it);
            } else {
                ++it;
            }
        }
    }
    // journal 已知归属主过期窗（批A 装载终态重建）：装载面重建的归属，其主
    // 未随登记面出现——窗内保持归属（不立即另端重建，设计稿 §4「过期窗不
    // 来」行），超窗未归队即释放回池（R 行）。真身已登记 = 除名（此后随
    // 登记面心跳/过期语义，不双轨）。
    for (auto it = pendingJournalOwners_.begin(); it != pendingJournalOwners_.end();) {
        if (cellRegistrations_.count(it->first) > 0) {
            it = pendingJournalOwners_.erase(it);
            continue;
        }
        if (now_ms - it->second > config_.cellExpiryMs) {
            worldLog().info("journal owner expired: cell=" + std::to_string(it->first));
            releaseCellSpaces(it->first);
            it = pendingJournalOwners_.erase(it);
        } else {
            ++it;
        }
    }
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
