#pragma once

#include "apollo/game/social/party.hpp"
#include "apollo/game/world/scene_descriptor.hpp"
#include "apollo/game/world/world.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
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

// 成员在线谓词：false = 不予准入。未设置则跳过在线校验（权威在线目录在
// Login 侧；宿主按自己的会话面装配）。
using MemberOnlineFn = std::function<bool(std::uint64_t player_id)>;

// 远端 World 派生钩子（本地满载转远端时调用）：返回 {instance_id, scene_id}；
// nullopt = 远端拒绝。后续条目以真实跨进程链路替换本钩子。
using RemoteSpawnFn = std::function<std::optional<std::pair<std::uint64_t, std::uint64_t>>(
    std::uint32_t world_id, const LaunchRequest& request)>;

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
    LaunchError validate(const LaunchRequest& request,
                         std::vector<std::uint64_t>& members) const;
    // 选世界：本地有空位优先，否则按 world_id 升序取首个空位（确定性）。
    [[nodiscard]] std::optional<std::uint32_t> pick_world() const;
    void erase_binding(std::unordered_map<std::uint64_t, ActiveSquadInstance>::iterator it);

    apollo::game::social::PartyManager& parties_;
    apollo::game::world::World* local_world_;
    std::uint32_t local_world_id_;
    std::size_t max_squad_size_ = apollo::game::social::Party::kMaxMembers;
    MemberOnlineFn member_online_;
    RemoteSpawnFn remote_spawner_;

    struct WorldSlot {
        std::size_t max_instances = 0;
        std::size_t used = 0;
    };
    std::map<std::uint32_t, WorldSlot> worlds_;

    std::unordered_map<std::uint64_t, ActiveSquadInstance> active_;  // instance_id → 绑定
    std::unordered_map<std::uint64_t, std::uint64_t> party_to_instance_;
    std::unordered_set<std::uint64_t> busy_members_;
    std::uint64_t name_seq_ = 0;
};

} // namespace squad_instance
