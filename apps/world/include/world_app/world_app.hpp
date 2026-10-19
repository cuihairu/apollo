#pragma once

#include "world_app/config.hpp"
#include "world_app/acceptance_queue.hpp"
#include "world_app/m1_link_server.hpp"
#include "world_app/world_types.hpp"
#include "squad_instance/squad_instance.hpp"
#include "apollo/game/cell/avatar.hpp"
#include "apollo/game/world/world.hpp"
#include "apollo/game/world/world_session_manager.hpp"
#include "apollo/runtime/world_host.hpp"
#include "apollo/core/metrics/metric_registry.h"
#include <chrono>
#include <memory>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <unordered_set>

namespace apollo::script {
class ScriptRuntime;
class ModuleRollout;
}

namespace player_store {
class StoreDial;
}

namespace world_app {

namespace protocol = apollo::protocol;

// WorldApp 服务器
class WorldApp {
public:
    explicit WorldApp(const WorldAppConfig& config);
    ~WorldApp();

    // 启动服务器
    void start();

    // 停止服务器
    void stop();

    // 是否运行中
    bool isRunning() const { return running_; }

    // 小队副本受理面（P2）：进程内装配完成；测试与后续 M1 入口共用。
    squad_instance::SquadInstanceService* squadInstanceService() noexcept {
        return squadInstanceService_.get();
    }

    // 受理点判定（M1 面与测试共用）：维护闸闭 / 顶格拒收 = false（M1 面
    // 据此回 2001/2101 即时应答；受理成功不回包，延时应答随 executeIntent
    // 消费后按 request_id 回带，ADR-017）
    bool submitIntent(IntentEnvelope intent);

    // 待执行意图数（观测/测试面：tick 边界 drain 断言用）
    std::size_t pendingIntentCount() const { return acceptanceQueue_.size(); }

    // M1 链路接收端（M1 上层接线批 2b-ii）：InterServerLink passive 端点，
    // 网关经它投递 cell 意图（924-928 契约族）；listenPort 查实占端口。
    M1LinkServer& m1Link() { return *m1Link_; }

    // scene 反查（gameLoop 线程内解析；测试面：eval 宿主接线定位投递目标用）。
    // 注：ScriptRuntime 刻意不外露——scripting-lua §2 一状态一逻辑线程，其指针
    // 只有 gameThread_ 可读；宿主对外的观察面是 Scene §7.1 debug eval 队列。
    apollo::game::world::Scene* find_scene(std::uint64_t scene_id);

    // 意图源会话 → 玩家号（worldSessionManager_ 反查；0 = 未知会话）。互斥
    // 会话表的只读反查，跨线程安全；观测/测试面：受理单写者纪律断言用
    // （submitIntent 不触碰模拟状态，会话绑定仅在 tick 边界 drain 后显形）。
    std::uint64_t resolveIntentPlayer(std::uint64_t session_id) const;

private:
    // 处理创建实体请求
    std::vector<uint8_t> handleCellCreateEntity(const std::vector<uint8_t>& request);

    // 处理销毁实体请求
    std::vector<uint8_t> handleCellDestroyEntity(const std::vector<uint8_t>& request);

    // 处理实体移动请求
    std::vector<uint8_t> handleCellEntityMove(const std::vector<uint8_t>& request);

    // 处理跨边界
    std::vector<uint8_t> handleCellCrossBorder(const std::vector<uint8_t>& request);

    // 处理战斗请求
    std::vector<uint8_t> handleCombatSkillCast(const std::vector<uint8_t>& request);

    // ---- 队伍与小队副本真实链路（P2 真实上行/下行；全在 gameThread_ tick
    // 边界经 executeIntent 执行）----
    std::vector<uint8_t> handlePartyCreate(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handlePartyJoin(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handleSquadLaunch(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handleSquadEnter(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handleSquadStart(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handleSquadKill(const std::vector<uint8_t>& request);

    // 客户端帧下行（939 client_deliver 出口；未装配链路 = 尽力面落地）。
    void deliverToClient(std::uint64_t session_id, const std::vector<uint8_t>& frame);
    // 组装并下行：队伍结果 / 副本结果 / 副本状态（单会话与名册全员两型）。
    void deliverPartyResult(std::uint64_t session_id, std::int32_t op, std::int32_t code,
                            std::uint64_t party_id);
    void deliverSquadResult(std::uint64_t session_id, std::int32_t op, std::int32_t code,
                            const squad_instance::ActiveSquadInstance* binding);
    void deliverSquadStateToParty(std::uint64_t party_id);
    // 玩法拍驱动（gameLoop 每拍：Running 副本 tick 推进 + 结算期副本收尾）。
    void driveSquadGameplay(double delta_seconds);

    // 游戏循环
    void gameLoop();

    // 意图执行（ADR-015(a)：gameThread_ tick 边界消费——复用既有 handler，
    // 返回值丢弃（延时应答随 M1 升级批））
    void executeIntent(const IntentEnvelope& intent);

    // 受理队列清空到 tick 执行（ADR-016：tick 预算锚定；停机 G-3 尾扫一次）
    void drainAcceptanceQueue();

    // 脚本宿主接线（scripting-lua §2/§5.1）：gameLoop 每拍幂等——未挂的
    // scene 挂上 runtime（新 scene 下一拍自动挂；attach 幂等守卫在 Scene）。
    void attach_script_to_scenes();

    // M1 意图受理点（2b-ii：泵线程 = gameThread_ 内同步调用）：submitIntent
    // 判定 + 拒收即时回执（顶格 2101 / 维护闸 2001）；受理成功不回包，回执随
    // executeIntent 消费后按 request_id 回带（ADR-017 延时应答）。
    void onM1Intent(IntentEnvelope intent);

    // 模块投递编排受理点（952，P4 仅代码更新批；gameThread_ 内同步执行）：
    // World 本地单目标 ModuleRollout（target = 本进程 runtime，canary = 自身）
    // prepare/commit/rollback/status，回执 953 即时回带（观察窗随拍自驱）。
    void onM1ModuleRollout(const std::string& peer, const ModuleRolloutRequest& request);

    // 编排推进（gameLoop 每拍，drainDebugEvalResults 同址）：观察窗推拍 +
    // promote/rollback 队列驱动 + 在装模块 onTick 驱动（fault 归因源——
    // canary 窗的 fault 计账靠模块函数真被调用）。
    void driveModuleRollout();

    // World 排空受理点（956，P4 更新排空批；gameThread_ 内同步执行——与
    // launch/玩法推进同线程，状态机单线程独占无锁）：op 1=begin（排空闸闭
    // 合 + 期限登记）2=status（快照回带），回执 957 即时回。
    void onM1WorldDrain(const std::string& peer, const WorldDrainRequest& request);
    // 排空推进（gameLoop 每拍，driveSquadGameplay 同址后拍）：draining 期
    // active_count 归零落 drained；期限到未清强停剩余副本（abort 取消线 /
    // dismiss 结算线收尾）。drained 终态不反闸——恢复 = 进程替换。
    void driveWorldDrain();

    // M1 延时应答出口（929 cell_intent_ack）；request_id = 0（legacy 路径）
    // 不作关联键，静默。
    void sendM1Ack(std::uint64_t request_id, std::int32_t code, const std::string& message);

    // 脚本调试 eval 受理（944，§7.1 attach transport；泵线程 = gameThread_）：
    // 定位 scene（0 = 默认）→ 未挂宿主/排队失败即时回 945 拒收；受理成功挂
    // 关联账（eval_id → 回传 peer），回执随 tick 窗口 drain 异步回 945。
    void onM1DebugEval(const std::string& peer, const DebugEvalRequest& request);
    // eval 回执出口（945 debug_eval_result；peer 按名查链路，未知 = 静默弃）。
    void sendM1EvalReply(const std::string& peer, std::uint64_t eval_id, bool ok,
                         const std::string& result, const std::string& error);
    // tick 窗口收账（gameLoop 每拍，worldHost tick 之后）：仅在有 M1 eval 在
    // 途时 drain 对应 scene——直接 enqueue 面（测试/程序内消费者）不受挤占。
    void drainDebugEvalResults();

    // 计算单帧间隔
    std::chrono::milliseconds tickInterval() const;

    // 广播消息给视野内玩家（P1-3：viewer set 驱动——接收集合来自实体所在
    // Scene 的 SceneAoi；下发面：每 viewer 按其会话号重编帧头后走 939 下行）
    void broadcastToViewers(EntityID entity_id, const std::vector<uint8_t>& message);

    // 实体号 → 所在 Scene（Avatar 挂 scene 归属反查；无归属返回 nullptr）
    apollo::game::world::Scene* find_scene_by_entity(EntityID entity_id);
    // 实体号 → 世界会话号（Scene 的 Avatar 容器反查玩家 + find_by_player；
    // 非玩家实体 / 无会话返回 0）
    std::uint64_t session_of_entity(EntityID entity_id,
                                    apollo::game::world::Scene* scene) const;
    // space → scene 路由解析（P2-1b 实体创建共用；未知空间落默认 scene）
    std::uint64_t resolve_scene_id(std::uint64_t space_id) const;

    void ensureDefaultScene();
    void attachPlayerWorldSession(protocol::SessionID session_id, protocol::PlayerID player_id,
                                  EntityID entity_id, const Position& position);
    void detachPlayerWorldSession(protocol::SessionID session_id, EntityID entity_id);

    // metrics admin 通道（逐拍源端渲染消费面）：gauge 现算即弃、counter
    // 绝对值增量推进（counter_advance_to）。场景标签动态生成，注册表查找
    // 每拍 2×场景数次（10Hz 量级，检测面非热路径）。
    void record_metrics(std::int64_t elapsed_ms);

    WorldAppConfig config_;
    // 小队副本受理面（P2）：PartyManager + 受理服务。入口链路（客户端→
    // Gateway→World 的真实上行）随后续条目接通；本批先在进程内装配并
    // 注册本 World 容量。
    apollo::game::social::PartyManager squadParties_;
    std::unique_ptr<squad_instance::SquadInstanceService> squadInstanceService_;
    // Zone 世界容器（P0-3）：create_scene / create_instance / scene.enter 路径
    std::unique_ptr<apollo::game::world::World> world_;
    std::shared_ptr<apollo::game::world::WorldSessionManager> worldSessionManager_;
    std::shared_ptr<apollo::runtime::WorldHost> worldHost_;

    std::unique_ptr<M1LinkServer> m1Link_;
    // Base 业务受理面拨号（P1 结算迁入；头文件只持指针——StoreDial 实现在
    // .cpp 侧包含，析构随 ~WorldApp 取完整类型）
    std::unique_ptr<player_store::StoreDial> baseBusinessDial_;
    // 业务受理关联号（gameThread_ 结算段独占）
    std::uint64_t businessSeq_{0};
    // 脚本宿主（scripting-lua §2 一状态一逻辑线程：随 gameThread_ 生灭，
    // 建/用/析全在 gameLoop 线程）。
    std::unique_ptr<apollo::script::ScriptRuntime> script_;
    // 模块投递编排（P4 仅代码更新批；gameThread_ 独占）：单目标编排器懒建
    //（终态可串行多轮）；受理过的模块名随轮累积（onTick 驱动面 = fault 归
    // 因源）。
    std::unique_ptr<apollo::script::ModuleRollout> moduleRollout_;
    std::vector<std::string> rolloutModuleNames_;
    // 在装模块名（自驱相位跃迁日志面：promoted/rolled back 行带模块名）
    std::string lastRolloutName_;
    // World 排空状态机（P4 更新排空批；gameThread_ 独占——956 受理点与
    // driveWorldDrain 同线程，无跨线程访问）。期限 0 = 无期限自然排空；
    // drained 终态不反闸——恢复 = 进程替换（副本临时态随进程消亡）。
    std::int32_t worldDrainState_{kWorldDrainServing};
    std::uint64_t worldDrainDeadlineMs_{0};  // 0 = 无期限（steady 毫秒域）
    std::string worldDrainDetail_;
    std::atomic<bool> running_{false};
    // 受理面（ADR-016/017）：accepting_=false 即维护闸（start 前拒新意图；
    // 停机先关闸再排空队列，G-3 镜像顺序）
    AcceptanceQueue acceptanceQueue_;
    std::atomic<bool> accepting_{false};
    std::uint32_t worldId_ = 1;
    std::uint64_t defaultSceneId_ = 0;
    // 队伍发号（契约 23 语义：服务端分配 party_id；gameThread_ 独占）。
    std::uint64_t nextPartyId_ = 1;
    // 队长启动玩法时的宿主玩法内容（骨架）：默认目标 + 首波怪。
    static constexpr std::uint64_t kHostGoalMonsters = 3;
    static constexpr std::uint64_t kHostGoalTimeoutTicks = 600;
    apollo::game::world::InstancePtr defaultInstance_;
    // 默认场景的 space 承载 id（P0-2 起与 instance id 分离，撤销旧「space=map
    // instance」复用；P0-3 由 SceneDescriptor/Scene 统一提供）
    std::uint64_t defaultSpaceId_ = 1;
    // space → scene 路由（P1-1 换幕目标解析；单进程单 scene 一条目，
    // 随 scene 增补）
    std::unordered_map<std::uint64_t, std::uint64_t> spaceToScene_;

    // M1 eval 关联账（944 受理面；gameLoop 线程独占——受理与 drain 同泵线程）：
    // eval_id → 回传 peer 自报名；scene_id → 在途 eval id 集（drain 圈定面）。
    std::unordered_map<std::uint64_t, std::string> evalReplyPeers_;

    std::unordered_map<std::uint64_t, std::unordered_set<std::uint64_t>> scenePendingEvals_;

    // metrics admin 通道计数面（引用在注册表存续期恒稳定——注册表头注；
    // last_* 为绝对值→Counter 增量推进的影子读数，gameLoop 线程独占）
    apollo::core::metrics::Counter& worldAppTicksTotal_{
        apollo::core::metrics::MetricRegistry::instance().counter("world_ticks_total")};
    apollo::core::metrics::Counter& worldAppIntentsReceivedTotal_{
        apollo::core::metrics::MetricRegistry::instance().counter("world_intents_received_total")};
    apollo::core::metrics::Counter& worldAppFramesRejectedTotal_{
        apollo::core::metrics::MetricRegistry::instance().counter("world_frames_rejected_total")};
    std::uint64_t lastIntentsReceived_{0};
    std::uint64_t lastFramesRejected_{0};

    std::thread gameThread_;
};

} // namespace world_app
