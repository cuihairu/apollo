// 小队创建请求单测（P2：验证队伍/成员/人数/准入，选择有容量的 World，
// 创建独立 Instance 与临时 Scene）。
//
// 覆盖：
//   1. 正常路径：队长发起 → 本地 World 创建独立 Instance + 临时 Scene；
//   2. 校验拒绝：队伍不存在 / 非队长 / 超人数 / 成员离线 / 成员已有副本；
//   3. 容量裁决：本地满载转远端；无容量拒绝；释放后容量回收；
//   4. 回收约束：仅终态 Instance 可释放，Scene 随 Instance 一并回收。

#include "apollo/game/social/party.hpp"
#include "apollo/game/world/world.hpp"
#include "squad_instance/squad_instance.hpp"

#include <cstdint>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#define TEST_ASSERT(cond, msg)                                                               \
    do {                                                                                     \
        if (!(cond)) {                                                                       \
            std::cerr << "FAIL: " << (msg) << " (" << __FILE__ << ":" << __LINE__ << ")"     \
                      << std::endl;                                                          \
            return false;                                                                    \
        }                                                                                    \
    } while (0)

namespace {

using apollo::game::social::PartyManager;
using apollo::game::world::World;
using squad_instance::ActiveSquadInstance;
using squad_instance::LaunchError;
using squad_instance::LaunchRequest;
using squad_instance::SquadInstanceService;

constexpr std::uint64_t kLeader = 100;
constexpr std::uint64_t kMemberB = 101;
constexpr std::uint64_t kMemberC = 102;
constexpr std::uint64_t kOutsider = 999;

LaunchRequest basic_request(std::uint64_t party_id, std::uint64_t requester = kLeader) {
    LaunchRequest req;
    req.party_id = party_id;
    req.requester_id = requester;
    req.map_id = 42;
    req.map_name = "squad-map";
    return req;
}

// 建 3 人队（首位即队长），返回 party_id。
std::uint64_t make_party(PartyManager& parties, std::uint64_t party_id,
                         std::uint64_t leader = kLeader,
                         std::uint64_t b = kMemberB, std::uint64_t c = kMemberC) {
    auto* party = parties.create(party_id, leader);
    party->add_member(b);
    party->add_member(c);
    return party_id;
}

bool test_squad_launch_happy_path() {
    PartyManager parties;
    World world;
    make_party(parties, 1);
    SquadInstanceService service(parties, &world, /*local_world_id=*/7);
    service.register_world(7, 4);

    const auto result = service.launch(basic_request(1));
    TEST_ASSERT(result.error == LaunchError::Ok, "launch accepted");
    TEST_ASSERT(result.world_id == 7, "local world chosen");
    TEST_ASSERT(result.instance_id != 0 && result.scene_id != 0, "ids allocated");

    auto* instance = world.find_instance(result.instance_id);
    TEST_ASSERT(instance != nullptr, "independent Instance created");
    TEST_ASSERT(instance->scene_id() == result.scene_id, "Instance bound to its Scene");
    TEST_ASSERT(world.find_instance_by_scene(result.scene_id) == instance,
                "Scene carries exactly this Instance");

    const auto* binding = service.find_by_party(1);
    TEST_ASSERT(binding != nullptr, "party binding recorded");
    TEST_ASSERT(binding->instance_id == result.instance_id, "binding instance id");
    TEST_ASSERT(binding->members.size() == 3, "roster snapshot taken");

    const auto* again = service.find_by_instance(result.instance_id);
    TEST_ASSERT(again != nullptr && again->party_id == 1, "instance lookup works");
    TEST_ASSERT(service.active_count() == 1, "active count");
    TEST_ASSERT(service.used_capacity(7) == 1, "capacity slot consumed");
    return true;
}

bool test_squad_launch_rejections() {
    PartyManager parties;
    World world;
    SquadInstanceService service(parties, &world, 1);
    service.register_world(1, 4);
    service.set_max_squad_size(4);  // 准入人数上限低于 Party 容量 5

    // 队伍不存在
    auto r = service.launch(basic_request(404));
    TEST_ASSERT(r.error == LaunchError::PartyNotFound, "unknown party rejected");

    // 非队长发起
    make_party(parties, 1);
    LaunchRequest req = basic_request(1);
    req.requester_id = kMemberB;
    r = service.launch(req);
    TEST_ASSERT(r.error == LaunchError::NotLeader, "non-leader rejected");

    // 人数超准入上限（5 人队 vs 上限 4）
    auto* party = parties.find(1);
    party->add_member(103);
    party->add_member(104);
    r = service.launch(basic_request(1));
    TEST_ASSERT(r.error == LaunchError::PartyTooLarge, "oversized party rejected");
    TEST_ASSERT(world.scene_count() == 0 && world.instance_count() == 0,
                "rejections create nothing");

    // 成员离线（先缩队到上限内，避免命中人数闸）
    party->leave(104);
    party->leave(103);
    service.set_member_online([](std::uint64_t id) { return id != kMemberC; });
    r = service.launch(basic_request(1));
    TEST_ASSERT(r.error == LaunchError::MemberOffline, "offline member rejected");
    return true;
}

bool test_squad_launch_busy_member() {
    PartyManager parties;
    World world;
    make_party(parties, 1);
    make_party(parties, 2, 200, kMemberB, 202);  // 第二队共享成员 B
    SquadInstanceService service(parties, &world, 1);
    service.register_world(1, 4);

    auto r = service.launch(basic_request(1));
    TEST_ASSERT(r.error == LaunchError::Ok, "first party admitted");

    r = service.launch(basic_request(2, 200));
    TEST_ASSERT(r.error == LaunchError::MemberBusy, "member with active instance rejected");
    TEST_ASSERT(service.active_count() == 1, "no second binding created");
    return true;
}

bool test_squad_launch_capacity_and_recycle() {
    PartyManager parties;
    World world;
    make_party(parties, 1, kLeader, kMemberB, kMemberC);
    make_party(parties, 2, 200, 201, 202);
    make_party(parties, 3, 300, 301, 302);
    SquadInstanceService service(parties, &world, 1);
    service.register_world(1, 1);  // 本地世界只挂 1 个副本

    TEST_ASSERT(service.launch(basic_request(1)).error == LaunchError::Ok, "first admitted");
    auto r = service.launch(basic_request(2, 200));
    TEST_ASSERT(r.error == LaunchError::NoWorldCapacity, "capacity exhausted rejected");
    TEST_ASSERT(world.scene_count() == 1, "rejected request left no scene");

    // 未到终态不可释放
    const auto* binding = service.find_by_party(1);
    TEST_ASSERT(binding != nullptr, "binding present");
    TEST_ASSERT(!service.release(binding->instance_id), "active instance not releasable");

    // 推进到终态后释放：Instance 与临时 Scene 一并回收，容量空位归还
    auto* instance = world.find_instance(binding->instance_id);
    TEST_ASSERT(instance != nullptr, "instance resolvable");
    TEST_ASSERT(instance->cancel() && instance->settle() && instance->drain() &&
                    instance->destroy(),
                "instance reaches terminal state");
    TEST_ASSERT(service.release(binding->instance_id), "terminal instance released");
    TEST_ASSERT(world.find_instance(binding->instance_id) == nullptr &&
                    world.scene_count() == 0,
                "instance and temp scene reclaimed");
    TEST_ASSERT(service.used_capacity(1) == 0, "capacity slot returned");

    TEST_ASSERT(service.launch(basic_request(3, 300)).error == LaunchError::Ok,
                "capacity freed for next squad");
    return true;
}

bool test_squad_launch_selects_world_with_capacity() {
    PartyManager parties;
    World world;
    make_party(parties, 1);
    make_party(parties, 2, 200, 201, 202);
    make_party(parties, 3, 300, 301, 302);
    SquadInstanceService service(parties, &world, /*local_world_id=*/1);
    service.register_world(1, 1);  // 本地满载
    service.register_world(2, 2);  // 远端有容量

    std::map<std::uint32_t, LaunchRequest> remote_calls;
    service.set_remote_spawner([&remote_calls](std::uint32_t world_id,
                                               const LaunchRequest& req)
                                   -> std::optional<std::pair<std::uint64_t, std::uint64_t>> {
        remote_calls[world_id] = req;
        return std::make_pair(static_cast<std::uint64_t>(9000 + world_id),
                              static_cast<std::uint64_t>(8000 + world_id));
    });

    auto r = service.launch(basic_request(1));
    TEST_ASSERT(r.error == LaunchError::Ok && r.world_id == 1, "local first");
    TEST_ASSERT(world.instance_count() == 1, "local instance created");

    r = service.launch(basic_request(2, 200));
    TEST_ASSERT(r.error == LaunchError::Ok && r.world_id == 2, "overflow routed to remote");
    TEST_ASSERT(remote_calls.count(2) == 1, "remote spawner invoked once");
    TEST_ASSERT(remote_calls[2].party_id == 2 && remote_calls[2].map_id == 42,
                "remote request carries launch payload");
    TEST_ASSERT(r.instance_id == 9002 && r.scene_id == 8002, "remote ids propagated");
    TEST_ASSERT(world.instance_count() == 1, "remote spawn left local world untouched");
    TEST_ASSERT(service.active_count() == 2, "both bindings tracked");

    // 远端拒绝：不产生绑定、不占容量
    service.set_remote_spawner([](std::uint32_t, const LaunchRequest&)
                                   -> std::optional<std::pair<std::uint64_t, std::uint64_t>> {
        return std::nullopt;
    });
    auto refused = service.launch(basic_request(3, 300));
    TEST_ASSERT(refused.error == LaunchError::SpawnRejected, "remote refusal propagated");
    TEST_ASSERT(service.active_count() == 2, "refused launch bound nothing");
    TEST_ASSERT(service.find_by_party(3) == nullptr, "no binding for refused launch");
    return true;
}

bool test_squad_launch_temp_scene_fresh_per_instance() {
    PartyManager parties;
    World world;
    make_party(parties, 1, kLeader, kMemberB, kMemberC);
    make_party(parties, 2, 200, 201, 202);
    SquadInstanceService service(parties, &world, 1);
    service.register_world(1, 4);

    auto first = service.launch(basic_request(1));
    auto second = service.launch(basic_request(2, 200));
    TEST_ASSERT(first.error == LaunchError::Ok && second.error == LaunchError::Ok,
                "both squads admitted");
    TEST_ASSERT(first.scene_id != second.scene_id, "fresh temp scene per Instance");
    TEST_ASSERT(first.instance_id != second.instance_id, "independent instances");
    TEST_ASSERT(world.scene_count() == 2, "two temp scenes live");
    auto* inst = world.find_instance(first.instance_id);
    TEST_ASSERT(inst != nullptr &&
                world.find_instance_by_scene(second.scene_id) != inst,
                "scene-to-instance pairing is one-to-one");
    return true;
}

struct Case {
    const char* name;
    bool (*fn)();
};

} // namespace

int main() {
    const std::vector<Case> cases = {
        {"squad_launch_happy_path", test_squad_launch_happy_path},
        {"squad_launch_rejections", test_squad_launch_rejections},
        {"squad_launch_busy_member", test_squad_launch_busy_member},
        {"squad_launch_capacity_and_recycle", test_squad_launch_capacity_and_recycle},
        {"squad_launch_selects_world_with_capacity", test_squad_launch_selects_world_with_capacity},
        {"squad_launch_temp_scene_fresh_per_instance", test_squad_launch_temp_scene_fresh_per_instance},
    };

    int passed = 0;
    int failed = 0;
    for (const auto& c : cases) {
        std::cout << "[ RUN  ] " << c.name << std::endl;
        if (c.fn()) {
            std::cout << "[  OK  ] " << c.name << std::endl;
            ++passed;
        } else {
            std::cout << "[ FAIL ] " << c.name << std::endl;
            ++failed;
        }
    }

    std::cout << "SquadInstanceTests: " << passed << " passed, " << failed << " failed"
              << std::endl;
    return failed == 0 ? 0 : 1;
}
