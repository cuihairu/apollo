#pragma once

#include "base/config.hpp"
#include "base/database_service.hpp"
#include "apollo/game/session/anchor_manager.hpp"
#include "apollo/game/session/session_locator.hpp"
#include "apollo/protocol/socket.hpp"
#include <memory>
#include <optional>
#include <thread>
#include <atomic>

namespace base {

namespace protocol = apollo::protocol;

// BaseApp 服务器
class BaseServer {
public:
    explicit BaseServer(const BaseConfig& config);
    ~BaseServer();

    // 启动服务器
    void start();

    // 停止服务器
    void stop();

    // 是否运行中
    bool isRunning() const { return running_; }

    std::shared_ptr<apollo::game::session::PlayerAnchor> activatePlayer(PlayerID playerId);
    bool bindSession(PlayerID playerId, const apollo::game::session::SessionBinding& binding);
    bool unbindSession(protocol::SessionID sessionId);
    bool assignWorld(PlayerID playerId, const apollo::game::session::WorldAssignment& assignment);
    bool clearWorldAssignment(PlayerID playerId);
    std::shared_ptr<apollo::game::session::PlayerAnchor> findAnchor(PlayerID playerId) const;
    std::optional<PlayerID> findPlayerBySession(protocol::SessionID sessionId) const;
    std::optional<apollo::game::session::WorldAssignment> resolveWorldAssignment(
        PlayerID playerId,
        protocol::SessionID sessionId = 0
    ) const;
    std::optional<apollo::game::session::SessionBinding> resolveSessionBinding(
        PlayerID playerId,
        protocol::SessionID sessionId = 0
    ) const;

private:
    // 处理数据库加载请求
    std::vector<uint8_t> handleDbLoadRequest(const std::vector<uint8_t>& request);

    // 处理数据库保存请求
    std::vector<uint8_t> handleDbSaveRequest(const std::vector<uint8_t>& request);

    // 处理查询请求
    std::vector<uint8_t> handleDbQueryRequest(const std::vector<uint8_t>& request);

    std::vector<uint8_t> handlePlayerActivateRequest(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handlePlayerBindSessionRequest(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handlePlayerAssignWorldRequest(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handlePlayerResolveRouteRequest(const std::vector<uint8_t>& request);

    // 处理心跳
    std::vector<uint8_t> handlePing(const std::vector<uint8_t>& request);

    // 自动保存循环
    void autoSaveLoop();

    int64_t getCurrentTimeMs() const;
    std::shared_ptr<apollo::game::session::PlayerAnchor> activatePlayerAnchor(PlayerID playerId);
    void finalizeSave(PlayerID playerId, bool success);

    BaseConfig config_;
    std::unique_ptr<DatabaseService> database_;
    std::unique_ptr<SaveQueue> saveQueue_;
    std::shared_ptr<apollo::game::session::AnchorManager> anchorManager_;
    std::shared_ptr<apollo::game::session::SessionLocator> sessionLocator_;

    std::unique_ptr<protocol::RepSocket> server_;
    std::atomic<bool> running_{false};

    std::thread autoSaveThread_;
};

} // namespace base
