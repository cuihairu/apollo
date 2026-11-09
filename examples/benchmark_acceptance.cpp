/**
 * @file benchmark_acceptance.cpp
 * @brief 受理面基准段（P3-2 批 D：受理/执行路径队列侧局部成本）
 */

#include "benchmark_sections.h"

#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <random>
#include <atomic>
#include <iomanip>
#include <map>
#include <unordered_map>
#include <mutex>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <memory>
#include <filesystem>

#include "apollo/game/cell/avatar.hpp"
#include "apollo/game/world/scene.hpp"
#include "apollo/game/world/scene_aoi.hpp"
#include "apollo/data/orm/persist_journal.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"
#include "world_app/acceptance_queue.hpp"

using namespace std;
using namespace std::chrono;
//==============================================================================
// 受理面基准（P3-2 批 D；ADR-016 #3 tick 预算锚定 + capacity §2.1 同型Avatar）
//==============================================================================
// 口径：受理面局部成本——受理路径（parseHeader + 信封构造 + 入队）与执行
// 路径队列侧（出队 + parseHeader + decodeBody）。world 变更面（handler 全量
// 语义：校验/变更/广播）不在本段，归 §2.1 scene 三模型Avatar；两段夹逼出受理
// 面全量成本。三跑取 min（下界口径，负载噪声期均值守不可信，同 §2.1 纪律）。

namespace {

constexpr std::size_t kAcceptBurstSize = 1024;  // = AcceptanceQueue::kDefaultCap
constexpr int kAcceptRuns = 3;
constexpr double kMessageSegmentBudgetMs = 20.0; // capacity §2 消息段预算
constexpr double kMsgPerPlayerPerSec = 5.0;      // capacity §1 上行画像
constexpr double kFrameRateHz = 10.0;            // clock-and-time §4

// 一条真实移动意图帧（与受理面变更类五型之一同型，头部+体全量编码）
std::vector<uint8_t> makeMoveFrame(std::uint64_t entity_id, float dx) {
    apollo::protocol::CellEntityMove move;
    move.entityId = static_cast<apollo::protocol::EntityID>(entity_id);
    move.oldPos = apollo::protocol::Position(0.0f, 0.0f, 0.0f);
    move.newPos = apollo::protocol::Position(dx, 0.0f, 0.0f);
    return apollo::protocol::MessageCodec::encode(move, /*sessionId*/ 1);
}

double elapsedUs(std::chrono::steady_clock::time_point start,
                 std::chrono::steady_clock::time_point end) {
    return std::chrono::duration<double, std::micro>(end - start).count();
}

// 受理路径（RPC worker 侧）：parseHeader + 信封构造[原始帧拷贝] + 入队
double measureAcceptNs(const std::vector<uint8_t>& frame,
                       std::uint64_t& checksum) {
    double best = 0;
    bool first = true;
    for (int run = 0; run < kAcceptRuns; ++run) {
        world_app::AcceptanceQueue queue;
        std::uint64_t sink = 0;
        auto start = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < kAcceptBurstSize; ++i) {
            auto header = apollo::protocol::MessageCodec::parseHeader(frame);
            world_app::IntentEnvelope intent;
            intent.type = static_cast<apollo::protocol::MessageType>(header.type);
            intent.frame = frame;  // 信封承载原始帧（同 acceptRequest）
            intent.session_id = header.sessionId;
            queue.push(std::move(intent));
            sink += header.length + i;
        }
        auto elapsed = elapsedUs(start, std::chrono::steady_clock::now());
        checksum += sink;
        if (first || elapsed < best) {
            best = elapsed;
            first = false;
        }
    }
    return best * 1000.0 / kAcceptBurstSize;
}

// 执行路径队列侧（gameThread tick 边界）：出队 + parseHeader + decodeBody
// ——handleCellEntityMove 的解码前缀同型；world 变更/广播面不计
double measureExecuteNs(const std::vector<uint8_t>& frame,
                        std::uint64_t& checksum) {
    double best = 0;
    bool first = true;
    for (int run = 0; run < kAcceptRuns; ++run) {
        world_app::AcceptanceQueue queue;
        for (std::size_t i = 0; i < kAcceptBurstSize; ++i) {
            world_app::IntentEnvelope intent;
            intent.type = apollo::protocol::MessageType::CELL_ENTITY_MOVE;
            intent.frame = frame;
            intent.session_id = 1;
            queue.push(std::move(intent));
        }
        std::uint64_t sink = 0;
        auto start = std::chrono::steady_clock::now();
        std::size_t drained = 0;
        while (auto intent = queue.try_pop()) {
            auto header = apollo::protocol::MessageCodec::parseHeader(intent->frame);
            std::vector<uint8_t> body(
                intent->frame.begin() + sizeof(apollo::protocol::MessageHeader),
                intent->frame.end());
            auto move = apollo::protocol::MessageCodec::decodeBody<
                apollo::protocol::CellEntityMove>(body);
            sink += header.type + move.entityId;
            ++drained;
        }
        auto elapsed = elapsedUs(start, std::chrono::steady_clock::now());
        checksum += sink + drained;
        if (first || elapsed < best) {
            best = elapsed;
            first = false;
        }
    }
    return best * 1000.0 / kAcceptBurstSize;
}

}  // namespace

void benchmarkAcceptanceFace() {
    cout << "\n=== Acceptance Face Benchmark (P3-2 D; ADR-016 #3 锚定) ===" << endl;
    cout << "负载：单场景 1024 条移动意图突发（= 受理队列 cap 顶格）；受理路径"
         << " 与执行路径队列侧分段；三跑取 min" << endl;
    cout << "口径：受理面局部成本下界——无 world 变更/广播面（归 §2.1 scene Avatar）"
         << "；帧预算 100ms @ 10Hz，消息段预算 20ms" << endl;

    auto frame = makeMoveFrame(1, 1.0f);
    std::uint64_t checksum = 0;
    const double accept_ns = measureAcceptNs(frame, checksum);
    const double execute_ns = measureExecuteNs(frame, checksum);
    const double total_ns = accept_ns + execute_ns;

    cout << left << setw(30) << "segment"
         << right << setw(14) << "ns/intent"
         << setw(14) << "µs/intent" << endl;
    cout << string(58, '-') << endl;
    cout << left << setw(30) << "accept (worker: header+env+push)"
         << right << setw(14) << fixed << setprecision(1) << accept_ns
         << setw(14) << setprecision(3) << accept_ns / 1000.0 << endl;
    cout << left << setw(30) << "execute queue-side (pop+decode)"
         << right << setw(14) << setprecision(1) << execute_ns
         << setw(14) << setprecision(3) << execute_ns / 1000.0 << endl;
    cout << left << setw(30) << "total (accept+execute)"
         << right << setw(14) << setprecision(1) << total_ns
         << setw(14) << setprecision(3) << total_ns / 1000.0 << endl;

    // cap 顶格突发全程 drain 预算对照（受理入队 + tick 边界执行 drain）
    const double cap_drain_ms = total_ns * kAcceptBurstSize / 1e6;
    cout << "\ntop-cap burst: " << kAcceptBurstSize << " intents drain = "
         << fixed << setprecision(2) << cap_drain_ms << " ms"
         << " (消息段预算 " << kMessageSegmentBudgetMs
         << " ms, 余量 " << setprecision(1)
         << (kMessageSegmentBudgetMs / cap_drain_ms) << "x)" << endl;
    cout << "(checksum=" << checksum << ", 防 DCE)" << endl;

    // 上行画像 → 每拍意图量 → 队列顶格占用（capacity §1：5 条/s/玩家，10Hz）
    const double per_player_per_tick = kMsgPerPlayerPerSec / kFrameRateHz;
    cout << "\n" << left << setw(22) << "规模档"
         << setw(12) << "意图/拍"
         << setw(14) << "成本 µs"
         << setw(12) << "占 20ms"
         << setw(12) << "占 cap" << endl;
    cout << string(72, '-') << endl;
    const struct {
        const char* name;
        int players;
    } scales[] = {
        {"C 单副本(10)", 10},
        {"B 单景(100)", 100},
        {"A 大场景(1000)", 1000},
        {"全容量(5000)", 5000},
    };
    for (const auto& s : scales) {
        const double intents = s.players * per_player_per_tick;
        const double cost_us = intents * total_ns / 1000.0;
        cout << left << setw(22) << s.name
             << right << setw(12) << fixed << setprecision(0) << intents
             << setw(14) << setprecision(1) << cost_us
             << setw(12) << setprecision(2) << cost_us / 1e3 / kMessageSegmentBudgetMs * 100.0 << "%"
             << setw(12) << setprecision(0) << intents / kAcceptBurstSize * 100.0 << "%"
             << endl;
    }
    cout << "\n口径注记：全容量档意图/拍超 cap 顶格——排队饱和即拒收路径的"
         << "设计消费面（ADR-016 不丢旧不静默）；上行限流兜底随 net M1。" << endl;
}

