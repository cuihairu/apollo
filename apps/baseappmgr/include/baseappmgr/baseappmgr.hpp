#pragma once

#include "apollo/game/session/session_locator.hpp"
#include "apollo/game/session/world_assignment.hpp"
#include "apollo/protocol/socket.hpp"
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace baseappmgr {

namespace protocol = apollo::protocol;
using PlayerID = protocol::PlayerID;

// BaseAppMgr 服务器——目录 + 落点裁决（BigWorld BaseAppMgr 直系）。
//
// 职责（调度面）：
//   - Directory：player→SessionBinding / player→WorldAssignment 的全局目录
//   - 落点裁决：assignWorld / clearWorldAssignment
//   - 路由解析：ResolveRoute（供 gateway 问「玩家在哪个 world、经哪个 gateway」）
//
// 边界：不承载玩家数据（Avatar 常驻数据归 baseapp），不跨进程改任何进程内存——
// 本进程只维护自己的目录表。
class BaseAppMgr {
public:
    explicit BaseAppMgr(uint16_t port, std::string host = "0.0.0.0");
    ~BaseAppMgr();

    void start();
    void stop();

    bool isRunning() const { return running_; }

    // ---- 目录裁决（内存为准） ----
    bool assignWorld(PlayerID playerId, const apollo::game::session::WorldAssignment& assignment);
    bool clearWorldAssignment(PlayerID playerId);
    bool bindSession(PlayerID playerId, const apollo::game::session::SessionBinding& binding);
    bool unbindSession(protocol::SessionID sessionId);

    // ---- 目录查询 ----
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
    std::vector<uint8_t> handlePlayerAssignWorldRequest(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handlePlayerResolveRouteRequest(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handlePing(const std::vector<uint8_t>& request);

    int64_t getCurrentTimeMs() const;

    std::string host_;
    uint16_t port_;
    std::unique_ptr<protocol::RepSocket> server_;
    std::atomic<bool> running_{false};

    // 目录：session→binding 双向映射复用 SessionLocator（自带锁）；
    // player→assignment 目录为本进程独有，独立互斥锁保护。
    apollo::game::session::SessionLocator sessionLocator_;
    mutable std::mutex assignmentsMutex_;
    std::unordered_map<PlayerID, apollo::game::session::WorldAssignment> assignments_;
};

} // namespace baseappmgr
