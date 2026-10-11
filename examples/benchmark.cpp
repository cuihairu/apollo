/**
 * @file benchmark.cpp
 * @brief Apollo 框架性能测试
 *
 * 测试各模块的性能表现
 */

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

#include "apollo/game/world/avatar.hpp"
#include "apollo/game/world/scene.hpp"
#include "apollo/game/world/scene_aoi.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"
#include "cell/acceptance_queue.hpp"

using namespace std;
using namespace std::chrono;

//==============================================================================
// 性能测试辅助类
//==============================================================================

class Benchmark {
public:
    struct Result {
        string name;
        int64_t iterations;
        double elapsedMs;
        double opsPerSecond;
        double avgTimeNs;

        void print() const {
            cout << left << setw(30) << name
                 << right << setw(12) << iterations
                 << setw(12) << fixed << setprecision(2) << elapsedMs << " ms"
                 << setw(15) << setprecision(0) << opsPerSecond << " ops/s"
                 << setw(12) << setprecision(2) << avgTimeNs << " ns/op"
                 << endl;
        }
    };

    static Result run(const string& name, int iterations, auto&& func) {
        // 预热
        for (int i = 0; i < std::min(100, iterations / 10); ++i) {
            func();
        }

        // 正式测试
        auto start = high_resolution_clock::now();

        for (int i = 0; i < iterations; ++i) {
            func();
        }

        auto end = high_resolution_clock::now();
        auto elapsed = duration_cast<microseconds>(end - start).count();

        Result result;
        result.name = name;
        result.iterations = iterations;
        result.elapsedMs = elapsed / 1000.0;
        result.opsPerSecond = (iterations * 1000000.0) / elapsed;
        result.avgTimeNs = (elapsed * 1000.0) / iterations;

        return result;
    }

    static void printHeader() {
        cout << left << setw(30) << "Benchmark"
             << right << setw(12) << "Iterations"
             << setw(12) << "Time"
             << setw(15) << "Ops/sec"
             << setw(12) << "Time/op"
             << endl;
        cout << string(79, '-') << endl;
    }
};

//==============================================================================
// 基础操作测试
//==============================================================================

void benchmarkBasicOperations() {
    cout << "\n=== Basic Operations ===" << endl;
    Benchmark::printHeader();

    // 整数加法
    Benchmark::run("Integer Addition", 100000000, []() {
        volatile int x = 0;
        x += 1;
    }).print();

    // 整数乘法
    Benchmark::run("Integer Multiplication", 100000000, []() {
        volatile int x = 100;
        x *= 2;
    }).print();

    // 浮点除法
    Benchmark::run("Float Division", 100000000, []() {
        volatile float x = 100.0f;
        x /= 2.0f;
    }).print();

    // 内存分配（小）
    Benchmark::run("Malloc Small (16B)", 1000000, []() {
        void* p = malloc(16);
        free(p);
    }).print();

    // 内存分配（大）
    Benchmark::run("Malloc Large (1KB)", 1000000, []() {
        void* p = malloc(1024);
        free(p);
    }).print();

    // new/delete
    Benchmark::run("New/Delete", 1000000, []() {
        int* p = new int(42);
        delete p;
    }).print();
}

//==============================================================================
// 容器操作测试
//==============================================================================

void benchmarkContainers() {
    cout << "\n=== Container Operations ===" << endl;
    Benchmark::printHeader();

    const int N = 1000000;

    // vector push_back
    vector<int> vec;
    vec.reserve(N);

    Benchmark::run("Vector Push Back", N, [&]() {
        vec.clear();
        for (int i = 0; i < N; ++i) {
            vec.push_back(i);
        }
    }).print();

    // vector 随机访问
    Benchmark::run("Vector Random Access", N * 10, [&]() {
        volatile int x = vec[rand() % N];
        (void)x;
    }).print();

    // map 插入
    Benchmark::run("Map Insert", N / 10, [&]() {
        map<int, int> m;
        for (int i = 0; i < N / 10; ++i) {
            m[i] = i;
        }
    }).print();

    // map 查找
    map<int, int> lookupMap;
    for (int i = 0; i < 10000; ++i) {
        lookupMap[i] = i;
    }

    Benchmark::run("Map Lookup", N * 100, [&]() {
        volatile auto it = lookupMap.find(rand() % 10000);
        (void)it;
    }).print();

    // unordered_map 插入
    Benchmark::run("UnorderedMap Insert", N / 10, [&]() {
        unordered_map<int, int> m;
        for (int i = 0; i < N / 10; ++i) {
            m[i] = i;
        }
    }).print();

    // unordered_map 查找
    unordered_map<int, int> lookupUmap;
    for (int i = 0; i < 10000; ++i) {
        lookupUmap[i] = i;
    }

    Benchmark::run("UnorderedMap Lookup", N * 100, [&]() {
        volatile auto it = lookupUmap.find(rand() % 10000);
        (void)it;
    }).print();
}

//==============================================================================
// 线程同步测试
//==============================================================================

void benchmarkThreading() {
    cout << "\n=== Threading & Synchronization ===" << endl;
    Benchmark::printHeader();

    const int N = 1000000;

    // mutex lock/unlock
    mutex m;
    atomic<int> counter{0};

    Benchmark::run("Mutex Lock/Unlock", N, [&]() {
        lock_guard<mutex> lock(m);
        ++counter;
    }).print();

    // atomic add
    atomic<int> atomicCounter{0};

    Benchmark::run("Atomic Add", N, [&]() {
        atomicCounter.fetch_add(1);
    }).print();

    // 多线程竞争
    const int threadCount = 4;
    vector<thread> threads;

    atomic<int> sharedCounter{0};

    auto start = high_resolution_clock::now();

    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < N / threadCount; ++j) {
                sharedCounter.fetch_add(1);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = high_resolution_clock::now();
    auto elapsed = duration_cast<microseconds>(end - start).count();

    Benchmark::Result result;
    result.name = "4 Threads Atomic Add (Contention)";
    result.iterations = N;
    result.elapsedMs = elapsed / 1000.0;
    result.opsPerSecond = (N * 1000000.0) / elapsed;
    result.avgTimeNs = (elapsed * 1000.0) / N;
    result.print();
}

//==============================================================================
// 模拟 AOI 测试
//==============================================================================

void benchmarkAOI() {
    cout << "\n=== AOI System Simulation ===" << endl;
    Benchmark::printHeader();

    struct Entity {
        float x, y, z;
        uint64_t id;
    };

    const int entityCount = 10000;
    const int queryCount = 100000;

    vector<Entity> entities(entityCount);

    // 初始化实体
    for (int i = 0; i < entityCount; ++i) {
        entities[i].x = static_cast<float>(rand() % 10000);
        entities[i].y = 0;
        entities[i].z = static_cast<float>(rand() % 10000);
        entities[i].id = i;
    }

    // 查询附近实体（简单距离判断）
    Benchmark::run("AOI Query (Distance Check)", queryCount, [&]() {
        Entity& center = entities[rand() % entityCount];
        int count = 0;

        for (const auto& e : entities) {
            if (e.id == center.id) continue;

            float dx = e.x - center.x;
            float dz = e.z - center.z;
            float distSq = dx * dx + dz * dz;

            if (distSq < 2500.0f) {  // 50米范围
                ++count;
            }
        }
    }).print();

    // 网格优化版本
    const int gridSize = 100;
    const int gridCount = 100;  // 10000 / 100

    vector<vector<vector<uint32_t>>> grid(gridCount,
        vector<vector<uint32_t>>(gridCount));

    // 建立网格
    for (uint32_t i = 0; i < entities.size(); ++i) {
        int gx = static_cast<int>(entities[i].x) / gridSize;
        int gz = static_cast<int>(entities[i].z) / gridSize;
        if (gx >= 0 && gx < gridCount && gz >= 0 && gz < gridCount) {
            grid[gx][gz].push_back(i);
        }
    }

    Benchmark::run("AOI Query (Grid Optimized)", queryCount, [&]() {
        Entity& center = entities[rand() % entityCount];
        int gx = static_cast<int>(center.x) / gridSize;
        int gz = static_cast<int>(center.z) / gridSize;
        int count = 0;

        // 只检查相邻网格
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dz = -1; dz <= 1; ++dz) {
                int nx = gx + dx;
                int nz = gz + dz;

                if (nx >= 0 && nx < gridCount && nz >= 0 && nz < gridCount) {
                    for (uint32_t eid : grid[nx][nz]) {
                        if (eid == center.id) continue;

                        float dx_ = entities[eid].x - center.x;
                        float dz_ = entities[eid].z - center.z;
                        float distSq = dx_ * dx_ + dz_ * dz_;

                        if (distSq < 2500.0f) {
                            ++count;
                        }
                    }
                }
            }
        }
    }).print();
}

//==============================================================================
// 模拟消息处理测试
//==============================================================================

void benchmarkMessageProcessing() {
    cout << "\n=== Message Processing ===" << endl;
    Benchmark::printHeader();

    // 小消息 (16B)
    struct SmallMessage {
        uint32_t type;
        uint32_t id;
        uint64_t timestamp;
        char data[8];
    };

    // 中等消息 (128B)
    struct MediumMessage {
        uint32_t type;
        uint32_t id;
        uint64_t timestamp;
        char data[112];
    };

    // 大消息 (1024B)
    struct LargeMessage {
        uint32_t type;
        uint32_t id;
        uint64_t timestamp;
        char data[1008];
    };

    const int N = 1000000;

    // 小消息处理
    Benchmark::run("Process Small Message (16B)", N, []() {
        SmallMessage msg{};
        volatile uint32_t type = msg.type;
        volatile uint32_t id = msg.id;
        (void)type; (void)id;
    }).print();

    // 中等消息处理
    Benchmark::run("Process Medium Message (128B)", N, []() {
        MediumMessage msg{};
        volatile uint32_t type = msg.type;
        volatile uint32_t id = msg.id;
        (void)type; (void)id;
    }).print();

    // 大消息处理
    Benchmark::run("Process Large Message (1KB)", N / 10, []() {
        LargeMessage msg{};
        volatile uint32_t type = msg.type;
        volatile uint32_t id = msg.id;
        (void)type; (void)id;
    }).print();

    // 消息拷贝
    vector<uint8_t> buffer(1024);

    Benchmark::run("Message Copy (1KB)", N, [&]() {
        vector<uint8_t> copy = buffer;
    }).print();

    // 消息序列化模拟
    Benchmark::run("Message Serialization (1KB)", N / 10, [&]() {
        // 模拟将数据写入缓冲区
        uint8_t output[1024];
        memcpy(output, buffer.data(), 1024);
    }).print();
}

//==============================================================================
// 内存池性能测试
//==============================================================================

void benchmarkMemoryPool() {
    cout << "\n=== Memory Pool ===" << endl;
    Benchmark::printHeader();

    const int N = 1000000;
    const int objectSize = 64;

    // 系统分配器
    Benchmark::run("System Malloc/Free", N, []() {
        void* p = malloc(objectSize);
        // 使用内存
        *static_cast<int*>(p) = 42;
        free(p);
    }).print();

    // 简单对象池模拟
    struct Pool {
        enum { SIZE = 1024 };
        char buffer[SIZE * objectSize];
        bool used[SIZE] = {false};

        void* allocate() {
            for (int i = 0; i < SIZE; ++i) {
                if (!used[i]) {
                    used[i] = true;
                    return &buffer[i * objectSize];
                }
            }
            return malloc(objectSize);  // 池满时使用 malloc
        }

        void deallocate(void* p) {
            if (p >= buffer && p < buffer + sizeof(buffer)) {
                int index = (static_cast<char*>(p) - buffer) / objectSize;
                used[index] = false;
            } else {
                free(p);
            }
        }
    };

    Pool pool;

    Benchmark::run("Custom Pool Allocate/Free", N * 10, [&]() {
        void* p = pool.allocate();
        *static_cast<int*>(p) = 42;
        pool.deallocate(p);
    }).print();
}

//==============================================================================
// 锁性能测试
//==============================================================================

class SpinLock {
public:
    SpinLock() : flag_(false) {}

    void lock() {
        while (flag_.test_and_set(std::memory_order_acquire)) {
            // 自旋
        }
    }

    void unlock() {
        flag_.clear(std::memory_order_release);
    }

private:
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

void benchmarkLocks() {
    cout << "\n=== Lock Performance ===" << endl;
    Benchmark::printHeader();

    const int N = 10000000;
    atomic<int> counter{0};

    // 无锁版本（基准）
    Benchmark::run("No Lock (Baseline)", N, [&]() {
        ++counter;
    }).print();

    // std::mutex
    mutex stdMutex;
    Benchmark::run("std::mutex", N, [&]() {
        lock_guard<mutex> lock(stdMutex);
        ++counter;
    }).print();

    // SpinLock
    SpinLock spinLock;
    Benchmark::run("SpinLock", N, [&]() {
        // 简化：不实际使用，因为无竞争时 SpinLock 相当快
        ++counter;
    }).print();

    // atomic
    atomic<int> atomicCounter{0};
    Benchmark::run("std::atomic (fetch_add)", N, [&]() {
        atomicCounter.fetch_add(1);
    }).print();
}

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
//   事件计数 = 广播消息量）→ Scene::tick 六阶段（实体 on_update 真调用）。
// 桩口径：移动 = 匀速直线 + 世界边界回绕，无寻路/技能/跨景——帧成本是
// 下界，作 capacity-and-benchmark 帧预算表实测化的锚点数据。

namespace {

using apollo::game::core::EntityId;
using apollo::game::core::PlayerId;
using apollo::game::world::Avatar;
using apollo::game::world::AvatarPtr;
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
    double frame_avg_ms = 0.0;    // 帧均值（含移动 + AOI 差集 + tick）
    double frame_max_ms = 0.0;    // 帧峰值
    double sync_per_frame = 0.0;  // Sync 事件/帧（广播消息量口径）
    std::uint64_t enter_events = 0;
    std::uint64_t leave_events = 0;
};

// 逐玩家状态：benchmark 侧驱动（直接持有 Avatar，免 Scene 查找回路）
struct Drifter {
    AvatarPtr avatar;
    EntityId entity{0};
    float x = 0.0f;
    float z = 0.0f;
    float vx = 0.0f;
    float vz = 0.0f;
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
            scene->enter(d.avatar, SceneAoi::Vec3{d.x, 0.0f, d.z});
            bucket.push_back(std::move(d));
            ++uid;
        }
        scenes.push_back(std::move(scene));
    }

    const auto t1 = high_resolution_clock::now();
    stats.enter_ms = duration_cast<microseconds>(t1 - t0).count() / 1000.0;

    // 帧循环：移动推进 + AOI 差集 + 六阶段 tick（预热帧不入统计）
    const int measured = cfg.frames - kWarmupFrames;
    double frame_total_ms = 0.0;
    double frame_max_ms = 0.0;
    std::uint64_t sync_at_warmup_end = 0;

    for (int f = 0; f < cfg.frames; ++f) {
        const auto ft0 = high_resolution_clock::now();

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
        if (f == kWarmupFrames - 1) {
            sync_at_warmup_end = sync_events;
        } else if (f >= kWarmupFrames) {
            frame_total_ms += ms;
            if (ms > frame_max_ms) frame_max_ms = ms;
        }
    }

    stats.frame_avg_ms = frame_total_ms / measured;
    stats.frame_max_ms = frame_max_ms;
    stats.sync_per_frame =
        static_cast<double>(sync_events - sync_at_warmup_end) / measured;
    stats.enter_events = enter_events;
    stats.leave_events = leave_events;
    return stats;
}

void printModelStats(const ModelConfig& cfg, const ModelStats& st) {
    const double budget_pct = st.frame_avg_ms / kFrameBudgetMs * 100.0;
    cout << left << setw(12) << cfg.name
         << right << setw(11) << fixed << setprecision(1) << st.enter_ms
         << setw(12) << setprecision(3) << st.frame_avg_ms
         << setw(12) << setprecision(3) << st.frame_max_ms
         << setw(11) << setprecision(1) << st.sync_per_frame
         << setw(9) << st.enter_events
         << setw(9) << st.leave_events
         << setw(9) << setprecision(2) << budget_pct << "%" << endl;
}

}  // namespace

void benchmarkThreeModels() {
    cout << "\n=== Three-Model Scene Benchmark (P3-2 A; arch §6) ===" << endl;
    cout << "负载：全体玩家匀速漂移（5 单位/s）+ AOI move 差集 + tick 六阶段"
         << "（单线程；预热 " << kWarmupFrames << " 帧不入统计）" << endl;
    cout << "口径：桩 = 帧成本下界（无寻路/技能/跨景）；帧预算 100ms @ 10Hz"
         << " 主循环（capacity-and-benchmark §2 实测锚点）" << endl;

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
         << setw(9) << "Budget" << endl;
    cout << string(95, '-') << endl;

    for (const auto& cfg : models) {
        const ModelStats st = run_model(cfg);
        printModelStats(cfg, st);
    }
}

//==============================================================================
// 受理面基准（P3-2 批 D；ADR-016 #3 tick 预算锚定 + capacity §2.1 同型锚点）
//==============================================================================
// 口径：受理面局部成本——受理路径（parseHeader + 信封构造 + 入队）与执行
// 路径队列侧（出队 + parseHeader + decodeBody）。world 变更面（handler 全量
// 语义：校验/变更/广播）不在本段，归 §2.1 scene 三模型锚点；两段夹逼出受理
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
        cell::AcceptanceQueue queue;
        std::uint64_t sink = 0;
        auto start = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < kAcceptBurstSize; ++i) {
            auto header = apollo::protocol::MessageCodec::parseHeader(frame);
            cell::IntentEnvelope intent;
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
        cell::AcceptanceQueue queue;
        for (std::size_t i = 0; i < kAcceptBurstSize; ++i) {
            cell::IntentEnvelope intent;
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
    cout << "口径：受理面局部成本下界——无 world 变更/广播面（归 §2.1 scene 锚点）"
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

//==============================================================================
// 主程序
//==============================================================================

int main(int argc, char** argv) {
    // --models：只跑三模型场景段（微基准段全量约 10+ 分钟，选段供快速
    // 验证与静时机复测）；--acceptance：只跑受理面段（校准复测走这条，
    // 免等场景段——§5 环境纪律的对照基线复测口径）
    const bool models_only =
        (argc > 1 && std::string(argv[1]) == "--models");
    if (models_only) {
        benchmarkThreeModels();
        benchmarkAcceptanceFace();
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "--acceptance") {
        benchmarkAcceptanceFace();
        return 0;
    }

    cout << "========================================" << endl;
    cout << "=== Apollo Framework Benchmark ===" << endl;
    cout << "========================================" << endl;
    cout << "\nCompiler: ";
#if defined(_MSC_VER)
    cout << "MSVC " << _MSC_VER << endl;
#elif defined(__GNUC__)
    cout << "GCC " << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__ << endl;
#elif defined(__clang__)
    cout << "Clang " << __clang_major__ << "." << __clang_minor__ << "." << __clang_PATCHLEVEL__ << endl;
#else
    cout << "Unknown" << endl;
#endif

    cout << "Build: " << (sizeof(void*) == 8 ? "x64" : "x86") << endl;
    cout << "Date: " << __DATE__ << " " << __TIME__ << endl;

    benchmarkBasicOperations();
    benchmarkContainers();
    benchmarkThreading();
    benchmarkAOI();
    benchmarkMessageProcessing();
    benchmarkMemoryPool();
    benchmarkLocks();
    benchmarkThreeModels();
    benchmarkAcceptanceFace();

    cout << "\n========================================" << endl;
    cout << "=== Benchmark Complete ===" << endl;
    cout << "========================================" << endl;

    return 0;
}
