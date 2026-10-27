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

namespace world_app {

namespace protocol = apollo::protocol;

// P1-3：Scene 持有 AOI 后内核侧直接消费 SceneAoi（Vec3/查询面）
using apollo::game::world::SceneAoi;

namespace {

// 库面日志出口：cat=world_app（§5.1 cat= 检索键；主程已接文件面）
apollo::core::log::Logger& worldLog() {
    return *apollo::core::log::global_log_manager().createLogger("world_app");
}

// 消息型 → 契约意图 id（gateway m1_egress contract_id_for 同表镜像；
// 0 = 不在 Lua 分发面）。
std::uint16_t contract_id_for(apollo::protocol::MessageType type) {
    using MT = apollo::protocol::MessageType;
    using apollo::contract::gen::MsgId;
    switch (type) {
        case MT::CELL_CREATE_ENTITY: return static_cast<std::uint16_t>(MsgId::cell_create_entity);
        case MT::CELL_DESTROY_ENTITY: return static_cast<std::uint16_t>(MsgId::cell_destroy_entity);
        case MT::CELL_ENTITY_MOVE: return static_cast<std::uint16_t>(MsgId::cell_entity_move);
        case MT::CELL_CROSS_BORDER: return static_cast<std::uint16_t>(MsgId::cell_cross_border);
        case MT::COMBAT_SKILL_CAST: return static_cast<std::uint16_t>(MsgId::cell_skill_cast);
        case MT::PARTY_CREATE_REQUEST: return static_cast<std::uint16_t>(MsgId::party_create_intent);
        case MT::PARTY_JOIN_REQUEST: return static_cast<std::uint16_t>(MsgId::party_join_intent);
        case MT::SQUAD_LAUNCH: return static_cast<std::uint16_t>(MsgId::squad_launch_intent);
        case MT::SQUAD_ENTER: return static_cast<std::uint16_t>(MsgId::squad_enter_intent);
        case MT::SQUAD_START: return static_cast<std::uint16_t>(MsgId::squad_start_intent);
        case MT::SQUAD_KILL: return static_cast<std::uint16_t>(MsgId::squad_kill_intent);
        default: return 0;
    }
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

std::vector<uint8_t> CellApp::handleCellCreateEntity(const std::vector<uint8_t>& request) {
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
            // L99：落点空间随请求（旧代码忽略 msg.spaceId 一律落默认空间）
            attachPlayerWorldSession(session_id, player_id, msg.entityId,
                                     {msg.position.x, msg.position.y, msg.position.z},
                                     msg.spaceId);
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

std::vector<uint8_t> CellApp::handleCellDestroyEntity(const std::vector<uint8_t>& request) {
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

std::vector<uint8_t> CellApp::handleCellEntityMove(const std::vector<uint8_t>& request) {
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

std::vector<uint8_t> CellApp::handleCellCrossBorder(const std::vector<uint8_t>& request) {
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

std::vector<uint8_t> CellApp::handleCombatSkillCast(const std::vector<uint8_t>& request) {
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

std::uint64_t CellApp::resolveIntentPlayer(std::uint64_t session_id) const {
    if (!worldSessionManager_) return 0;
    const auto session = worldSessionManager_->find_session(session_id);
    if (!session) return 0;
    return session->player_id().value();
}

void CellApp::deliverToClient(std::uint64_t session_id, const std::vector<uint8_t>& frame) {
    if (!egress_ || frame.empty()) return;  // 链路未装配 = 尽力面落地
    egress_->sendClientDeliver(session_id, 0, frame);
}

void CellApp::deliverPartyResult(std::uint64_t session_id, std::int32_t op,
                                 std::int32_t code, std::uint64_t party_id) {
    protocol::PartyResult msg;
    msg.op = op;
    msg.code = code;
    msg.partyId = party_id;
    deliverToClient(session_id, protocol::MessageCodec::encode(msg, session_id));
}

void CellApp::deliverSquadResult(std::uint64_t session_id, std::int32_t op,
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

void CellApp::deliverSquadStateToParty(std::uint64_t party_id) {
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

std::vector<uint8_t> CellApp::handlePartyCreate(const std::vector<uint8_t>& request) {
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

std::vector<uint8_t> CellApp::handlePartyJoin(const std::vector<uint8_t>& request) {
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

std::vector<uint8_t> CellApp::handleSquadLaunch(const std::vector<uint8_t>& request) {
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

std::vector<uint8_t> CellApp::handleSquadEnter(const std::vector<uint8_t>& request) {
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

std::vector<uint8_t> CellApp::handleSquadStart(const std::vector<uint8_t>& request) {
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

std::vector<uint8_t> CellApp::handleSquadKill(const std::vector<uint8_t>& request) {
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

void CellApp::driveSquadGameplay(double delta_seconds) {
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

void CellApp::attach_script_to_scenes() {
    if (!script_) {
        return;
    }
    for (auto* scene : world_->scenes()) {
        if (!scene->script_attached()) {
            scene->attach_script(*script_);
        }
    }
}

void CellApp::register_world_module() {
    if (!script_) {
        return;
    }
    static const luaL_Reg kWorldFuncs[] = {
        {"broadcast_viewers", &CellApp::lua_world_broadcast_viewers},
        {"push_session", &CellApp::lua_world_push_session},
        {nullptr, nullptr},
    };
    script_->register_module_ctx("world", kWorldFuncs, this);
}

int CellApp::lua_world_broadcast_viewers(lua_State* L) {
    auto* self = static_cast<CellApp*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (self == nullptr) {
        return luaL_error(L, "world module: host missing");
    }
    const auto entity_id = static_cast<EntityID>(luaL_checkinteger(L, 1));
    const auto msg_type = static_cast<std::uint16_t>(luaL_checkinteger(L, 2));
    std::size_t body_size = 0;
    const char* body = luaL_checklstring(L, 3, &body_size);

    // 脚本源帧：头按编码器默认面组装，sessionId 置 0——真实寻址随
    // broadcastToViewers 逐 viewer 会话号重编（批⑩ 下行管线复用）。
    protocol::MessageHeader header;
    header.magic = protocol::MessageHeader::MAGIC;
    header.version = protocol::MessageHeader::CURRENT_VERSION;
    header.type = msg_type;
    header.length = static_cast<std::uint32_t>(body_size);
    header.sequence = 0;
    header.sessionId = 0;
    auto frame = protocol::MessageCodec::encodeHeader(header);
    frame.insert(frame.end(), body, body + body_size);
    self->broadcastToViewers(entity_id, frame);
    return 0;
}

int CellApp::lua_world_push_session(lua_State* L) {
    auto* self = static_cast<CellApp*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (self == nullptr) {
        return luaL_error(L, "world module: host missing");
    }
    const auto session_id = static_cast<std::uint64_t>(luaL_checkinteger(L, 1));
    const auto msg_type = static_cast<std::uint16_t>(luaL_checkinteger(L, 2));
    std::size_t body_size = 0;
    const char* body = luaL_checklstring(L, 3, &body_size);

    protocol::MessageHeader header;
    header.magic = protocol::MessageHeader::MAGIC;
    header.version = protocol::MessageHeader::CURRENT_VERSION;
    header.type = msg_type;
    header.length = static_cast<std::uint32_t>(body_size);
    header.sequence = 0;
    header.sessionId = session_id;
    auto frame = protocol::MessageCodec::encodeHeader(header);
    frame.insert(frame.end(), body, body + body_size);
    self->deliverToClient(session_id, frame);
    return 0;
}

void CellApp::dispatch_script_intent(const IntentEnvelope& intent) {
    // 客户端消息 → Lua（L51 消费端面）：C++ handler 已先行（权威状态变更），
    // 这里按契约路由带参分发；脚本未挂 / 契约未覆盖 = 静默跳过。
    if (!script_) {
        return;
    }
    const auto route_id = contract_id_for(intent.type);
    if (route_id == 0) {
        return;
    }
    if (script_->contract_routes().find(route_id) == script_->contract_routes().end()) {
        return;
    }
    namespace ls = apollo::script;
    std::vector<ls::LuaValue> args;
    const auto body = std::vector<std::uint8_t>(
        intent.frame.begin() + sizeof(protocol::MessageHeader), intent.frame.end());
    // 字段序 = gateway m1_egress translate_and_send 同序镜像（real = double 面
    // 经 LuaValue 数字通道；Lua 侧按位取参）。
    switch (intent.type) {
        case protocol::MessageType::CELL_CREATE_ENTITY: {
            const auto m = protocol::MessageCodec::decodeBody<protocol::CellCreateEntity>(body);
            args = {ls::LuaValue::of_int(static_cast<std::int64_t>(m.entityId)),
                    ls::LuaValue::of_int(static_cast<std::uint8_t>(m.entityType)),
                    ls::LuaValue::of_int(static_cast<std::int64_t>(m.spaceId)),
                    ls::LuaValue::of_double(m.position.x),
                    ls::LuaValue::of_double(m.position.y),
                    ls::LuaValue::of_double(m.position.z)};
            break;
        }
        case protocol::MessageType::CELL_DESTROY_ENTITY: {
            const auto m = protocol::MessageCodec::decodeBody<protocol::CellDestroyEntity>(body);
            args = {ls::LuaValue::of_int(static_cast<std::int64_t>(m.entityId))};
            break;
        }
        case protocol::MessageType::CELL_ENTITY_MOVE: {
            const auto m = protocol::MessageCodec::decodeBody<protocol::CellEntityMove>(body);
            args = {ls::LuaValue::of_int(static_cast<std::int64_t>(m.entityId)),
                    ls::LuaValue::of_double(m.oldPos.x), ls::LuaValue::of_double(m.oldPos.y),
                    ls::LuaValue::of_double(m.oldPos.z), ls::LuaValue::of_double(m.newPos.x),
                    ls::LuaValue::of_double(m.newPos.y), ls::LuaValue::of_double(m.newPos.z)};
            break;
        }
        case protocol::MessageType::CELL_CROSS_BORDER: {
            const auto m = protocol::MessageCodec::decodeBody<protocol::CellCrossBorder>(body);
            args = {ls::LuaValue::of_int(static_cast<std::int64_t>(m.entityId)),
                    ls::LuaValue::of_int(static_cast<std::int64_t>(m.fromSpace)),
                    ls::LuaValue::of_int(static_cast<std::int64_t>(m.toSpace)),
                    ls::LuaValue::of_double(m.position.x),
                    ls::LuaValue::of_double(m.position.y),
                    ls::LuaValue::of_double(m.position.z)};
            break;
        }
        case protocol::MessageType::COMBAT_SKILL_CAST: {
            const auto m = protocol::MessageCodec::decodeBody<protocol::CombatSkillCast>(body);
            args = {ls::LuaValue::of_int(static_cast<std::int64_t>(m.casterId)),
                    ls::LuaValue::of_int(static_cast<std::int64_t>(m.targetId)),
                    ls::LuaValue::of_int(static_cast<std::int64_t>(m.skillId)),
                    ls::LuaValue::of_double(m.casterPos.x), ls::LuaValue::of_double(m.casterPos.y),
                    ls::LuaValue::of_double(m.casterPos.z), ls::LuaValue::of_double(m.targetPos.x),
                    ls::LuaValue::of_double(m.targetPos.y), ls::LuaValue::of_double(m.targetPos.z)};
            break;
        }
        // 队伍/小队族：带源会话号单参（帧体族语义在 C++ 受理面，Lua 只观测）。
        case protocol::MessageType::PARTY_CREATE_REQUEST:
        case protocol::MessageType::PARTY_JOIN_REQUEST:
        case protocol::MessageType::SQUAD_LAUNCH:
        case protocol::MessageType::SQUAD_ENTER:
        case protocol::MessageType::SQUAD_START:
        case protocol::MessageType::SQUAD_KILL:
            args = {ls::LuaValue::of_int(static_cast<std::int64_t>(intent.session_id))};
            break;
        default:
            return;
    }
    if (!script_->call_route_args(route_id, args)) {
        // call_route_args 已发 runtime fault 事件面（handler 缺失/执行错）；
        // 这里补运维对账行，拍不停（同 boot 源纪律）。
        worldLog().warning("script route dispatch failed: route=" + std::to_string(route_id));
    }
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

void CellApp::broadcastToViewers(EntityID entity_id, const std::vector<uint8_t>& message) {
    // P1-3：viewer set 驱动——接收集合 = 实体所在 Scene 的 SceneAoi 视野
    // 查询（AOI 归 scene、scene_id 隔离）。下发面：每 viewer 按其会话号重编
    // 帧头（body 原样）走 939 client_deliver 网关下行；无会话 viewer（NPC
    // 等非玩家实体）自然落过。
    apollo::game::world::Scene* scene = find_scene_by_entity(entity_id);
    if (scene == nullptr) {
        return;
    }
    if (message.size() < sizeof(protocol::MessageHeader)) {
        return;  // 残帧不广播
    }
    const auto header = protocol::MessageCodec::parseHeader(message);
    const std::vector<uint8_t> body(message.begin() + sizeof(protocol::MessageHeader),
                                    message.end());
    const auto viewers = scene->aoi().viewers_of(apollo::game::core::EntityId(entity_id));
    std::size_t delivered = 0;
    for (const auto& viewer : viewers) {
        const auto viewer_session = session_of_entity(viewer.value(), scene);
        if (viewer_session == 0) {
            continue;
        }
        protocol::MessageHeader routed = header;
        routed.sessionId = viewer_session;
        auto frame = protocol::MessageCodec::encodeHeader(routed);
        frame.insert(frame.end(), body.begin(), body.end());
        deliverToClient(viewer_session, frame);
        ++delivered;
    }
    if (delivered != 0) {
        worldLog().info("broadcast: entity=" + std::to_string(entity_id) + " scene="
                    + std::to_string(scene->scene_id()) + " viewers="
                    + std::to_string(viewers.size()) + " delivered="
                    + std::to_string(delivered) + " bytes="
                    + std::to_string(message.size()));
    }
}

std::uint64_t CellApp::session_of_entity(EntityID entity_id,
                                         apollo::game::world::Scene* scene) const {
    if (scene == nullptr) {
        return 0;
    }
    const auto entity = apollo::game::core::EntityId(entity_id);
    for (const auto& player_id : scene->avatars()) {
        const auto avatar = scene->get_avatar(player_id);
        if (avatar && avatar->entity_id() == entity) {
            if (worldSessionManager_) {
                if (const auto session = worldSessionManager_->find_by_player(player_id)) {
                    return session->session_id();
                }
            }
            return 0;
        }
    }
    return 0;
}

std::uint64_t CellApp::resolve_scene_id(std::uint64_t space_id) const {
    // 单进程单 scene 一条目（P1-1 口径）；未知空间回落默认 scene——
    // 与 create 的旧「空间无关全落全局表」行为等价
    const auto route = spaceToScene_.find(space_id);
    return route != spaceToScene_.end() ? route->second : defaultSceneId_;
}

apollo::game::world::SceneDescriptor space_descriptor_for(const WorldAppConfig& config,
                                                          std::uint64_t space_id,
                                                          const std::string& name) {
    // 声明（--add-space）与分配（962）两面的共用装配；几何 = 全局字段。
    apollo::game::world::SceneDescriptor descriptor;
    descriptor.map_id = 1;  // 唯一在册地图资产——静态资产面不虚构 id
    descriptor.map_name = name.empty() ? ("space-" + std::to_string(space_id)) : name;
    descriptor.width = config.spaceWidth;
    descriptor.height = config.spaceHeight;
    descriptor.grid_size = config.gridSize;
    descriptor.view_radius = config.viewRadius;
    return descriptor;
}

const std::string& declared_space_name(const WorldAppConfig& config,
                                       std::uint64_t space_id) {
    // 声明名回查（L100 多 Space 声明面后续接线）：已声明的 space_id 返回声明
    // 名；未声明/默认 Space 返回空，调用方走 space_descriptor_for 的
    // space-<id> 缺省回退。
    static const std::string kEmpty;
    for (const auto& decl : config.spaces) {
        if (decl.space_id == space_id) {
            return decl.name;
        }
    }
    return kEmpty;
}

std::uint64_t CellApp::add_space(std::uint64_t space_id,
                                 const apollo::game::world::SceneDescriptor& descriptor) {
    // L99：显式登记面——space_id（路由键域）与 scene_id（world 自增域）
    // 数值独立；本方法只建 scene + 登路由，不建 Instance（持续开放 Space
    // 可无临时 Instance，玩法侧经副本受理面自建）。
    const auto existing = spaceToScene_.find(space_id);
    if (existing != spaceToScene_.end()) {
        return existing->second;  // 幂等：重复登记不重建 scene
    }
    auto* scene = world_->create_scene(descriptor);
    if (scene == nullptr) {
        return 0;  // descriptor 无效（is_valid 拒绝）
    }
    spaceToScene_[space_id] = scene->scene_id();
    return scene->scene_id();
}

std::uint64_t CellApp::remove_space(std::uint64_t space_id) {
    // L108 首切片：撤销面——World 经 962 ready=0 通知本 cell 让渡一个 Space。
    // 返回被撤 scene id（0 = 未撤）：默认 Space 是进程骨架不参与回收；未知
    // 空间幂等；占用中（avatar_count > 0）拒绝并保持路由——不迁不销毁实体，
    // 重平衡/排空联动后续批。已空则先擦路由再销毁 scene（World::destroy_scene
    // 自身再拒未销毁的 Instance；顺序防 use-after-free，sanitizer 树实抓）。
    // 内核线程上下文（同 add_space）：路由表与 world 容器由游戏线程单写。
    if (space_id == defaultSpaceId_) {
        return 0;  // 默认 Space 不回收
    }
    const auto route = spaceToScene_.find(space_id);
    if (route == spaceToScene_.end()) {
        return 0;  // 幂等：无路由可撤
    }
    const std::uint64_t scene_id = route->second;
    const auto* scene = world_->find_scene(scene_id);
    if (scene == nullptr) {
        spaceToScene_.erase(route);  // 残留路由：清表即可
        return 0;
    }
    if (scene->avatar_count() != 0) {
        return 0;  // 占用中：拒绝，路由保持
    }
    spaceToScene_.erase(route);
    if (world_->destroy_scene(scene_id)) {
        return scene_id;
    }
    spaceToScene_[space_id] = scene_id;  // 销毁失败则恢复路由，保持同存
    return 0;
}

bool CellApp::request_space_revoke(std::uint64_t space_id) {
    // 受理点（L108 占用联动切片）：只入队不读内核状态（跨线程安全面同
    // submitIntent 纪律——世界状态只允许内核线程读写）；默认 Space 拒收
    //（remove_space 同口径），重复待撤幂等拒收。
    if (space_id == defaultSpaceId_) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(pendingRevokesMutex_);
        if (!pendingRevokesSet_.insert(space_id).second) {
            return false;  // 已在待撤表
        }
    }
    pendingRevokes_.fetch_add(1, std::memory_order_acq_rel);
    return true;
}

void CellApp::cancel_space_revoke(std::uint64_t space_id) {
    {
        std::lock_guard<std::mutex> lock(pendingRevokesMutex_);
        if (pendingRevokesSet_.erase(space_id) == 0) {
            return;
        }
    }
    pendingRevokes_.fetch_sub(1, std::memory_order_acq_rel);
}

void CellApp::drainPendingRevokes() {
    // tick 边界重试（L108 占用联动切片，内核线程单写者）：腾空即撤销成功
    // 出表，占用被拒留在表下一拍再试；路由已消失（他面撤过）直接出表。
    if (pendingRevokes_.load(std::memory_order_acquire) == 0) {
        return;
    }
    std::size_t done = 0;
    {
        std::lock_guard<std::mutex> lock(pendingRevokesMutex_);
        for (auto it = pendingRevokesSet_.begin(); it != pendingRevokesSet_.end();) {
            const auto space_id = *it;
            if (spaceToScene_.find(space_id) == spaceToScene_.end()) {
                it = pendingRevokesSet_.erase(it);  // 路由已消失：出表
                ++done;
                continue;
            }
            if (remove_space(space_id) != 0) {
                it = pendingRevokesSet_.erase(it);
                ++done;
                worldLog().info("space revoked (pending retry): space="
                                + std::to_string(space_id));
                continue;
            }
            ++it;  // 占用中：下一拍再试
        }
    }
    if (done != 0) {
        pendingRevokes_.fetch_sub(done, std::memory_order_acq_rel);
    }
}

apollo::game::world::Scene* CellApp::find_scene_by_entity(EntityID entity_id) {
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

void CellApp::ensureDefaultScene() {
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

void CellApp::attachPlayerWorldSession(
    protocol::SessionID session_id,
    protocol::PlayerID player_id_raw,
    EntityID entity_id,
    const Position& position,
    std::uint64_t space_id) {
    ensureDefaultScene();

    // 强分型（P0-2）：玩家身份与实体身份在类型面上分离——旧代码 entity_id 直接
    // 当 player_id 传（find_by_player/attach 三参同号），编译器不可见。
    const auto player_id = apollo::game::core::PlayerId(player_id_raw);
    const auto avatar_entity_id = apollo::game::core::EntityId(entity_id);

    auto session = worldSessionManager_->find_session(session_id);
    if (!session) {
        session = worldSessionManager_->create_session(session_id, player_id);
    }

    // L99：会话落点随 space 路由解析（未知空间回落默认 scene）；实例按落点
    // scene 反查——无实例的空间绑 0（持续开放 Space 可无临时 Instance）。
    const bool known_space = spaceToScene_.find(space_id) != spaceToScene_.end();
    const auto effective_space = known_space ? space_id : defaultSpaceId_;
    const auto scene_id = resolve_scene_id(space_id);
    auto* scene = world_->find_scene(scene_id);
    auto* instance = scene != nullptr ? world_->find_instance_by_scene(scene_id) : nullptr;

    // create Avatar + scene.enter / instance.enter（P0-3 任务书 §27 API 路径：
    // Avatar 由 Scene 持有（进 scene 生/出 scene 死），玩法侧走 instance.enter）
    auto avatar = std::make_shared<apollo::game::cell::Avatar>(player_id, avatar_entity_id);
    if (scene != nullptr) {
        scene->enter(avatar, {position.x, position.y, position.z});
    }
    if (instance != nullptr) {
        instance->enter(player_id);
    }

    session->assign_world(worldId_);
    session->assign_map_instance(instance != nullptr ? instance->id() : 0);
    // P0-2：space 与 instance 分离——space 承载 id 独立（旧代码以 map instance
    // 号兼任 space 号）。P0-3 拆 SceneDescriptor 后由 scene 提供。L99：登记的是
    // 实际落点空间（未知空间回落默认，与 resolve_scene_id 同一口径）。
    session->assign_space(effective_space);
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

void CellApp::detachPlayerWorldSession(protocol::SessionID session_id, EntityID entity_id) {
    auto session = worldSessionManager_->find_session(session_id);
    if (!session) {
        // 无会话 id 时按玩家身份反查（P0-2/P0-3：经 Scene 的 Avatar 容器由
        // 实体号定位玩家号，再按玩家查会话；不再把实体号当玩家号传）
        // L99：落点 scene 由实体实际所在反查（多空间后不再假设默认 scene）
        if (auto* scene = find_scene_by_entity(entity_id)) {
            const auto avatar_entity_id = apollo::game::core::EntityId(entity_id);
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
    // L99：落点 scene / 实例由实体实际所在反查（多空间后不再假设默认 scene）
    const auto player_id = session->player_id();
    auto* scene = find_scene_by_entity(entity_id);
    if (scene == nullptr) {
        // 实体号缺失或已失效时按玩家号反查落点 scene
        for (apollo::game::world::Scene* candidate : world_->scenes()) {
            if (candidate != nullptr && candidate->get_avatar(player_id) != nullptr) {
                scene = candidate;
                break;
            }
        }
    }
    if (scene != nullptr) {
        scene->leave(player_id);
        if (auto* instance = world_->find_instance_by_scene(scene->scene_id())) {
            instance->leave(player_id);
        }
    }

    worldSessionManager_->suspend_session(session->session_id());
    worldSessionManager_->close_session(session->session_id());
    // close 两段式收尾（P0-4）：Leaving 可观察窗口后显式终结（Closed 终态、
    // 摘除双索引）；内核侧登出无延迟清理面，窗口内即 finalize。
    worldSessionManager_->finalize_session(session->session_id());
}

} // namespace world_app
