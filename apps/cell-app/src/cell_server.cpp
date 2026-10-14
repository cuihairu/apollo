#include "cell/cell_server.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/net/linktrace/span_emitter.hpp"
#include "apollo/protocol/messages.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/game/world/instance.hpp"
#include "apollo/game/world/scene.hpp"
#include "apollo/game/world/scene_transfer.hpp"
#include "apollo/game/world/world_session_manager.hpp"
#include "apollo/runtime/world_host.hpp"
#include "apollo/script/script_runtime.hpp"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace cell {

namespace {
// 库面日志出口：cat=cell_server（§5.1 cat= 检索键；主程已接文件面）
apollo::core::log::Logger& celllog() {
    return *apollo::core::log::global_log_manager().createLogger("cell_server");
}
} // namespace

namespace protocol = apollo::protocol;

// P1-3：Scene 持有 AOI 后 cell 侧直接消费 SceneAoi（Vec3/查询面）
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

class CellWorldService final : public apollo::runtime::IWorldService {
public:
    CellWorldService(
        apollo::game::world::World& world,
        CellConfig config)
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
        // P0-3：世界 tick = 逐 scene 六阶段（simulate/recalc/aoidecay/
        // collect/budget+flush/persist-batch）
        world_.tick(static_cast<double>(context.delta_seconds));
    }

    void shutdown() override {
        tick_count_ = 0;
    }

private:
    apollo::game::world::World& world_;
    CellConfig config_;
    std::uint64_t tick_count_ = 0;
};

} // namespace

//==============================================================================
//==============================================================================
// CellServer 实现
//==============================================================================

CellServer::CellServer(const CellConfig& config)
    : config_(config)
    , world_(std::make_unique<apollo::game::world::World>())
    , worldSessionManager_(std::make_shared<apollo::game::world::WorldSessionManager>())
    , worldHost_(std::make_shared<apollo::runtime::WorldHost>(
          static_cast<std::uint32_t>(1000 / std::max(config.tickRateMs, 1)))) {
    worldHost_->add_world_service(
        std::make_shared<CellWorldService>(*world_, config_));
}

CellServer::~CellServer() {
    stop();
}

void CellServer::start() {
    if (running_) return;

    // net M1 接收端（2b-ii，2b-iii 起 cell 唯一受理入口——legacy RepSocket
    // 已退役）：InterServerLink passive 端点装配（监听在 start 内 bind/listen，
    // bind 失败同契约显形）
    m1Link_ = makeM1LinkServer(
        M1LinkConfig{config_.host, config_.m1Port, config_.m1RouteJson},
        [this](IntentEnvelope intent) { onM1Intent(std::move(intent)); },
        [this](const std::string& peer, const DebugEvalRequest& request) {
            onM1DebugEval(peer, request);
        });

    // 开闸先于 m1Link_->start()：受理面就绪后网关才见得到入口
    accepting_ = true;

    if (!m1Link_->start()) {
        accepting_ = false;
        m1Link_.reset();
        throw std::runtime_error("failed to start cell M1 link endpoint (route table load or bind failed)");
    }

    ensureDefaultScene();

    if (!worldHost_->start()) {
        accepting_ = false;
        m1Link_->stop();
        m1Link_.reset();
        throw std::runtime_error("failed to start WorldHost");
    }

    running_ = true;

    // 启动游戏循环线程
    gameThread_ = std::thread(&CellServer::gameLoop, this);

    celllog().info("Cell M1 link listening on " + config_.host + ":"
                + std::to_string(m1Link_->listenPort()));
    celllog().info("Space: " + config_.spaceName + " ("
                + std::to_string(config_.spaceWidth) + "x"
                + std::to_string(config_.spaceHeight) + ")");
}

void CellServer::stop() {
    // 停机 G-3 镜像顺序（ADR-016）：停受理 → 队列 drain → gameThread join →
    // M1 端点停 → worldHost stop（尾扫 drain 的延时应答仍要有发送面，M1
    // 端点停在其后）
    accepting_ = false;
    running_ = false;

    if (gameThread_.joinable()) {
        gameThread_.join();
    }

    if (m1Link_) {
        m1Link_->stop();
        m1Link_.reset();
    }

    if (worldHost_) {
        worldHost_->stop();
    }
}

bool CellServer::submitIntent(IntentEnvelope intent) {
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
        celllog().warning("acceptance queue high watermark: pending="
                       + std::to_string(acceptanceQueue_.size()) + "/"
                       + std::to_string(acceptanceQueue_.cap()));
    }
    return true;
}

void CellServer::onM1Intent(IntentEnvelope intent) {
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

void CellServer::sendM1Ack(std::uint64_t request_id, std::int32_t code, const std::string& message) {
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

void CellServer::onM1DebugEval(const std::string& peer, const DebugEvalRequest& request) {
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

void CellServer::sendM1EvalReply(const std::string& peer, std::uint64_t eval_id, bool ok,
                                 const std::string& result, const std::string& error) {
    if (m1Link_) {
        m1Link_->sendEvalResult(peer, eval_id, ok, result, error);
    }
}

void CellServer::drainDebugEvalResults() {
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

std::vector<uint8_t> CellServer::handleCellCreateEntity(const std::vector<uint8_t>& request) {
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

std::vector<uint8_t> CellServer::handleCellDestroyEntity(const std::vector<uint8_t>& request) {
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

std::vector<uint8_t> CellServer::handleCellEntityMove(const std::vector<uint8_t>& request) {
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

std::vector<uint8_t> CellServer::handleCellCrossBorder(const std::vector<uint8_t>& request) {
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
        celllog().warning("scene transfer rejected: player="
                       + std::to_string(player_id.value()) + " target_scene="
                       + std::to_string(transfer.target_scene_id) + " result="
                       + std::string(apollo::game::world::to_string(outcome.result))
                       + " at_step=" + std::to_string(static_cast<int>(outcome.failed_at)));
    }

    return {};
}

std::vector<uint8_t> CellServer::handleCombatSkillCast(const std::vector<uint8_t>& request) {
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

        celllog().info("Skill cast: " + std::to_string(msg.casterId) + " -> "
                    + std::to_string(msg.targetId) + " (skill: "
                    + std::to_string(msg.skillId) + ")");
    }

    return {};
}

void CellServer::executeIntent(const IntentEnvelope& intent) {
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
        default:
            break;
    }

    // trace 子集（P3-3 语义层批）：采样命中的意图发 cell span（受理→执行
    // 完成全程；parent = 发端 gateway span id，随 932 信封上行）。未采样
    // /bare 路径零开销跳过——emit_span 自守门，此处只备料。
    if (intent.trace.has_trace() && intent.trace.sampled) {
        trace::SpanRecord span;
        span.name = "cell_intent_execute";
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

void CellServer::drainAcceptanceQueue() {
    while (auto intent = acceptanceQueue_.try_pop()) {
        executeIntent(*intent);
    }
}

void CellServer::gameLoop() {
    // 脚本宿主生命周期（scripting-lua §2 一状态一逻辑线程）：runtime 建/
    // 用/析全在本线程。boot 源加载失败不停机（fault 走 runtime 事件面，
    // cell 继续服务）。
    script_ = std::make_unique<apollo::script::ScriptRuntime>();
    if (!config_.scriptBootFile.empty()) {
        std::ifstream boot(config_.scriptBootFile, std::ios::binary);
        if (boot) {
            std::string src((std::istreambuf_iterator<char>(boot)),
                            std::istreambuf_iterator<char>());
            if (!script_->call_string("cell_boot", src)) {
                celllog().warning("script boot failed: " + config_.scriptBootFile);
            }
        } else {
            celllog().warning("script boot file missing: " + config_.scriptBootFile);
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

        // §7.1 eval 回执收账（tick 内固定调试点已执行在途 eval）
        drainDebugEvalResults();

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

void CellServer::attach_script_to_scenes() {
    if (!script_) {
        return;
    }
    for (auto* scene : world_->scenes()) {
        if (!scene->script_attached()) {
            scene->attach_script(*script_);
        }
    }
}

apollo::game::world::Scene* CellServer::find_scene(std::uint64_t scene_id) {
    return world_ ? world_->find_scene(scene_id) : nullptr;
}

std::chrono::milliseconds CellServer::tickInterval() const {
    return std::chrono::milliseconds(std::max(config_.tickRateMs, 1));
}

void CellServer::record_metrics(std::int64_t elapsed_ms) {
    auto& registry = apollo::core::metrics::MetricRegistry::instance();
    // 末拍瞬时值走 gauge（_last 后缀与同名直方图族分居——同族双 TYPE
    // 相撞是出口面 scrape 错误）
    registry.gauge("cell_tick_frame_ms_last").store(elapsed_ms);
    // 帧耗时分布（P3-3 余项批）：桶位 ms 档覆盖亚毫秒抖动到秒级卡顿，
    // 20ms = ADR-016 消息段预算锚点
    static const std::vector<double> kFrameMsBuckets = {
        0.1, 0.5, 1, 2, 5, 10, 20, 50, 100, 500, 1000};
    registry.histogram("cell_tick_frame_ms", {}, kFrameMsBuckets)
        .observe(static_cast<double>(elapsed_ms));
    registry.gauge("cell_queue_depth")
        .store(static_cast<std::int64_t>(acceptanceQueue_.size()));
    cellTicksTotal_.increment(1);
    // M1 受理/拒收为进程期只增原子（m1_link_server 绝对值）——增量推进
    counter_advance_to(cellIntentsReceivedTotal_, lastIntentsReceived_,
                       m1Link_ ? m1Link_->receivedIntents() : 0);
    counter_advance_to(cellFramesRejectedTotal_, lastFramesRejected_,
                       m1Link_ ? m1Link_->rejectedFrames() : 0);
    for (const auto* scene : world_->scenes()) {
        const std::string scene_label = std::to_string(scene->scene_id());
        registry.gauge("cell_entities", {{"scene", scene_label}})
            .store(static_cast<std::int64_t>(scene->get_entity_count()));
        registry.gauge("cell_scene_tick", {{"scene", scene_label}})
            .store(static_cast<std::int64_t>(scene->tick_count()));
    }
}

void CellServer::broadcastToViewers(EntityID entity_id, const std::vector<uint8_t>& message) {
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
        celllog().info("broadcast: entity=" + std::to_string(entity_id) + " scene="
                    + std::to_string(scene->scene_id()) + " viewers="
                    + std::to_string(viewers.size()) + " bytes="
                    + std::to_string(message.size()));
    }
}

std::uint64_t CellServer::resolve_scene_id(std::uint64_t space_id) const {
    // 单进程单 scene 一条目（P1-1 口径）；未知空间回落默认 scene——
    // 与 create 的旧「空间无关全落全局表」行为等价
    const auto route = spaceToScene_.find(space_id);
    return route != spaceToScene_.end() ? route->second : defaultSceneId_;
}

apollo::game::world::Scene* CellServer::find_scene_by_entity(EntityID entity_id) {
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

void CellServer::ensureDefaultScene() {
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

void CellServer::attachPlayerWorldSession(
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
        celllog().warning("world session resume rejected for session "
                    + std::to_string(session_id));
    }
}

void CellServer::detachPlayerWorldSession(protocol::SessionID session_id, EntityID entity_id) {
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
    // 摘除双索引）；cell 侧登出无延迟清理面，窗口内即 finalize。
    worldSessionManager_->finalize_session(session->session_id());
}

} // namespace cell
