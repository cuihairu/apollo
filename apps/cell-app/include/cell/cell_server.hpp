#pragma once

#include "cell/config.hpp"
#include "cell/acceptance_queue.hpp"
#include "cell/m1_link_server.hpp"
#include "cell/cell_manager.hpp"
#include "apollo/game/world/avatar.hpp"
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
}

namespace cell {

namespace protocol = apollo::protocol;

// CellApp 服务器
class CellServer {
public:
    explicit CellServer(const CellConfig& config);
    ~CellServer();

    // 启动服务器
    void start();

    // 停止服务器
    void stop();

    // 是否运行中
    bool isRunning() const { return running_; }

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
    // Scene 的 SceneAoi；网关下发面随 P3-2）
    void broadcastToViewers(EntityID entity_id, const std::vector<uint8_t>& message);

    // 实体号 → 所在 Scene（Avatar 挂 scene 归属反查；无归属返回 nullptr）
    apollo::game::world::Scene* find_scene_by_entity(EntityID entity_id);
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

    CellConfig config_;
    // Zone 世界容器（P0-3）：create_scene / create_instance / scene.enter 路径
    std::unique_ptr<apollo::game::world::World> world_;
    std::shared_ptr<apollo::game::world::WorldSessionManager> worldSessionManager_;
    std::shared_ptr<apollo::runtime::WorldHost> worldHost_;

    std::unique_ptr<M1LinkServer> m1Link_;
    // 脚本宿主（scripting-lua §2 一状态一逻辑线程：随 gameThread_ 生灭，
    // 建/用/析全在 gameLoop 线程）。
    std::unique_ptr<apollo::script::ScriptRuntime> script_;
    std::atomic<bool> running_{false};
    // 受理面（ADR-016/017）：accepting_=false 即维护闸（start 前拒新意图；
    // 停机先关闸再排空队列，G-3 镜像顺序）
    AcceptanceQueue acceptanceQueue_;
    std::atomic<bool> accepting_{false};
    std::uint32_t worldId_ = 1;
    std::uint64_t defaultSceneId_ = 0;
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
    apollo::core::metrics::Counter& cellTicksTotal_{
        apollo::core::metrics::MetricRegistry::instance().counter("cell_ticks_total")};
    apollo::core::metrics::Counter& cellIntentsReceivedTotal_{
        apollo::core::metrics::MetricRegistry::instance().counter("cell_intents_received_total")};
    apollo::core::metrics::Counter& cellFramesRejectedTotal_{
        apollo::core::metrics::MetricRegistry::instance().counter("cell_frames_rejected_total")};
    std::uint64_t lastIntentsReceived_{0};
    std::uint64_t lastFramesRejected_{0};

    std::thread gameThread_;
};

} // namespace cell
