#include "world_app/cell_app.hpp"
#include "player_store/business_wire.hpp"
#include "player_store/store_dial.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/net/linktrace/span_emitter.hpp"
#include "apollo/protocol/messages.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/game/world/instance.hpp"
#include "apollo/game/world/scene.hpp"
#include "apollo/game/world/scene_transfer.hpp"
#include "apollo/script/script_runtime.hpp"
#include "apollo_contract.h"
#include <algorithm>
#include <fstream>
#include <optional>
#include <stdexcept>

#include "cell_app_detail.hpp"

namespace world_app {

namespace protocol = apollo::protocol;

// CellApp 拆分实现单元共用日志出口（LAY-001；声明见 cell_app_detail.hpp）
apollo::core::log::Logger& cell_detail::world_log() {
    return *apollo::core::log::global_log_manager().createLogger("world_app");
}

namespace {

// 本 TU 沿用的日志出口别名（LAY-001 拆分面；单一定义见 cell_app.cpp）
apollo::core::log::Logger& worldLog() {
    return cell_detail::world_log();
}

// 读整个 Lua 源文件（binary 模式——Windows CRLF 落盘纪律）；缺文件返回
// std::nullopt。仅启动装载序调用（内核线程首拍，§2 一状态一逻辑线程）。
std::optional<std::string> read_script_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

// 世界 tick 服务适配（P0-3）：世界 tick = 逐 scene 六阶段
//（simulate/recalc/aoidecay/collect/budget+flush/persist-batch）
class CellWorldService final : public apollo::runtime::IWorldService {
public:
    CellWorldService(
        apollo::game::world::World& world,
        WorldAppConfig config)
        : world_(world)
        , config_(std::move(config)) {
    }

    std::string_view service_name() const override {
        return "CellWorldService";
    }

    bool initialize() override {
        tick_count_ = 0;
        return true;
    }

    void tick(const apollo::runtime::WorldTickContext& context) override {
        tick_count_ = context.tick_index;
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
// CellApp 实现（cell 执行内核）
//==============================================================================

CellApp::CellApp(const WorldAppConfig& config)
    : config_(config)
    , world_(std::make_unique<apollo::game::world::World>())
    , worldSessionManager_(std::make_shared<apollo::game::world::WorldSessionManager>())
    , worldHost_(std::make_shared<apollo::runtime::WorldHost>(
          static_cast<std::uint32_t>(1000 / std::max(config.tickRateMs, 1)))) {
    worldId_ = config.worldId;
    worldHost_->add_world_service(
        std::make_shared<CellWorldService>(*world_, config_));
    // 小队副本受理面（P2）：注册本 World 容量；在线谓词按本进程会话面
    // 裁定（权威在线目录在 Login 侧，跨进程校验随后续条目接通）。
    squadInstanceService_ = std::make_unique<squad_instance::SquadInstanceService>(
        squadParties_, world_.get(), config_.worldId);
    // 资料归属切换（L19）：档案宿主 = base，内核不持有 PlayerStore
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

CellApp::~CellApp() {
    stop();
}

void CellApp::start() {
    if (running_) return;

    ensureDefaultScene();

    // L100 多 Space 声明面：config 声明的额外 Space 在启动窗逐条登记（同
    // add_space 幂等语义；Instance-less——只建 scene + 路由，实例随玩法
    // 副本流程自建）。id 缺位/与默认 Space 相撞、descriptor 失效 = 启动
    // 失败显形（不带隐式回退）。
    for (const auto& decl : config_.spaces) {
        if (decl.space_id == 0) {
            throw std::runtime_error("declared space id 0 invalid");
        }
        if (decl.space_id == defaultSpaceId_) {
            throw std::runtime_error("declared space id "
                                     + std::to_string(decl.space_id)
                                     + " collides with default space");
        }
        const auto descriptor = space_descriptor_for(config_, decl.space_id, decl.name);
        if (add_space(decl.space_id, descriptor) == 0) {
            throw std::runtime_error("failed to create declared space "
                                     + std::to_string(decl.space_id)
                                     + " (invalid descriptor)");
        }
        worldLog().info("declared space: id=" + std::to_string(decl.space_id) + " name="
                        + descriptor.map_name);
    }

    if (!worldHost_->start()) {
        throw std::runtime_error("failed to start WorldHost");
    }

    // Base 业务受理面（P1 结算迁入）：结算落账 = 返回业务请求拨 base
    // store M1 面（958/959——资料归属切换后锚点写权在 Base）。拨号装配
    // 失败降级（警告 + 挂账面照旧），不拦启动。Sink 在内核线程结算段
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

    // 结算凭据持久化（L67 去重凭据面生产接线）：运行目录持久提交
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

    // 开闸先于宿主传输监听（WorldApp::start 随后挂 M1 端点）：受理面就绪
    // 后网关才见得到入口
    accepting_ = true;
    running_ = true;

    // 启动内核循环线程
    gameThread_ = std::thread(&CellApp::gameLoop, this);

    worldLog().info("Space: " + config_.spaceName + " ("
                + std::to_string(config_.spaceWidth) + "x"
                + std::to_string(config_.spaceHeight) + ")");
}

void CellApp::set_frozen(bool frozen) {
    // 冻结面（L101 失联策略切片；§4：宽限期到期 tick 边界冻结 + IO 层关新
    // 受理；解冻只随 961 复核，宿主监听器调用本面）。调用上下文 = pre-tick
    // 泵上下文（内核线程）或测试线程——frozen_/accepting_ 均 atomic。
    const bool was = frozen_.exchange(frozen, std::memory_order_acq_rel);
    if (was == frozen) {
        return;
    }
    if (frozen) {
        accepting_ = false;  // IO 层关闭新输入受理（新意图 2001）
        worldLog().warning("kernel frozen: simulation paused (control link lost)");
    } else {
        accepting_ = running_.load(std::memory_order_acquire);  // 闸随解冻恢复
        worldLog().info("kernel resumed: re-admitted by world");
    }
}

void CellApp::stop() {
    // 停机 G-3 镜像顺序（ADR-016）：停受理 → 队列尾扫（内核循环退出前）→
    // join → 拨号面收口 → worldHost stop。延时应答经 egress 缝——宿主传输
    // 面由 WorldApp::stop 在本调用返回后停（尾扫回执仍要有发送面）。
    // 停机语义覆盖冻结：尾扫照常清队（冻结只是暂停模拟推进，非停机意图）。
    accepting_ = false;
    running_ = false;

    if (gameThread_.joinable()) {
        gameThread_.join();
    }

    // Base 业务拨号面随内核线程收口（结算面唯一调用者已 join）。
    if (baseBusinessDial_) {
        baseBusinessDial_->shutdown();
        baseBusinessDial_.reset();
    }

    // 待撤销重试表随内核线程收口清空（消费者已 join，无竞争；未完成的
    // 撤销由 World 侧权威视图兜底——重注册收敛）
    {
        std::lock_guard<std::mutex> lock(pendingRevokesMutex_);
        pendingRevokes_.store(0, std::memory_order_release);
        pendingRevokesSet_.clear();
    }

    if (worldHost_) {
        worldHost_->stop();
    }
}

bool CellApp::submitIntent(IntentEnvelope intent) {
    // 受理点判定（ADR-017 决策 1，M1 面与测试共用）：只入队意图信封，
    // 不做 world 读写；回执纪律见 onIntent。
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

void CellApp::onIntent(IntentEnvelope intent) {
    // M1 受理点（泵线程 = 内核线程）：submitIntent 判定 + 拒收即时回执。
    // 延时应答（ADR-017）：受理成功不回包，回执随 executeIntent 消费后按
    // request_id 回带；即时回执面只剩两类永不入队的拒收。
    const auto request_id = intent.request_id;
    if (submitIntent(std::move(intent))) {
        return;
    }
    if (accepting_.load(std::memory_order_acquire)) {
        // ADR-016：顶格拒收不丢旧不静默——M1 面回契约错误码 2101
        sendAck(request_id, kIntentAckBackpressure,
                "server overloaded: acceptance queue full");
    } else {
        // 维护闸：start 前 / stop 后一律回 2001（窄窗竞态下受理判定与闸状态
        // 间取回执时点状态，与拒收同型观察面）
        sendAck(request_id, kIntentAckInternal,
                "server maintenance: not accepting requests");
    }
}

void CellApp::sendAck(std::uint64_t request_id, std::int32_t code, const std::string& message) {
    if (request_id == 0) {
        return;  // legacy 路径（ack-on-accept）无应答关联键
    }
    if (egress_) {
        egress_->sendIntentAck(request_id, code, message);
    }
}

void CellApp::deliverToClient(std::uint64_t session_id, const std::vector<uint8_t>& frame) {
    if (!egress_ || frame.empty()) return;  // 链路未装配 = 尽力面落地
    egress_->sendClientDeliver(session_id, 0, frame);
}

void CellApp::executeIntent(const IntentEnvelope& intent) {
    // 执行面（ADR-015(a)）：内核线程 tick 边界——复用既有 handler 全量
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

    // 客户端消息 → Lua 消费端（L51 接线）：C++ handler 先行（权威状态），
    // 随后按契约路由带参分发——场景脚本在此挂玩法消费/推送源。
    dispatch_script_intent(intent);

    // trace 子集（P3-3 语义层批）：采样命中的意图发 span（受理→执行全程；
    // parent = 发端 gateway span id，随 932 信封上行）。未采样 /bare 路径
    // 零开销跳过——emit_span 自守门，此处只备料。
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
    sendAck(intent.request_id, kIntentAckOk, "");
}

void CellApp::drainAcceptanceQueue() {
    while (auto intent = acceptanceQueue_.try_pop()) {
        // L69/L42 兼容底线：畸形意图（残帧头/残 body）只丢本条（已出队，
        // 不堵队列、不炸 gameLoop）；无延时回执=请求方按超时口径收敛
        //（P2 分离面的回执纪律余量）。
        try {
            executeIntent(*intent);
        } catch (const std::exception& error) {
            worldLog().warning("malformed intent dropped: type=" +
                           std::to_string(static_cast<int>(intent->type)) +
                           " reason=" + error.what());
        }
    }
}

void CellApp::gameLoop() {
    // 脚本宿主生命周期（scripting-lua §2 一状态一逻辑线程）：runtime 建/
    // 用/析全在本线程。boot 源加载失败不停机（fault 走 runtime 事件面，
    // cell 继续服务）。
    script_ = std::make_unique<apollo::script::ScriptRuntime>();
    if (faultSink_) {
        script_->set_fault_sink(faultSink_);
    }
    // world 模块先于 handler/契约/boot 落地（§4.1 启动期注入唯一通道）——
    // handler 面与 boot 源即可用 world.* 挂推送源。
    register_world_module();
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

        // 拍首钩子（宿主传输泵一拍）：链路泵与受理队列入队同线程（单写者
        // 纪律，ADR-015(a)）；回执发送在后续拍的泵内上线
        if (preTick_) {
            preTick_(static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    startTime.time_since_epoch()).count()));
        }

        // 冻结拍（L101 失联策略）：停模拟推进（意图消费/脚本补挂/worldHost
        // tick/玩法拍——已受理意图保序滞留）；泵与钩子照跑，链路必须活着
        // 才能听到 World 复核解冻
        const auto delta_seconds = static_cast<double>(tickInterval().count()) / 1000.0;
        if (!frozen_.load(std::memory_order_acquire)) {
            // ADR-015(a)：意图消费在 tick 边界（world 变更只发生在此线程）
            drainAcceptanceQueue();

            // 脚本宿主接线：未挂的 scene 幂等补挂（新 scene 下一拍生效）
            attach_script_to_scenes();

            worldHost_->tick();

            // 占用撤销重试（L108 占用联动切片）：本拍跨境腾空后下一拍即可
            // 成功的撤销，无需 World 重推
            drainPendingRevokes();

            // 小队副本玩法拍（P2 真实链路）：Running 副本 tick 推进（含已受理
            // 击杀的 battle 输入序）+ 结算期副本收尾；状态帧随后续操作/收尾下发
            driveSquadGameplay(delta_seconds);
        }

        // 玩法拍后钩子（管理推进：排空 / eval 收账 / 模块编排——宿主在
        // 内核线程内驱动，单写者纪律不破）
        if (postTick_) {
            postTick_(delta_seconds);
        }

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

    // 脚本宿主随内核线程退出而灭（scripting-lua §2）：先摘全部 scene
    // 再析构 runtime，避免 scene 侧残悬引用。
    for (auto* scene : world_->scenes()) {
        scene->detach_script();
    }
    script_.reset();
}

apollo::game::world::Scene* CellApp::find_scene(std::uint64_t scene_id) {
    return world_ ? world_->find_scene(scene_id) : nullptr;
}

std::chrono::milliseconds CellApp::tickInterval() const {
    return std::chrono::milliseconds(std::max(config_.tickRateMs, 1));
}

void CellApp::record_metrics(std::int64_t elapsed_ms) {
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
    cellTicksTotal_.increment(1);
    // M1 受理/拒收为进程期只增原子（出口缝绝对值）——增量推进
    counter_advance_to(cellIntentsReceivedTotal_, lastIntentsReceived_,
                       egress_ ? egress_->receivedIntents() : 0);
    counter_advance_to(cellFramesRejectedTotal_, lastFramesRejected_,
                       egress_ ? egress_->rejectedFrames() : 0);
    for (const auto* scene : world_->scenes()) {
        const std::string scene_label = std::to_string(scene->scene_id());
        registry.gauge("world_entities", {{"scene", scene_label}})
            .store(static_cast<std::int64_t>(scene->get_entity_count()));
        registry.gauge("world_scene_tick", {{"scene", scene_label}})
            .store(static_cast<std::int64_t>(scene->tick_count()));
    }
}

} // namespace world_app
