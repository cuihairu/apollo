/**
 * @file benchmark_scene.cpp
 * @brief 场景对象基准段（AOI 模拟 + 消息处理编码）
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

