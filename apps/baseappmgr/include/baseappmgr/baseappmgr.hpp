#pragma once

#include "apollo/game/session/directory_mirror.hpp"
#include "apollo/game/session/player_directory.hpp"
#include "apollo/game/session/session_locator.hpp"
#include "apollo/game/session/world_assignment.hpp"
#include "apollo/protocol/socket.hpp"
#include <atomic>
#include <cstdint>
#include <functional>
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

    // ---- 在线目录（P1-2：目录语义升级——行为契约见 PlayerDirectory）----
    // 条目级真值在 directory_；SessionLocator/assignments_ 保留为路由投影
    // （ResolveRoute 消费面不变）。顶号预裁、事件族、对账、anchor_epoch
    // 全部由目录承载；跨进程镜像见 set_directory_event_listener。
    const apollo::game::session::PlayerDirectory& directory() const { return directory_; }

    // 目录事件追加监听（P3-1 增量②：跨进程镜像 publisher 从进程壳链入；
    // 默认日志面之后追加调用）。反复设置以最后注册者为准。
    void set_directory_event_listener(
        apollo::game::session::PlayerDirectory::EventSink listener);

    // 准入闸门（P3-1 增量③：恢复相位排他——拒新语义注入点）。gate 返回
    // false 时 bindSession/assignWorld 拒绝（wire 应答 success=false）。
    // 未设置恒放行。
    void set_admission_gate(std::function<bool()> gate);

    // 全量重报 intake 透传（P3-1 增量③：恢复相位各 Zone/gateway 全量重报
    // → PlayerDirectory::intake_full_report，restore-not-kick 语义）
    std::size_t intake_directory_full_report(
        const std::vector<apollo::game::session::MirrorEntry>& sessions);

    // 对账（30s 周期，owner 驱动）：上报在线集与目录比对；失配走快照重置。
    // 返回是否一致（P3 起由周期定时器驱动，单进程阶段由测试/运维触发）。
    bool reconcileDirectory(const std::vector<PlayerID>& reported_online) const;

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
    // P1-2：条目级真值升级为 PlayerDirectory（顶号/事件族/epoch/对账）。
    apollo::game::session::SessionLocator sessionLocator_;
    mutable std::mutex assignmentsMutex_;
    std::unordered_map<PlayerID, apollo::game::session::WorldAssignment> assignments_;
    apollo::game::session::PlayerDirectory directory_;
    apollo::game::session::PlayerDirectory::EventSink directory_listener_;
    std::function<bool()> admission_gate_;
};

} // namespace baseappmgr
