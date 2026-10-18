#include "world_app/world_app.hpp"
#include "player_store/business_wire.hpp"
#include "player_store/store_dial.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/net/linktrace/span_emitter.hpp"
#include "apollo/protocol/messages.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/game/world/instance.hpp"
#include "apollo/game/world/scene.hpp"
#include "apollo/game/world/scene_transfer.hpp"
#include "apollo/game/world/world_session_manager.hpp"
#include "apollo/runtime/world_host.hpp"
#include "apollo/script/module_rollout.hpp"
#include "apollo/script/script_runtime.hpp"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <vector>

namespace world_app {

namespace {
// 库面日志出口：cat=world_app（§5.1 cat= 检索键；主程已接文件面）
apollo::core::log::Logger& worldLog() {
    return *apollo::core::log::global_log_manager().createLogger("world_app");
}

// 读整个 Lua 源文件（binary 模式——Windows CRLF 落盘纪律）；缺文件返回
// std::nullopt。仅启动装载序调用（gameThread 首拍，§2 一状态一逻辑线程）。
std::optional<std::string> read_script_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
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
} // namespace

namespace protocol = apollo::protocol;

// P1-3：Scene 持有 AOI 后 WorldApp 侧直接消费 SceneAoi（Vec3/查询面）
using apollo::game::world::SceneAoi;

namespace {

// P2-1b：契约枚举 → Scene 实体类型串（core::Entity 以字符串承载类型语义）
const char* entity_type_name(protocol::EntityType type) {
    switch (type) {
        case protocol::EntityType::PLAYER:   return "PLAYER";
        case protocol::EntityType::NPC:      return "NPC";
        case protocol::EntityType::MONSTER:  return "MONSTER";
        case protocol::EntityType::PET:      return "PET";
        default:                             return "UNKNOWN";
    }
}

class WorldAppService final : public apollo::runtime::IWorldService {
public:
    WorldAppService(
        apollo::game::world::World& world,
        WorldAppConfig config)
        : world_(world)
        , config_(std::move(config)) {
    }

    std::string_view service_name() const override {
        return "WorldAppService";
    }

    bool initialize() override {
        tick_count_ = 0;
        return true;
    }

    void tick(const apollo::runtime::WorldTickContext& context) override {
        tick_count_ = context.tick_index;
        // P0-3：世界 tick = 逐 scene 六阶段（simulate/recalc/aoidecay/
        // collect/budget+flush/persist-batch）
        world_.tick(static_cast<double>(context.delta_seconds));
    }

    void shutdown() override {
        tick_count_ = 0;
    }

private:
    apollo::game::world::World& world_;
    WorldAppConfig config_;
    std::uint64_t tick_count_ = 0;
};

} // namespace

//==============================================================================
//==============================================================================
// WorldApp 实现
//==============================================================================

WorldApp::WorldApp(const WorldAppConfig& config)
    : config_(config)
    , world_(std::make_unique<apollo::game::world::World>())
    , worldSessionManager_(std::make_shared<apollo::game::world::WorldSessionManager>())
    , worldHost_(std::make_shared<apollo::runtime::WorldHost>(
          static_cast<std::uint32_t>(1000 / std::max(config.tickRateMs, 1)))) {
    worldId_ = config.worldId;
    worldHost_->add_world_service(
        std::make_shared<WorldAppService>(*world_, config_));
    // 小队副本受理面（P2）：注册本 World 容量；在线谓词按本进程会话面
    // 裁定（权威在线目录在 Login 侧，跨进程校验随后续条目接通）。
    squadInstanceService_ = std::make_unique<squad_instance::SquadInstanceService>(
        squadParties_, world_.get(), config_.worldId);
    // 资料归属切换（L19）：档案宿主 = baseServer，World 不再持有 PlayerStore
    // ——结算落账解析器留空（SettlementLedger 挂账待补的设计降级态），
    // Base 业务受理接口随后续条目接线后恢复落账。
    squadInstanceService_->register_world(config_.worldId, config_.maxInstances);
    squadInstanceService_->set_member_online(
        [sessions = worldSessionManager_](std::uint64_t player_id) {
            return sessions &&
                   sessions->find_by_player(
                       apollo::game::core::PlayerId{player_id}) != nullptr;
        });
}

WorldApp::~WorldApp() {
    stop();
}

void WorldApp::start() {
    if (running_) return;


    // net M1 接收端（2b-ii，2b-iii 起 WorldApp 唯一受理入口——legacy RepSocket
    // 已退役）：InterServerLink passive 端点装配（监听在 start 内 bind/listen，
    // bind 失败同契约显形）
    m1Link_ = makeM1LinkServer(
        M1LinkConfig{config_.host, config_.m1Port, config_.m1RouteJson,
                     config_.m1AuthorityEpoch},
        [this](IntentEnvelope intent) { onM1Intent(std::move(intent)); },
        [this](const std::string& peer, const DebugEvalRequest& request) {
            onM1DebugEval(peer, request);
        },
        [this](const std::string& peer, const ModuleRolloutRequest& request) {
            onM1ModuleRollout(peer, request);
        },
        [this](const std::string& peer, const WorldDrainRequest& request) {
            onM1WorldDrain(peer, request);
        });

    // 开闸先于 m1Link_->start()：受理面就绪后网关才见得到入口
    accepting_ = true;

    if (!m1Link_->start()) {
        accepting_ = false;
        m1Link_.reset();
        throw std::runtime_error("failed to start WorldApp M1 link endpoint (route table load or bind failed)");
    }

    ensureDefaultScene();

    if (!worldHost_->start()) {
        accepting_ = false;
        m1Link_->stop();
        m1Link_.reset();
        throw std::runtime_error("failed to start WorldHost");
    }

    // Base 业务受理面（P1 结算迁入）：结算落账 = 返回业务请求拨 baseServer
    // store M1 面（958/959——资料归属切换后锚点写权在 Base）。拨号装配
    // 失败降级（警告 + 挂账面照旧），不拦启动。Sink 在 gameThread_ 结算段
    // 同步调 call（自泵线程禁自调纪律不涉——StoreDial 泵独立线程）。
    if (config_.baseBusinessPort != 0) {
        baseBusinessDial_ = std::make_unique<player_store::StoreDial>(
            config_.m1RouteJson, "world", "base", config_.baseBusinessHost,
            config_.baseBusinessPort);
        if (baseBusinessDial_->ready()) {
            squadInstanceService_->set_business_sink(
                [this](std::uint64_t player_id, std::int64_t score) {
                    player_store::business::ScoreCreditRequest req;
                    req.request_id = businessSeq_++;
                    req.player_id = player_id;
                    req.score = score;
                    req.source_zone_id = config_.worldId;
                    const auto reply = baseBusinessDial_->call(
                        player_store::business::encodeFrame(req));
                    player_store::business::ScoreCreditReply rep;
                    protocol::SessionID sid = 0;
                    if (!reply ||
                        !player_store::business::decodeFrame(*reply, rep, sid)) {
                        return squad_instance::SettlementLedger::ApplyError::
                            AvatarUnavailable;  // 传输不可达/回包畸形：挂账待补
                    }
                    return rep.accepted
                               ? squad_instance::SettlementLedger::ApplyError::Ok
                               : squad_instance::SettlementLedger::ApplyError::
                                     AvatarUnavailable;  // 离线/迁移中：挂账待补
                });
            worldLog().info(
                "base business dialing " + config_.baseBusinessHost + ":"
                + std::to_string(config_.baseBusinessPort));
        } else {
            baseBusinessDial_.reset();
            worldLog().warning(
                "base business dial degraded: route table unavailable ("
                + config_.m1RouteJson + "), settlement stays deferred");
        }
    }

    // 结算凭据持久化（L67 去重凭据面生产接线）：World 运行目录持久提交
    // (小队, 玩家, 分值) 凭据——settle 与 release 崩溃窗内重启装载回填，
    // 重放不双记；未配置 = 降级态（凭据仅进程内存）。
    if (!config_.settlementCredentialDir.empty()) {
        squadInstanceService_->set_credential_dir(
            config_.settlementCredentialDir);
        const auto restored =
            squadInstanceService_->ingest_restored_credentials();
        worldLog().info("settlement credentials dir "
                        + config_.settlementCredentialDir + " (restored parties="
                        + std::to_string(restored) + ")");
    }

    running_ = true;

    // 启动游戏循环线程
    gameThread_ = std::thread(&WorldApp::gameLoop, this);

    worldLog().info("WorldApp M1 link listening on " + config_.host + ":"
                + std::to_string(m1Link_->listenPort()));
    worldLog().info("Space: " + config_.spaceName + " ("
                + std::to_string(config_.spaceWidth) + "x"
                + std::to_string(config_.spaceHeight) + ")");
}

void WorldApp::stop() {
    // 停机 G-3 镜像顺序（ADR-016）：停受理 → 队列 drain → gameThread join →
    // M1 端点停 → worldHost stop（尾扫 drain 的延时应答仍要有发送面，M1
    // 端点停在其后）
    accepting_ = false;
    running_ = false;

    if (gameThread_.joinable()) {
        gameThread_.join();
    }

    // Base 业务拨号面随 gameThread_ 收口（结算面唯一调用者已 join）。
    if (baseBusinessDial_) {
        baseBusinessDial_->shutdown();
        baseBusinessDial_.reset();
    }

    if (m1Link_) {
        m1Link_->stop();
        m1Link_.reset();
    }

    if (worldHost_) {
        worldHost_->stop();
    }
}

bool WorldApp::submitIntent(IntentEnvelope intent) {
    // 受理点判定（ADR-017 决策 1，M1 面与测试共用）：只入队意图信封，
    // 不做 world 读写；回执纪律见 onM1Intent。
    if (!accepting_.load(std::memory_order_acquire)) {
        return false;
    }
    auto result = acceptanceQueue_.push(std::move(intent));
    if (!result.accepted) {
        return false;  // ADR-016：顶格拒收不丢旧，M1 面回 2101
    }
    if (result.high_watermark) {
        // ADR-016 两级水位：越告警线提示一次，回落滞回后再报
        worldLog().warning("acceptance queue high watermark: pending="
                       + std::to_string(acceptanceQueue_.size()) + "/"
                       + std::to_string(acceptanceQueue_.cap()));
    }
    return true;
}

void WorldApp::onM1Intent(IntentEnvelope intent) {
    // M1 受理点（2b-ii，泵线程 = gameThread_）：submitIntent 判定 + 拒收即时
    // 回执。延时应答（ADR-017）：受理成功不回包，回执随 executeIntent 消费后
    // 按 request_id 回带；即时回执面只剩两类永不入队的拒收。
    const auto request_id = intent.request_id;
    if (submitIntent(std::move(intent))) {
        return;
    }
    if (accepting_.load(std::memory_order_acquire)) {
        // ADR-016：顶格拒收不丢旧不静默——M1 面回契约错误码 2101
        sendM1Ack(request_id, kIntentAckBackpressure,
                  "server overloaded: acceptance queue full");
    } else {
        // 维护闸：start 前 / stop 后一律回 2001（窄窗竞态下受理判定与闸状态
        // 间取回执时点状态，与拒收同型观察面）
        sendM1Ack(request_id, kIntentAckInternal,
                  "server maintenance: not accepting requests");
    }
}

void WorldApp::sendM1Ack(std::uint64_t request_id, std::int32_t code, const std::string& message) {
    if (request_id == 0) {
        return;  // legacy 路径（ack-on-accept）无应答关联键
    }
    if (m1Link_) {
        m1Link_->sendIntentAck(request_id, code, message);
    }
}

// M1 eval 在途关联账上限（host 级背压——防丢最旧语义下的无界挂账；
// 对齐 Scene 队列深度的量级放大，超过即时回执拒收）。
constexpr std::size_t kM1EvalBacklogMax = 64;

void WorldApp::onM1DebugEval(const std::string& peer, const DebugEvalRequest& request) {
    // 944 受理（§7.1 attach transport；本回调在 gameThread_ 泵上下文）：
    // 语法面已在 M1LinkServer 解码，此处只做定位与背压判定——失败即时回
    // 945（不进 tick 窗口），成功挂账待 Scene tick 执行后回传。
    if (request.eval_id == 0) {
        return;  // 0 保留（admin 关联键缺位，拒收面都不给）
    }
    if (evalReplyPeers_.size() >= kM1EvalBacklogMax) {
        sendM1EvalReply(peer, request.eval_id, false, "", "eval backlog full");
        return;
    }
    const std::uint64_t scene_id =
        request.scene_id != 0 ? request.scene_id : defaultSceneId_;
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

void WorldApp::onM1ModuleRollout(const std::string& peer,
                                 const ModuleRolloutRequest& request) {
    // 952 受理（P4 仅代码更新批）：本回调在 gameThread_ 泵上下文 =
    // ModuleRollout 契约线程（owning 线程直驱，无 tick 排队）。request_id
    // = 0 保留；受理点同步回 953（观察窗推进随 gameLoop 拍，控制面按
    // status 轮询聚合）。
    if (request.request_id == 0) {
        return;
    }
    auto reply = [&](bool ok, std::int32_t state, std::uint64_t faults,
                     std::uint64_t version, const std::string& detail) {
        if (m1Link_) {
            m1Link_->sendModuleRolloutReply(peer, request.request_id, request.op,
                                            ok, state, faults, version, detail);
        }
    };
    if (script_ == nullptr) {
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
                // 单目标编排器（懒建，仅 prepare 造对象）：target = 本进程
                // runtime，canary = 自身（跨 World 灰度 = 控制面按世界分相位
                // 投递，世界内不再细分）。commit/rollback/status 不复活——
                // 无在途轮次即显式拒（半开状态不可默默重建）。
                moduleRollout_ = std::make_unique<apollo::script::ModuleRollout>(
                    std::vector<apollo::script::RolloutTarget>{{"world", script_.get()}});
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
            reply(ok, state_of(), 0, script_->module_version(request.name),
                  detail_of());
            return;
        }
        case 2: {  // commit：kCanaryPending → 提交本目标（单目标即入观察窗）
            if (moduleRollout_ == nullptr) {
                reply(false, 0, 0, script_->module_version(request.name),
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
            reply(ok, state_of(), 0, script_->module_version(request.name),
                  detail_of());
            return;
        }
        case 3: {  // rollback / abort：kRollingBack = 回滚已提交集；
                   // kCanaryPending = 灰度未提交 → 丢弃预备轮复位（编排器已随
                   // 早失败即止，活跃模块表未被触碰——下轮 prepare 从零重来）
            if (moduleRollout_ == nullptr) {
                // 无在途轮次：幂等空操作（编排器清尾可能追到已复位的世界）
                reply(true, 0, 0, script_->module_version(request.name),
                      "no rollout in flight");
                return;
            }
            if (moduleRollout_->state() ==
                apollo::script::RolloutState::kCanaryPending) {
                moduleRollout_.reset();
                worldLog().info("module rollout aborted: " + request.name);
                reply(true, 0, 0, script_->module_version(request.name),
                      "aborted: nothing committed");
                return;
            }
            moduleRollout_->rollback_target("world");
            const bool ok = moduleRollout_->state() ==
                            apollo::script::RolloutState::kRolledBack;
            if (ok) {
                worldLog().info("module rollout rolled back: " + request.name);
            }
            reply(ok, state_of(), 0, script_->module_version(request.name),
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
                  script_->module_version(request.name),
                  moduleRollout_ ? detail_of() : std::string("no rollout"));
            return;
        }
        default:
            reply(false, state_of(), 0, script_->module_version(request.name),
                  "unknown op " + std::to_string(request.op));
            return;
    }
}

void WorldApp::onM1WorldDrain(const std::string& peer, const WorldDrainRequest& request) {
    // 956 受理（P4 更新排空批）：gameThread_ 泵上下文 = launch/玩法推进同
    // 线程——排空状态机单线程独占无锁，受理点同步回 957。request_id = 0 保留。
    if (request.request_id == 0) {
        return;
    }
    const auto now_ms = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
    const auto active_of = [&]() -> std::uint64_t {
        return squadInstanceService_ ? squadInstanceService_->active_count() : 0;
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
    if (squadInstanceService_ == nullptr) {
        reply(false, "squad service not ready");
        return;
    }
    switch (request.op) {
        case 1: {  // begin：排空闸闭合 + 期限登记（重复 begin 幂等，期限只紧不松）
            if (worldDrainState_ == kWorldDrainServing) {
                squadInstanceService_->set_drain(true);
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
    // gameLoop 拍（driveSquadGameplay 同拍位后拍）：本拍结算收尾已推进，
    // 归零即落 drained；期限到未清强停剩余副本——Create..Running 走 abort
    //（取消线，不发成功奖励），已入结算走 dismiss（收尾推进），两原语覆盖
    // 全部活动相位。drained 终态不反闸（恢复 = 进程替换）。
    if (worldDrainState_ != kWorldDrainDraining || squadInstanceService_ == nullptr) {
        return;
    }
    auto active = squadInstanceService_->active_count();
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
    for (const auto& binding : squadInstanceService_->activeBindings()) {
        if (squadInstanceService_->abort(binding.party_id)
            || squadInstanceService_->dismiss(binding.party_id)) {
            ++force_stopped;
        }
    }
    active = squadInstanceService_->active_count();
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

void WorldApp::driveModuleRollout() {
    // gameLoop 拍（drainDebugEvalResults 同址）：先驱在装模块 onTick（fault
    // 归因源——canary 窗计账靠模块函数真被调用，runtime 按 active 版本解
    // 析），再推观察窗/推广/回滚队列（frame 序 = rollout 单测同型）。
    if (script_ == nullptr) {
        return;
    }
    for (const auto& name : rolloutModuleNames_) {
        (void)script_->call_module(name, "onTick");
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

void WorldApp::drainDebugEvalResults() {
    // tick 窗口收账：Scene::tick 的 §7.1 固定调试点已执行在途 eval，此处取
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

std::vector<uint8_t> WorldApp::handleCellCreateEntity(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto msg = protocol::MessageCodec::decodeBody<protocol::CellCreateEntity>(bodyData);

    // P2-1b：实体集合归 Scene（EntityManager 删除）。玩家实体经 attach 路径
    // 入 avatars_（AOI 随 scene.enter）；非玩家实体 spawn_entity 入 entities_，
    // 随 Scene::tick 六阶段驱动 on_update。空间路由沿 spaceToScene_（跨境同款），
    // 未知空间落默认 scene；位置承载随 NPC AOI 玩法批（现读方为零，不设存储）。
    auto* scene = world_->find_scene(resolve_scene_id(msg.spaceId));

    if (scene) {
        if (msg.entityType == protocol::EntityType::PLAYER) {
            // P0-2：双 id 复用收敛。消息面（CellCreateEntity）尚无独立 player_id
            // 字段——这是契约缺陷，P2-3 修文补字段；P0 代码侧先把身份显式化：
            //   玩家身份 = 创建请求当下携带的唯一玩家号（现为 entity 号派生，
            //   修文后替换为 msg.playerId）；实体身份独立强分型；
            //   不再把 entity_id 兼任 session_id。
            const auto session_id = header.sessionId;
            const auto player_id = static_cast<protocol::PlayerID>(msg.entityId);
            attachPlayerWorldSession(session_id, player_id, msg.entityId,
                                     {msg.position.x, msg.position.y, msg.position.z});
        } else {
            // P2-1b：非玩家实体入 Scene 实体集合（随 Scene::tick 驱动）
            scene->spawn_entity(std::make_shared<apollo::game::core::Entity>(
                apollo::game::core::EntityId(msg.entityId),
                entity_type_name(msg.entityType)));
        }
    }

    // 返回确认（简化）
    return {};  // 空响应表示成功
}

std::vector<uint8_t> WorldApp::handleCellDestroyEntity(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto msg = protocol::MessageCodec::decodeBody<protocol::CellDestroyEntity>(bodyData);

    // P1-3：AOI 出列由 detach 路径 scene.leave 承担（Scene 持有 AOI）
    detachPlayerWorldSession(header.sessionId, msg.entityId);
    // P2-1b：非玩家实体自 Scene 实体集合出列（玩家实体在 avatars_，由 detach 收口）
    if (auto* scene = find_scene_by_entity(msg.entityId)) {
        scene->despawn_entity(apollo::game::core::EntityId(msg.entityId));
    }

    return {};  // 空响应表示成功
}

std::vector<uint8_t> WorldApp::handleCellEntityMove(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto msg = protocol::MessageCodec::decodeBody<protocol::CellEntityMove>(bodyData);

    // P1-3 AOI 收敛：Scene 持有的 SceneAoi 随动；P2-1b 起实体必属 Scene
    //（含非玩家，见 handleCellCreateEntity），无场景归属即不存在
    if (auto* scene = find_scene_by_entity(msg.entityId)) {
        const auto entity_id = apollo::game::core::EntityId(msg.entityId);
        scene->aoi().move(entity_id,
                          SceneAoi::Vec3{msg.newPos.x, msg.newPos.y, msg.newPos.z});

        // 广播移动消息给视野内玩家（viewer set 驱动，见 broadcastToViewers）
        broadcastToViewers(msg.entityId, request);
    }

    return {};  // 空响应表示成功
}

std::vector<uint8_t> WorldApp::handleCellCrossBorder(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());
    auto msg = protocol::MessageCodec::decodeBody<protocol::CellCrossBorder>(bodyData);

    // P1-1 重写：走 scene_transfer 流程模块（prepare→begin→detach→attach→
    // complete，六差距收口），替换旧「字段搬运 + 瞬时 complete」路径——
    // 旧路径无准入、无对象投影、无回滚、目标不存在时静默同场搬运。
    const auto route = spaceToScene_.find(static_cast<std::uint64_t>(msg.toSpace));
    if (route == spaceToScene_.end()) {
        // 目标 space 无路由（差距 ⑥ 显式驳回面；跨 cell 转移随 P3 部署面）
        return {};
    }

    // 玩家身份定位：优先按会话；缺失时经 Scene 的 Avatar 容器由实体号
    // 反查（P0-2 强分型，不再把实体号当玩家号传）
    auto session = worldSessionManager_->find_session(header.sessionId);
    apollo::game::core::PlayerId player_id{};
    if (session) {
        player_id = session->player_id();
    } else {
        const auto caster_entity = apollo::game::core::EntityId(msg.entityId);
        for (apollo::game::world::Scene* scene : world_->scenes()) {
            if (scene == nullptr) {
                continue;
            }
            for (const auto& pid : scene->avatars()) {
                const auto avatar = scene->get_avatar(pid);
                if (avatar && avatar->entity_id() == caster_entity) {
                    player_id = pid;
                    break;
                }
            }
            if (player_id.is_valid()) {
                break;
            }
        }
    }
    if (!player_id.is_valid()) {
        return {};
    }

    apollo::game::world::SceneTransferRequest transfer;
    transfer.player_id = player_id;
    transfer.target_scene_id = route->second;
    // 落点：跨边界请求携带的位置（契约口径——坐标易失态不带走，以
    // 请求落点进场；服务端权威可改）
    transfer.landing = SceneAoi::Vec3{msg.position.x, msg.position.y, msg.position.z};

    const auto outcome =
        apollo::game::world::execute_scene_transfer(*world_, *worldSessionManager_, transfer);
    if (!outcome.ok()) {
        // 驳回/回滚面：单进程阶段仅可观察日志；错误下发随网关面（P1-6）
        worldLog().warning("scene transfer rejected: player="
                       + std::to_string(player_id.value()) + " target_scene="
                       + std::to_string(transfer.target_scene_id) + " result="
                       + std::string(apollo::game::world::to_string(outcome.result))
                       + " at_step=" + std::to_string(static_cast<int>(outcome.failed_at)));
    }

    return {};
}

std::vector<uint8_t> WorldApp::handleCombatSkillCast(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto msg = protocol::MessageCodec::decodeBody<protocol::CombatSkillCast>(bodyData);

    // 处理技能逻辑（P2-1b：在场判定 = 实体归属某 Scene）
    const bool caster_present = find_scene_by_entity(msg.casterId) != nullptr;
    const bool target_present = find_scene_by_entity(msg.targetId) != nullptr;

    if (caster_present && target_present) {
        // 简化伤害计算
        protocol::CombatDamage damageMsg;
        damageMsg.targetId = msg.targetId;
        damageMsg.damage = -10;  // 固定伤害
        damageMsg.sourceId = msg.casterId;

        auto damageData = protocol::MessageCodec::encode(damageMsg);

        // 广播伤害
        broadcastToViewers(msg.targetId, damageData);

        worldLog().info("Skill cast: " + std::to_string(msg.casterId) + " -> "
                    + std::to_string(msg.targetId) + " (skill: "
                    + std::to_string(msg.skillId) + ")");
    }

    return {};
}

//==============================================================================
// 队伍与小队副本真实链路（P2 真实上行/下行）
//==============================================================================

std::uint64_t WorldApp::resolveIntentPlayer(std::uint64_t session_id) const {
    if (!worldSessionManager_) return 0;
    const auto session = worldSessionManager_->find_session(session_id);
    if (!session) return 0;
    return session->player_id().value();
}

void WorldApp::deliverToClient(std::uint64_t session_id, const std::vector<uint8_t>& frame) {
    if (!m1Link_ || frame.empty()) return;  // 链路未装配 = 尽力面落地
    m1Link_->sendClientDeliver(session_id, 0, frame);
}

void WorldApp::deliverPartyResult(std::uint64_t session_id, std::int32_t op,
                                  std::int32_t code, std::uint64_t party_id) {
    protocol::PartyResult msg;
    msg.op = op;
    msg.code = code;
    msg.partyId = party_id;
    deliverToClient(session_id, protocol::MessageCodec::encode(msg, session_id));
}

void WorldApp::deliverSquadResult(std::uint64_t session_id, std::int32_t op,
                                  std::int32_t code,
                                  const squad_instance::ActiveSquadInstance* binding) {
    protocol::SquadResult msg;
    msg.op = op;
    msg.code = code;
    msg.partyId = binding ? binding->party_id : 0;
    msg.instanceId = binding ? binding->instance_id : 0;
    msg.worldId = binding ? binding->world_id : 0;
    msg.sceneId = binding ? binding->scene_id : 0;
    deliverToClient(session_id, protocol::MessageCodec::encode(msg, session_id));
}

void WorldApp::deliverSquadStateToParty(std::uint64_t party_id) {
    const auto* binding = squadInstanceService_ ? squadInstanceService_->find_by_party(party_id)
                                                : nullptr;
    if (!binding || !world_ || !worldSessionManager_) return;
    const auto* instance = world_->find_instance(binding->instance_id);
    if (!instance) return;
    const auto progress = squadInstanceService_->gameplay(party_id);

    protocol::SquadState msg;
    msg.partyId = binding->party_id;
    msg.instanceId = binding->instance_id;
    msg.phase = static_cast<std::int32_t>(instance->state());
    msg.monstersDefeated = progress ? progress->monsters_defeated : 0;
    msg.ticks = static_cast<protocol::EntityID>(instance->tick_count());
    msg.goalSet = progress && progress->goal_set ? 1 : 0;
    if (progress) {
        msg.monsters.assign(progress->monsters_alive.begin(),
                            progress->monsters_alive.end());
    }

    // 名册全员状态广播：会话解析得到的成员才收（离线成员随恢复面补推）
    for (const auto member : binding->members) {
        const auto session = worldSessionManager_->find_by_player(
            apollo::game::core::PlayerId{member});
        if (!session) continue;
        deliverToClient(session->session_id(),
                        protocol::MessageCodec::encode(msg, session->session_id()));
    }
}

std::vector<uint8_t> WorldApp::handlePartyCreate(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    const auto player_id = resolveIntentPlayer(header.sessionId);
    if (player_id == 0) {
        worldLog().warning("party create from unknown session "
                           + std::to_string(header.sessionId));
        return {};
    }
    // 服务端分配 party_id（契约 23 语义）；首位成员即队长
    const auto party_id = nextPartyId_++;
    const auto* party = squadParties_.create(party_id, player_id);
    deliverPartyResult(header.sessionId,
                       static_cast<std::int32_t>(protocol::SquadOp::PARTY_CREATE),
                       party != nullptr ? 0 : 2001, party_id);
    return {};
}

std::vector<uint8_t> WorldApp::handlePartyJoin(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());
    auto msg = protocol::MessageCodec::decodeBody<protocol::PartyJoinRequest>(bodyData);

    const auto player_id = resolveIntentPlayer(header.sessionId);
    if (player_id == 0) {
        worldLog().warning("party join from unknown session "
                           + std::to_string(header.sessionId));
        return {};
    }
    auto* party = squadParties_.find(msg.partyId);
    std::int32_t code = 0;
    if (party == nullptr) {
        code = 1101;  // squad_party_not_found
    } else if (party->has_member(player_id)) {
        code = 1111;  // squad_duplicate_enter
    } else if (!party->add_member(player_id)) {
        code = 1103;  // squad_party_too_large
    }
    deliverPartyResult(header.sessionId,
                       static_cast<std::int32_t>(protocol::SquadOp::PARTY_JOIN), code,
                       msg.partyId);
    return {};
}

std::vector<uint8_t> WorldApp::handleSquadLaunch(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());
    auto msg = protocol::MessageCodec::decodeBody<protocol::SquadLaunch>(bodyData);

    const auto player_id = resolveIntentPlayer(header.sessionId);
    if (player_id == 0) {
        worldLog().warning("squad launch from unknown session "
                           + std::to_string(header.sessionId));
        return {};
    }
    squad_instance::LaunchRequest launch;
    launch.party_id = msg.partyId;
    launch.requester_id = player_id;
    launch.map_id = msg.mapId != 0 ? msg.mapId : 1;

    const auto result = squadInstanceService_->launch(launch);
    std::int32_t code = 0;
    switch (result.error) {
        case squad_instance::LaunchError::Ok: break;
        case squad_instance::LaunchError::InvalidRequest: code = 1100; break;
        case squad_instance::LaunchError::PartyNotFound: code = 1101; break;
        case squad_instance::LaunchError::NotLeader: code = 1102; break;
        case squad_instance::LaunchError::PartyTooLarge: code = 1103; break;
        case squad_instance::LaunchError::MemberOffline: code = 1104; break;
        case squad_instance::LaunchError::MemberBusy: code = 1105; break;
        case squad_instance::LaunchError::NoWorldCapacity: code = 1106; break;
        case squad_instance::LaunchError::SpawnRejected: code = 1107; break;
        case squad_instance::LaunchError::Draining: code = 1118; break;
    }
    const auto* binding = squadInstanceService_->find_by_party(msg.partyId);
    deliverSquadResult(header.sessionId,
                       static_cast<std::int32_t>(protocol::SquadOp::SQUAD_LAUNCH), code, binding);
    if (code == 0) deliverSquadStateToParty(msg.partyId);
    return {};
}

std::vector<uint8_t> WorldApp::handleSquadEnter(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());
    auto msg = protocol::MessageCodec::decodeBody<protocol::SquadEnter>(bodyData);

    const auto player_id = resolveIntentPlayer(header.sessionId);
    if (player_id == 0) {
        worldLog().warning("squad enter from unknown session "
                           + std::to_string(header.sessionId));
        return {};
    }
    std::int32_t code = 0;
    switch (squadInstanceService_->enter(msg.partyId, player_id)) {
        case squad_instance::SquadInstanceService::EnterError::Ok: break;
        case squad_instance::SquadInstanceService::EnterError::NoBinding: code = 1108; break;
        case squad_instance::SquadInstanceService::EnterError::NotLocal: code = 2001; break;
        case squad_instance::SquadInstanceService::EnterError::NotRoster: code = 1109; break;
        case squad_instance::SquadInstanceService::EnterError::StateRejected: code = 1110; break;
        case squad_instance::SquadInstanceService::EnterError::Duplicate: code = 1111; break;
        case squad_instance::SquadInstanceService::EnterError::NotLeader: code = 1102; break;
        case squad_instance::SquadInstanceService::EnterError::NotAllEntered: code = 1112; break;
    }
    const auto* binding = squadInstanceService_->find_by_party(msg.partyId);
    deliverSquadResult(header.sessionId,
                       static_cast<std::int32_t>(protocol::SquadOp::SQUAD_ENTER), code, binding);
    if (code == 0) deliverSquadStateToParty(msg.partyId);
    return {};
}

std::vector<uint8_t> WorldApp::handleSquadStart(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());
    auto msg = protocol::MessageCodec::decodeBody<protocol::SquadStart>(bodyData);

    const auto player_id = resolveIntentPlayer(header.sessionId);
    if (player_id == 0) {
        worldLog().warning("squad start from unknown session "
                           + std::to_string(header.sessionId));
        return {};
    }
    std::int32_t code = 0;
    switch (squadInstanceService_->start_gameplay(msg.partyId, player_id)) {
        case squad_instance::SquadInstanceService::EnterError::Ok: break;
        case squad_instance::SquadInstanceService::EnterError::NoBinding: code = 1108; break;
        case squad_instance::SquadInstanceService::EnterError::NotLocal: code = 2001; break;
        case squad_instance::SquadInstanceService::EnterError::NotRoster: code = 1109; break;
        case squad_instance::SquadInstanceService::EnterError::StateRejected: code = 1110; break;
        case squad_instance::SquadInstanceService::EnterError::Duplicate: code = 1111; break;
        case squad_instance::SquadInstanceService::EnterError::NotLeader: code = 1102; break;
        case squad_instance::SquadInstanceService::EnterError::NotAllEntered: code = 1112; break;
    }
    if (code == 0) {
        // 宿主玩法内容（骨架）：启动即设默认目标 + 首波怪（击杀配额/波次
        // 数值随玩法批；本批打通击杀上报→battle 分值→结算全链）
        squad_instance::GameplayGoal goal;
        goal.monsters_to_defeat = kHostGoalMonsters;
        goal.timeout_ticks = kHostGoalTimeoutTicks;
        (void)squadInstanceService_->set_goal(msg.partyId, goal);
        (void)squadInstanceService_->spawn_wave(msg.partyId, kHostGoalMonsters);
    }
    const auto* binding = squadInstanceService_->find_by_party(msg.partyId);
    deliverSquadResult(header.sessionId,
                       static_cast<std::int32_t>(protocol::SquadOp::SQUAD_START), code, binding);
    if (code == 0) deliverSquadStateToParty(msg.partyId);
    return {};
}

std::vector<uint8_t> WorldApp::handleSquadKill(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());
    auto msg = protocol::MessageCodec::decodeBody<protocol::SquadKill>(bodyData);

    const auto player_id = resolveIntentPlayer(header.sessionId);
    if (player_id == 0) {
        worldLog().warning("squad kill from unknown session "
                           + std::to_string(header.sessionId));
        return {};
    }
    std::int32_t code = 0;
    switch (squadInstanceService_->report_monster_defeated(
        msg.partyId, msg.monsterEntityId, player_id)) {
        case squad_instance::SquadInstanceService::GameplayError::Ok: break;
        case squad_instance::SquadInstanceService::GameplayError::NoBinding: code = 1108; break;
        case squad_instance::SquadInstanceService::GameplayError::NotLocal: code = 2001; break;
        case squad_instance::SquadInstanceService::GameplayError::NotRunning: code = 1114; break;
        case squad_instance::SquadInstanceService::GameplayError::InvalidRequest: code = 1100; break;
        case squad_instance::SquadInstanceService::GameplayError::InvalidGoal: code = 1113; break;
        case squad_instance::SquadInstanceService::GameplayError::AlreadySet: code = 1113; break;
        case squad_instance::SquadInstanceService::GameplayError::NoGoal: code = 1114; break;
        case squad_instance::SquadInstanceService::GameplayError::WaveTooLarge: code = 1115; break;
        case squad_instance::SquadInstanceService::GameplayError::UnknownMonster: code = 1116; break;
    }
    const auto* binding = squadInstanceService_->find_by_party(msg.partyId);
    deliverSquadResult(header.sessionId,
                       static_cast<std::int32_t>(protocol::SquadOp::SQUAD_KILL), code, binding);
    if (code == 0) deliverSquadStateToParty(msg.partyId);
    return {};
}

void WorldApp::driveSquadGameplay(double delta_seconds) {
    if (!squadInstanceService_ || !world_) return;
    // 逐拍推进全部本地 Running 副本（远端绑定随跨进程条目）；结算期副本
    // 收尾（dismiss：移出玩家 → 终态链 → 双回收）。快照遍历——收尾会改
    // 绑定表。
    for (const auto& binding : squadInstanceService_->activeBindings()) {
        if (binding.world_id != worldId_) continue;
        auto* instance = world_->find_instance(binding.instance_id);
        if (instance == nullptr) continue;
        if (instance->state() == apollo::game::world::Instance::State::Running) {
            (void)squadInstanceService_->advance_tick(binding.party_id, 1, delta_seconds);
            instance = world_->find_instance(binding.instance_id);
            if (instance == nullptr) continue;  // 失败线收尾可能已回收
        }
        const auto state = instance->state();
        if (state >= apollo::game::world::Instance::State::Finishing &&
            state != apollo::game::world::Instance::State::Destroyed) {
            // 终态回执取数在 dismiss 前（binding/instance 随收尾释放）
            const auto party_id = binding.party_id;
            const auto instance_id = binding.instance_id;
            const auto members = binding.members;
            if (!squadInstanceService_->dismiss(party_id)) continue;
            protocol::SquadState final;
            final.partyId = party_id;
            final.instanceId = instance_id;
            final.phase = static_cast<std::int32_t>(
                apollo::game::world::Instance::State::Destroyed);
            final.monstersDefeated = 0;
            final.ticks = 0;
            final.goalSet = 0;
            for (const auto member : members) {
                const auto session = worldSessionManager_->find_by_player(
                    apollo::game::core::PlayerId{member});
                if (!session) continue;
                deliverToClient(session->session_id(),
                                protocol::MessageCodec::encode(
                                    final, session->session_id()));
            }
        }
    }
}

void WorldApp::executeIntent(const IntentEnvelope& intent) {
    // 执行面（ADR-015(a)）：gameThread_ tick 边界——复用既有 handler 全量
    // 语义（解码+校验+world 变更+广播）；变更类返回值为空 ack，丢弃
    switch (intent.type) {
        case protocol::MessageType::CELL_CREATE_ENTITY:
            (void)handleCellCreateEntity(intent.frame);
            break;
        case protocol::MessageType::CELL_DESTROY_ENTITY:
            (void)handleCellDestroyEntity(intent.frame);
            break;
        case protocol::MessageType::CELL_ENTITY_MOVE:
            (void)handleCellEntityMove(intent.frame);
            break;
        case protocol::MessageType::CELL_CROSS_BORDER:
            (void)handleCellCrossBorder(intent.frame);
            break;
        case protocol::MessageType::COMBAT_SKILL_CAST:
            (void)handleCombatSkillCast(intent.frame);
            break;
        // 队伍与小队副本族（P2 真实链路）：受理面全量语义 + 结果/状态下行
        case protocol::MessageType::PARTY_CREATE_REQUEST:
            (void)handlePartyCreate(intent.frame);
            break;
        case protocol::MessageType::PARTY_JOIN_REQUEST:
            (void)handlePartyJoin(intent.frame);
            break;
        case protocol::MessageType::SQUAD_LAUNCH:
            (void)handleSquadLaunch(intent.frame);
            break;
        case protocol::MessageType::SQUAD_ENTER:
            (void)handleSquadEnter(intent.frame);
            break;
        case protocol::MessageType::SQUAD_START:
            (void)handleSquadStart(intent.frame);
            break;
        case protocol::MessageType::SQUAD_KILL:
            (void)handleSquadKill(intent.frame);
            break;
        default:
            break;
    }

    // trace 子集（P3-3 语义层批）：采样命中的意图发 WorldApp span（受理→执行
    // 完成全程；parent = 发端 gateway span id，随 932 信封上行）。未采样
    // /bare 路径零开销跳过——emit_span 自守门，此处只备料。
    if (intent.trace.has_trace() && intent.trace.sampled) {
        trace::SpanRecord span;
        span.name = "world_intent_execute";
        span.ctx = intent.trace;
        span.span_id = trace::generate_span_id();
        span.start_ms = intent.received_ms;
        span.end_ms = trace::now_ms();
        trace::emit_span(span);
    }

    // ADR-017 延时应答（M1 升级落地）：意图被消费后按 request_id 回执
    //（code 0 = 已消费；业务驳回语义仍在 handler 面内，不下沉回执码）；
    // legacy 路径（request_id = 0）在此静默，ack-on-accept 现状不变。
    sendM1Ack(intent.request_id, kIntentAckOk, "");
}

void WorldApp::drainAcceptanceQueue() {
    while (auto intent = acceptanceQueue_.try_pop()) {
        // L69/L42 兼容底线：畸形意图（残帧头/残 body）只丢本条（已出队，
        // 不堵队列、不炸 gameLoop）；无延时回执=请求方按超时口径收敛
        // （P2 分离面的回执纪律余量）。
        try {
            executeIntent(*intent);
        } catch (const std::exception& error) {
            worldLog().warning("malformed intent dropped: type=" +
                           std::to_string(static_cast<int>(intent->type)) +
                           " reason=" + error.what());
        }
    }
}

void WorldApp::gameLoop() {
    // 脚本宿主生命周期（scripting-lua §2 一状态一逻辑线程）：runtime 建/
    // 用/析全在本线程。boot 源加载失败不停机（fault 走 runtime 事件面，
    // cell 继续服务）。
    script_ = std::make_unique<apollo::script::ScriptRuntime>();
    // 启动装载序（scripting-lua §3.4）：handler 面 → 契约 → boot 源。契约
    // 装载闸逐 route resolve_handler——handler 未先行在位即拒载；boot 源
    // 可读 contract 全局。任一步失败只发 warning，cell 继续服务（同 boot
    // 源纪律；fault 走 runtime 事件面）。
    if (!config_.scriptHandersFile.empty()) {
        const std::optional<std::string> handlers = read_script_file(config_.scriptHandersFile);
        if (!handlers) {
            worldLog().warning("script handlers file missing: " + config_.scriptHandersFile);
        } else if (!script_->call_string("cell_handlers", *handlers)) {
            worldLog().warning("script handlers load failed: " + config_.scriptHandersFile);
        }
    }
    if (!config_.scriptContractFile.empty()) {
        const std::optional<std::string> contract = read_script_file(config_.scriptContractFile);
        if (!contract) {
            worldLog().warning("script contract file missing: " + config_.scriptContractFile);
        } else if (!script_->load_contract(*contract)) {
            worldLog().warning("script contract load failed: " + config_.scriptContractFile);
        }
    }
    if (!config_.scriptBootFile.empty()) {
        const std::optional<std::string> boot = read_script_file(config_.scriptBootFile);
        if (!boot) {
            worldLog().warning("script boot file missing: " + config_.scriptBootFile);
        } else if (!script_->call_string("cell_boot", *boot)) {
            worldLog().warning("script boot failed: " + config_.scriptBootFile);
        }
    }

    while (running_) {
        auto startTime = std::chrono::steady_clock::now();

        // net M1 接收面一拍（2b-ii）：链路泵与受理队列入队同线程（单写者
        // 纪律，ADR-015(a)）；回执发送在后续拍的泵内上线
        if (m1Link_) {
            m1Link_->pump(std::chrono::duration_cast<std::chrono::milliseconds>(
                startTime.time_since_epoch()).count());
        }

        // ADR-015(a)：意图消费在 tick 边界（world 变更只发生在此线程）
        drainAcceptanceQueue();

        // 脚本宿主接线：未挂的 scene 幂等补挂（新 scene 下一拍生效）
        attach_script_to_scenes();

        worldHost_->tick();

        // 小队副本玩法拍（P2 真实链路）：Running 副本 tick 推进（含已受理
        // 击杀的 battle 输入序）+ 结算期副本收尾；状态帧随后续操作/收尾下发
        driveSquadGameplay(static_cast<double>(tickInterval().count()) / 1000.0);

        // World 排空推进（P4 更新排空批；driveSquadGameplay 后拍——本拍结算
        // 收尾先推进，归零判定用新值）
        driveWorldDrain();

        // §7.1 eval 回执收账（tick 内固定调试点已执行在途 eval）
        drainDebugEvalResults();

        // 952 编排推进（P4 仅代码更新批）：在装模块 onTick + 观察窗/推广/
        // 回滚队列（drainDebugEvalResults 同拍位，owning 线程纪律）
        driveModuleRollout();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime
        ).count();

        // 检测原语接线（metrics admin 通道批）：逐拍快照的源端消费
        record_metrics(elapsed);

        const auto interval = tickInterval();
        const auto sleepMs = static_cast<int>(interval.count() - elapsed);
        if (sleepMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
        }
    }

    // 停机 G-3 尾扫：关闸后残余意图一次性清空（此后不再有新受理）
    drainAcceptanceQueue();

    // 脚本宿主随 gameThread_ 退出而灭（scripting-lua §2）：先摘全部 scene
    // 再析构 runtime，避免 scene 侧残悬引用。
    for (auto* scene : world_->scenes()) {
        scene->detach_script();
    }
    script_.reset();
}

void WorldApp::attach_script_to_scenes() {
    if (!script_) {
        return;
    }
    for (auto* scene : world_->scenes()) {
        if (!scene->script_attached()) {
            scene->attach_script(*script_);
        }
    }
}

apollo::game::world::Scene* WorldApp::find_scene(std::uint64_t scene_id) {
    return world_ ? world_->find_scene(scene_id) : nullptr;
}

std::chrono::milliseconds WorldApp::tickInterval() const {
    return std::chrono::milliseconds(std::max(config_.tickRateMs, 1));
}

void WorldApp::record_metrics(std::int64_t elapsed_ms) {
    auto& registry = apollo::core::metrics::MetricRegistry::instance();
    // 末拍瞬时值走 gauge（_last 后缀与同名直方图族分居——同族双 TYPE
    // 相撞是出口面 scrape 错误）
    registry.gauge("world_tick_frame_ms_last").store(elapsed_ms);
    // 帧耗时分布（P3-3 余项批）：桶位 ms 档覆盖亚毫秒抖动到秒级卡顿，
    // 20ms = ADR-016 消息段预算锚点
    static const std::vector<double> kFrameMsBuckets = {
        0.1, 0.5, 1, 2, 5, 10, 20, 50, 100, 500, 1000};
    registry.histogram("world_tick_frame_ms", {}, kFrameMsBuckets)
        .observe(static_cast<double>(elapsed_ms));
    registry.gauge("world_queue_depth")
        .store(static_cast<std::int64_t>(acceptanceQueue_.size()));
    worldAppTicksTotal_.increment(1);
    // M1 受理/拒收为进程期只增原子（m1_link_server 绝对值）——增量推进
    counter_advance_to(worldAppIntentsReceivedTotal_, lastIntentsReceived_,
                       m1Link_ ? m1Link_->receivedIntents() : 0);
    counter_advance_to(worldAppFramesRejectedTotal_, lastFramesRejected_,
                       m1Link_ ? m1Link_->rejectedFrames() : 0);
    for (const auto* scene : world_->scenes()) {
        const std::string scene_label = std::to_string(scene->scene_id());
        registry.gauge("world_entities", {{"scene", scene_label}})
            .store(static_cast<std::int64_t>(scene->get_entity_count()));
        registry.gauge("world_scene_tick", {{"scene", scene_label}})
            .store(static_cast<std::int64_t>(scene->tick_count()));
    }
}

void WorldApp::broadcastToViewers(EntityID entity_id, const std::vector<uint8_t>& message) {
    // P1-3：viewer set 驱动（旧 aoiManager_->getViewers 九宫格查询退役）——
    // 接收集合 = 实体所在 Scene 的 SceneAoi 视野查询（AOI 归 scene、scene_id
    // 隔离）；网关下发面随 P3-2（当前阶段下发 = 可观察日志）
    (void)message;
    apollo::game::world::Scene* scene = find_scene_by_entity(entity_id);
    if (scene == nullptr) {
        return;
    }
    const auto viewers = scene->aoi().viewers_of(apollo::game::core::EntityId(entity_id));
    if (!viewers.empty()) {
        worldLog().info("broadcast: entity=" + std::to_string(entity_id) + " scene="
                    + std::to_string(scene->scene_id()) + " viewers="
                    + std::to_string(viewers.size()) + " bytes="
                    + std::to_string(message.size()));
    }
}

std::uint64_t WorldApp::resolve_scene_id(std::uint64_t space_id) const {
    // 单进程单 scene 一条目（P1-1 口径）；未知空间回落默认 scene——
    // 与 create 的旧「空间无关全落全局表」行为等价
    const auto route = spaceToScene_.find(space_id);
    return route != spaceToScene_.end() ? route->second : defaultSceneId_;
}

apollo::game::world::Scene* WorldApp::find_scene_by_entity(EntityID entity_id) {
    // 玩家实体 → Avatar 实体号反查；非玩家实体 → Scene 实体集合直查
    //（P0-2 强分型 + P2-1b 实体集合归 Scene；实体号唯一归属一个 scene）
    const auto avatar_entity_id = apollo::game::core::EntityId(entity_id);
    for (apollo::game::world::Scene* scene : world_->scenes()) {
        if (scene == nullptr) {
            continue;
        }
        if (scene->get_entity(avatar_entity_id) != nullptr) {
            return scene;
        }
        for (const auto& player_id : scene->avatars()) {
            const auto avatar = scene->get_avatar(player_id);
            if (avatar && avatar->entity_id() == avatar_entity_id) {
                return scene;
            }
        }
    }
    return nullptr;
}

void WorldApp::ensureDefaultScene() {
    // P0-3：world.create_scene 路径（替代旧 ensureDefaultMapInstance 三层壳）。
    // 最小闭环：1 scene（descriptor 绑定地图资产）+ 1 instance（八态推进至 Running）。
    if (world_->find_scene(defaultSceneId_) != nullptr) {
        return;
    }

    apollo::game::world::SceneDescriptor descriptor;
    descriptor.map_id = 1;
    descriptor.map_name =
        config_.spaceName.empty() ? std::string("default-world") : config_.spaceName;
    descriptor.width = config_.spaceWidth;
    descriptor.height = config_.spaceHeight;
    descriptor.grid_size = config_.gridSize;
    descriptor.view_radius = config_.viewRadius;

    auto* scene = world_->create_scene(descriptor);
    if (scene == nullptr) {
        throw std::runtime_error("failed to create default scene (invalid descriptor)");
    }
    defaultSceneId_ = scene->scene_id();

    defaultInstance_ = world_->create_instance(defaultSceneId_, "default");
    // 八态推进至 Running（Create→Initialize→Waiting→Running；逐态校验）
    defaultInstance_->initialize();
    defaultInstance_->ready();
    defaultInstance_->start();

    // space → scene 路由登记（P1-1 换幕目标解析；多 scene 随 descriptor 扩充）
    spaceToScene_[defaultSpaceId_] = defaultSceneId_;
}

void WorldApp::attachPlayerWorldSession(
    protocol::SessionID session_id,
    protocol::PlayerID player_id_raw,
    EntityID entity_id,
    const Position& position) {
    ensureDefaultScene();

    // 强分型（P0-2）：玩家身份与实体身份在类型面上分离——旧代码 entity_id 直接
    // 当 player_id 传（find_by_player/attach 三参同号），编译器不可见。
    const auto player_id = apollo::game::core::PlayerId(player_id_raw);
    const auto avatar_entity_id = apollo::game::core::EntityId(entity_id);

    auto session = worldSessionManager_->find_session(session_id);
    if (!session) {
        session = worldSessionManager_->create_session(session_id, player_id);
    }

    auto* scene = world_->find_scene(defaultSceneId_);

    // create Avatar + scene.enter / instance.enter（P0-3 任务书 §27 API 路径：
    // Avatar 由 Scene 持有（进 scene 生/出 scene 死），玩法侧走 instance.enter）
    auto avatar = std::make_shared<apollo::game::world::Avatar>(player_id, avatar_entity_id);
    if (scene != nullptr) {
        scene->enter(avatar, {position.x, position.y, position.z});
    }
    if (defaultInstance_) {
        defaultInstance_->enter(player_id);
    }

    session->assign_world(worldId_);
    session->assign_map_instance(defaultInstance_ ? defaultInstance_->id() : 0);
    // P0-2：space 与 instance 分离——space 承载 id 独立（旧代码以 map instance
    // 号兼任 space 号）。P0-3 拆 SceneDescriptor 后由 scene 提供。
    session->assign_space(defaultSpaceId_);
    session->bind_avatar(avatar_entity_id);
    session->set_route_version(session->route_version() + 1);
    session->set_state(apollo::game::world::WorldSessionState::Entering);
    // Entering → Active（resume 唯一合法前态；Closed/Leaving 会拒绝——接线异常
    // 的可观察面，不静默吞）
    if (!session->resume()) {
        worldLog().warning("world session resume rejected for session "
                    + std::to_string(session_id));
    }
}

void WorldApp::detachPlayerWorldSession(protocol::SessionID session_id, EntityID entity_id) {
    auto session = worldSessionManager_->find_session(session_id);
    if (!session) {
        // 无会话 id 时按玩家身份反查（P0-2/P0-3：经 Scene 的 Avatar 容器由
        // 实体号定位玩家号，再按玩家查会话；不再把实体号当玩家号传）
        const auto avatar_entity_id = apollo::game::core::EntityId(entity_id);
        auto* scene = world_->find_scene(defaultSceneId_);
        if (scene != nullptr) {
            for (const auto& player_id : scene->avatars()) {
                const auto avatar = scene->get_avatar(player_id);
                if (avatar && avatar->entity_id() == avatar_entity_id) {
                    session = worldSessionManager_->find_by_player(player_id);
                    break;
                }
            }
        }
    }

    if (!session) {
        return;
    }

    // 出 scene 死：Avatar 由 scene.leave 解除归属并出 AOI（对象随 shared_ptr
    // 释放——scene 是唯一持有者，player-object-model §3「出 scene 死」）
    const auto player_id = session->player_id();
    auto* scene = world_->find_scene(defaultSceneId_);
    if (scene != nullptr) {
        scene->leave(player_id);
    }
    if (defaultInstance_) {
        defaultInstance_->leave(player_id);
    }

    worldSessionManager_->suspend_session(session->session_id());
    worldSessionManager_->close_session(session->session_id());
    // close 两段式收尾（P0-4）：Leaving 可观察窗口后显式终结（Closed 终态、
    // 摘除双索引）；WorldApp 侧登出无延迟清理面，窗口内即 finalize。
    worldSessionManager_->finalize_session(session->session_id());
}

} // namespace world_app
