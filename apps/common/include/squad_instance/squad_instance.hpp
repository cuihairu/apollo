#pragma once

#include "apollo/game/core/entity.hpp"
#include "apollo/game/social/party.hpp"
#include "apollo/game/world/scene_descriptor.hpp"
#include "apollo/game/world/world.hpp"
#include "squad_instance/settlement_ledger.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace squad_instance {

// 小队创建请求受理（P2 起步项）：验证队伍/成员/人数/准入，选择有容量的
// World，创建独立 Instance 与临时 Scene。目录权威（玩家在哪个 World 的
// 哪个 Instance）仍归 Login 的 OnlineDirectory；本服务持有小队→副本的
// 业务绑定，真实上行链路（客户端→Gateway→World）随后续条目接通。

enum class LaunchError : std::uint32_t {
    Ok = 0,
    InvalidRequest,   // map_id 等请求字段不合法
    PartyNotFound,    // 队伍不存在
    NotLeader,        // 发起者非队长
    PartyTooLarge,    // 人数超过准入上限
    MemberOffline,    // 有成员不在线（在线谓词判否）
    MemberBusy,       // 有成员已绑在活动副本
    NoWorldCapacity,  // 所有已注册 World 均满载
    SpawnRejected,    // 派生被拒（本地创建失败/远端 World 拒绝）
};

struct LaunchRequest {
    std::uint64_t party_id = 0;
    std::uint64_t requester_id = 0;  // 须为队长
    std::uint64_t map_id = 0;        // 临时 Scene 的地图资产 id（非 0）
    std::string map_name;            // 空则用默认名
    float width = 1000.0f;
    float height = 1000.0f;
};

struct LaunchResult {
    LaunchError error = LaunchError::Ok;
    std::uint32_t world_id = 0;
    std::uint64_t instance_id = 0;
    std::uint64_t scene_id = 0;
};

struct ActiveSquadInstance {
    std::uint64_t party_id = 0;
    std::uint32_t world_id = 0;
    std::uint64_t instance_id = 0;
    std::uint64_t scene_id = 0;
    std::vector<std::uint64_t> members;  // 受理时的队伍快照
};

// 刷怪与目标/失败判定参数（P2）：目标完成与超时失败驱动 Instance 进入结算。
struct GameplayGoal {
    std::uint64_t monsters_to_defeat = 0;  // 目标击杀配额（非 0）
    std::uint64_t timeout_ticks = 0;       // 玩法超时 tick（0 = 不限）
};

struct GameplayProgress {
    bool goal_set = false;
    std::uint64_t monsters_to_defeat = 0;
    std::uint64_t monsters_spawned = 0;
    std::uint64_t monsters_defeated = 0;
    std::uint64_t ticks_elapsed = 0;  // Instance 玩法时钟（Running 起）
    bool timed_out = false;           // 失败判定：超时
};

// 成员在线谓词：false = 不予准入。未设置则跳过在线校验（权威在线目录在
// Login 侧；宿主按自己的会话面装配）。
using MemberOnlineFn = std::function<bool(std::uint64_t player_id)>;

// 远端 World 派生钩子（本地满载转远端时调用）：返回 {instance_id, scene_id}；
// nullopt = 远端拒绝。后续条目以真实跨进程链路替换本钩子。
using RemoteSpawnFn = std::function<std::optional<std::pair<std::uint64_t, std::uint64_t>>(
    std::uint32_t world_id, const LaunchRequest& request)>;

// 副本生命周期钩子（宿主装配；须在宿主玩法线程上同步调用——ScriptRuntime
// 归属线程即由钩子宿主保证）：
//   started：Waiting→Running 启动玩法时（宿主据此建副本私有脚本状态）；
//   closing：终态释放前一刻（宿主据此销毁私有脚本状态与副本内残留）。
using InstanceStartedFn = std::function<void(std::uint64_t instance_id, std::uint64_t scene_id)>;
using InstanceClosingFn = std::function<void(std::uint64_t instance_id)>;

class SquadInstanceService {
public:
    SquadInstanceService(apollo::game::social::PartyManager& parties,
                         apollo::game::world::World* local_world,
                         std::uint32_t local_world_id);

    // ---- 装配 ----
    void register_world(std::uint32_t world_id, std::size_t max_instances);
    void set_max_squad_size(std::size_t max_members) noexcept;  // 默认 Party::kMaxMembers
    void set_member_online(MemberOnlineFn fn);
    void set_remote_spawner(RemoteSpawnFn fn);

    // ---- 受理 ----
    LaunchResult launch(const LaunchRequest& request);

    // ---- 成员进入与玩法启动（P2：进入受理快照成员；首进驱动
    // Create→Initialize→Waiting；全员到齐后队长启动 Waiting→Running）----
    enum class EnterError : std::uint32_t {
        Ok = 0,
        NoBinding,      // 小队无活动副本
        NotLocal,       // 副本不在本 World（跨进程进入随后续条目接通）
        NotRoster,      // 非受理快照成员
        StateRejected,  // 副本状态不允许（非 Waiting/Running 或推进失败）
        Duplicate,      // 重复进入
        NotLeader,      // 启动请求者非队长
        NotAllEntered,  // 尚有成员未进入
    };
    EnterError enter(std::uint64_t party_id, std::uint64_t player_id);
    EnterError start_gameplay(std::uint64_t party_id, std::uint64_t requester_id);

    // ---- 刷怪与目标/失败判定（P2：驱动 Instance 进入结算阶段）----
    // 副本刷怪实体（目标判定的击杀对象）：最小类型，玩法内容随后续条目接入。
    class MonsterEntity : public apollo::game::core::Entity {
    public:
        explicit MonsterEntity(apollo::game::core::EntityId id)
            : apollo::game::core::Entity(id, "monster") {}
    };

    enum class GameplayError : std::uint32_t {
        Ok = 0,
        NoBinding,       // 小队无活动副本
        NotLocal,        // 副本不在本 World
        NotRunning,      // 副本未处于 Running 玩法期（含已入结算/终态）
        InvalidRequest,  // 刷怪数量为 0
        InvalidGoal,     // 目标击杀配额为 0
        AlreadySet,      // 目标已设置（一副本一目标）
        NoGoal,          // 目标未设置先刷怪/上报
        WaveTooLarge,    // 累计刷怪超出目标配额
        UnknownMonster,  // 击杀上报的实体不属于本副本
    };

    // 目标设置：仅 Running 玩法期、一副本一次。超时基线取设置时的
    // Instance 玩法时钟。
    GameplayError set_goal(std::uint64_t party_id, const GameplayGoal& goal);
    // 刷怪：怪物实体入副本私有 Scene；累计不得超出目标配额。
    GameplayError spawn_wave(std::uint64_t party_id, std::uint64_t count);
    // 击杀上报：只认本副本存活怪物；击杀数达标即目标完成，
    // Instance.finish() 进入结算阶段（成功线）。
    GameplayError report_monster_defeated(std::uint64_t party_id,
                                          std::uint64_t monster_entity_id);
    // 玩法时钟推进（驱动 Instance tick）；跨过超时线即失败判定，
    // Instance.cancel() 进入结算阶段（失败线，不发成功奖励）。
    GameplayError advance_tick(std::uint64_t party_id, std::uint64_t ticks = 1,
                               double delta_seconds = 0.0);
    // 目标与进度查询；无绑定或非本地副本返回 nullopt。
    [[nodiscard]] std::optional<GameplayProgress> gameplay(
        std::uint64_t party_id) const;

    // ---- 结算幂等落账（P2-4：既有奖励/持久化接口 + 重试/失败/恢复）----
    // 锚点解析装配（须在 launch 前设置；副本受理时开账即取本解析器）。
    void set_anchor_resolver(AnchorResolveFn fn);
    // 显式奖励口（宿主奖励表/过渡期直测面）：单条幂等落账。
    SettlementLedger::ApplyError credit_settlement(std::uint64_t party_id,
                                                   std::uint64_t player_id,
                                                   std::int64_t score);
    // 补账：重试该副本全部未落条目，返回本次补账人数。
    std::size_t retry_settlement(std::uint64_t party_id);
    // 恢复面：按持久化痕迹回填已落账标记（防重启重放双记）。
    void restore_settlement(std::uint64_t party_id,
                            const std::vector<std::uint64_t>& applied_players);
    // 结算状态查询；无绑定（或未开账）返回 nullopt。
    [[nodiscard]] std::optional<SettlementLedger::Snapshot> settlement(
        std::uint64_t party_id) const;

    // ---- 生命周期钩子装配 ----
    void set_instance_started(InstanceStartedFn fn);
    void set_instance_closing(InstanceClosingFn fn);

    // 仅终态（Destroyed）Instance 可释放；本地 World 同步销毁 Instance 与其
    // 临时 Scene（复用 World 终态双回收），容量空位与小队绑定一并归还。
    // 非终态/未知 id 返回 false。远端 World 的实际回收随跨进程条目接通。
    bool release(std::uint64_t instance_id);

    // ---- 查询 ----
    [[nodiscard]] const ActiveSquadInstance* find_by_party(std::uint64_t party_id) const noexcept;
    [[nodiscard]] const ActiveSquadInstance* find_by_instance(
        std::uint64_t instance_id) const noexcept;
    [[nodiscard]] std::size_t active_count() const noexcept;
    [[nodiscard]] std::size_t used_capacity(std::uint32_t world_id) const noexcept;

private:
    // 玩法期记账（instance_id 维度，随绑定释放一并清除）。
    struct GameplayState {
        GameplayGoal goal{};
        bool goal_set = false;
        std::uint64_t goal_base_ticks = 0;  // 目标设置时的玩法时钟
        std::uint64_t spawned = 0;
        std::uint64_t defeated = 0;
        bool timed_out = false;
        std::vector<apollo::game::core::EntityId> monsters;  // 存活怪
    };

    LaunchError validate(const LaunchRequest& request,
                         std::vector<std::uint64_t>& members) const;
    // 玩法期通用守卫：绑定存在 → 本地 → Running。任一不满足即拒。
    GameplayError resolve_running(std::uint64_t party_id,
                                  const ActiveSquadInstance** binding,
                                  apollo::game::world::Instance** instance);
    // 选世界：本地有空位优先，否则按 world_id 升序取首个空位（确定性）。
    [[nodiscard]] std::optional<std::uint32_t> pick_world() const;
    void erase_binding(std::unordered_map<std::uint64_t, ActiveSquadInstance>::iterator it);

    apollo::game::social::PartyManager& parties_;
    apollo::game::world::World* local_world_;
    std::uint32_t local_world_id_;
    std::size_t max_squad_size_ = apollo::game::social::Party::kMaxMembers;
    MemberOnlineFn member_online_;
    RemoteSpawnFn remote_spawner_;
    InstanceStartedFn instance_started_;
    InstanceClosingFn instance_closing_;
    AnchorResolveFn anchor_resolver_;

    struct WorldSlot {
        std::size_t max_instances = 0;
        std::size_t used = 0;
    };
    std::map<std::uint32_t, WorldSlot> worlds_;

    std::unordered_map<std::uint64_t, ActiveSquadInstance> active_;  // instance_id → 绑定
    std::unordered_map<std::uint64_t, std::uint64_t> party_to_instance_;
    std::unordered_map<std::uint64_t, GameplayState> gameplay_;  // instance_id → 玩法记账
    // unique_ptr 保地址稳定：battle 持账本观察指针跨 tick 存活。
    std::unordered_map<std::uint64_t, std::unique_ptr<SettlementLedger>> settlements_;
    std::unordered_set<std::uint64_t> busy_members_;
    std::uint64_t name_seq_ = 0;
    std::uint64_t monster_seq_ = 0;  // 副本怪实体 id 发号（Scene 私有，全局递增）
};

} // namespace squad_instance
