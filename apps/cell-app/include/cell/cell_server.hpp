#pragma once

#include "cell/config.hpp"
#include "cell/cell_manager.hpp"
#include "apollo/protocol/socket.hpp"
#include <memory>
#include <thread>
#include <atomic>

namespace cell {

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

    // 广播消息给视野内玩家
    void broadcastToViewers(Entity* entity, const std::vector<uint8_t>& message);

    CellConfig config_;
    std::unique_ptr<EntityManager> entityManager_;
    std::unique_ptr<AOIManager> aoiManager_;

    std::unique_ptr<protocol::RepSocket> server_;
    std::atomic<bool> running_{false};

    std::thread gameThread_;
};

} // namespace cell
