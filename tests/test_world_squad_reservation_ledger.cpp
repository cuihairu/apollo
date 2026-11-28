// 小队容量预留账本单测（D11 issuer=Base 批②验收，04 §4.3 分批表）：
// 预留记账四路径——无容量(1) / 排空(3) / 过期回收(sweep+validate 窗已过) /
// 消费释放(consume 逐槽销账)，外加实例未知(2)与 hold 抵扣超卖拒绝。
// 直编 squad_reservation_ledger.cpp（自包含纯组件，无 net 依赖，两态可编译）。
#include "world_app/squad_reservation_ledger.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

using world_app::SquadCapacityGrant;
using world_app::SquadCapacityRequest;
using world_app::SquadCapacityView;
using world_app::SquadReservationLedger;

int g_failures = 0;

#define TEST_ASSERT(cond, msg)                                                     \
    do {                                                                           \
        if (!(cond)) {                                                             \
            std::cout << "FAIL: " << (msg) << " (" << #cond << ") at "             \
                      << __LINE__ << std::endl;                                    \
            ++g_failures;                                                          \
            return false;                                                          \
        }                                                                          \
    } while (0)

// 可编程容量视图：测试事实源（instance 已知/排空/余量逐例注入）。
struct FakeCapacityView final : SquadCapacityView {
    std::vector<std::uint64_t> known;
    std::vector<std::uint64_t> draining;
    std::vector<std::pair<std::uint64_t, std::int32_t>> free;  // cell → 余量

    bool instance_known(std::uint64_t instance_id) const override {
        for (const auto id : known) {
            if (id == instance_id) {
                return true;
            }
        }
        return false;
    }
    bool instance_draining(std::uint64_t instance_id) const override {
        for (const auto id : draining) {
            if (id == instance_id) {
                return true;
            }
        }
        return false;
    }
    std::int32_t free_slots(std::uint64_t cell_id) const override {
        for (const auto& pair : free) {
            if (pair.first == cell_id) {
                return pair.second;
            }
        }
        return 0;
    }
};

SquadCapacityRequest make_request(std::uint64_t request_id, std::uint64_t party_id,
                                  std::uint64_t instance_id, std::uint64_t cell_id,
                                  std::int32_t slots) {
    SquadCapacityRequest req;
    req.request_id = request_id;
    req.party_id = party_id;
    req.instance_id = instance_id;
    req.cell_id = cell_id;
    req.roster_hash = "hash-" + std::to_string(party_id);
    req.slots = slots;
    return req;
}

// 正路径 + hold 抵扣：视图余量不因预留变，有效余量 = 原值 − hold。
bool test_grant_and_hold_deduction() {
    FakeCapacityView view;
    view.known = {42};
    view.free = {{7, 5}};
    SquadReservationLedger ledger(view, 1000);  // 窗 1000ms

    const auto g1 = ledger.request(make_request(1, 100, 42, 7, 3), 100);
    TEST_ASSERT(g1.granted && g1.code == 0, "余量充足应 grant");
    TEST_ASSERT(g1.reservation_id == "sq-1", "账本单调铸造 id");
    TEST_ASSERT(g1.expires_at == 1100, "expires = now + 窗");
    TEST_ASSERT(ledger.held_slots(7) == 3, "hold 记账");
    TEST_ASSERT(ledger.live_count() == 1, "live 条数");

    world_app::SquadReservationLedger::Reservation snap;
    TEST_ASSERT(ledger.find("sq-1", snap), "快照可读");
    TEST_ASSERT(snap.party_id == 100 && snap.instance_id == 42 && snap.cell_id == 7,
                "快照字段回读");
    TEST_ASSERT(snap.roster_hash == "hash-100", "roster_hash 透传");
    TEST_ASSERT(snap.slots == 3 && snap.held == 3, "原始槽数与未消费余槽");

    // 视图余量仍 5，有效余量 = 5 − 3 = 2：申请 3 拒，申请 2 过。
    const auto g2 = ledger.request(make_request(2, 101, 42, 7, 3), 110);
    TEST_ASSERT(!g2.granted && g2.code == 1, "hold 抵扣后不足应拒 code 1");
    const auto g3 = ledger.request(make_request(3, 102, 42, 7, 2), 120);
    TEST_ASSERT(g3.granted && g3.reservation_id == "sq-2", "恰余可 grant");
    TEST_ASSERT(ledger.held_slots(7) == 5, "hold 合计");
    TEST_ASSERT(ledger.live_count() == 2, "双 live");
    return true;
}

// 实例未知（code 2）：不记账不铸造。
bool test_unknown_instance_rejected() {
    FakeCapacityView view;
    view.known = {42};
    view.free = {{7, 5}};
    SquadReservationLedger ledger(view, 1000);

    const auto g = ledger.request(make_request(1, 100, 43, 7, 2), 100);
    TEST_ASSERT(!g.granted && g.code == 2, "未知实例 code 2");
    TEST_ASSERT(g.reservation_id.empty(), "拒绝不铸 id");
    TEST_ASSERT(ledger.live_count() == 0 && ledger.held_slots(7) == 0,
                "拒绝零记账");
    return true;
}

// 排空（code 3）：优先于容量裁决（design：排空中明确码回传）。
bool test_draining_instance_rejected() {
    FakeCapacityView view;
    view.known = {42};
    view.draining = {42};
    view.free = {{7, 5}};
    SquadReservationLedger ledger(view, 1000);

    const auto g = ledger.request(make_request(1, 100, 42, 7, 2), 100);
    TEST_ASSERT(!g.granted && g.code == 3, "排空 code 3");
    TEST_ASSERT(ledger.live_count() == 0, "排空零记账");
    return true;
}

// 无容量（code 1）：余量不足与非法槽数都不记账。
bool test_no_capacity_rejected() {
    FakeCapacityView view;
    view.known = {42};
    view.free = {{7, 2}};
    SquadReservationLedger ledger(view, 1000);

    const auto g1 = ledger.request(make_request(1, 100, 42, 7, 3), 100);
    TEST_ASSERT(!g1.granted && g1.code == 1, "超余量 code 1");
    const auto g2 = ledger.request(make_request(2, 101, 42, 7, 0), 110);
    TEST_ASSERT(!g2.granted && g2.code == 1, "非法槽数 code 1");
    TEST_ASSERT(ledger.live_count() == 0, "拒绝零记账");
    return true;
}

// 消费释放：逐凭证 consume 每张放一槽，hold 清零即销账；重复消费 false。
bool test_consume_releases_slots() {
    FakeCapacityView view;
    view.known = {42};
    view.free = {{7, 3}};
    SquadReservationLedger ledger(view, 1000);

    const auto g = ledger.request(make_request(1, 100, 42, 7, 3), 100);
    TEST_ASSERT(g.granted, "前置 grant");

    TEST_ASSERT(!ledger.consume("sq-9", 150), "未知 id 消费 false");
    TEST_ASSERT(ledger.consume("sq-1", 150), "第一张 consume true");
    TEST_ASSERT(ledger.held_slots(7) == 2, "hold -1");
    world_app::SquadReservationLedger::Reservation snap;
    TEST_ASSERT(ledger.find("sq-1", snap) && snap.held == 2, "余槽 2");
    TEST_ASSERT(ledger.consume("sq-1", 160), "第二张 true");
    TEST_ASSERT(ledger.consume("sq-1", 170), "第三张 true（销账）");
    TEST_ASSERT(ledger.live_count() == 0 && ledger.held_slots(7) == 0,
                "hold 清零销账");
    TEST_ASSERT(!ledger.consume("sq-1", 180), "销账后消费 false");
    TEST_ASSERT(ledger.find("sq-1", snap) == false, "销账不可读");

    // 释放后容量可复用：同余量再 grant 过。
    const auto g2 = ledger.request(make_request(2, 200, 42, 7, 3), 190);
    TEST_ASSERT(g2.granted, "消费释放后容量复用");
    return true;
}

// 过期回收：sweep 整批回收放 hold；validate 窗已过 false（签发面 code 4 依据）；
// 过期触达 consume false 且即回收。
bool test_expiry_sweep_and_validate() {
    FakeCapacityView view;
    view.known = {42};
    view.free = {{7, 4}};
    SquadReservationLedger ledger(view, 100);  // 窗 100ms

    const auto g = ledger.request(make_request(1, 100, 42, 7, 4), 100);
    TEST_ASSERT(g.granted && g.expires_at == 200, "窗 100ms grant");
    TEST_ASSERT(ledger.validate("sq-1", 199), "期内 validate true");
    TEST_ASSERT(!ledger.validate("sq-1", 200), "到期边界 validate false");
    TEST_ASSERT(!ledger.validate("sq-x", 150), "未知 id validate false");

    TEST_ASSERT(ledger.sweep_expired(150) == 0, "未到期 sweep 零回收");
    TEST_ASSERT(ledger.live_count() == 1, "未到期仍在册");
    TEST_ASSERT(ledger.sweep_expired(200) == 1, "到期 sweep 回收一条");
    TEST_ASSERT(ledger.live_count() == 0 && ledger.held_slots(7) == 0,
                "回收放 hold");

    // 回收后容量复用。
    const auto g2 = ledger.request(make_request(2, 200, 42, 7, 4), 210);
    TEST_ASSERT(g2.granted, "回收后容量复用");

    // 先消费一张释放一槽（有效余量 4−3=1 可再 grant）；过期触达 consume：
    // false 且即回收（该条 hold 整条放）。
    TEST_ASSERT(ledger.consume("sq-2", 215), "消费一槽释放 hold");
    const auto g3 = ledger.request(make_request(3, 300, 42, 7, 1), 220);
    TEST_ASSERT(g3.granted, "释放后余量可 grant");
    TEST_ASSERT(!ledger.consume("sq-3", 400), "过期 consume false");
    TEST_ASSERT(ledger.live_count() == 1, "仅剩 sq-2");
    TEST_ASSERT(ledger.held_slots(7) == 3, "sq-2 hold 3（sq-3 过期回收放 1）");
    return true;
}

} // namespace

int main() {
    struct Named {
        const char* name;
        bool (*fn)();
    };
    const Named tests[] = {
        {"grant_and_hold_deduction", test_grant_and_hold_deduction},
        {"unknown_instance_rejected", test_unknown_instance_rejected},
        {"draining_instance_rejected", test_draining_instance_rejected},
        {"no_capacity_rejected", test_no_capacity_rejected},
        {"consume_releases_slots", test_consume_releases_slots},
        {"expiry_sweep_and_validate", test_expiry_sweep_and_validate},
    };

    for (const auto& t : tests) {
        std::cout << "Running test_" << t.name << "... " << std::flush;
        if (t.fn()) {
            std::cout << "PASSED" << std::endl;
        } else {
            std::cout << "FAILED" << std::endl;
            std::cerr << "world_squad_reservation_ledger_tests failed" << std::endl;
            return 1;
        }
    }

    std::cout << "world_squad_reservation_ledger_tests passed" << std::endl;
    return 0;
}
