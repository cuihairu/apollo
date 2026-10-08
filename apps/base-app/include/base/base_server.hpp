#pragma once

#include "base/config.hpp"
#include "base/database_service.hpp"
#include "apollo/data/orm/persist_journal.hpp"
#include "apollo/game/session/anchor_manager.hpp"
#include "apollo/game/session/player_directory.hpp"
#include "apollo/game/session/recovery.hpp"
#include "apollo/game/session/session_locator.hpp"
#include "apollo/protocol/socket.hpp"
#include <cstddef>
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

    // 停机维护位（G-3，attribute-sync §10.2-①）：true = 停机中，受理面
    // 一律维护中应答（不收新 intent）
    bool is_shutting_down() const { return shuttingDown_.load(); }

    // 受理面（start() 注册的 handler 即此件）：按消息类型分派。公开为
    // 停机维护闸的直测入口——stub 传输树无真实 REP 面，socket round-trip
    // 测不了受理语义
    std::vector<uint8_t> dispatchRequest(const std::vector<uint8_t>& data);

    std::shared_ptr<apollo::game::session::PlayerAnchor> activatePlayer(PlayerID playerId);
    bool bindSession(PlayerID playerId, const apollo::game::session::SessionBinding& binding);
    bool unbindSession(protocol::SessionID sessionId);
    std::shared_ptr<apollo::game::session::PlayerAnchor> findAnchor(PlayerID playerId) const;
    std::optional<PlayerID> findPlayerBySession(protocol::SessionID sessionId) const;

    // 目录事件上报源（P1-2，session-and-online-directory §2 事件源①）：
    // bind/unbind 产 SessionUp/SessionDown 同形事件，经 sink 上报 manager
    // 域目录（权威表在 baseappmgr）；单进程阶段 sink 由宿主注入（默认
    // 无 sink 静默），跨进程总线上报随 P3。
    void set_directory_event_sink(
        apollo::game::session::PlayerDirectory::EventSink sink);

    // 锚点域快照出口（P3-1 增量③：恢复相位全量重报的供数面——进程壳
    // 从 AnchorManager 快照收集现存会话发 FullReport）
    std::shared_ptr<apollo::game::session::AnchorManager> anchor_manager() const {
        return anchorManager_;
    }

private:
    // 处理数据库加载请求
    std::vector<uint8_t> handleDbLoadRequest(const std::vector<uint8_t>& request);

    // 处理数据库保存请求
    std::vector<uint8_t> handleDbSaveRequest(const std::vector<uint8_t>& request);

    // 处理查询请求
    std::vector<uint8_t> handleDbQueryRequest(const std::vector<uint8_t>& request);

    std::vector<uint8_t> handlePlayerActivateRequest(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handlePlayerBindSessionRequest(const std::vector<uint8_t>& request);

    // 落点裁决与路由解析（PLAYER_ASSIGN_WORLD / PLAYER_RESOLVE_ROUTE）已拆出，
    // 归 apps/baseappmgr（目录 + 调度面，BW BaseAppMgr 直系）。

    // 处理心跳
    std::vector<uint8_t> handlePing(const std::vector<uint8_t>& request);

    // 自动保存循环
    void autoSaveLoop();

    // 脏锚点 flush（P0-4）：把 needs_save() 的锚点置 Saving 并入保存队列；
    // 关闭协议与自动保存共用。返回入队数量（占位实现，落库接 P1）。
    std::size_t flushDirtyAnchors(const char* reason);

    // journal write-ahead 与消费侧（P1-5）
    std::size_t appendJournal(const PlayerData& data);
    std::size_t drainJournal();

    int64_t getCurrentTimeMs() const;
    std::shared_ptr<apollo::game::session::PlayerAnchor> activatePlayerAnchor(PlayerID playerId);
    void finalizeSave(PlayerID playerId, bool success);

    BaseConfig config_;
    std::unique_ptr<DatabaseService> database_;
    std::unique_ptr<SaveQueue> saveQueue_;
    std::unique_ptr<apollo::data::journal::PersistJournal> journal_;
    std::unique_ptr<apollo::game::session::RecoveryCoordinator> recovery_;
    std::shared_ptr<apollo::game::session::AnchorManager> anchorManager_;
    std::shared_ptr<apollo::game::session::SessionLocator> sessionLocator_;
    apollo::game::session::PlayerDirectory::EventSink directoryEventSink_;

    std::unique_ptr<protocol::RepSocket> server_;
    std::atomic<bool> running_{false};
    std::atomic<bool> shuttingDown_{false};  // 停机五阶段幂等闸 + 维护位（G-3）

    std::thread autoSaveThread_;
};

} // namespace base
