#pragma once

// cell 执行内核（L98 首切片：执行/管理分离）。
//
// 目标职责（todo P2 L98）：模拟循环、WorldHost/Scene/Avatar、AOI、战斗与
// Lua 全部迁入执行端——本类型是执行面的唯一宿主；管理面（注册/拓扑/容量/
// 迁移编排、模块投递、排空、debug eval）不在本类型，由宿主进程（WorldApp
// 或未来独立 cell 进程壳）经注入缝驱动。
//
// 传输缝：内核对网络零依赖——下行（939）与延时应答（929）走 IM1Egress，
// 传输泵与管理推进走 pre/post tick 钩子；未装配 = 尽力面落地（同 WorldApp
// 「m1Link_ 缺席」语义）。内核因此可裸编测试（tests/test_cell_app.cpp 只编
// 本文件 + 协议源）。
//
// 线程纪律不变（ADR-015(a)）：内核状态单写者 = 内核循环线程；onIntent 只
// 入队不写 world；访问器（find_scene/script_runtime 等）仅限内核线程上下文
// ——管理面经 pre/post tick 钩子在内核线程内执行（WorldApp 的 eval/编排/
// 排空受理点照旧跑在泵上下文）。

#include "world_app/config.hpp"
#include "world_app/acceptance_queue.hpp"
#include "world_app/world_types.hpp"
#include "squad_instance/squad_instance.hpp"
#include "apollo/game/cell/avatar.hpp"
#include "apollo/game/world/world.hpp"
#include "apollo/game/world/world_session_manager.hpp"
#include "apollo/runtime/world_host.hpp"
#include "apollo/core/metrics/metric_registry.h"
#include <lua.hpp>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace apollo::script {
class ScriptRuntime;
}

namespace player_store {
class StoreDial;
}

namespace world_app {

// 声明/分配 Space 共用的 SceneDescriptor 装配（L100 多 Space + L101 分配面）：
// 几何 = 全局配置字段；map 资产引用 = 唯一在册资产 1（静态资产面不虚构 id，
// 每 Space 独立资产随资产管线）；名缺省回退 space-<id>。
apollo::game::world::SceneDescriptor space_descriptor_for(const WorldAppConfig& config,
                                                          std::uint64_t space_id,
                                                          const std::string& name);

// 声明名回查（L100 多 Space 声明面后续接线）：space_id 在 config.spaces 已
// 声明时返回其名，未声明/默认 Space 返回空。分配（962）应用面据此保留声明
// 名——撤路由后重认领的声明 Space 不会静默回退成 space-<id> 默认名。
const std::string& declared_space_name(const WorldAppConfig& config,
                                       std::uint64_t space_id);


namespace protocol = apollo::protocol;

// M1 出口缝：cell 内核对传输面只依赖本接口——客户端帧下行（939
// client_deliver）与意图延时应答（929 cell_intent_ack）+ 受理观测计数。
// WorldApp 以真 M1LinkServer 适配实现；测试以桩实现收帧。
class IM1Egress {
public:
    virtual ~IM1Egress() = default;

    // 939 client_deliver：payload = 客户端 wire 全帧（MessageHeader + body），
    // 网关按 session_id 路由投递不解释。
    virtual void sendClientDeliver(std::uint64_t session_id, std::uint32_t gateway_id,
                                   const std::vector<std::uint8_t>& payload) = 0;

    // 929 cell_intent_ack：request_id 关联（0 = legacy 不作关联键）。
    virtual void sendIntentAck(std::uint64_t request_id, std::int32_t code,
                               const std::string& message) = 0;

    // 受理观测计数（metrics 推进源；缺省 0 = 无传输面）。
    virtual std::uint64_t receivedIntents() const { return 0; }
    virtual std::uint64_t rejectedFrames() const { return 0; }
};

// cell 执行内核：模拟域全量所有权 + 受理队列（ADR-016）+ tick 边界消费
//（ADR-015(a)）+ 延时应答（ADR-017）。
class CellApp {
public:
    explicit CellApp(const WorldAppConfig& config);
    ~CellApp();

    // ---- 注入缝（start 前装配；缺省 = 尽力面）----
    void set_egress(IM1Egress* egress) { egress_ = egress; }
    // 拍首钩子：传输泵（WorldApp = m1Link_->pump(now_ms)）。now_ms = 单调毫秒。
    void set_pre_tick(std::function<void(std::uint64_t now_ms)> hook) {
        preTick_ = std::move(hook);
    }
    // 玩法拍后钩子：管理推进（WorldApp = 排空/eval 收账/模块编排三驱动）。
    // delta_seconds = 本拍时长（秒），与 driveSquadGameplay 同值。
    void set_post_tick(std::function<void(double delta_seconds)> hook) {
        postTick_ = std::move(hook);
    }

    // ---- 生命周期 ----
    void start();
    void stop();
    bool isRunning() const { return running_; }

    // ---- 控制面失联冻结面（L101 失联策略切片，§4 失联策略）----
    // 冻结 = tick 边界停模拟推进（意图消费/脚本补挂/worldHost tick/玩法拍
    // 暂停，已受理意图保序滞留）+ 关受理闸（新输入拒收 2001 = IO 层关闭新
    // 受理）；pre/post tick 钩子与传输泵照跑——链路必须活着才能听到 World
    // 复核。解冻只随 961 复核受理（恢复连接本身不解冻——不能自行解冻），
    // 闸随解冻恢复（仅 running 期；停机语义不受冻结影响，尾扫照常清队）。
    // 调用上下文：pre-tick 泵上下文（= 内核线程，注册客户端监听器）或测试
    // 线程（atomic 可见）。
    void set_frozen(bool frozen);
    bool frozen() const { return frozen_.load(std::memory_order_acquire); }

    // ---- 受理面 ----
    // M1 意图受理点（泵线程 = 内核线程）：submitIntent 判定 + 拒收即时回执
    //（顶格 2101 / 维护闸 2001）；受理成功不回包，回执随 tick 边界消费后按
    // request_id 回带（ADR-017）。
    void onIntent(IntentEnvelope intent);

    // 受理点判定（M1 面与测试共用）：维护闸闭 / 顶格拒收 = false。
    bool submitIntent(IntentEnvelope intent);

    // 待执行意图数（观测/测试面：tick 边界 drain 断言用）
    std::size_t pendingIntentCount() const { return acceptanceQueue_.size(); }

    // 意图受理队列容量（注册面负载上报的归一化分母；构造后不变，跨线程只读
    // 安全）。0 = 容量未配置（宿主上报侧按空载处理）。
    std::size_t intentQueueCap() const { return acceptanceQueue_.cap(); }

    // 意图源会话 → 玩家号（0 = 未知会话）。互斥会话表的只读反查，跨线程
    // 安全；受理单写者纪律断言用。
    std::uint64_t resolveIntentPlayer(std::uint64_t session_id) const;

    // ---- 内核线程上下文访问器（管理面经 pre/post tick 钩子在内核线程内
    // 消费；跨线程持有指针违反单写者纪律）----
    apollo::game::world::Scene* find_scene(std::uint64_t scene_id);
    std::uint64_t default_scene_id() const { return defaultSceneId_; }

    // ---- Space ↔ Scene 映射面（L99 首切片）----
    // 口径（docs/design/bigworld-apollo-mapping §2/§6）：Space 对应现有 Scene
    // 的空间语义，不叠同义容器；space_id 是内核路由键（spaceToScene_），
    // scene_id 由 world 自增分配——两域数值独立，映射是真实的表而非恒等别名。
    // 三 ID 域分开：space id = 路由键；cell 分区 id 今日退化为 scene id（每
    // Space 单全幅 Cell，跨 Space 拆分随 L101/L102 注册批）；执行进程身份 =
    // 本进程（worldId；注册后由 worldServer 分配）。add_space 在内核线程 /
    // start 前注册窗调用（同 ctor 时序）；返回目标 scene id（0 = 创建失败）。
    // 重复空间幂等返回既有 scene。
    std::uint64_t add_space(std::uint64_t space_id,
                            const apollo::game::world::SceneDescriptor& descriptor);
    // 撤销面（L108 回收切片，同 add_space 的内核线程约束）：World 经 962
    // ready=0 通知让渡时撤路由 + 销毁已空 scene。返回被撤 scene id；0 = 未
    // 撤——默认 Space 拒撤、未知空间幂等、占用中（avatar_count > 0）拒绝
    // 保持路由（排空/迁移联动后续批）。
    std::uint64_t remove_space(std::uint64_t space_id);
    // 撤销待重试面（L108 占用联动切片）：request_space_revoke 受理跨线程
    // 只入队（互斥表，不读内核状态——同 submitIntent 纪律；默认 Space 与
    // 重复待撤拒收 false），消费在 gameLoop tick 边界（内核线程单写者）——
    // 占用中被拒的撤销每拍重试至腾空成功。cancel_space_revoke 供 World 重
    // 认领时取消陈旧待撤（最新 World 视图优先），重复取消 no-op。
    bool request_space_revoke(std::uint64_t space_id);
    void cancel_space_revoke(std::uint64_t space_id);
    // 待撤计数（atomic 观测面，跨线程安全）
    std::size_t pending_revoke_count() const {
        return pendingRevokes_.load(std::memory_order_acquire);
    }
    // 路由查询（同 default_scene_id 的内核线程上下文约束；未知空间回落默认）
    std::uint64_t scene_id_of_space(std::uint64_t space_id) const {
        return resolve_scene_id(space_id);
    }
    // 路由存在查询（同 scene_id_of_space 的内核线程上下文约束）：仅本表命
    // 中，不回落默认——撤销面「真持有」判据（962 全池视图推给每个注册
    // cell，从未认领过某 Space 的 cell 见它方归属不得空入待撤表）。
    bool hosts_space(std::uint64_t space_id) const {
        return spaceToScene_.find(space_id) != spaceToScene_.end();
    }
    std::size_t space_count() const { return spaceToScene_.size(); }

    squad_instance::SquadInstanceService* squadInstanceService() noexcept {
        return squadInstanceService_.get();
    }
    // 脚本宿主（管理面专用：模块编排 target / 版本查询）。scripting-lua §2
    // 一状态一逻辑线程——指针只有内核线程可读。
    apollo::script::ScriptRuntime* script_runtime() noexcept { return script_.get(); }

private:
    // ---- 意图执行（tick 边界，内核线程）----
    std::vector<uint8_t> handleCellCreateEntity(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handleCellDestroyEntity(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handleCellEntityMove(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handleCellCrossBorder(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handleCombatSkillCast(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handlePartyCreate(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handlePartyJoin(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handleSquadLaunch(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handleSquadEnter(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handleSquadStart(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handleSquadKill(const std::vector<uint8_t>& request);

    void executeIntent(const IntentEnvelope& intent);
    void drainAcceptanceQueue();
    void gameLoop();

    // 脚本宿主接线 / world 模块注入 / 契约路由 Lua 分发（L51 消费端面）
    void attach_script_to_scenes();
    void register_world_module();
    static int lua_world_broadcast_viewers(lua_State* L);
    static int lua_world_push_session(lua_State* L);
    void dispatch_script_intent(const IntentEnvelope& intent);

    // 下行面（经 egress 缝；未装配 = 尽力面落地）
    void deliverToClient(std::uint64_t session_id, const std::vector<uint8_t>& frame);
    void deliverPartyResult(std::uint64_t session_id, std::int32_t op, std::int32_t code,
                            std::uint64_t party_id);
    void deliverSquadResult(std::uint64_t session_id, std::int32_t op, std::int32_t code,
                            const squad_instance::ActiveSquadInstance* binding);
    void deliverSquadStateToParty(std::uint64_t party_id);
    void driveSquadGameplay(double delta_seconds);
    // 待撤销重试（tick 边界，内核线程——见 request_space_revoke 注）
    void drainPendingRevokes();

    // 延时应答出口（929；request_id = 0 静默）。经 egress 缝。
    void sendAck(std::uint64_t request_id, std::int32_t code, const std::string& message);

    void broadcastToViewers(EntityID entity_id, const std::vector<uint8_t>& message);
    apollo::game::world::Scene* find_scene_by_entity(EntityID entity_id);
    std::uint64_t session_of_entity(EntityID entity_id,
                                    apollo::game::world::Scene* scene) const;
    std::uint64_t resolve_scene_id(std::uint64_t space_id) const;

    void ensureDefaultScene();
    // space_id 承载会话落点：路由解析（未知空间回落默认）后进 scene；
    // 实例绑定按落点 scene 反查（无实例的空间绑 0——Space 可无临时 Instance）
    void attachPlayerWorldSession(protocol::SessionID session_id, protocol::PlayerID player_id,
                                  EntityID entity_id, const Position& position,
                                  std::uint64_t space_id);
    void detachPlayerWorldSession(protocol::SessionID session_id, EntityID entity_id);

    void record_metrics(std::int64_t elapsed_ms);
    std::chrono::milliseconds tickInterval() const;

    WorldAppConfig config_;
    // 小队副本受理面（P2）：PartyManager + 受理服务
    apollo::game::social::PartyManager squadParties_;
    std::unique_ptr<squad_instance::SquadInstanceService> squadInstanceService_;
    // Zone 世界容器（P0-3）：create_scene / create_instance / scene.enter 路径
    std::unique_ptr<apollo::game::world::World> world_;
    std::shared_ptr<apollo::game::world::WorldSessionManager> worldSessionManager_;
    std::shared_ptr<apollo::runtime::WorldHost> worldHost_;

    // 注入缝（宿主装配）
    IM1Egress* egress_ = nullptr;
    std::function<void(std::uint64_t now_ms)> preTick_;
    std::function<void(double delta_seconds)> postTick_;

    // Base 业务受理面拨号（P1 结算迁入；实现随析构取完整类型）
    std::unique_ptr<player_store::StoreDial> baseBusinessDial_;
    // 业务受理关联号（内核线程结算段独占）
    std::uint64_t businessSeq_{0};
    // 脚本宿主（scripting-lua §2 一状态一逻辑线程：随内核线程生灭）
    std::unique_ptr<apollo::script::ScriptRuntime> script_;

    std::atomic<bool> running_{false};
    // 受理面（ADR-016/017）：accepting_=false 即维护闸
    AcceptanceQueue acceptanceQueue_;
    std::atomic<bool> accepting_{false};
    // 冻结面（L101 失联策略）：true = 模拟推进暂停 + 受理闸关（见 set_frozen）
    std::atomic<bool> frozen_{false};
    std::uint32_t worldId_ = 1;
    std::uint64_t defaultSceneId_ = 0;
    // 队伍发号（契约 23 语义：服务端分配 party_id；内核线程独占）
    std::uint64_t nextPartyId_ = 1;
    // 队长启动玩法时的宿主玩法内容（骨架）：默认目标 + 首波怪
    static constexpr std::uint64_t kHostGoalMonsters = 3;
    static constexpr std::uint64_t kHostGoalTimeoutTicks = 600;
    apollo::game::world::InstancePtr defaultInstance_;
    std::uint64_t defaultSpaceId_ = 1;
    // space → scene 路由（P1-1 换幕目标解析）
    std::unordered_map<std::uint64_t, std::uint64_t> spaceToScene_;
    // 待撤销重试表（L108 占用联动切片）：受理跨线程入队 / gameLoop tick
    // 边界消费，互斥表 + 计数 atomic 双保险（消费侧持锁只做表操作与
    // remove_space 调用，remove_space 本身不取本锁——无锁序反转）。
    std::mutex pendingRevokesMutex_;
    std::unordered_set<std::uint64_t> pendingRevokesSet_;
    std::atomic<std::size_t> pendingRevokes_{0};

    // metrics admin 通道计数面（last_* 为绝对值→Counter 增量推进的影子读数，
    // 内核线程独占）
    apollo::core::metrics::Counter& cellTicksTotal_{
        apollo::core::metrics::MetricRegistry::instance().counter("world_ticks_total")};
    apollo::core::metrics::Counter& cellIntentsReceivedTotal_{
        apollo::core::metrics::MetricRegistry::instance().counter("world_intents_received_total")};
    apollo::core::metrics::Counter& cellFramesRejectedTotal_{
        apollo::core::metrics::MetricRegistry::instance().counter("world_frames_rejected_total")};
    std::uint64_t lastIntentsReceived_{0};
    std::uint64_t lastFramesRejected_{0};

    std::thread gameThread_;
};

} // namespace world_app
