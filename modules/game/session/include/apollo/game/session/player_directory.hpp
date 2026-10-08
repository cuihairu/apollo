#pragma once

#include "apollo/game/session/player_anchor.hpp"
#include "apollo/game/session/world_assignment.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace apollo::game::session {

// 镜像/重报条目（跨进程投影面公共货币，G-1 收尾批增量②③；目录条目的
// 定位面投影，无 gateway_addr——镜像是定位面不是连接面）。前置声明 +
// 类后定义（成员签名可见性），防 player_directory ↔ directory_mirror
// 循环包含。
struct MirrorEntry;

// PlayerDirectory（在线目录）——P1-2，session-and-online-directory 设计的
// 单进程落地（§9 P2 形态：manager 域进程内表）。
//
// 行为契约（本类即契约，单测逐条断言）：
//   - 数据模型（§1）：条目 = SessionBinding + WorldAssignment + state +
//     anchor_epoch + deadline_tick + zone_id；全部内存哈希，无落盘。
//   - 状态机（§1）：(无)→Online→Suspended→Removed / Online→Leaving→Removed；
//     任意态 + 新登录命中 → 旧 Removed(kicked) + 新条目。
//   - 顶号（§3）：新顶旧——旧条目删除并产 SessionKicked，新条目分配新
//     anchor_epoch；旧 epoch 的 resume/迟到事件按失效拒绝（§3-③）。
//   - 掉线窗口（§4）：mark_suspended 进 Suspended（deadline = now+window）；
//     resume 成功回 Online（只改状态列）；sweep 到期删条目 + SessionDown。
//   - 幂等/乱序（§2）：事件携带 (player_id, anchor_epoch)；旧 epoch 迟到
//     事件丢弃；set 语义天然幂等。
//   - 对账（§2）：reconcile 比对上报在线集；失配返回 false，由
//     snapshot_reset 全量重置（DirectorySnapshot 语义，30s 周期由 owner 驱动）。
//
// 事件面（§8 消息面的单进程形态）：全部变更产 DirectoryEvent（SessionUp/
// Down/Moved/Kicked），经 set_event_sink 同步分发；跨进程镜像/总线留 P3。
class PlayerDirectory {
public:
    // ---- 事件族（§8：单进程阶段即消息面的进程内形态）----
    struct Event {
        enum class Kind : std::uint8_t {
            SessionUp = 0,   // 登记锚点（准入通过 + Zone 接受）
            SessionDown,     // 显式下线 / 窗口满终结
            SessionMoved,    // 落点迁移（换幕/换 Zone handoff）
            SessionKicked,   // 顶号踢除（manager → 旧 Zone）
        };

        Kind kind = Kind::SessionUp;
        std::uint64_t player_id = 0;
        std::uint64_t anchor_epoch = 0;      // 幂等键 + 竞争裁决键
        std::uint32_t zone_id = 0;           // 宿主 Zone（单进程 = 0）
        std::uint32_t reason = 0;            // Down/Kicked 原因码（0 = 未注明）
        SessionBinding binding;              // Up/Moved/Kicked 携带
        WorldAssignment assignment;          // Up/Moved 携带
    };

    using EventSink = std::function<void(const Event&)>;

    // 下线原因码（框架保留段；业务段随 errors.xml 扩充）
    static constexpr std::uint32_t kReasonLogout = 1;
    static constexpr std::uint32_t kReasonWindowExpired = 2;
    static constexpr std::uint32_t kReasonKickedByRelogin = 3;

    // 条目状态（§1 状态机列）
    enum class EntryState : std::uint8_t { Online = 0, Suspended, Leaving };
    struct Entry {
        SessionBinding binding;
        WorldAssignment assignment;
        EntryState state = EntryState::Online;
        std::uint64_t anchor_epoch = 0;      // 同账号第 N 次会话（从 1 起）
        std::uint64_t deadline_tick = 0;     // Suspended 窗口截止（tick 号）
        std::uint32_t zone_id = 0;
    };

    void set_event_sink(EventSink sink);

    // ---- 写路径（§2 三事件源 → 目录）----

    // 登记锚点：准入通过 + Zone 接受后调用。无条目 → Online + SessionUp；
    // 已有条目（Online/Suspended/Leaving 皆然）→ 顶号（§3）：旧条目产
    // SessionKicked 后删除，新条目写入并分配新 epoch。返回新条目 epoch
    // （player_id == 0 返回 0 且无副作用）。
    std::uint64_t session_up(std::uint64_t player_id,
                             const SessionBinding& binding,
                             const WorldAssignment& assignment,
                             std::uint32_t zone_id = 0);

    // 显式下线（登出/实体销毁）：删除条目 + SessionDown。未知玩家 false。
    bool session_down(std::uint64_t player_id, std::uint32_t reason = kReasonLogout);

    // 掉线进保活窗口：Online → Suspended（deadline = now_tick + window_ticks）。
    // 未知或非 Online 条目 false（Leaving 不回窗口）。
    bool mark_suspended(std::uint64_t player_id,
                        std::uint64_t now_tick,
                        std::uint64_t window_ticks);

    // 掉线保活窗口——批量反查处置（§6 Zone/gateway 死亡行；G-1 收尾批遗留
    // 项落地）：宿主进程（Zone/gateway）死亡时按定位列反查，全部 Online 条目
    // 一次性进 Suspended（deadline = now_tick + window_ticks）。口径：
    //   - 只动 Online 条目（已 Suspended 不重置窗口——与条目级 mark_suspended
    //     同语义；Leaving 不回窗口）；
    //   - 不产事件（§4/§8：Suspended 迁移无事件族成员——窗口满 sweep 的
    //     SessionDown / resume 的重报才是可见面）；
    //   - 反查键 0 = 未声明（Zone 未分配 / 未入编队），拒绝批量处置返回 0。
    // 返回迁移条数。
    std::size_t mark_suspended_by_zone(std::uint32_t zone_id,
                                       std::uint64_t now_tick,
                                       std::uint64_t window_ticks);
    std::size_t mark_suspended_by_gateway(std::uint32_t gateway_id,
                                          std::uint64_t now_tick,
                                          std::uint64_t window_ticks);

    // resume：Suspended → Online（§4 只改状态列）。校验 (session_id,
    // anchor_epoch) 双锚——旧会话/旧 epoch 的 resume 拒绝（§3-③ 竞态窗口）。
    bool resume(std::uint64_t player_id,
                std::uint64_t session_id,
                std::uint64_t anchor_epoch);

    // 顶号（§3 新顶旧）：命中已有条目 → 删除 + SessionKicked，返回旧 epoch；
    // 无条目返回 0（新条目由 session_up 写入）。
    std::uint64_t evict_for_relogin(std::uint64_t player_id,
                                    std::uint32_t reason = kReasonKickedByRelogin);

    // 落点迁移（换幕/换 Zone handoff）：更新 assignment + SessionMoved。
    // 仅 Online/Leaving 态可迁移；未知玩家 false。
    bool moved(std::uint64_t player_id, const WorldAssignment& assignment);

    // 显式登出中间态（§1 Leaving）：Online → Leaving；未知/非 Online false。
    bool begin_leave(std::uint64_t player_id);

    // 窗口扫描（manager tick 驱动）：到期 Suspended → 删条目 + SessionDown
    // (kReasonWindowExpired)。返回终结条数。
    std::size_t sweep(std::uint64_t now_tick);

    // ---- 对账（§2 周期 30s，owner 驱动）----

    // 比对上报在线集与目录 Online 集：完全一致返回 true；失配 false
    // （差集经 online_ids()/missing 语义可见，由 snapshot_reset 兜底）。
    bool reconcile(const std::vector<std::uint64_t>& reported_online) const;

    // 快照重置（DirectorySnapshot Reply 语义）：以权威上报集全量重置——
    // 目录多出的条目静默删除（无事件：对账失配属状态层，§2 与事件层互不兜底），
    // 返回删除数。
    std::size_t snapshot_reset(const std::vector<std::uint64_t>& authoritative_online);

    // 全量重报 intake（§6 恢复相位，G-1 收尾批增量③；restore-not-kick）：
    // 报告方（Zone/gateway 进程）现存会话重建目录——已有条目保 epoch 只刷
    // 绑定/定位列（顶号裁决键不因进程重启漂移），无条目 session_up 写入
    // （新 epoch + SessionUp 事件面照常），**绝不出 Kicked**（恢复期无顶号）。
    // 空 report 不清目录（intake 是增量重建不是重置）。返回 intake 条数。
    std::size_t intake_full_report(const std::vector<MirrorEntry>& sessions);

    // ---- 查询面（§5 集中真相）----

    const Entry* find(std::uint64_t player_id) const;
    std::vector<std::uint64_t> online_ids() const;
    std::vector<std::uint64_t> suspended_ids() const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }

private:
    void emit(const Event& event);

    std::unordered_map<std::uint64_t, Entry> entries_;
    std::uint64_t next_epoch_ = 1;           // 进程内单调；顶号/resume 竞争裁决键
    EventSink sink_;
};

// 镜像/重报条目定义（类后：引用 EntryState 完型）
struct MirrorEntry {
    std::uint64_t player_id = 0;
    std::uint64_t anchor_epoch = 0;
    std::uint64_t session_id = 0;
    std::uint32_t gateway_id = 0;
    std::uint32_t zone_id = 0;
    PlayerDirectory::EntryState state = PlayerDirectory::EntryState::Online;
    WorldAssignment assignment{};

    [[nodiscard]] bool operator==(const MirrorEntry&) const noexcept = default;
};

} // namespace apollo::game::session
