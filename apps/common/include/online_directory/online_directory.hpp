#pragma once

#include "apollo/game/session/directory_mirror.hpp"
#include "apollo/game/session/player_directory.hpp"
#include "apollo/game/session/session_locator.hpp"
#include "apollo/game/session/world_assignment.hpp"
#include "apollo/protocol/messages.hpp"
#include "online_directory/m1_server.hpp"
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace online_directory {

namespace protocol = apollo::protocol;
using PlayerID = protocol::PlayerID;

// OnlineDirectory 服务器——目录 + 落点裁决（BigWorld OnlineDirectory 直系）。
//
// 职责（调度面）：
//   - Directory：player→SessionBinding / player→WorldAssignment 的全局目录
//   - 落点裁决：assignWorld / clearWorldAssignment
//   - 路由解析：ResolveRoute（供 gateway 问「玩家在哪个 world、经哪个 gateway」）
//
// 边界：不承载玩家数据（Avatar 常驻数据归 baseapp），不跨进程改任何进程内存——
// 本进程只维护自己的目录表。
class OnlineDirectory {
public:
    explicit OnlineDirectory(uint16_t port, std::string host = "0.0.0.0");
    ~OnlineDirectory();

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

    // epoch 水位持久化透传（backup-revive §9.5 主案前置：进程壳装配期
    // attach，重启续起不回退；语义见 PlayerDirectory 注释）。
    void attach_epoch_store(const std::string& path) {
        directory_.attach_epoch_store(path);
    }

    // 目录事件追加监听（P3-1 增量②：跨进程镜像 publisher 从进程壳链入；
    // 默认日志面之后追加调用）。反复设置以最后注册者为准。
    void set_directory_event_listener(
        apollo::game::session::PlayerDirectory::EventSink listener);

    // 准入闸门（P3-1 增量③：恢复相位排他——拒新语义注入点）。gate 返回
    // false 时 bindSession/assignWorld 拒绝（wire 应答 success=false）。
    // 未设置恒放行。
    void set_admission_gate(std::function<bool()> gate);

    // 更新排空闸（backup-revive §8 路1 缺口接线 / §9.1 manager 行「停止
    // 分配新玩家」态）：置位期 bindSession/assignWorld 拒新——落点不再指
    // 向进更新窗的编队（骨架编队单 zone，全局闸 = per-zone 闸）。与恢复
    // 相位拒新共用闸门语义（§9.6 准入闸行），排空不挡查询/对账/镜像。
    void set_drain(bool on);
    [[nodiscard]] bool drain_active() const noexcept {
        return drain_.load(std::memory_order_acquire);
    }

    // 全量重报 intake 透传（P3-1 增量③：恢复相位各 Zone/gateway 全量重报
    // → PlayerDirectory::intake_full_report，restore-not-kick 语义）
    std::size_t intake_directory_full_report(
        const std::vector<apollo::game::session::MirrorEntry>& sessions);

    // §6 死亡行窗口处置透传（G-1 收尾批遗留项：宿主进程死亡 → 目录批量
    // 反查进保活窗口）。语义见 PlayerDirectory::mark_suspended_by_zone /
    // mark_suspended_by_gateway（只动 Online、不产事件、反查键 0 拒绝）。
    std::size_t suspend_zone_sessions(std::uint32_t zone_id,
                                      std::uint64_t now_tick,
                                      std::uint64_t window_ticks);
    std::size_t suspend_gateway_sessions(std::uint32_t gateway_id,
                                         std::uint64_t now_tick,
                                         std::uint64_t window_ticks);

    // 受理面（start() 注册的 handler 即此件）：按消息类型分派。公开为
    // M1 控制面 handler 直通（P4-4 批④）——nng REP 已随批⑤退役，本面
    // = 唯一受理面
    std::vector<uint8_t> dispatchRequest(const std::vector<uint8_t>& data);

    // M1 控制面装配（P4-4 批④；start 前调用。port 0 = 面关默认）
    void set_m1_face(std::uint16_t port, std::string route_json) {
        m1_enabled_ = true;
        m1_port_ = port;
        m1_route_json_ = std::move(route_json);
    }
    std::uint16_t listenPort() const { return m1Server_ ? m1Server_->listenPort() : 0; }

    // ---- 玩家迁移编排（P0-2 路 2 五步，backup-revive §9.2）----
    struct MigrationResult {
        bool ok = false;
        std::string error_message;      // 失败步（① 停写 / ③ 交写权 / ④ 改指）
        std::uint64_t new_epoch = 0;    // ③ 交割值（ok 有效）
        std::uint32_t new_zone_id = 0;  // ④ 改指落点（ok 有效）
        int64_t freeze_ms = 0;
        int64_t handoff_ms = 0;
        int64_t total_ms = 0;
    };

    // ① 停写(933) → ③ 交写权(937 带源地址，新 zone 现场拉 935/936) →
    // ④ 目录改指 + 立即推快照（§9.2 ④：不许等 10s 快照周期）。同步串行
    // 调用（并发度缺省 1，§9.3；参数命名挂接线批）。⑤ 放行归 gateway
    // 上行缓冲面（§9.9-2）。③ 之后不可撤销——③ 失败如实返回（老侧停留
    // Transferring，§9.2「本轮跳过 + 升级人工介入」）。
    MigrationResult migratePlayer(std::uint64_t player_id,
                                  const std::string& source_host,
                                  std::uint16_t source_port,
                                  const std::string& target_host,
                                  std::uint16_t target_port,
                                  std::uint32_t target_zone_id);

    // 目录快照发布口（§9.2 ④）：注入后 migratePlayer 改指后立即推全量；
    // 未注入 = 由宿主主循环按周期推（快照周期兜底）。
    void set_snapshot_publisher(
        std::shared_ptr<apollo::game::session::DirectoryPublisher> publisher);

    // 窗口满扫描透传（§4 tick 驱动）：到期 Suspended → 删条目 +
    // SessionDown(kReasonWindowExpired)（事件链自动进镜像面）。返回终结数。
    std::size_t sweep_suspended(std::uint64_t now_tick);

    // 对账（30s 周期，owner 驱动）：上报在线集与目录比对；失配走快照重置。
    // 返回是否一致（P3 起由周期定时器驱动，单进程阶段由测试/运维触发）。
    bool reconcileDirectory(const std::vector<PlayerID>& reported_online) const;

private:
    std::vector<uint8_t> handlePlayerAssignWorldRequest(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handlePlayerResolveRouteRequest(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handlePlayerMigrateRequest(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handlePing(const std::vector<uint8_t>& request);

    // 目录条目 → 镜像条目收集（§9.2 ④ 改指立即推的供数；main 壳同型）
    std::vector<apollo::game::session::MirrorEntry> collectMirrorEntries() const;

    int64_t getCurrentTimeMs() const;

    std::string host_;
    uint16_t port_;
    std::atomic<bool> running_{false};

    // M1 控制面（P4-4 批④；m1_port_ = 0 = 不装配）
    std::uint16_t m1_port_ = 0;
    bool m1_enabled_ = false;
    std::string m1_route_json_;
    std::unique_ptr<M1ControlServer> m1Server_;

    // 目录：session→binding 双向映射复用 SessionLocator（自带锁）；
    // player→assignment 目录为本进程独有，独立互斥锁保护。
    // P1-2：条目级真值升级为 PlayerDirectory（顶号/事件族/epoch/对账）。
    apollo::game::session::SessionLocator sessionLocator_;
    mutable std::mutex assignmentsMutex_;
    std::unordered_map<PlayerID, apollo::game::session::WorldAssignment> assignments_;
    apollo::game::session::PlayerDirectory directory_;
    apollo::game::session::PlayerDirectory::EventSink directory_listener_;
    std::function<bool()> admission_gate_;
    std::atomic<bool> drain_{false};  // 更新排空闸（§8 路1；主循环置位）

    // 玩家迁移编排（§9.2）：快照发布口（④ 改指立即推）+ 编排 request_id
    // 单调序列（①③ 关联键，进程内单调）
    std::shared_ptr<apollo::game::session::DirectoryPublisher> snapshot_publisher_;
    std::atomic<std::uint64_t> migrateSeq_{1};
};

} // namespace online_directory
