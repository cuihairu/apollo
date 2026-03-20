# Q65: 如何减少 CPU 缓存未命中？

## 问题分析

本题考察对 CPU 缓存优化的理解：
- CPU 缓存层次结构
- 缓存行 (Cache Line)
- 数据局部性
- 内存对齐
- 伪共享问题

---

## 一、CPU 缓存基础

### 1.1 缓存层次结构

```
┌─────────────────────────────────────────────────────────────┐
│                    CPU 缓存层次                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  L1 Cache (一级缓存):                                        │
│  ├── 大小: 32-64 KB per core                               │
│  ├── 延迟: ~4 cycles                                        │
│  └── 作用: 最热数据                                         │
│                          │                                  │
│  L2 Cache (二级缓存):                                        │
│  ├── 大小: 256-512 KB per core                             │
│  ├── 延迟: ~12 cycles                                       │
│  └── 作用: 热数据                                           │
│                          │                                  │
│  L3 Cache (三级缓存):                                        │
│  ├── 大小: 8-32 MB 共享                                     │
│  ├── 延迟: ~40 cycles                                       │
│  └── 作用: 共享数据                                         │
│                          │                                  │
│  主内存 (RAM):                                              │
│  ├── 大小: GB 级                                            │
│  ├── 延迟: ~200 cycles                                      │
│  └── 作用: 所有数据                                         │
│                                                             │
└─────────────────────────────────────────────────────────────┘

性能对比:
L1 命中: ~4 cycles
L2 命中: ~12 cycles
L3 命中: ~40 cycles
内存读取: ~200 cycles  (慢 50 倍！)
```

### 1.2 缓存行 (Cache Line)

```
┌─────────────────────────────────────────────────────────────┐
│                    缓存行概念                                  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  CPU 缓存以缓存行为单位传输数据:                              │
│  ├── 常见大小: 64 字节                                       │
│  ├── 对齐要求: 数据起始地址应为 64 的倍数                     │
│  └── 伪共享: 多个核心写同一缓存行导致性能下降                  │
│                                                             │
│  示例:                                                      │
│  ┌─────────────────────────────────────────────────┐       │
│  │ Cache Line (64 bytes)                            │       │
│  │ ┌────┬────┬────┬────┬────┬────┬────┬────┐        │       │
│  │ │ A  │ B  │ C  │ D  │ ...                    │        │       │
│  │ │ 8B │ 8B │ 8B │ 8B │                         │        │       │
│  │ └────┴────┴────┴────┴────────────────────────┘        │       │
│  │                                                         │       │
│  │ 读取 A 时，整个缓存行 (64B) 被加载                       │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、优化技术

### 2.1 数据布局优化

```cpp
// ❌ 不好: 跨缓存行访问
struct BadLayout {
    int id;          // 4 bytes
    char name[60];   // 60 bytes
    int value;       // 4 bytes - 可能跨越缓存行
};
// sizeof = 68，跨越两个缓存行

// ✅ 好: 缓存行对齐
struct GoodLayout {
    int id;
    int value;
    char name[60];
};
// sizeof = 68，但 id 和 value 在同一缓存行

// ✅ 更好: 显式对齐
struct AlignedLayout {
    int id;
    int value;
    char name[60];
} __attribute__((aligned(64)));  // GCC/Clang
// struct alignas(64) AlignedLayout {};  // C++11
```

### 2.2 避免伪共享

```cpp
// ❌ 伪共享示例
struct Counter {
    std::atomic<int> value;
};

Counter counters[8];  // 可能在同一缓存行

// 多线程竞争
void threadFunc(int index) {
    for (int i = 0; i < 1000000; ++i) {
        counters[index].value++;  // 导致缓存行失效
    }
}

// ✅ 解决方案: 缓存行填充
struct PaddedCounter {
    std::atomic<int> value;
    char padding[64 - sizeof(std::atomic<int>)];
};

PaddedCounter counters[8];  // 每个元素独立缓存行

// ✅ C++17 方式
struct alignas(64) AlignedCounter {
    std::atomic<int> value;
};
```

### 2.3 数据局部性

```cpp
// ❌ 不好: 随机访问
void sumRandom(const std::vector<int>& data, const std::vector<size_t>& indices) {
    int sum = 0;
    for (size_t idx : indices) {
        sum += data[idx];  // 随机访问，缓存未命中
    }
}

// ✅ 好: 顺序访问
void sumSequential(const std::vector<int>& data) {
    int sum = 0;
    for (int value : data) {
        sum += value;  // 顺序访问，预取友好
    }
}

// ✅ 分块处理 (提高空间局部性)
void matrixMultiplyBlocked(const float* A, const float* B, float* C, int n) {
    const int BLOCK = 64;  // 适合 L1 缓存

    for (int i = 0; i < n; i += BLOCK) {
        for (int j = 0; j < n; j += BLOCK) {
            for (int k = 0; k < n; k += BLOCK) {
                // 处理一个块
                for (int ii = i; ii < i + BLOCK; ++ii) {
                    for (int jj = j; jj < j + BLOCK; ++jj) {
                        float sum = 0;
                        for (int kk = k; kk < k + BLOCK; ++kk) {
                            sum += A[ii * n + kk] * B[kk * n + jj];
                        }
                        C[ii * n + jj] += sum;
                    }
                }
            }
        }
    }
}
```

---

## 三、游戏引擎优化

### 3.1 实体数据布局

```cpp
// 游戏实体优化

// ❌ AoS (Array of Structures) - 缓存不友好
struct EntityAoS {
    float position[3];    // 12 bytes
    float velocity[3];    // 12 bytes
    float health;         // 4 bytes
    float mana;           // 4 bytes
    int flags;            // 4 bytes
};

std::vector<EntityAoS> entities;

void updatePositionsAoS(std::vector<EntityAoS>& entities, float dt) {
    for (auto& e : entities) {
        e.position[0] += e.velocity[0] * dt;
        e.position[1] += e.velocity[1] * dt;
        e.position[2] += e.velocity[2] * dt;
    }
    // 每次迭代加载: position + velocity + health + mana + flags
    // 缓存行浪费
}

// ✅ SoA (Structure of Arrays) - 缓存友好
struct EntitiesSoA {
    std::vector<float> posX, posY, posZ;
    std::vector<float> velX, velY, velZ;
    std::vector<float> health;
    std::vector<float> mana;
    std::vector<int> flags;
};

void updatePositionsSoA(EntitiesSoA& entities, float dt) {
    size_t n = entities.posX.size();
    for (size_t i = 0; i < n; ++i) {
        entities.posX[i] += entities.velX[i] * dt;
        entities.posY[i] += entities.velY[i] * dt;
        entities.posZ[i] += entities.velZ[i] * dt;
    }
    // 顺序访问，预取高效
    // 只加载需要的数组
}
```

### 3.2 热数据和冷数据分离

```cpp
// ✅ 分离热冷数据

// 热数据 (每帧访问)
struct EntityHot {
    float position[3];
    float velocity[3];
    int health;
};

// 冷数据 (偶尔访问)
struct EntityCold {
    std::string name;
    std::vector<Item> inventory;
    QuestData quests;
    GuildData guild;
};

// 实体只持有热数据
class Entity {
public:
    EntityHot hot;  // 紧凑，缓存友好

    // 冷数据通过 ID 按需加载
    uint64_t getColdDataId() const { return coldId_; }
    EntityCold loadColdData() const;

private:
    uint64_t coldId_;
};
```

---

## 四、预取优化

### 4.1 软件预取

```cpp
// 软件预取示例

#include <xmmintrin.h>  // _mm_prefetch

void processWithPrefetch(const Item* items, size_t count) {
    const size_t PREFETCH_DISTANCE = 8;

    for (size_t i = 0; i < count; ++i) {
        // 预取未来数据
        if (i + PREFETCH_DISTANCE < count) {
            _mm_prefetch((const char*)&items[i + PREFETCH_DISTANCE],
                        _MM_HINT_T0);  // 预取到 L1
        }

        // 处理当前数据
        processItem(items[i]);
    }
}

// 链表预取
void traverseListWithPrefetch(const Node* head) {
    const Node* curr = head;
    const Node* prefetch = head;

    // 先预取几个节点
    for (int i = 0; i < 4 && prefetch; ++i) {
        _mm_prefetch((const char*)prefetch->next, _MM_HINT_T0);
        prefetch = prefetch->next;
    }

    while (curr) {
        processNode(curr);

        // 预取更远的节点
        if (prefetch) {
            _mm_prefetch((const char*)prefetch->next, _MM_HINT_T0);
            prefetch = prefetch->next;
        }

        curr = curr->next;
    }
}
```

---

## 五、优化效果

### 5.1 优化对比

| 场景 | 优化前 | 优化后 | 提升 |
|------|--------|--------|------|
| **顺序遍历** | 100 ms | 20 ms | 5x |
| **SoA vs AoS** | 150 ms | 40 ms | 3.75x |
| **避免伪共享** | 80 ms | 15 ms | 5.3x |
| **分块矩阵** | 500 ms | 100 ms | 5x |

### 5.2 缓存命中率

```
L1 缓存命中率目标:
- > 90%: 优秀
- 70-90%: 良好
- < 70%: 需要优化

测量工具:
- Linux: perf stat -e cache-references,cache-misses
- VTune: Hardware Event Counts
```

---

## 六、最佳实践

| 实践 | 说明 |
|------|------|
| **顺序访问** | 利用预取器 |
| **SoA 布局** | 提高局部性 |
| **缓存行对齐** | 避免跨越边界 |
| **避免伪共享** | 使用填充 |
| **热冷分离** | 减少工作集 |

---

## 七、总结

```
缓存优化 = 数据布局 + 访问模式 + 避免伪共享 + 预取
- 顺序访问优先
- 相关数据聚集
- 避免竞争缓存行
- 预取未来数据
```

---

## 参考资料

- [Intel Optimization Manual](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
- [Cache Optimization](https://www.agner.org/optimize/optimizing_cpp.pdf)
- [KBEngine Performance](https://kbengine.github.io/docs/)
