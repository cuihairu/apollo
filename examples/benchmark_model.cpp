/**
 * @file benchmark_model.cpp
 * @brief 三模型场景段（P3-2：Scene/AOI/Avatar 帧循环 + persist 全帧段实测）
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
// 三模型场景基准（P3-2 批 A；rearchitecture/architecture.md §6 口径）
//==============================================================================
//
// 模型 A：1 Scene / 1000 Players（大场景千人）
// 模型 B：10 Scenes / 100 Players（单进程多景）
// 模型 C：100 Scenes / 10 Players（副本制典型负载）
//
// 负载链（全部真实路径，单线程驱动——Scene 单写者纪律）：
//   Avatar 匀速漂移 → SceneAoi::move（ViewerState 差集 → Enter/Sync/Leave
//   事件计数 = 广播消息量）→ Scene::tick 六阶段（实体 on_update 真调用）
//   → flush 段（P3-2 ①全帧段实测化：逐事件编真实协议帧——Enter→
//   CellCreateEntity / Sync→CellEntityMove / Leave→CellDestroyEntity，
//   MessageCodec 全量编码 = 下发面装包成本，网络 socket 发送不计——
//   网关下发面仍是桩（world_app broadcastToViewers 只日志），本段
//   量的是「collect+flush」两段的可测部分）。
// 桩口径：移动 = 匀速直线 + 世界边界回绕，无寻路/技能/跨景——帧成本是
// 下界，作 capacity-and-benchmark 帧预算表实测化的Avatar数据。

namespace {

using apollo::game::core::EntityId;
using apollo::game::core::PlayerId;
using apollo::game::cell::Avatar;
using apollo::game::cell::AvatarPtr;
using apollo::game::world::Scene;
using apollo::game::world::SceneAoi;

struct ModelConfig {
    const char* name;
    int scenes;
    int players_per_scene;
    int frames;
    float world_size;  // 单 scene 世界边长
};

struct ModelStats {
    double enter_ms = 0.0;        // 进场总耗时（构造 + enter × N）
    double frame_avg_ms = 0.0;    // 帧均值（scene 局部：移动 + AOI 差集 + tick）
    double frame_max_ms = 0.0;    // 帧峰值
    double sync_per_frame = 0.0;  // Sync 事件/帧（广播消息量口径）
    std::uint64_t enter_events = 0;
    std::uint64_t leave_events = 0;
    // flush 段（全帧段实测化）：AOI 事件 → 协议帧编码（collect+flush 可测面）
    double flush_avg_ms = 0.0;    // 编码段均值/帧
    double msgs_per_frame = 0.0;  // 编码消息数/帧（= 事件面全量，不止 Sync）
    double bytes_per_frame = 0.0; // 编码字节/帧（下行带宽口径）
    // persist 段（全帧段实测化）：journal 交 DB 线程面（write-ahead append
    // + 定额 drain；DB 线程序列化不计）
    double persist_avg_ms = 0.0;  // journal 段均值/帧
    double rows_per_frame = 0.0;  // journal 行数/帧（脏实体口径）
};

// 逐玩家状态：benchmark 侧驱动（直接持有 Avatar，免 Scene 查找回路）
struct Drifter {
    AvatarPtr avatar;
    EntityId entity{0};
    float x = 0.0f;
    float z = 0.0f;
    float vx = 0.0f;
    float vz = 0.0f;
    std::string persist_key;  // journal 行键（玩家持久键——进程内恒定，预生成）
};

constexpr float kFrameDelta = 0.1f;  // 10Hz 主循环（clock-and-time §4 定案；
                                     // 20Hz 是 movement 上报节拍，非主循环）
constexpr double kFrameBudgetMs = 100.0;
constexpr float kSpeed = 5.0f;  // 单位/秒
constexpr int kWarmupFrames = 10;

// 黄金比例散点：均匀不打格（同格堆叠会掩盖九宫格开销差异）
float scatter(std::uint32_t i, float bound) {
    const float g = (i % 2u == 0u) ? 0.6180339887f : 0.7548776662f;
    return std::fmod(static_cast<float>(i) * g, 1.0f) * bound;
}

// AOI 事件 → 真实协议帧（flush 段：collect+flush 的可测面——编码装包；
// 事件→帧型映射 = 广播下发面语义：观察者视野里主体出现/移动/消失）。
// session_id 0 = 系统广播（非会话回执路径）。
std::vector<std::uint8_t> encode_aoi_event(const SceneAoi::Event& e) {
    switch (e.kind) {
    case SceneAoi::Event::Kind::Enter: {
        apollo::protocol::CellCreateEntity msg;
        msg.entityId = static_cast<apollo::protocol::EntityID>(e.subject.value());
        msg.entityType = apollo::protocol::EntityType::PLAYER;
        msg.spaceId = 0;
        msg.position = apollo::protocol::Position(0.0f, 0.0f, 0.0f);
        return apollo::protocol::MessageCodec::encode(msg, /*sessionId*/ 0);
    }
    case SceneAoi::Event::Kind::Sync: {
        apollo::protocol::CellEntityMove msg;
        msg.entityId = static_cast<apollo::protocol::EntityID>(e.subject.value());
        msg.oldPos = apollo::protocol::Position(0.0f, 0.0f, 0.0f);
        msg.newPos = apollo::protocol::Position(0.0f, 0.0f, 0.0f);
        return apollo::protocol::MessageCodec::encode(msg, /*sessionId*/ 0);
    }
    case SceneAoi::Event::Kind::Leave:
    default: {
        apollo::protocol::CellDestroyEntity msg;
        msg.entityId = static_cast<apollo::protocol::EntityID>(e.subject.value());
        return apollo::protocol::MessageCodec::encode(msg, /*sessionId*/ 0);
    }
    }
}

ModelStats run_model(const ModelConfig& cfg) {
    ModelStats stats;
    std::uint64_t sync_events = 0;
    std::uint64_t enter_events = 0;
    std::uint64_t leave_events = 0;

    const auto t0 = high_resolution_clock::now();

    // 构造：逐 scene 建容器 + AOI + sink，再逐玩家进场（散布落点）
    std::vector<std::unique_ptr<Scene>> scenes;
    std::vector<std::vector<Drifter>> per_scene(static_cast<std::size_t>(cfg.scenes));
    scenes.reserve(static_cast<std::size_t>(cfg.scenes));

    // flush 段事件收集面：sink 计数 + 逐帧暂存（编码在 tick 后统一做，
    // 计时段与 scene 局部段分离——全帧段实测化的分段口径）
    std::vector<SceneAoi::Event> frame_events;
    frame_events.reserve(16384);

    // persist 段 journal（write-ahead 面在 game thread——§8.2 落库形态）：
    // 临时文件逐跑唯一，跑毕清理；开不了 = 基准Avatar缺失，响亮失败
    static std::atomic<int> journal_run{0};
    const std::string journal_path =
        (std::filesystem::temp_directory_path()
         / ("apollo-bench-journal-" + std::to_string(journal_run.fetch_add(1))
            + ".log")).string();
    std::remove(journal_path.c_str());
    apollo::data::journal::PersistJournal journal(journal_path);
    if (!journal.open()) {
        std::cerr << "benchmark: cannot open journal " << journal_path
                  << std::endl;
        std::exit(1);
    }

    std::uint32_t uid = 0;
    for (int s = 0; s < cfg.scenes; ++s) {
        auto scene = std::make_unique<Scene>(
            static_cast<std::uint64_t>(s + 1),
            std::string(cfg.name) + "-s" + std::to_string(s));
        scene->aoi() = SceneAoi(cfg.world_size, cfg.world_size, 25.0f, 20.0f);
        scene->aoi().set_event_sink([&](const SceneAoi::Event& e) {
            switch (e.kind) {
            case SceneAoi::Event::Kind::Enter:
                ++enter_events;
                break;
            case SceneAoi::Event::Kind::Sync:
                ++sync_events;
                break;
            case SceneAoi::Event::Kind::Leave:
                ++leave_events;
                break;
            }
            frame_events.push_back(e);
        });

        auto& bucket = per_scene[static_cast<std::size_t>(s)];
        bucket.reserve(static_cast<std::size_t>(cfg.players_per_scene));
        for (int p = 0; p < cfg.players_per_scene; ++p) {
            const auto id = static_cast<std::uint64_t>(uid);
            Drifter d;
            d.avatar = std::make_shared<Avatar>(PlayerId(id), EntityId(id),
                                                "p" + std::to_string(id));
            d.entity = EntityId(id);
            d.x = scatter(uid, cfg.world_size);
            d.z = scatter(uid * 7u + 13u, cfg.world_size);
            const float heading = scatter(uid * 3u + 5u, 6.2831853f);
            d.vx = std::cos(heading) * kSpeed;
            d.vz = std::sin(heading) * kSpeed;
            d.persist_key = std::to_string(id);
            scene->enter(d.avatar, SceneAoi::Vec3{d.x, 0.0f, d.z});
            bucket.push_back(std::move(d));
            ++uid;
        }
        scenes.push_back(std::move(scene));
    }

    const auto t1 = high_resolution_clock::now();
    stats.enter_ms = duration_cast<microseconds>(t1 - t0).count() / 1000.0;

    // 帧循环：移动推进 + AOI 差集 + 六阶段 tick（预热帧不入统计）
    // + flush 段（事件→协议帧编码，tick 后统一做——分段口径与 §2 预算表对齐）
    const int measured = cfg.frames - kWarmupFrames;
    double frame_total_ms = 0.0;
    double frame_max_ms = 0.0;
    std::uint64_t sync_at_warmup_end = 0;
    double flush_total_ms = 0.0;
    double flush_bytes_total = 0.0;
    double flush_msgs_total = 0.0;
    double persist_total_ms = 0.0;
    double persist_rows_total = 0.0;

    for (int f = 0; f < cfg.frames; ++f) {
        const auto ft0 = high_resolution_clock::now();

        frame_events.clear();
        for (int s = 0; s < cfg.scenes; ++s) {
            SceneAoi& aoi = scenes[static_cast<std::size_t>(s)]->aoi();
            for (auto& d : per_scene[static_cast<std::size_t>(s)]) {
                d.x += d.vx * kFrameDelta;
                d.z += d.vz * kFrameDelta;
                if (d.x >= cfg.world_size) d.x -= cfg.world_size;
                if (d.x < 0.0f) d.x += cfg.world_size;
                if (d.z >= cfg.world_size) d.z -= cfg.world_size;
                if (d.z < 0.0f) d.z += cfg.world_size;
                d.avatar->set_position({d.x, 0.0f, d.z});
                aoi.move(d.entity, SceneAoi::Vec3{d.x, 0.0f, d.z});
            }
        }
        for (auto& scene : scenes) {
            scene->tick(kFrameDelta);
        }

        const auto ft1 = high_resolution_clock::now();
        const double ms = duration_cast<microseconds>(ft1 - ft0).count() / 1000.0;

        // flush 段：逐事件编真实协议帧（collect+flush 可测面；预热帧同跑
        // 不入统计——编码路径同样需要预热）
        std::size_t flush_bytes = 0;
        const auto fs0 = high_resolution_clock::now();
        for (const auto& ev : frame_events) {
            const auto msg = encode_aoi_event(ev);
            flush_bytes += msg.size();
        }
        const auto fs1 = high_resolution_clock::now();
        const double flush_ms =
            duration_cast<microseconds>(fs1 - fs0).count() / 1000.0;

        // persist 段：journal 交 DB 线程面（§2 预算表 persist-batch 行）。
        // 脏实体口径 = 每实体一条 L2 增量行（~30B 小载荷代表 HP/exp 类变
        // 更；移动本身是 L3 易失不落 journal——载荷是负载模型非语义声明）。
        // write-ahead append 在 game thread（§8.2 落库形态）+ 定额 drain
        // （稳态出队率=入队率；sink=移交计数，DB 线程序列化面不计）。
        std::size_t appended = 0;
        const auto ps0 = high_resolution_clock::now();
        for (int s = 0; s < cfg.scenes; ++s) {
            auto& bucket = per_scene[static_cast<std::size_t>(s)];
            for (std::size_t i = 0; i < bucket.size(); ++i) {
                const Drifter& d = bucket[i];
                const std::string payload =
                    "{\"hp\":" + std::to_string((f + static_cast<int>(i)) % 100)
                    + ",\"exp\":" + std::to_string(f) + "}";
                if (journal.append(d.persist_key, payload,
                                   static_cast<std::int64_t>(f) * 100)) {
                    ++appended;
                }
            }
        }
        journal.drain(
            [](const apollo::data::journal::JournalEntry&) { return true; },
            static_cast<std::size_t>(cfg.scenes)
                * static_cast<std::size_t>(cfg.players_per_scene));
        const auto ps1 = high_resolution_clock::now();
        const double persist_ms =
            duration_cast<microseconds>(ps1 - ps0).count() / 1000.0;

        if (f == kWarmupFrames - 1) {
            sync_at_warmup_end = sync_events;
        } else if (f >= kWarmupFrames) {
            frame_total_ms += ms;
            if (ms > frame_max_ms) frame_max_ms = ms;
            flush_total_ms += flush_ms;
            flush_bytes_total += static_cast<double>(flush_bytes);
            flush_msgs_total += static_cast<double>(frame_events.size());
            persist_total_ms += persist_ms;
            persist_rows_total += static_cast<double>(appended);
        }
    }

    // 临时 journal 清理（跑毕即弃；compact 面不在基准段内——定期压薄是
    // 独立节拍，非逐帧路径）
    {
        std::error_code ec;
        std::filesystem::remove(journal_path, ec);
    }

    stats.frame_avg_ms = frame_total_ms / measured;
    stats.frame_max_ms = frame_max_ms;
    stats.sync_per_frame =
        static_cast<double>(sync_events - sync_at_warmup_end) / measured;
    stats.enter_events = enter_events;
    stats.leave_events = leave_events;
    stats.flush_avg_ms = flush_total_ms / measured;
    stats.msgs_per_frame = flush_msgs_total / measured;
    stats.bytes_per_frame = flush_bytes_total / measured;
    stats.persist_avg_ms = persist_total_ms / measured;
    stats.rows_per_frame = persist_rows_total / measured;
    return stats;
}

void printModelStats(const ModelConfig& cfg, const ModelStats& st) {
    const double budget_pct = st.frame_avg_ms / kFrameBudgetMs * 100.0;
    const double total_ms = st.frame_avg_ms + st.flush_avg_ms + st.persist_avg_ms;
    const double total_pct = total_ms / kFrameBudgetMs * 100.0;
    cout << left << setw(12) << cfg.name
         << right << setw(11) << fixed << setprecision(1) << st.enter_ms
         << setw(12) << setprecision(3) << st.frame_avg_ms
         << setw(12) << setprecision(3) << st.frame_max_ms
         << setw(11) << setprecision(1) << st.sync_per_frame
         << setw(9) << st.enter_events
         << setw(9) << st.leave_events
         << setw(9) << setprecision(2) << budget_pct << "%"
         << setw(10) << setprecision(3) << st.flush_avg_ms
         << setw(10) << setprecision(0) << st.msgs_per_frame
         << setw(10) << setprecision(1) << st.bytes_per_frame / 1024.0
         << setw(11) << setprecision(3) << st.persist_avg_ms
         << setw(9) << setprecision(0) << st.rows_per_frame
         << setw(10) << setprecision(3) << total_ms
         << setw(9) << setprecision(2) << total_pct << "%" << endl;
}

}  // namespace

void benchmarkThreeModels() {
    cout << "\n=== Three-Model Scene Benchmark (P3-2 全帧段实测化) ===" << endl;
    cout << "负载：全体玩家匀速漂移（5 单位/s）+ AOI move 差集 + tick 六阶段"
         << " + flush 段（事件→协议帧编码）+ persist 段（journal append+drain）"
         << "（单线程；预热 " << kWarmupFrames << " 帧不入统计）" << endl;
    cout << "口径：帧预算 100ms @ 10Hz 主循环（capacity-and-benchmark §2 Avatar）；"
         << "scene 局部 = 移动+AOI+tick，flush = collect+flush 编码段，"
         << "persist = journal 交 DB 线程面（append 落盘实盘 IO，介质=宿主 /tmp；"
         << "DB 线程序列化不计），total = 全帧（§2 预算表三段）" << endl;

    const ModelConfig models[] = {
        {"A(1x1000)", 1, 1000, 200, 1000.0f},
        {"B(10x100)", 10, 100, 200, 100.0f},
        {"C(100x10)", 100, 10, 200, 100.0f},
    };

    cout << left << setw(12) << "Model"
         << right << setw(11) << "Enter ms"
         << setw(12) << "Frame avg"
         << setw(12) << "Frame max"
         << setw(11) << "Sync/f"
         << setw(9) << "EnterEv"
         << setw(9) << "LeaveEv"
         << setw(9) << "Scene%"
         << setw(10) << "Flush ms"
         << setw(10) << "Msgs/f"
         << setw(10) << "KB/f"
         << setw(11) << "Persist ms"
         << setw(9) << "Rows/f"
         << setw(10) << "Total ms"
         << setw(9) << "Total%" << endl;
    cout << string(154, '-') << endl;

    for (const auto& cfg : models) {
        const ModelStats st = run_model(cfg);
        printModelStats(cfg, st);
    }
}

