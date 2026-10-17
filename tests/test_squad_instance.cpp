// 小队副本单测（P2：创建受理、成员进入与玩法启动、刷怪与目标/失败判定）。
//
// 覆盖：
//   1. 正常路径：队长发起 → 本地 World 创建独立 Instance + 临时 Scene；
//   2. 校验拒绝：队伍不存在 / 非队长 / 超人数 / 成员离线 / 成员已有副本；
//   3. 容量裁决：本地满载转远端；无容量拒绝；释放后容量回收；
//   4. 回收约束：仅终态 Instance 可释放，Scene 随 Instance 一并回收；
//   5. 进入与启动：受理快照校验、首进建场、全员到齐队长启动 Running；
//   6. 实体隔离：各副本 Scene 私有实体集与 AOI；
//   7. 刷怪与目标：配额刷怪入 Scene、击杀上报达标 → Finishing；
//   8. 失败判定：超时 → cancel 进入结算（不发成功奖励）、可释放回收。

#include "apollo/game/core/entity.hpp"
#include "apollo/game/session/anchor_reward_sink.hpp"
#include "apollo/game/session/player_anchor.hpp"
#include "apollo/game/social/party.hpp"
#include "apollo/game/world/scene.hpp"
#include "apollo/game/world/world.hpp"
#include "squad_instance/squad_instance.hpp"

#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
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

// 最小实体（实体隔离用例；模式同 test_scene 的 CountingEntity）
class TaggedEntity : public apollo::game::core::Entity {
public:
    explicit TaggedEntity(apollo::game::core::EntityId id) : Entity(id, "tagged") {}
};

bool test_squad_enter_and_start_gameplay() {
    PartyManager parties;
    World world;
    make_party(parties, 1);
    SquadInstanceService service(parties, &world, 1);
    service.register_world(1, 4);

    TEST_ASSERT(service.enter(404, kLeader) ==
                    SquadInstanceService::EnterError::NoBinding,
                "enter without binding rejected");

    std::uint64_t started_instance = 0, started_scene = 0;
    int started_calls = 0;
    service.set_instance_started(
        [&](std::uint64_t instance_id, std::uint64_t scene_id) {
            ++started_calls;
            started_instance = instance_id;
            started_scene = scene_id;
        });

    const auto launch = service.launch(basic_request(1));
    TEST_ASSERT(launch.error == LaunchError::Ok, "launched");
    TEST_ASSERT(service.enter(1, kOutsider) ==
                    SquadInstanceService::EnterError::NotRoster,
                "outsider enter rejected");
    auto* instance = world.find_instance(launch.instance_id);
    TEST_ASSERT(instance != nullptr &&
                    instance->state() == apollo::game::world::Instance::State::Create,
                "fresh instance in Create");

    // 首进驱动 Create→Initialize→Waiting
    TEST_ASSERT(service.enter(1, kLeader) == SquadInstanceService::EnterError::Ok,
                "leader enters");
    TEST_ASSERT(instance->state() == apollo::game::world::Instance::State::Waiting,
                "first enter drives to Waiting");
    TEST_ASSERT(service.enter(1, kMemberB) == SquadInstanceService::EnterError::Ok,
                "member B enters");
    TEST_ASSERT(service.enter(1, kLeader) == SquadInstanceService::EnterError::Duplicate,
                "duplicate enter rejected");

    // 未全员到齐：非队长拒、缺员拒
    TEST_ASSERT(service.start_gameplay(1, kMemberB) ==
                    SquadInstanceService::EnterError::NotLeader,
                "non-leader cannot start");
    TEST_ASSERT(service.start_gameplay(1, kLeader) ==
                    SquadInstanceService::EnterError::NotAllEntered,
                "start blocked until all roster entered");

    TEST_ASSERT(service.enter(1, kMemberC) == SquadInstanceService::EnterError::Ok,
                "member C enters");
    TEST_ASSERT(service.start_gameplay(1, kLeader) == SquadInstanceService::EnterError::Ok,
                "leader starts gameplay");
    TEST_ASSERT(instance->state() == apollo::game::world::Instance::State::Running,
                "instance Running");
    TEST_ASSERT(started_calls == 1 && started_instance == launch.instance_id &&
                    started_scene == launch.scene_id,
                "started hook fired once with ids");
    TEST_ASSERT(service.start_gameplay(1, kLeader) ==
                    SquadInstanceService::EnterError::StateRejected,
                "second start rejected");
    return true;
}

bool test_squad_instance_isolation_between_instances() {
    PartyManager parties;
    World world;
    make_party(parties, 1);
    make_party(parties, 2, 200, 201, 202);
    SquadInstanceService service(parties, &world, 1);
    service.register_world(1, 4);

    const auto a = service.launch(basic_request(1));
    const auto b = service.launch(basic_request(2, 200));
    TEST_ASSERT(a.error == LaunchError::Ok && b.error == LaunchError::Ok, "both live");
    TEST_ASSERT(a.scene_id != b.scene_id, "distinct temp scenes");

    auto* scene_a = world.find_scene(a.scene_id);
    auto* scene_b = world.find_scene(b.scene_id);
    TEST_ASSERT(scene_a != nullptr && scene_b != nullptr && scene_a != scene_b,
                "independent Scene runtimes");

    // 实体隔离：A 场实体在 B 场不可见（AOI 与实体集均为 Scene 私有）
    scene_a->spawn_entity(std::make_shared<TaggedEntity>(apollo::game::core::EntityId(10)));
    TEST_ASSERT(scene_a->get_entity_count() == 1, "scene A holds its entity");
    TEST_ASSERT(scene_b->get_entity_count() == 0, "scene B sees nothing of A");
    TEST_ASSERT(scene_b->get_entity(apollo::game::core::EntityId(10)) == nullptr,
                "entity lookup cannot cross scenes");

    scene_b->spawn_entity(std::make_shared<TaggedEntity>(apollo::game::core::EntityId(11)));
    TEST_ASSERT(scene_a->get_entity_count() == 1, "scene A unaffected by B spawn");
    return true;
}

// 队 1 走完整启动流（launch → 三人进入 → 队长启动），返回 Running 实例。
// launch_first=false 用于绑定已存在（受理校验另测）的用例。
apollo::game::world::Instance* run_party_to_running(SquadInstanceService& service,
                                                    World& world, bool launch_first = true) {
    if (launch_first && service.launch(basic_request(1)).error != LaunchError::Ok)
        return nullptr;
    for (const auto member : std::vector<std::uint64_t>{kLeader, kMemberB, kMemberC}) {
        if (service.enter(1, member) != SquadInstanceService::EnterError::Ok) return nullptr;
    }
    if (service.start_gameplay(1, kLeader) != SquadInstanceService::EnterError::Ok)
        return nullptr;
    const auto* binding = service.find_by_party(1);
    if (!binding) return nullptr;
    return world.find_instance(binding->instance_id);
}

bool test_squad_gameplay_goal_completion_drives_settlement() {
    PartyManager parties;
    World world;
    make_party(parties, 1);
    SquadInstanceService service(parties, &world, 1);
    service.register_world(1, 4);

    // 未启动不可设目标；启动后设目标，一副本一目标
    squad_instance::GameplayGoal goal;
    TEST_ASSERT(service.set_goal(404, goal) ==
                    SquadInstanceService::GameplayError::NoBinding,
                "goal without binding rejected");
    TEST_ASSERT(service.launch(basic_request(1)).error == LaunchError::Ok, "launched");
    TEST_ASSERT(service.set_goal(1, goal) ==
                    SquadInstanceService::GameplayError::NotRunning,
                "goal rejected before Running");
    auto* instance = run_party_to_running(service, world, /*launch_first=*/false);
    TEST_ASSERT(instance != nullptr &&
                    instance->state() == apollo::game::world::Instance::State::Running,
                "instance Running");
    TEST_ASSERT(service.set_goal(1, goal) ==
                    SquadInstanceService::GameplayError::InvalidGoal,
                "zero quota rejected");
    goal.monsters_to_defeat = 3;
    TEST_ASSERT(service.set_goal(1, goal) == SquadInstanceService::GameplayError::Ok,
                "goal set while Running");
    TEST_ASSERT(service.set_goal(1, goal) ==
                    SquadInstanceService::GameplayError::AlreadySet,
                "goal set once per instance");

    // 刷怪入副本私有 Scene；累计不得超出目标配额
    const auto* binding = service.find_by_party(1);
    auto* scene = world.find_scene(binding->scene_id);
    TEST_ASSERT(scene != nullptr && scene->get_entity_count() == 0, "scene empty pre-waves");
    TEST_ASSERT(service.spawn_wave(1, 2) == SquadInstanceService::GameplayError::Ok,
                "first wave admitted");
    TEST_ASSERT(scene->get_entity_count() == 2, "wave monsters live in instance scene");
    TEST_ASSERT(service.spawn_wave(1, 2) ==
                    SquadInstanceService::GameplayError::WaveTooLarge,
                "wave capped by quota");
    TEST_ASSERT(scene->get_entity_count() == 2, "rejected wave spawned nothing");
    TEST_ASSERT(service.spawn_wave(1, 0) ==
                    SquadInstanceService::GameplayError::InvalidRequest,
                "empty wave rejected");
    TEST_ASSERT(service.spawn_wave(1, 1) == SquadInstanceService::GameplayError::Ok,
                "final wave fills quota");

    // 击杀上报：只认本副本怪物；达标即目标完成判定成立，结算随下一拍进入
    //（成功线 → Finishing——分值先入 battle 输入序）
    TEST_ASSERT(service.report_monster_defeated(1, 424242) ==
                    SquadInstanceService::GameplayError::UnknownMonster,
                "unknown monster rejected");
    TEST_ASSERT(service.report_monster_defeated(1, 1, kLeader) ==
                    SquadInstanceService::GameplayError::Ok,
                "kill 1");
    TEST_ASSERT(service.report_monster_defeated(1, 1, kLeader) ==
                    SquadInstanceService::GameplayError::UnknownMonster,
                "dead monster not reportable twice");
    TEST_ASSERT(service.report_monster_defeated(1, 2, kMemberB) ==
                    SquadInstanceService::GameplayError::Ok,
                "kill 2");
    TEST_ASSERT(instance->state() == apollo::game::world::Instance::State::Running,
                "quota not yet met");
    TEST_ASSERT(service.report_monster_defeated(1, 3, kMemberC) ==
                    SquadInstanceService::GameplayError::Ok,
                "kill 3 meets quota");
    TEST_ASSERT(instance->state() == apollo::game::world::Instance::State::Running,
                "goal met pending tick");
    TEST_ASSERT(service.advance_tick(1, 1) == SquadInstanceService::GameplayError::Ok,
                "tick carries kill scores then settles");
    TEST_ASSERT(instance->state() == apollo::game::world::Instance::State::Finishing,
                "goal completion drives settlement entry");
    TEST_ASSERT(scene->get_entity_count() == 0, "defeated monsters despawned");

    // 进入结算后拒绝迟到玩法输入
    TEST_ASSERT(service.report_monster_defeated(1, 4) ==
                    SquadInstanceService::GameplayError::NotRunning,
                "post-settlement kill rejected");
    TEST_ASSERT(service.spawn_wave(1, 1) == SquadInstanceService::GameplayError::NotRunning,
                "post-settlement spawn rejected");
    TEST_ASSERT(service.advance_tick(1) == SquadInstanceService::GameplayError::NotRunning,
                "post-settlement tick rejected");

    const auto progress = service.gameplay(1);
    TEST_ASSERT(progress.has_value() && progress->goal_set &&
                    progress->monsters_defeated == 3 && progress->monsters_spawned == 3,
                "progress reflects goal run");
    TEST_ASSERT(!progress->timed_out, "no timeout flagged");
    TEST_ASSERT(instance->settle(), "settlement advances to Rewarding");
    return true;
}

bool test_squad_gameplay_timeout_drives_failure_settlement() {
    PartyManager parties;
    World world;
    make_party(parties, 1);
    SquadInstanceService service(parties, &world, 1);
    service.register_world(1, 4);

    auto* instance = run_party_to_running(service, world);
    TEST_ASSERT(instance != nullptr, "running baseline");

    squad_instance::GameplayGoal goal;
    goal.monsters_to_defeat = 3;
    goal.timeout_ticks = 10;
    TEST_ASSERT(service.set_goal(1, goal) == SquadInstanceService::GameplayError::Ok,
                "goal with timeout set");
    TEST_ASSERT(service.spawn_wave(1, 3) == SquadInstanceService::GameplayError::Ok,
                "wave spawned");

    // 玩法时钟由 Instance tick 驱动；未到期限不判负
    TEST_ASSERT(service.advance_tick(1, 5) == SquadInstanceService::GameplayError::Ok,
                "ticks advance");
    TEST_ASSERT(instance->tick_count() == 5, "instance gameplay clock advances");
    TEST_ASSERT(instance->state() == apollo::game::world::Instance::State::Running,
                "within timeout");
    TEST_ASSERT(!service.gameplay(1)->timed_out, "no timeout yet");

    // 超时 → 失败判定：cancel 进入结算（不发成功奖励）
    TEST_ASSERT(service.advance_tick(1, 5) == SquadInstanceService::GameplayError::Ok,
                "timeout-crossing ticks accepted");
    TEST_ASSERT(service.gameplay(1)->timed_out, "timeout flagged");
    TEST_ASSERT(instance->state() == apollo::game::world::Instance::State::Finishing,
                "failure drives settlement entry");
    TEST_ASSERT(service.report_monster_defeated(1, 1) ==
                    SquadInstanceService::GameplayError::NotRunning,
                "late kill rejected after failure");
    const auto progress = service.gameplay(1);
    TEST_ASSERT(progress->monsters_defeated == 0 && progress->timed_out,
                "no kills credited on failure");

    // 失败收尾走完终态链并释放：Scene/Instance 回收、容量归还
    for (const auto member : std::vector<std::uint64_t>{kLeader, kMemberB, kMemberC}) {
        TEST_ASSERT(instance->leave(apollo::game::core::PlayerId{member}), "member leaves");
    }
    TEST_ASSERT(instance->settle() && instance->drain() && instance->destroy(),
                "failure path reaches terminal state");
    TEST_ASSERT(service.release(instance->id()), "released");
    TEST_ASSERT(world.scene_count() == 0 && world.instance_count() == 0,
                "scene and instance reclaimed");
    TEST_ASSERT(service.used_capacity(1) == 0, "capacity returned");
    TEST_ASSERT(!service.gameplay(1).has_value(), "progress cleared with binding");
    return true;
}

bool test_squad_gameplay_guards_and_quotas() {
    PartyManager parties;
    World world;
    make_party(parties, 1);
    SquadInstanceService service(parties, &world, 1);
    service.register_world(1, 4);

    // 无绑定 / 非玩法期一律拒绝
    TEST_ASSERT(service.set_goal(404, {}) == SquadInstanceService::GameplayError::NoBinding,
                "goal without binding rejected");
    TEST_ASSERT(service.spawn_wave(404, 1) ==
                    SquadInstanceService::GameplayError::NoBinding,
                "spawn without binding rejected");
    TEST_ASSERT(service.report_monster_defeated(404, 1) ==
                    SquadInstanceService::GameplayError::NoBinding,
                "report without binding rejected");
    TEST_ASSERT(service.advance_tick(404) ==
                    SquadInstanceService::GameplayError::NoBinding,
                "tick without binding rejected");

    // Waiting（全员已进未启动）仍非玩法期
    TEST_ASSERT(service.launch(basic_request(1)).error == LaunchError::Ok, "launched");
    for (const auto member : std::vector<std::uint64_t>{kLeader, kMemberB, kMemberC}) {
        TEST_ASSERT(service.enter(1, member) == SquadInstanceService::EnterError::Ok,
                    "roster enters");
    }
    const auto* binding = service.find_by_party(1);
    auto* instance = world.find_instance(binding->instance_id);
    TEST_ASSERT(instance->state() == apollo::game::world::Instance::State::Waiting,
                "Waiting pre-start");
    TEST_ASSERT(service.set_goal(1, {}) == SquadInstanceService::GameplayError::NotRunning,
                "goal rejected while Waiting");

    // 目标未设先刷怪/上报：拒；tick 可走（玩法时钟与目标解耦）
    TEST_ASSERT(service.start_gameplay(1, kLeader) ==
                    SquadInstanceService::EnterError::Ok,
                "started");
    TEST_ASSERT(service.spawn_wave(1, 1) == SquadInstanceService::GameplayError::NoGoal,
                "spawn without goal rejected");
    TEST_ASSERT(service.report_monster_defeated(1, 1) ==
                    SquadInstanceService::GameplayError::NoGoal,
                "report without goal rejected");
    TEST_ASSERT(service.advance_tick(1, 3) == SquadInstanceService::GameplayError::Ok,
                "clock runs without goal");
    TEST_ASSERT(instance->tick_count() == 3, "ticks counted from start");

    squad_instance::GameplayGoal goal;
    goal.monsters_to_defeat = 2;
    goal.timeout_ticks = 100;
    TEST_ASSERT(service.set_goal(1, goal) == SquadInstanceService::GameplayError::Ok,
                "goal set after ticks");
    TEST_ASSERT(service.gameplay(1)->ticks_elapsed == 3,
                "progress clock mirrors instance clock");
    return true;
}


// 结算锚点面：roster 成员各持锚；missing 名单注入 resolve 失败（pending 路径）。
struct AnchorTable {
    std::map<std::uint64_t, std::unique_ptr<apollo::game::session::PlayerAnchor>> anchors;
    std::set<std::uint64_t> missing;
    apollo::game::session::PlayerAnchor* resolve(std::uint64_t id) {
        if (missing.contains(id)) return nullptr;
        const auto it = anchors.find(id);
        return it == anchors.end() ? nullptr : it->second.get();
    }
};

AnchorTable make_anchor_table(std::initializer_list<std::uint64_t> ids) {
    AnchorTable table;
    for (const auto id : ids)
        table.anchors.emplace(id, std::make_unique<apollo::game::session::PlayerAnchor>(id));
    return table;
}

std::int64_t exp_of(const AnchorTable& table, std::uint64_t id) {
    return table.anchors.at(id)->progress(apollo::game::session::AnchorRewardSink::kExpKey);
}

bool test_squad_settlement_idempotent_credit_and_retry() {
    PartyManager parties;
    World world;
    make_party(parties, 1);
    SquadInstanceService service(parties, &world, 1);
    service.register_world(1, 4);
    auto table = make_anchor_table({kLeader, kMemberB, kMemberC});
    service.set_anchor_resolver([&table](std::uint64_t id) { return table.resolve(id); });

    const auto launch = service.launch(basic_request(1));
    TEST_ASSERT(launch.error == LaunchError::Ok, "launched");
    TEST_ASSERT(!service.settlement(404).has_value(), "no settlement for unknown party");

    // 受理即开账：名册快照入账本，未落账不 complete
    auto snap = service.settlement(1);
    TEST_ASSERT(snap.has_value() && snap->roster_size == 3 && snap->applied == 0 &&
                    !snap->complete,
                "settlement opened with roster snapshot");

    // 显式奖励口（宿主奖励表/直测面）：幂等——重复落账只记一次
    TEST_ASSERT(service.credit_settlement(1, kLeader, 50) ==
                    squad_instance::SettlementLedger::ApplyError::Ok,
                "leader credited");
    TEST_ASSERT(exp_of(table, kLeader) == 50, "reward lands on anchor exp");
    TEST_ASSERT(table.anchors.at(kLeader)->needs_save(),
                "settlement marks anchor dirty for write-behind");
    TEST_ASSERT(service.credit_settlement(1, kLeader, 50) ==
                    squad_instance::SettlementLedger::ApplyError::AlreadyApplied,
                "double credit refused");
    TEST_ASSERT(exp_of(table, kLeader) == 50, "no double exp on retry");
    TEST_ASSERT(service.credit_settlement(1, kOutsider, 10) ==
                    squad_instance::SettlementLedger::ApplyError::UnknownPlayer,
                "non-roster credit refused");

    // 锚点暂不可得：挂账不落；恢复后补账一次
    table.missing.insert(kMemberC);
    TEST_ASSERT(service.credit_settlement(1, kMemberC, 30) ==
                    squad_instance::SettlementLedger::ApplyError::AnchorUnavailable,
                "unavailable anchor unsettled");
    snap = service.settlement(1);
    TEST_ASSERT(snap->applied == 1 && snap->pending == 1 && !snap->complete,
                "partial settlement visible");
    table.missing.erase(kMemberC);
    TEST_ASSERT(service.retry_settlement(1) == 1, "retry drains unsettled");
    TEST_ASSERT(exp_of(table, kMemberC) == 30, "drained reward lands once");
    TEST_ASSERT(service.retry_settlement(1) == 0, "settled entries not re-credited");

    // 剩余成员经补账收口
    TEST_ASSERT(service.credit_settlement(1, kMemberB, 20) ==
                    squad_instance::SettlementLedger::ApplyError::Ok,
                "member B credited");
    snap = service.settlement(1);
    TEST_ASSERT(snap->complete && snap->applied == 3, "settlement complete");
    return true;
}

bool test_squad_settlement_battle_settles_on_goal_completion() {
    PartyManager parties;
    World world;
    make_party(parties, 1);
    SquadInstanceService service(parties, &world, 1);
    service.register_world(1, 4);
    auto table = make_anchor_table({kLeader, kMemberB, kMemberC});
    service.set_anchor_resolver([&table](std::uint64_t id) { return table.resolve(id); });

    TEST_ASSERT(service.launch(basic_request(1)).error == LaunchError::Ok, "launched");
    auto* instance = run_party_to_running(service, world, /*launch_first=*/false);
    TEST_ASSERT(instance != nullptr, "running");

    // 目标完成 → Instance finish → battle 结算段全名册落账（既有奖励接口）
    squad_instance::GameplayGoal goal;
    goal.monsters_to_defeat = 1;
    TEST_ASSERT(service.set_goal(1, goal) == SquadInstanceService::GameplayError::Ok,
                "goal set");
    TEST_ASSERT(service.spawn_wave(1, 1) == SquadInstanceService::GameplayError::Ok,
                "wave spawned");
    const auto* binding = service.find_by_party(1);
    TEST_ASSERT(service.report_monster_defeated(1, 1) ==
                    SquadInstanceService::GameplayError::Ok,
                "goal met");
    TEST_ASSERT(service.advance_tick(1, 1) == SquadInstanceService::GameplayError::Ok,
                "settle tick");
    TEST_ASSERT(instance->state() == apollo::game::world::Instance::State::Finishing,
                "settlement entered");
    auto snap = service.settlement(1);
    TEST_ASSERT(snap.has_value() && snap->complete && snap->applied == 3,
                "battle settle applied whole roster");

    // battle 结算已收口：显式补记一律幂等拒绝
    TEST_ASSERT(service.credit_settlement(1, kLeader, 1) ==
                    squad_instance::SettlementLedger::ApplyError::AlreadyApplied,
                "post-settle credit refused");

    // 无上报人归属（reporter=0）→ 事件不落名册账：零分值标记已落账但不写锚
    TEST_ASSERT(exp_of(table, kLeader) == 0, "unattributed kill credits no exp");
    TEST_ASSERT(!table.anchors.at(kLeader)->needs_save(),
                "zero-score settle leaves anchor clean");
    return true;
}

// 击杀上报 → battle 正路径分值：上报人命中分值经 battle 输入序落账
//（分值来源唯一 = battle 命中事件，不在账本侧另开入口）。
bool test_squad_kill_scores_flow_through_battle() {
    PartyManager parties;
    World world;
    make_party(parties, 1);
    SquadInstanceService service(parties, &world, 1);
    service.register_world(1, 4);
    auto table = make_anchor_table({kLeader, kMemberB, kMemberC});
    service.set_anchor_resolver([&table](std::uint64_t id) { return table.resolve(id); });

    TEST_ASSERT(service.launch(basic_request(1)).error == LaunchError::Ok, "launched");
    auto* instance = run_party_to_running(service, world, /*launch_first=*/false);
    TEST_ASSERT(instance != nullptr, "running");

    squad_instance::GameplayGoal goal;
    goal.monsters_to_defeat = 1;
    TEST_ASSERT(service.set_goal(1, goal) == SquadInstanceService::GameplayError::Ok,
                "goal set");
    TEST_ASSERT(service.spawn_wave(1, 1) == SquadInstanceService::GameplayError::Ok,
                "wave spawned");

    // 击杀带上报人 → 下一拍输入序携带命中 → finish → 结算分值 > 0
    TEST_ASSERT(service.report_monster_defeated(1, 1, kLeader) ==
                    SquadInstanceService::GameplayError::Ok,
                "attributed kill");
    TEST_ASSERT(service.advance_tick(1, 1) == SquadInstanceService::GameplayError::Ok,
                "tick rides the hit then settles");
    TEST_ASSERT(instance->state() == apollo::game::world::Instance::State::Finishing,
                "settlement entered after score ride");
    const auto snap = service.settlement(1);
    TEST_ASSERT(snap.has_value() && snap->complete && snap->applied == 3,
                "roster settled");
    TEST_ASSERT(exp_of(table, kLeader) > 0, "reporter score credited via battle");
    TEST_ASSERT(exp_of(table, kMemberB) == 0, "non-reporter scores nothing");
    TEST_ASSERT(table.anchors.at(kLeader)->needs_save(), "score dirties anchor");
    return true;
}

bool test_squad_settlement_recovery_and_failure_line() {
    PartyManager parties;
    World world;
    make_party(parties, 1);
    SquadInstanceService service(parties, &world, 1);
    service.register_world(1, 4);
    auto table = make_anchor_table({kLeader, kMemberB, kMemberC});
    service.set_anchor_resolver([&table](std::uint64_t id) { return table.resolve(id); });

    const auto launch = service.launch(basic_request(1));
    TEST_ASSERT(launch.error == LaunchError::Ok, "launched");

    // 恢复面：回填已落账标记，重放不双记（重启后防重复结算）
    service.restore_settlement(1, {kLeader});
    TEST_ASSERT(service.credit_settlement(1, kLeader, 50) ==
                    squad_instance::SettlementLedger::ApplyError::AlreadyApplied,
                "restored mark dedups replay");
    TEST_ASSERT(exp_of(table, kLeader) == 0, "replay credits nothing");
    auto snap = service.settlement(1);
    TEST_ASSERT(snap->applied == 1 && !snap->complete, "restored state visible");

    // 失败线：超时 cancel 收场——battle 不结算、无奖励落账
    auto* instance = run_party_to_running(service, world, /*launch_first=*/false);
    TEST_ASSERT(instance != nullptr, "running");
    squad_instance::GameplayGoal goal;
    goal.monsters_to_defeat = 3;
    goal.timeout_ticks = 1;
    TEST_ASSERT(service.set_goal(1, goal) == SquadInstanceService::GameplayError::Ok,
                "goal with timeout");
    TEST_ASSERT(service.advance_tick(1, 2) == SquadInstanceService::GameplayError::Ok,
                "timeout crossed");
    snap = service.settlement(1);
    TEST_ASSERT(snap->applied == 1 && !snap->complete,
                "failure line settles no rewards");
    TEST_ASSERT(service.retry_settlement(1) == 0, "nothing to retry after failure");
    TEST_ASSERT(exp_of(table, kMemberB) == 0 && exp_of(table, kMemberC) == 0,
                "no exp on failure");

    // 终态释放后账本随绑定清除
    for (const auto member : std::vector<std::uint64_t>{kLeader, kMemberB, kMemberC}) {
        TEST_ASSERT(instance->leave(apollo::game::core::PlayerId{member}), "member leaves");
    }
    TEST_ASSERT(instance->settle() && instance->drain() && instance->destroy(),
                "terminal reached");
    TEST_ASSERT(service.release(launch.instance_id), "released");
    TEST_ASSERT(!service.settlement(1).has_value(), "settlement cleared with binding");
    return true;
}


bool test_squad_dismiss_evicts_players_and_reclaims() {
    PartyManager parties;
    World world;
    make_party(parties, 1);
    make_party(parties, 2, 200, 201, 202);
    SquadInstanceService service(parties, &world, 1);
    service.register_world(1, 4);
    auto table = make_anchor_table({kLeader, kMemberB, kMemberC});
    service.set_anchor_resolver([&table](std::uint64_t id) { return table.resolve(id); });

    int closing_calls = 0;
    service.set_instance_closing([&](std::uint64_t) { ++closing_calls; });

    const auto launch = service.launch(basic_request(1));
    TEST_ASSERT(launch.error == LaunchError::Ok, "launched");
    auto* instance = run_party_to_running(service, world, /*launch_first=*/false);
    TEST_ASSERT(instance != nullptr, "running");

    // 玩法期未入结算：不允许收尾
    TEST_ASSERT(!service.dismiss(1), "dismiss refused while Running");

    // 成功线入结算 → 收尾：移出玩家 → 结算推进 → 销毁 → 释放
    squad_instance::GameplayGoal goal;
    goal.monsters_to_defeat = 1;
    TEST_ASSERT(service.set_goal(1, goal) == SquadInstanceService::GameplayError::Ok,
                "goal set");
    TEST_ASSERT(service.spawn_wave(1, 1) == SquadInstanceService::GameplayError::Ok,
                "wave spawned");
    TEST_ASSERT(service.report_monster_defeated(1, 1) ==
                    SquadInstanceService::GameplayError::Ok,
                "goal met");
    TEST_ASSERT(service.advance_tick(1, 1) == SquadInstanceService::GameplayError::Ok,
                "settle tick");
    TEST_ASSERT(service.dismiss(1), "dismiss completes teardown");
    TEST_ASSERT(closing_calls == 1, "closing hook fired once");

    // World 注册表干净：Instance/临时 Scene 一并回收
    TEST_ASSERT(world.instance_count() == 0 && world.scene_count() == 0,
                "instance and temp scene reclaimed");
    TEST_ASSERT(world.find_instance(launch.instance_id) == nullptr &&
                    world.find_scene(launch.scene_id) == nullptr,
                "registry lookups empty");
    // 绑定与账面随之清除
    TEST_ASSERT(service.find_by_party(1) == nullptr && service.active_count() == 0,
                "binding erased");
    TEST_ASSERT(!service.gameplay(1).has_value() && !service.settlement(1).has_value(),
                "gameplay and settlement ledgers cleared");
    TEST_ASSERT(service.used_capacity(1) == 0, "capacity returned");

    // 失败线同样可收尾（超时 cancel → Finishing）
    const auto second = service.launch(basic_request(2, 200));
    TEST_ASSERT(second.error == LaunchError::Ok, "capacity reusable after dismiss");
    (void)second;
    return true;
}

bool test_squad_dismiss_on_failure_line_and_guards() {
    PartyManager parties;
    World world;
    make_party(parties, 1);
    SquadInstanceService service(parties, &world, 1);
    service.register_world(1, 4);
    auto table = make_anchor_table({kLeader, kMemberB, kMemberC});
    service.set_anchor_resolver([&table](std::uint64_t id) { return table.resolve(id); });

    // 无绑定收尾拒绝
    TEST_ASSERT(!service.dismiss(404), "dismiss without binding refused");

    const auto launch = service.launch(basic_request(1));
    TEST_ASSERT(launch.error == LaunchError::Ok, "launched");

    // Create/Waiting 期不允许收尾
    TEST_ASSERT(!service.dismiss(1), "dismiss refused before settlement");
    auto* instance = run_party_to_running(service, world, /*launch_first=*/false);
    TEST_ASSERT(instance != nullptr, "running");

    // 失败线：超时入结算 → 收尾
    squad_instance::GameplayGoal goal;
    goal.monsters_to_defeat = 3;
    goal.timeout_ticks = 1;
    TEST_ASSERT(service.set_goal(1, goal) == SquadInstanceService::GameplayError::Ok,
                "goal with timeout");
    TEST_ASSERT(service.advance_tick(1, 2) == SquadInstanceService::GameplayError::Ok,
                "timeout crossed");
    TEST_ASSERT(instance->state() == apollo::game::world::Instance::State::Finishing,
                "failure settlement entered");
    TEST_ASSERT(service.dismiss(1), "dismiss completes failure teardown");
    TEST_ASSERT(world.instance_count() == 0 && world.scene_count() == 0,
                "failure path reclaims too");
    return true;
}

bool test_squad_late_input_refused_after_settlement() {
    PartyManager parties;
    World world;
    make_party(parties, 1);
    SquadInstanceService service(parties, &world, 1);
    service.register_world(1, 4);
    auto table = make_anchor_table({kLeader, kMemberB, kMemberC});
    service.set_anchor_resolver([&table](std::uint64_t id) { return table.resolve(id); });

    TEST_ASSERT(service.launch(basic_request(1)).error == LaunchError::Ok, "launched");
    auto* instance = run_party_to_running(service, world, /*launch_first=*/false);
    TEST_ASSERT(instance != nullptr, "running");

    // 入结算（超时失败线）
    squad_instance::GameplayGoal goal;
    goal.monsters_to_defeat = 1;
    goal.timeout_ticks = 1;
    TEST_ASSERT(service.set_goal(1, goal) == SquadInstanceService::GameplayError::Ok,
                "goal set");
    TEST_ASSERT(service.advance_tick(1, 2) == SquadInstanceService::GameplayError::Ok,
                "timeout crossed");
    TEST_ASSERT(instance->state() == apollo::game::world::Instance::State::Finishing,
                "settlement entered");

    // 迟到进入/启动：结算期一律 StateRejected（此前误报 Duplicate）
    TEST_ASSERT(service.enter(1, kMemberB) ==
                    SquadInstanceService::EnterError::StateRejected,
                "late enter refused as StateRejected");
    TEST_ASSERT(service.start_gameplay(1, kLeader) ==
                    SquadInstanceService::EnterError::StateRejected,
                "late start refused");
    // 迟到玩法输入：NotRunning
    TEST_ASSERT(service.spawn_wave(1, 1) == SquadInstanceService::GameplayError::NotRunning,
                "late spawn refused");
    TEST_ASSERT(service.credit_settlement(1, kLeader, 5) ==
                    squad_instance::SettlementLedger::ApplyError::Ok,
                "settlement credit still accepted in settlement phase");

    TEST_ASSERT(service.dismiss(1), "dismissed");
    // 收尾后绑定消失：所有入口 NoBinding
    TEST_ASSERT(service.enter(1, kLeader) == SquadInstanceService::EnterError::NoBinding,
                "enter after dismiss is NoBinding");
    TEST_ASSERT(service.credit_settlement(1, kLeader, 5) ==
                    squad_instance::SettlementLedger::ApplyError::UnknownPlayer,
                "credit after dismiss refused");
    TEST_ASSERT(!service.dismiss(1), "second dismiss refused");
    return true;
}


bool test_squad_nobody_entered_abort_policy() {
    PartyManager parties;
    World world;
    make_party(parties, 1);
    make_party(parties, 2, 200, 201, 202);
    SquadInstanceService service(parties, &world, 1);
    service.register_world(1, 4);

    int closing_calls = 0;
    service.set_instance_closing([&](std::uint64_t) { ++closing_calls; });

    // 无人进入（Create 态悬挂）：策略收尾 = abort，全量回收
    TEST_ASSERT(service.launch(basic_request(1)).error == LaunchError::Ok, "launched");
    TEST_ASSERT(service.abort(1), "abort reclaims never-entered instance");
    TEST_ASSERT(closing_calls == 1, "closing hook fired");
    TEST_ASSERT(world.instance_count() == 0 && world.scene_count() == 0,
                "nothing left behind");
    TEST_ASSERT(service.used_capacity(1) == 0, "capacity returned");
    TEST_ASSERT(service.launch(basic_request(1)).error == LaunchError::Ok,
                "same party can relaunch");

    // 部分进入（Waiting 悬挂）同样可 abort，玩家被移出
    TEST_ASSERT(service.enter(1, kLeader) == SquadInstanceService::EnterError::Ok,
                "leader enters");
    auto* instance = world.find_instance(service.find_by_party(1)->instance_id);
    TEST_ASSERT(instance->state() == apollo::game::world::Instance::State::Waiting,
                "Waiting after partial entry");
    TEST_ASSERT(service.abort(1), "abort from Waiting");
    TEST_ASSERT(world.instance_count() == 0 && world.scene_count() == 0,
                "Waiting abort reclaims");
    TEST_ASSERT(service.enter(1, kLeader) == SquadInstanceService::EnterError::NoBinding,
                "enter after abort is NoBinding");

    // 玩法期 abort：取消线（不发成功奖励），结算面为空
    auto table = make_anchor_table({kLeader, kMemberB, kMemberC});
    service.set_anchor_resolver([&table](std::uint64_t id) { return table.resolve(id); });
    TEST_ASSERT(service.launch(basic_request(1)).error == LaunchError::Ok, "relaunched");
    instance = run_party_to_running(service, world, /*launch_first=*/false);
    TEST_ASSERT(instance != nullptr, "running");
    squad_instance::GameplayGoal goal;
    goal.monsters_to_defeat = 5;
    TEST_ASSERT(service.set_goal(1, goal) == SquadInstanceService::GameplayError::Ok,
                "goal set");
    TEST_ASSERT(service.abort(1), "abort mid-gameplay");
    TEST_ASSERT(!service.find_by_party(1) && service.gameplay(1) == std::nullopt,
                "binding and gameplay cleared");
    TEST_ASSERT(!service.settlement(1).has_value(),
                "settlement cleared with binding");
    TEST_ASSERT(exp_of(table, kLeader) == 0, "abort settles no rewards");
    TEST_ASSERT(service.launch(basic_request(2, 200)).error == LaunchError::Ok,
                "capacity freed for other squad");
    TEST_ASSERT(service.abort(2), "fresh instance abortable");
    // 无绑定 abort 与重复 abort 拒绝（收尾即释放绑定）
    TEST_ASSERT(!service.abort(404), "abort without binding refused");
    TEST_ASSERT(!service.abort(2), "second abort refused");
    return true;
}

bool test_squad_all_exit_drives_failure_line() {
    PartyManager parties;
    World world;
    make_party(parties, 1);
    SquadInstanceService service(parties, &world, 1);
    service.register_world(1, 4);
    auto table = make_anchor_table({kLeader, kMemberB, kMemberC});
    service.set_anchor_resolver([&table](std::uint64_t id) { return table.resolve(id); });

    TEST_ASSERT(service.launch(basic_request(1)).error == LaunchError::Ok, "launched");
    auto* instance = run_party_to_running(service, world, /*launch_first=*/false);
    TEST_ASSERT(instance != nullptr, "running");
    squad_instance::GameplayGoal goal;
    goal.monsters_to_defeat = 5;  // 不设超时，专测退出线
    TEST_ASSERT(service.set_goal(1, goal) == SquadInstanceService::GameplayError::Ok,
                "goal without timeout");

    // 非名册退出拒绝
    TEST_ASSERT(service.leave(1, kOutsider) ==
                    SquadInstanceService::LeaveError::NotRoster,
                "outsider leave refused");
    TEST_ASSERT(service.leave(404, kLeader) ==
                    SquadInstanceService::LeaveError::NoBinding,
                "unknown party leave refused");

    // 中途退出可再进（Running 准入窗口）
    TEST_ASSERT(service.leave(1, kMemberB) == SquadInstanceService::LeaveError::Ok,
                "member B leaves");
    TEST_ASSERT(instance->state() == apollo::game::world::Instance::State::Running,
                "instance survives partial exit");
    TEST_ASSERT(service.enter(1, kMemberB) == SquadInstanceService::EnterError::Ok,
                "member B re-enters");

    // 全员退出 → 失败判定（无人可续）：进入结算、不发奖励
    TEST_ASSERT(service.leave(1, kMemberB) == SquadInstanceService::LeaveError::Ok,
                "member B leaves again");
    TEST_ASSERT(service.leave(1, kMemberC) == SquadInstanceService::LeaveError::Ok,
                "member C leaves");
    TEST_ASSERT(instance->state() == apollo::game::world::Instance::State::Running,
                "leader alone keeps it running");
    TEST_ASSERT(service.leave(1, kLeader) == SquadInstanceService::LeaveError::Ok,
                "last member leaves");
    const auto progress = service.gameplay(1);
    TEST_ASSERT(progress.has_value() && progress->all_exited, "all-exit flagged");
    TEST_ASSERT(instance->state() == apollo::game::world::Instance::State::Finishing,
                "all-exit drives settlement entry");
    TEST_ASSERT(service.settlement(1)->applied == 0, "no rewards on all-exit");
    TEST_ASSERT(service.dismiss(1), "dismiss completes all-exit teardown");
    TEST_ASSERT(world.instance_count() == 0 && world.scene_count() == 0,
                "reclaimed after all-exit");
    return true;
}

bool test_squad_party_disband_aborts_instance() {
    PartyManager parties;
    World world;
    make_party(parties, 1);
    SquadInstanceService service(parties, &world, 1);
    service.register_world(1, 4);
    auto table = make_anchor_table({kLeader, kMemberB, kMemberC});
    service.set_anchor_resolver([&table](std::uint64_t id) { return table.resolve(id); });

    // 无绑定时解散：服务面无需动作
    TEST_ASSERT(!service.abort(1), "abort without instance refused");

    TEST_ASSERT(service.launch(basic_request(1)).error == LaunchError::Ok, "launched");
    auto* instance = run_party_to_running(service, world, /*launch_first=*/false);
    TEST_ASSERT(instance != nullptr, "running");
    squad_instance::GameplayGoal goal;
    goal.monsters_to_defeat = 5;
    TEST_ASSERT(service.set_goal(1, goal) == SquadInstanceService::GameplayError::Ok,
                "goal set");

    // 队伍解散 → abort 收尾：副本销毁、成员解绑（MemberBusy 清除）
    TEST_ASSERT(parties.disband(1, kLeader), "party disbanded");
    TEST_ASSERT(service.abort(1), "instance aborted on disband");
    TEST_ASSERT(world.instance_count() == 0 && world.scene_count() == 0,
                "disband teardown reclaims");
    TEST_ASSERT(service.active_count() == 0, "binding erased");
    // 成员可再组队开新副本（忙位已清）
    auto* party2 = parties.create(2, kLeader);
    party2->add_member(kMemberB);
    party2->add_member(kMemberC);
    TEST_ASSERT(service.launch(basic_request(2)).error == LaunchError::Ok,
                "same members admitted in new party");
    return true;
}

bool test_squad_process_recovery_policy() {
    PartyManager parties;
    World world;
    make_party(parties, 1);
    SquadInstanceService service(parties, &world, 1);
    service.register_world(1, 4);
    auto table = make_anchor_table({kLeader, kMemberB, kMemberC});
    service.set_anchor_resolver([&table](std::uint64_t id) { return table.resolve(id); });

    // 进程一：副本开跑并落账一条
    TEST_ASSERT(service.launch(basic_request(1)).error == LaunchError::Ok, "launched");
    TEST_ASSERT(service.credit_settlement(1, kLeader, 40) ==
                    squad_instance::SettlementLedger::ApplyError::Ok,
                "credited in first process");
    TEST_ASSERT(exp_of(table, kLeader) == 40, "exp persisted via anchor");
    TEST_ASSERT(service.abort(1), "process one torn down");
    TEST_ASSERT(service.active_count() == 0, "process one settled down");

    // 进程二（重启 = 全新服务/World，副本临时态随进程消亡）：从零受理
    SquadInstanceService restarted(parties, &world, 1);
    restarted.register_world(1, 4);
    restarted.set_anchor_resolver([&table](std::uint64_t id) { return table.resolve(id); });
    TEST_ASSERT(restarted.active_count() == 0, "no phantom bindings after restart");
    TEST_ASSERT(restarted.launch(basic_request(1)).error == LaunchError::Ok,
                "fresh admission after restart");

    // 恢复面：按持久化痕迹回填已落标记，重放不双记
    restarted.restore_settlement(1, {kLeader});
    TEST_ASSERT(restarted.credit_settlement(1, kLeader, 40) ==
                    squad_instance::SettlementLedger::ApplyError::AlreadyApplied,
                "replay refused after restore");
    TEST_ASSERT(exp_of(table, kLeader) == 40, "no double exp across restart");
    return true;
}


bool test_squad_hooks_close_on_release() {
    PartyManager parties;
    World world;
    make_party(parties, 1);
    SquadInstanceService service(parties, &world, 1);
    service.register_world(1, 4);

    int closing_calls = 0;
    std::uint64_t closing_instance = 0;
    service.set_instance_closing([&](std::uint64_t instance_id) {
        ++closing_calls;
        closing_instance = instance_id;
    });

    const auto launch = service.launch(basic_request(1));
    TEST_ASSERT(launch.error == LaunchError::Ok, "launched");
    for (const auto member : std::vector<std::uint64_t>{kLeader, kMemberB, kMemberC}) {
        TEST_ASSERT(service.enter(1, member) == SquadInstanceService::EnterError::Ok,
                    "roster enters");
    }
    TEST_ASSERT(service.start_gameplay(1, kLeader) == SquadInstanceService::EnterError::Ok,
                "gameplay started");
    TEST_ASSERT(closing_calls == 0, "no closing while running");

    auto* instance = world.find_instance(launch.instance_id);
    // 玩家退出后才允许销毁（P2 回收约束）
    for (const auto member : std::vector<std::uint64_t>{kLeader, kMemberB, kMemberC}) {
        TEST_ASSERT(instance->leave(apollo::game::core::PlayerId{member}), "member leaves");
    }
    TEST_ASSERT(instance->cancel() && instance->settle() && instance->drain() &&
                    instance->destroy(),
                "reaches terminal state");
    TEST_ASSERT(service.release(launch.instance_id), "released");
    TEST_ASSERT(closing_calls == 1 && closing_instance == launch.instance_id,
                "closing hook fired once before reclaim");
    return true;
}

// 单 World 多副本玩法期隔离（P2-8）：两副本并发 Running——怪号全流水不撞号、
// 击杀进度/玩法时钟互不串场、结算收尾单边释放不惊动邻副本、账面随副本隔离。
bool test_squad_concurrent_instances_gameplay_isolation() {
    PartyManager parties;
    World world;
    make_party(parties, 1);
    make_party(parties, 2, 200, 201, 202);
    SquadInstanceService service(parties, &world, 1);
    service.register_world(1, 4);

    const auto a = service.launch(basic_request(1));
    const auto b = service.launch(basic_request(2, 200));
    TEST_ASSERT(a.error == LaunchError::Ok && b.error == LaunchError::Ok, "both launched");
    TEST_ASSERT(a.world_id == 1 && b.world_id == 1 && a.instance_id != b.instance_id,
                "two instances on one world");
    TEST_ASSERT(a.scene_id != b.scene_id, "distinct temp scenes");

    // 双副本并进到 Running（进入序列各按各队名册）
    const auto run_to_running = [&](std::uint64_t party_id, std::uint64_t leader,
                                    std::uint64_t m1, std::uint64_t m2) {
        for (const auto member : std::vector<std::uint64_t>{leader, m1, m2}) {
            if (service.enter(party_id, member) != SquadInstanceService::EnterError::Ok)
                return false;
        }
        return service.start_gameplay(party_id, leader) ==
               SquadInstanceService::EnterError::Ok;
    };
    TEST_ASSERT(run_to_running(1, kLeader, kMemberB, kMemberC), "party 1 Running");
    TEST_ASSERT(run_to_running(2, 200, 201, 202), "party 2 Running");

    squad_instance::GameplayGoal goal;
    goal.monsters_to_defeat = 2;
    TEST_ASSERT(service.set_goal(1, goal) == SquadInstanceService::GameplayError::Ok,
                "goal A set");
    TEST_ASSERT(service.set_goal(2, goal) == SquadInstanceService::GameplayError::Ok,
                "goal B set");
    TEST_ASSERT(service.spawn_wave(1, 2) == SquadInstanceService::GameplayError::Ok,
                "wave A");
    TEST_ASSERT(service.spawn_wave(2, 2) == SquadInstanceService::GameplayError::Ok,
                "wave B");

    // 怪号全局发号：两副本存活怪集合不相交
    const auto progress_a = service.gameplay(1);
    const auto progress_b = service.gameplay(2);
    TEST_ASSERT(progress_a && progress_b && progress_a->monsters_alive.size() == 2 &&
                    progress_b->monsters_alive.size() == 2,
                "both waves live");
    const std::set<std::uint64_t> ids_a(progress_a->monsters_alive.begin(),
                                        progress_a->monsters_alive.end());
    const std::set<std::uint64_t> ids_b(progress_b->monsters_alive.begin(),
                                        progress_b->monsters_alive.end());
    TEST_ASSERT(ids_a.size() == 2 && ids_b.size() == 2, "no duplicate ids within a wave");
    for (const auto id : ids_a) {
        TEST_ASSERT(ids_b.count(id) == 0, "monster ids disjoint across instances");
    }

    // 交错上报：A 一杀、B 两杀——进度只进各自账
    const auto first_b = *ids_b.begin();
    const auto second_b = *std::next(ids_b.begin());
    TEST_ASSERT(service.report_monster_defeated(1, *ids_a.begin(), kLeader) ==
                    SquadInstanceService::GameplayError::Ok,
                "kill in A");
    TEST_ASSERT(service.report_monster_defeated(2, first_b, 200) ==
                    SquadInstanceService::GameplayError::Ok,
                "kill 1 in B");
    TEST_ASSERT(service.report_monster_defeated(2, second_b, 200) ==
                    SquadInstanceService::GameplayError::Ok,
                "kill 2 in B");
    auto now_a = service.gameplay(1);
    auto now_b = service.gameplay(2);
    TEST_ASSERT(now_a->monsters_defeated == 1 && now_a->monsters_alive.size() == 1,
                "A progress isolated");
    TEST_ASSERT(now_b->monsters_defeated == 2 && now_b->monsters_alive.empty(),
                "B progress isolated");

    // B 达标过拍进结算并收尾；A 全程无感
    TEST_ASSERT(service.advance_tick(2, 1) == SquadInstanceService::GameplayError::Ok,
                "B tick settles");
    const auto* binding_b = service.find_by_party(2);
    TEST_ASSERT(binding_b && world.find_instance(binding_b->instance_id)->state() ==
                                 apollo::game::world::Instance::State::Finishing,
                "B entering settlement");
    TEST_ASSERT(service.dismiss(2), "B dismissed");
    TEST_ASSERT(service.find_by_party(2) == nullptr, "B binding gone");
    TEST_ASSERT(!service.settlement(2).has_value(), "B ledger cleared");

    TEST_ASSERT(service.find_by_party(1) != nullptr, "A binding survives B teardown");
    now_a = service.gameplay(1);
    TEST_ASSERT(now_a && now_a->goal_set && now_a->monsters_defeated == 1 &&
                    now_a->monsters_alive.size() == 1,
                "A gameplay untouched");
    auto* scene_a = world.find_scene(service.find_by_party(1)->scene_id);
    TEST_ASSERT(scene_a != nullptr && scene_a->get_entity_count() == 1,
                "A monster still live in its scene");

    // A 收官：最后一杀过拍结算收尾，容量与绑定全数归还
    TEST_ASSERT(service.report_monster_defeated(1, *now_a->monsters_alive.begin(),
                                                kLeader) ==
                    SquadInstanceService::GameplayError::Ok,
                "A final kill");
    TEST_ASSERT(service.advance_tick(1, 1) == SquadInstanceService::GameplayError::Ok,
                "A tick settles");
    TEST_ASSERT(service.dismiss(1), "A dismissed");
    TEST_ASSERT(service.active_count() == 0 && service.used_capacity(1) == 0,
                "both reclaimed, capacity returned");
    return true;
}

// 多 World 分配选路（P2-8）：本地满载溢出远端时按 world_id 升序取首个空位
//（注册顺序无关），各 World 容量独立记账，回收后空位可复用；全满拒绝。
bool test_squad_multi_world_allocation_order() {
    PartyManager parties;
    World world;
    make_party(parties, 1);
    make_party(parties, 2, 200, 201, 202);
    make_party(parties, 3, 300, 301, 302);
    make_party(parties, 4, 400, 401, 402);
    SquadInstanceService service(parties, &world, 1);
    service.register_world(1, 1);
    service.register_world(3, 1);  // 后注册高号位
    service.register_world(2, 1);  // 后注册低号位——升序裁决与注册顺序无关

    std::map<std::uint32_t, LaunchRequest> remote_calls;
    service.set_remote_spawner(
        [&remote_calls](std::uint32_t world_id, const LaunchRequest& request) {
            remote_calls[world_id] = request;
            return std::make_pair(9000 + world_id, 8000 + world_id);
        });

    const auto first = service.launch(basic_request(1));
    TEST_ASSERT(first.error == LaunchError::Ok && first.world_id == 1,
                "local capacity taken first");
    const auto second = service.launch(basic_request(2, 200));
    TEST_ASSERT(second.error == LaunchError::Ok && second.world_id == 2,
                "overflow picks lowest remote id");
    const auto third = service.launch(basic_request(3, 300));
    TEST_ASSERT(third.error == LaunchError::Ok && third.world_id == 3,
                "next overflow takes next world");
    TEST_ASSERT(remote_calls.size() == 2 && remote_calls.count(2) && remote_calls.count(3),
                "remote spawner invoked once per remote pick");
    TEST_ASSERT(second.instance_id == 9002 && third.scene_id == 8003,
                "remote ids propagated per world");
    TEST_ASSERT(service.used_capacity(1) == 1 && service.used_capacity(2) == 1 &&
                    service.used_capacity(3) == 1,
                "capacity accounted per world");

    const auto exhausted = service.launch(basic_request(4, 400));
    TEST_ASSERT(exhausted.error == LaunchError::NoWorldCapacity, "all full refused");

    // 回收线归还本地空位后复用：本地优先裁决不漂移（远端绑定回收面随
    // 跨进程条目接通，abort/dismiss 只服务本地 World）
    TEST_ASSERT(service.abort(1), "party 1 aborted");
    TEST_ASSERT(service.used_capacity(1) == 0, "local slot returned");
    const auto re_picked = service.launch(basic_request(4, 400));
    TEST_ASSERT(re_picked.error == LaunchError::Ok && re_picked.world_id == 1,
                "local slot reused ahead of remotes");
    return true;
}


} // namespace

int main() {
    const std::vector<Case> cases = {
        {"squad_launch_happy_path", test_squad_launch_happy_path},
        {"squad_launch_rejections", test_squad_launch_rejections},
        {"squad_launch_busy_member", test_squad_launch_busy_member},
        {"squad_launch_capacity_and_recycle", test_squad_launch_capacity_and_recycle},
        {"squad_launch_selects_world_with_capacity", test_squad_launch_selects_world_with_capacity},
        {"squad_launch_temp_scene_fresh_per_instance", test_squad_launch_temp_scene_fresh_per_instance},
        {"squad_enter_and_start_gameplay", test_squad_enter_and_start_gameplay},
        {"squad_instance_isolation_between_instances", test_squad_instance_isolation_between_instances},
        {"squad_gameplay_goal_completion_drives_settlement",
         test_squad_gameplay_goal_completion_drives_settlement},
        {"squad_gameplay_timeout_drives_failure_settlement",
         test_squad_gameplay_timeout_drives_failure_settlement},
        {"squad_gameplay_guards_and_quotas", test_squad_gameplay_guards_and_quotas},
        {"squad_settlement_idempotent_credit_and_retry",
         test_squad_settlement_idempotent_credit_and_retry},
        {"squad_settlement_battle_settles_on_goal_completion",
         test_squad_settlement_battle_settles_on_goal_completion},
        {"squad_kill_scores_flow_through_battle", test_squad_kill_scores_flow_through_battle},
        {"squad_settlement_recovery_and_failure_line",
         test_squad_settlement_recovery_and_failure_line},
        {"squad_dismiss_evicts_players_and_reclaims",
         test_squad_dismiss_evicts_players_and_reclaims},
        {"squad_dismiss_on_failure_line_and_guards",
         test_squad_dismiss_on_failure_line_and_guards},
        {"squad_late_input_refused_after_settlement",
         test_squad_late_input_refused_after_settlement},
        {"squad_nobody_entered_abort_policy", test_squad_nobody_entered_abort_policy},
        {"squad_all_exit_drives_failure_line", test_squad_all_exit_drives_failure_line},
        {"squad_party_disband_aborts_instance", test_squad_party_disband_aborts_instance},
        {"squad_process_recovery_policy", test_squad_process_recovery_policy},
        {"squad_hooks_close_on_release", test_squad_hooks_close_on_release},
        {"squad_concurrent_instances_gameplay_isolation",
         test_squad_concurrent_instances_gameplay_isolation},
        {"squad_multi_world_allocation_order", test_squad_multi_world_allocation_order},
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
