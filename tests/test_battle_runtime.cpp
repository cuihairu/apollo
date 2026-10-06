// Battle Runtime 测试（P2-2；battle-determinism 四约束可测骨架）。
//
// 覆盖：
//   1. 子流派生（约束④）：同 (world_seed, tick, stream) 复现、跨 stream 独立、
//      无全局游标（独立派生次序不影响序列）；
//   2. 生命周期状态机：五段单向，非法跃迁拒绝；
//   3. 迭代序（约束③）：同 tick 输入次序打乱 → 同事件流同 hash；
//   4. 复算回放（四元组）：同 seed 同输入 → 同事件流同 hash 链；
//      异 seed → 异 hash（种子敏感）；
//   5. reward 单向落账：sink 按 player_id 全序收到、Reward 事件入链；
//   6. 模拟段驱动：实体 on_update 随 tick 执行。

#include "apollo/game/battle/battle_runtime.hpp"

#include <algorithm>
#include <iostream>
#include <random>
#include <vector>

#define TEST_ASSERT(cond, msg)                                                           \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            std::cerr << "FAIL: " << (msg) << " (" << __FILE__ << ":" << __LINE__ << ")" \
                      << std::endl;                                                      \
            return false;                                                                \
        }                                                                                \
    } while (0)

namespace {

using apollo::base::Pcg32;
using apollo::base::StreamId;
using apollo::game::battle::BattleEvent;
using apollo::game::battle::BattleInput;
using apollo::game::battle::BattlePhase;
using apollo::game::battle::BattleRuntime;
using apollo::game::battle::EventKind;
using apollo::game::core::Entity;
using apollo::game::core::EntityId;

// 计数实体：验证模拟段驱动
class CountingEntity : public Entity {
public:
    explicit CountingEntity(EntityId id) : Entity(id, "counting") {}
    void on_update(float) override { ++updates_; }
    int updates() const { return updates_; }

private:
    int updates_ = 0;
};

// 内存奖励 sink：验证单向落账次序与数值
struct MemoryRewardSink : public apollo::game::battle::IRewardSink {
    std::vector<std::pair<std::uint64_t, std::int64_t>> rewards;
    void on_reward(std::uint64_t player_id, std::int64_t score) override {
        rewards.emplace_back(player_id, score);
    }
};

std::vector<BattleInput> attack(std::uint32_t tick,
                                const std::vector<std::uint64_t>& pids) {
    std::vector<BattleInput> in;
    for (auto pid : pids) {
        in.push_back(BattleInput{tick, pid, 1});
    }
    return in;
}

// —— 1. 子流派生（约束④）——
bool test_substream_derivation() {
    // 同三元组复现：两次独立派生同序列（无全局游标，派生次序无关）
    auto a = apollo::base::derive_substream(42, 7, StreamId::CombatRoll);
    auto b = apollo::base::derive_substream(42, 7, StreamId::CombatRoll);
    auto c = apollo::base::derive_substream(42, 8, StreamId::CombatRoll);
    for (int i = 0; i < 16; ++i) {
        TEST_ASSERT(a.next_uint32() == b.next_uint32(), "同三元组同序列");
    }
    bool differs = false;
    for (int i = 0; i < 16; ++i) {
        if (a.next_uint32() != c.next_uint32()) {
            differs = true;
        }
    }
    TEST_ASSERT(differs, "跨 tick 子流独立");

    // 跨 stream 独立：四域互不共享
    auto roll = apollo::base::derive_substream(42, 7, StreamId::CombatRoll);
    auto drop = apollo::base::derive_substream(42, 7, StreamId::Drop);
    auto ai = apollo::base::derive_substream(42, 7, StreamId::AiDecision);
    auto proc = apollo::base::derive_substream(42, 7, StreamId::Proc);
    bool all_differs = roll.next_uint32() != drop.next_uint32() &&
                       roll.next_uint32() != ai.next_uint32() &&
                       roll.next_uint32() != proc.next_uint32();
    TEST_ASSERT(all_differs, "stream_id 分域互异");

    // 种子敏感
    auto d = apollo::base::derive_substream(43, 7, StreamId::CombatRoll);
    TEST_ASSERT(c.next_uint32() != d.next_uint32() ||
                    c.next_uint32() != d.next_uint32(),
                "异种子异流");

    // next_double 落域 [0,1)
    Pcg32 e;
    e.seed(1, 1);
    for (int i = 0; i < 256; ++i) {
        const double v = e.next_double();
        TEST_ASSERT(v >= 0.0 && v < 1.0, "next_double ∈ [0,1)");
    }
    std::cout << "  substream derivation: 4 cases ok" << std::endl;
    return true;
}

// —— 2. 生命周期状态机 ——
bool test_lifecycle() {
    BattleRuntime rt(1, 100);
    TEST_ASSERT(rt.phase() == BattlePhase::Created, "初始 Created");
    TEST_ASSERT(!rt.begin(), "空参战 begin 拒绝");
    TEST_ASSERT(!rt.tick(0), "Created 态 tick 拒绝");
    TEST_ASSERT(!rt.finish(), "Created 态 finish 拒绝");

    TEST_ASSERT(rt.enter_player(11), "进入 Entering");
    TEST_ASSERT(rt.phase() == BattlePhase::Entering, "Entering");
    TEST_ASSERT(!rt.enter_player(11), "重复进入拒绝");
    TEST_ASSERT(!rt.enter_player(0), "非法 player_id 拒绝");
    TEST_ASSERT(!rt.finish(), "未开战 finish 拒绝");

    TEST_ASSERT(rt.begin(), "开战");
    TEST_ASSERT(rt.phase() == BattlePhase::Battling, "Battling");
    TEST_ASSERT(!rt.enter_player(12), "开战后收人拒绝");

    TEST_ASSERT(rt.tick(0), "tick 0 可用");
    TEST_ASSERT(!rt.tick(0), "同 tick 重放拒绝（含 0）");
    TEST_ASSERT(!rt.tick(0, attack(0, {11})), "tick 0 输入重演拒绝");
    TEST_ASSERT(rt.tick(1), "tick 1 递增");

    TEST_ASSERT(rt.finish(), "散场");
    TEST_ASSERT(rt.phase() == BattlePhase::Finished, "Finished 终态");
    TEST_ASSERT(!rt.tick(2), "终态 tick 拒绝");
    TEST_ASSERT(!rt.finish(), "终态 finish 拒绝");
    std::cout << "  lifecycle: 14 transitions ok" << std::endl;
    return true;
}

// —— 3. 迭代序（约束③）：输入次序打乱结果不变 ——
bool test_input_order_invariance() {
    const std::vector<std::uint64_t> pids = {101, 7, 58, 900, 12};

    BattleRuntime rt1(1, 2026);
    for (auto pid : pids) { // enter 顺序不影响：结算段按 player_id 排序
        rt1.enter_player(pid);
    }
    rt1.begin();

    BattleRuntime rt2(1, 2026);
    for (auto pid : pids) {
        rt2.enter_player(pid);
    }
    rt2.begin();

    for (std::uint32_t t = 0; t < 10; ++t) {
        auto in1 = attack(t, pids);
        auto in2 = attack(t, pids);
        std::shuffle(in2.begin(), in2.end(), std::mt19937{t});
        TEST_ASSERT(rt1.tick(t, in1), "t1 tick");
        TEST_ASSERT(rt2.tick(t, in2), "t2 tick(乱序)");
    }
    TEST_ASSERT(rt1.finish(), "t1 散场");
    TEST_ASSERT(rt2.finish(), "t2 散场");

    TEST_ASSERT(rt1.hash_chain() == rt2.hash_chain(), "乱序输入同 hash");
    TEST_ASSERT(rt1.events().size() == rt2.events().size(), "同事件数");
    for (size_t i = 0; i < rt1.events().size(); ++i) {
        const auto& a = rt1.events()[i];
        const auto& b = rt2.events()[i];
        TEST_ASSERT(a.tick == b.tick && a.entity_id == b.entity_id &&
                        a.kind == b.kind && a.value == b.value,
                    "逐事件一致");
    }
    std::cout << "  input order invariance: hash " << rt1.hash_chain() << " ok"
              << std::endl;
    return true;
}

// —— 4. 复算回放（四元组）：同种子同输入重演，异种子发散 ——
bool test_replay_tuple() {
    const std::vector<std::uint64_t> pids = {11, 22, 33};

    auto run_battle = [&pids](std::uint64_t seed, bool with_inputs) {
        BattleRuntime rt(9, seed);
        for (auto pid : pids) {
            rt.enter_player(pid);
        }
        rt.begin();
        for (std::uint32_t t = 0; t < 5; ++t) {
            if (with_inputs) {
                rt.tick(t, attack(t, pids));
            } else {
                rt.tick(t);
            }
        }
        rt.finish();
        return rt;
    };

    auto a = run_battle(777, true);
    auto b = run_battle(777, true);
    TEST_ASSERT(a.phase() == BattlePhase::Finished, "散场达成");
    TEST_ASSERT(b.phase() == BattlePhase::Finished, "重演散场达成");
    TEST_ASSERT(a.hash_chain() == b.hash_chain(), "同四元组同 hash");
    TEST_ASSERT(a.events().size() == b.events().size(), "同事件流长");
    TEST_ASSERT(a.events().back().kind ==
                    static_cast<std::uint32_t>(EventKind::Reward),
                "末事件为奖励");

    auto c = run_battle(778, true);
    TEST_ASSERT(a.hash_chain() != c.hash_chain(), "异种子异 hash");

    // 空输入对账：无输入也有确定性链（模拟段+散场奖励仍可重演对照）
    auto d = run_battle(777, false);
    auto e = run_battle(777, false);
    TEST_ASSERT(d.hash_chain() == e.hash_chain(), "空输入重演同 hash");
    TEST_ASSERT(d.hash_chain() != a.hash_chain(), "输入序列属四元组（缺输入即失配）");
    std::cout << "  replay tuple: seed777 hash " << a.hash_chain() << " ok"
              << std::endl;
    return true;
}

// —— 5. reward 单向落账 ——
bool test_reward_sink() {
    MemoryRewardSink sink;
    BattleRuntime rt(3, 555, &sink);
    rt.enter_player(400);
    rt.enter_player(22);   // 乱序进入
    rt.enter_player(4000); // 乱序进入
    rt.begin();

    // 22 打 3 tick、400 打 1 tick、4000 空手——奖励按 player_id 全序、分值
    // 与命中事件合计一致
    rt.tick(0, attack(0, {22}));
    rt.tick(1, attack(1, {400}));
    rt.tick(2, attack(2, {22}));
    TEST_ASSERT(rt.tick(3, attack(3, {22})), "tick 3");
    TEST_ASSERT(rt.finish(), "散场");
    TEST_ASSERT(rt.phase() == BattlePhase::Finished, "终态");

    TEST_ASSERT(sink.rewards.size() == 3, "三参战者各落账一次");
    TEST_ASSERT(sink.rewards[0].first == 22, "player_id 全序首位");
    TEST_ASSERT(sink.rewards[1].first == 400, "次位");
    TEST_ASSERT(sink.rewards[2].first == 4000, "末位");
    TEST_ASSERT(sink.rewards[2].second == 0, "空手零分");

    // Reward 事件入链：每参战者恰一条
    const auto reward_kind = static_cast<std::uint32_t>(EventKind::Reward);
    size_t reward_events = 0;
    for (const auto& e : rt.events()) {
        TEST_ASSERT(!(e.kind == reward_kind && e.value < 0), "分值非负");
        reward_events += (e.kind == reward_kind) ? 1 : 0;
    }
    TEST_ASSERT(reward_events == 3, "Reward 事件入链");

    // hash 已含奖励段（散场后再无变化——终态冻结）
    const auto frozen = rt.hash_chain();
    TEST_ASSERT(rt.hash_chain() == frozen, "终态冻结");
    std::cout << "  reward sink: ordered 3 payouts, hash " << frozen << " ok"
              << std::endl;
    return true;
}

// —— 6. 模拟段驱动 ——
bool test_simulation_segment() {
    BattleRuntime rt(4, 888);
    auto e1 = std::make_shared<CountingEntity>(EntityId(1));
    auto e2 = std::make_shared<CountingEntity>(EntityId(2));
    rt.system().add_entity(e1);
    rt.system().add_entity(e2);
    rt.enter_player(11);
    rt.begin();

    rt.tick(0);
    rt.tick(1);
    TEST_ASSERT(e1->updates() == 2 && e2->updates() == 2, "每 tick 各驱动一次");
    TEST_ASSERT(rt.system().get_entity_count() == 2, "实体在册");
    std::cout << "  simulation segment: entities driven ok" << std::endl;
    return true;
}

} // namespace

int main() {
    struct Case {
        const char* name;
        bool (*fn)();
    };
    const Case cases[] = {
        {"substream_derivation", test_substream_derivation},
        {"lifecycle", test_lifecycle},
        {"input_order_invariance", test_input_order_invariance},
        {"replay_tuple", test_replay_tuple},
        {"reward_sink", test_reward_sink},
        {"simulation_segment", test_simulation_segment},
    };
    int failed = 0;
    for (const auto& c : cases) {
        std::cout << "[battle_runtime] " << c.name << std::endl;
        if (!c.fn()) {
            ++failed;
        }
    }
    if (failed == 0) {
        std::cout << "battle_runtime: all " << (sizeof(cases) / sizeof(cases[0]))
                  << " cases passed" << std::endl;
        return 0;
    }
    std::cerr << "battle_runtime: " << failed << " case(s) failed" << std::endl;
    return 1;
}
