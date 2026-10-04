#pragma once

#include "cell/config.hpp"
#include "cell/cell_manager.hpp"
#include "apollo/game/world/avatar.hpp"
#include "apollo/game/world/map_instance_manager.hpp"
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

    // 计算单帧间隔
    std::chrono::milliseconds tickInterval() const;

    // 广播消息给视野内玩家
    void broadcastToViewers(Entity* entity, const std::vector<uint8_t>& message);

    void ensureDefaultMapInstance();
    void attachPlayerWorldSession(protocol::SessionID session_id, protocol::PlayerID player_id,
                                  EntityID entity_id, const Position& position);
    void detachPlayerWorldSession(protocol::SessionID session_id, EntityID entity_id);

    CellConfig config_;
    std::unique_ptr<EntityManager> entityManager_;
    std::unique_ptr<AOIManager> aoiManager_;
    std::shared_ptr<apollo::game::world::MapInstanceManager> mapInstanceManager_;
    std::shared_ptr<apollo::game::world::WorldSessionManager> worldSessionManager_;
    std::shared_ptr<apollo::runtime::WorldHost> worldHost_;

    std::unique_ptr<protocol::RepSocket> server_;
    std::atomic<bool> running_{false};
    std::uint32_t worldId_ = 1;
    apollo::game::world::MapInstance::InstanceId defaultMapInstanceId_ = 1;
    // 默认场景的 space 承载 id（P0-2 起与 instance id 分离，撤销旧「space=map
    // instance」复用；P0-3 由 SceneDescriptor/Scene 统一提供）
    std::uint64_t defaultSpaceId_ = 1;

    // 场景内空间权威（Avatar）容器：按玩家身份索引、进 scene 生/出 scene 死。
    // P0-2 由 CellServer 持有（最小闭环）；P0-3 迁入 Scene（实体/AOI 集合）。
    std::unordered_map<apollo::game::core::PlayerId, apollo::game::world::AvatarPtr> avatars_;

    std::thread gameThread_;
};

} // namespace cell
