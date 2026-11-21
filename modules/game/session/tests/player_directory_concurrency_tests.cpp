// PlayerDirectory 跨线程并发回归（audit P0#1 修复 2026-11-12）。
// 此前 M1 泵线程（bindSession/assignWorld → session_up/moved）与 login
// fleetLoop 线程（sweep/suspend）并发改同一 entries_ 无锁 → 迭代器失效/UAF。
// 修复：PlayerDirectory 内部 recursive_mutex 串行化全部公开入口。
// 本测试：两线程并发混合写（session_up/evict/session_down）与读
// （find/online_ids/size/sweep），压 2s 不崩 + 事后条目真值一致性。
#include "apollo/game/session/player_directory.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

namespace {

using apollo::game::session::PlayerDirectory;
using apollo::game::base::SessionBinding;
using apollo::game::base::WorldAssignment;

apollo::game::base::SessionBinding makeBinding(std::uint64_t session_id) {
    apollo::game::base::SessionBinding b;
    b.session_id = session_id;
    b.gateway_id = 7;
    b.gateway_addr = "gateway://127.0.0.1:8888";
    b.bind_time_ms = static_cast<std::int64_t>(session_id);
    b.session_version = session_id;  // L116 绑定代次域（异值可辨）
    return b;
}

apollo::game::base::WorldAssignment makeAssignment(std::uint64_t world_id) {
    apollo::game::base::WorldAssignment a;
    a.world_id = static_cast<std::uint32_t>(world_id);
    a.map_id = 100;
    a.instance_id = 1;
    a.space_id = 1000;
    a.route_version = 1;
    // L116/L117 新域：cell/模拟代次/拓扑版本异值进并发写面
    a.cell_id = world_id * 10;
    a.simulation_epoch = world_id * 100;
    a.topology_version = world_id * 1000;
    a.write_gen = world_id * 10000;  // L118 归属域（资料写权代次）异值
    return a;
}

// 并发压测：写线程持续 session_up/evict/session_down 随机玩家集，
// 读线程持续 find/online_ids/size/sweep。崩溃/死锁即测试失败。
bool test_concurrent_read_write() {
    PlayerDirectory dir;
    constexpr std::uint64_t kPlayers = 64;
    constexpr auto kDuration = std::chrono::seconds(2);

    std::atomic<bool> stop{false};
    std::atomic<bool> writer_error{false};
    std::atomic<bool> reader_error{false};

    std::thread writer([&]() {
        std::uint64_t seq = 1000;
        while (!stop.load()) {
            for (std::uint64_t p = 1; p <= kPlayers; ++p) {
                const auto id = p + (seq % 3) * 100;  // 3 个错开窗口
                dir.session_up(id, makeBinding(seq++), makeAssignment(id));
                if (seq % 5 == 0) {
                    dir.evict_for_relogin(id);
                }
                if (seq % 7 == 0) {
                    dir.session_down(id);
                }
                dir.moved(id, makeAssignment(id + 1));
            }
        }
    });

    std::thread reader([&]() {
        std::uint64_t tick = 0;
        while (!stop.load()) {
            // 只读面并发遍历（修复前：写线程 erase 使迭代器失效/悬空）。
            // 注意 find 命中与否不设错：online_ids 读后到 find 前 writer 可
            // 合法删除该玩家，find-miss 属合法交错。回归信号 = 不崩溃/不死锁。
            const auto online = dir.online_ids();
            for (const auto id : online) {
                (void)dir.find(id);
            }
            (void)dir.size();
            dir.sweep(tick++);  // sweep 与写并发（修复前路径）
        }
    });

    std::this_thread::sleep_for(kDuration);
    stop = true;
    writer.join();
    reader.join();

    if (writer_error.load() || reader_error.load()) {
        return false;
    }
    // 压后真值一致：online_ids 每个成员 find 命中且状态 Online
    for (const auto id : dir.online_ids()) {
        const auto* entry = dir.find(id);
        if (entry == nullptr ||
            entry->state != PlayerDirectory::EntryState::Online) {
            return false;
        }
    }
    return true;
}

// 语义保持回归：加锁不改行为——顶号/窗口/迁移单线程面全保（抽核心断言）。
bool test_semantics_preserved() {
    PlayerDirectory dir;
    const auto epoch = dir.session_up(42, makeBinding(9001), makeAssignment(1), 3);
    if (epoch != 1 || dir.size() != 1) {
        return false;
    }
    if (!dir.mark_suspended(42, 100, 50)) {
        return false;
    }
    const auto* entry = dir.find(42);
    if (entry == nullptr || entry->state != PlayerDirectory::EntryState::Suspended) {
        return false;
    }
    if (!dir.resume(42, 9001, 1)) {
        return false;
    }
    if (!dir.moved(42, makeAssignment(2))) {
        return false;
    }
    if (!dir.begin_leave(42)) {
        return false;
    }
    if (!dir.session_down(42, PlayerDirectory::kReasonLogout) || dir.size() != 0) {
        return false;
    }
    // 顶号：新顶旧产 Kicked（player 42 已耗 epoch 1，player 7 得 epoch 2）
    const auto epoch7 = dir.session_up(7, makeBinding(1), makeAssignment(1));
    if (epoch7 != 2) {
        return false;
    }
    const auto old_epoch = dir.evict_for_relogin(7);
    if (old_epoch != epoch7) {
        return false;
    }
    return true;
}

// L118 并发面新域用例：两个写线程竞写同一玩家集合（session_up 顶号新条目
// → moved 以锚点 epoch 推进域值），域值取 epoch 的既定倍数——收敛后逐条
// 核验锚点与五域值出自同一写轮次（route/cell/sim_epoch/topology/write_gen
// 配对，归属域与模拟域分域同轮推进）。
// 迟到 moved 被水位判失效属合法交错（对方顶号已推进条目），不算失败；
// 终态不变量：条目 epoch 只能配到最后一次成功落表的域值组合，撕裂/错轮
// 配对即红。session_version 随绑定稳定携带。
bool test_domain_fields_no_tear() {
    PlayerDirectory dir;
    constexpr std::uint64_t kPlayers = 24;
    constexpr std::uint64_t kRounds = 2000;
    constexpr std::uint64_t kZone = 5;

    std::atomic<bool> ok{true};
    auto paired = [](std::uint64_t epoch) {
        apollo::game::base::WorldAssignment a;
        a.world_id = 1;
        a.route_version = epoch;
        a.cell_id = epoch * 10;
        a.simulation_epoch = epoch * 100;
        a.topology_version = epoch * 1000;
        a.write_gen = epoch * 10000;  // L118 归属域配对
        return a;
    };

    auto writer = [&]() {
        for (std::uint64_t r = 0; r < kRounds && ok.load(); ++r) {
            for (std::uint64_t p = 1; p <= kPlayers; ++p) {
                const auto id = p;
                apollo::game::base::WorldAssignment fresh;  // up 时域未声明（0），moved 配对补齐
                fresh.world_id = 1;
                const auto epoch = dir.session_up(id, makeBinding(id), fresh, kZone);
                if (epoch == 0) {
                    ok = false;
                    return;
                }
                (void)dir.moved(id, paired(epoch), epoch, kZone);  // 迟到失效合法
            }
        }
    };

    std::thread w1([&]() { writer(); });
    std::thread w2([&]() { writer(); });
    w1.join();
    w2.join();
    if (!ok.load()) {
        return false;
    }
    for (std::uint64_t p = 1; p <= kPlayers; ++p) {
        const auto* entry = dir.find(p);
        if (entry == nullptr || entry->state != PlayerDirectory::EntryState::Online) {
            return false;
        }
        const auto epoch = entry->anchor_epoch;
        const auto& a = entry->assignment;
        if (entry->binding.session_version != p) {
            return false;
        }
        if (a.route_version != epoch || a.cell_id != epoch * 10 ||
            a.simulation_epoch != epoch * 100 ||
            a.topology_version != epoch * 1000 ||
            a.write_gen != epoch * 10000) {
            return false;
        }
        if (entry->zone_id != kZone) {
            return false;
        }
    }
    return true;
}

} // namespace

int main() {
    struct Named {
        const char* name;
        bool (*fn)();
    };
    const Named tests[] = {
        {"concurrent_read_write", test_concurrent_read_write},
        {"semantics_preserved", test_semantics_preserved},
        {"domain_fields_no_tear", test_domain_fields_no_tear},
    };

    for (const auto& t : tests) {
        std::cout << "Running test_" << t.name << "... ";
        if (t.fn()) {
            std::cout << "PASSED" << std::endl;
        } else {
            std::cout << "FAILED" << std::endl;
            std::cerr << "player_directory_concurrency_tests failed" << std::endl;
            return 1;
        }
    }

    std::cout << "player_directory_concurrency_tests passed" << std::endl;
    return 0;
}
