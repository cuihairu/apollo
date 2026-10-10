#pragma once

#include "cell/config.hpp"
#include "cell/acceptance_queue.hpp"
#include "cell/cell_manager.hpp"
#include "apollo/game/world/avatar.hpp"
#include "apollo/game/world/world.hpp"
#include "apollo/game/world/world_session_manager.hpp"
#include "apollo/protocol/socket.hpp"
#include "apollo/runtime/world_host.hpp"
#include <chrono>
#include <memory>
#include <thread>
#include <atomic>
#include <unordered_map>

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

    // 受理点（ADR-017 决策 1）：RPC worker 线程调用——只解码分类 + 入队意图
    // 信封，回执 ack-on-accept（变更类空 ack 逐字节同现状；过载回错误包）；
    // PING 在受理点直接回 Pong（纯 wire echo，ADR-017 决策 3）。
    std::vector<uint8_t> acceptRequest(const std::vector<uint8_t>& request);

    // 待执行意图数（观测/测试面：tick 边界 drain 断言用）
    std::size_t pendingIntentCount() const { return acceptanceQueue_.size(); }

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

    // 处理心跳
    std::vector<uint8_t> handlePing(const std::vector<uint8_t>& request);

    // 游戏循环
    void gameLoop();

    // 意图执行（ADR-015(a)：gameThread_ tick 边界消费——复用既有 handler，
    // 返回值丢弃（延时应答随 M1 升级批））
    void executeIntent(const IntentEnvelope& intent);

    // 受理队列清空到 tick 执行（ADR-016：tick 预算锚定；停机 G-3 尾扫一次）
    void drainAcceptanceQueue();

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

    CellConfig config_;
    // Zone 世界容器（P0-3）：create_scene / create_instance / scene.enter 路径
    std::unique_ptr<apollo::game::world::World> world_;
    std::shared_ptr<apollo::game::world::WorldSessionManager> worldSessionManager_;
    std::shared_ptr<apollo::runtime::WorldHost> worldHost_;

    std::unique_ptr<protocol::RepSocket> server_;
    std::atomic<bool> running_{false};
    // 受理面（ADR-016/017）：accepting_=false 即维护闸（start 前请新请求回
    // 维护中错误包；停机先关闸再排空队列，G-3 镜像顺序）
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

    std::thread gameThread_;
};

} // namespace cell
