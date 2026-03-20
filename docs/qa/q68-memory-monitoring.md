# Q68: 如何设计内存监控？

## 问题分析

本题考察对内存监控系统的理解：
- 内存分配追踪
- 泄漏检测
- 使用统计
- 报警机制

---

## 一、内存监控系统

### 1.1 系统架构

```
┌─────────────────────────────────────────────────────────────┐
│                    内存监控系统                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  采集层 (Collection Layer):                                 │
│  ├── 内存分配钩子                                           │
│  ├── 释放追踪                                               │
│  ├── 堆采样                                                 │
│  └── 系统内存读取                                           │
│                          │                                  │
│  ▼                                                          │
│  分析层 (Analysis Layer):                                   │
│  ├── 内存分类                                               │
│  ├── 泄漏检测                                               │
│  ├── 碎片分析                                               │
│  └── 趋势计算                                               │
│                          │                                  │
│  ▼                                                          │
│  报警层 (Alert Layer):                                      │
│  ├── 阈值检测                                               │
│  ├── 异常告警                                               │
│  ├── 通知发送                                               │
│  └── 自动响应                                               │
│                          │                                  │
│  ▼                                                          │
│  可视化层 (Visualization Layer):                            │
│  ├── 实时仪表盘                                             │
│  ├── 历史图表                                               │
│  ├── 分配火焰图                                             │
│  └── 泄漏报告                                               │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 监控指标

```
┌─────────────────────────────────────────────────────────────┐
│                    内存监控指标                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  系统级:                                                     │
│  ├── RSS (常驻内存)                                          │
│  ├── VSZ (虚拟内存)                                          │
│  ├── Heap Size                                              │
│  ├── Stack Size                                             │
│  └── Shared Memory                                          │
│                                                             │
│  应用级:                                                     │
│  ├── Total Allocated                                        │
│  ├── Active Allocations                                     │
│  ├── Allocation Rate                                        │
│  ├── Free Rate                                              │
│  └── Object Counts                                          │
│                                                             │
│  泄漏检测:                                                   │
│  ├── Growing Allocations                                    │
│  ├── Leaked Objects                                         │
│  ├── Unused Buffers                                         │
│  └── Cyclic References                                      │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、内存追踪实现

### 2.1 分配追踪器

```cpp
// 内存分配追踪器

class MemoryTracker {
public:
    static MemoryTracker& instance() {
        static MemoryTracker tracker;
        return tracker;
    }

    // 记录分配
    void recordAllocation(void* ptr, size_t size,
                          const char* type, const char* file, int line) {
        std::lock_guard<std::mutex> lock(mutex_);

        AllocationInfo info;
        info.size = size;
        info.type = type;
        info.file = file;
        info.line = line;
        info.timestamp = getCurrentTime();
        info.stackTrace = captureStackTrace();

        allocations_[ptr] = info;

        // 更新统计
        statsByType_[type].count++;
        statsByType_[type].totalSize += size;
        stats_.totalAllocations++;
        stats_.totalAllocated += size;
        stats_.currentAllocated += size;

        // 更新峰值
        if (stats_.currentAllocated > stats_.peakAllocated) {
            stats_.peakAllocated = stats_.currentAllocated;
        }
    }

    // 记录释放
    void recordDeallocation(void* ptr) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = allocations_.find(ptr);
        if (it == allocations_.end()) {
            stats_.invalidFrees++;
            return;
        }

        const auto& info = it->second;

        // 更新统计
        stats_.currentAllocated -= info.size;
        statsByType_[info.type].count--;
        statsByType_[info.type].totalSize -= info.size;
        stats_.totalDeallocations++;

        allocations_.erase(it);
    }

    // 检测泄漏
    void reportLeaks() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!allocations_.empty()) {
            std::cout << "\n=== Memory Leak Report ===\n";
            std::cout << "Leaked allocations: " << allocations_.size() << "\n";
            std::cout << "Total leaked: " << stats_.currentAllocated << " bytes\n\n";

            // 按类型分组
            std::unordered_map<const char*, size_t> leaksByType;
            for (const auto& [ptr, info] : allocations_) {
                leaksByType[info.type] += info.size;
            }

            // 打印泄漏排行
            std::vector<std::pair<const char*, size_t>> sorted(
                leaksByType.begin(), leaksByType.end()
            );
            std::sort(sorted.begin(), sorted.end(),
                [](const auto& a, const auto& b) { return a.second > b.second; });

            for (const auto& [type, size] : sorted) {
                std::cout << "  " << type << ": " << size << " bytes\n";
            }
        }
    }

    // 生成报告
    void printReport() {
        std::lock_guard<std::mutex> lock(mutex_);

        std::cout << "\n=== Memory Report ===\n";
        std::cout << "Total Allocations: " << stats_.totalAllocations << "\n";
        std::cout << "Total Deallocations: " << stats_.totalDeallocations << "\n";
        std::cout << "Current Allocated: " << stats_.currentAllocated << " bytes\n";
        std::cout << "Peak Allocated: " << stats_.peakAllocated << " bytes\n";
        std::cout << "Invalid Frees: " << stats_.invalidFrees << "\n";

        std::cout << "\n--- By Type ---\n";
        for (const auto& [type, stat] : statsByType_) {
            std::cout << type << ":\n";
            std::cout << "  Count: " << stat.count << "\n";
            std::cout << "  Total: " << stat.totalSize << " bytes\n";
        }
    }

private:
    struct AllocationInfo {
        size_t size;
        const char* type;
        const char* file;
        int line;
        uint64_t timestamp;
        std::string stackTrace;
    };

    struct TypeStats {
        size_t count = 0;
        size_t totalSize = 0;
    };

    struct GlobalStats {
        size_t totalAllocations = 0;
        size_t totalDeallocations = 0;
        size_t totalAllocated = 0;
        size_t currentAllocated = 0;
        size_t peakAllocated = 0;
        size_t invalidFrees = 0;
    };

    std::string captureStackTrace() {
        // 实现栈回溯
        return "";
    }

    uint64_t getCurrentTime() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }

    std::mutex mutex_;
    std::unordered_map<void*, AllocationInfo> allocations_;
    std::unordered_map<const char*, TypeStats> statsByType_;
    GlobalStats stats_;
};

// 重载 new/delete
#ifdef ENABLE_MEMORY_TRACKING

inline void* operator new(size_t size, const char* file, int line) {
    void* ptr = malloc(size);
    MemoryTracker::instance().recordAllocation(ptr, size, "unknown", file, line);
    return ptr;
}

inline void operator delete(void* ptr) noexcept {
    if (ptr) {
        MemoryTracker::instance().recordDeallocation(ptr);
        free(ptr);
    }
}

#define DEBUG_NEW new(__FILE__, __LINE__)
#define new DEBUG_NEW

#endif
```

### 2.2 堆采样器

```cpp
// 堆采样分析器

class HeapSampler {
public:
    // 采样内存使用
    void sample() {
        std::lock_guard<std::mutex> lock(mutex_);

        auto snapshot = takeSnapshot();

        if (lastSnapshot_) {
            analyzeGrowth(snapshot);
        }

        lastSnapshot_ = std::move(snapshot);
    }

    struct Snapshot {
        uint64_t timestamp;
        size_t rss;
        size_t heap;
        std::unordered_map<std::string, size_t> byType;
    };

    void printHistory() const {
        std::cout << "\n=== Memory History ===\n";
        for (const auto& snapshot : history_) {
            std::cout << "Time: " << snapshot.timestamp
                      << " RSS: " << snapshot.rss
                      << " Heap: " << snapshot.heap << "\n";
        }
    }

private:
    Snapshot takeSnapshot() {
        Snapshot snapshot;
        snapshot.timestamp = getCurrentTime();
        snapshot.rss = getRSS();
        snapshot.heap = getHeapSize();

        // 获取进程统计
        // 实现取决于平台
        return snapshot;
    }

    void analyzeGrowth(const Snapshot& current) {
        if (!lastSnapshot_) return;

        size_t growth = current.heap - lastSnapshot_->heap;

        if (growth > WARNING_THRESHOLD) {
            std::cout << "WARNING: Memory grew by "
                      << growth << " bytes in "
                      << (current.timestamp - lastSnapshot_->timestamp)
                      << " ms\n";
        }
    }

    static constexpr size_t WARNING_THRESHOLD = 10 * 1024 * 1024;  // 10MB

    uint64_t getCurrentTime() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }

    size_t getRSS() const {
        // 读取 /proc/self/statm (Linux)
        // 或 GetProcessMemoryInfo (Windows)
        return 0;
    }

    size_t getHeapSize() const {
        // 使用 mallinfo 或类似
        return 0;
    }

    std::mutex mutex_;
    std::unique_ptr<Snapshot> lastSnapshot_;
    std::vector<Snapshot> history_;
};
```

---

## 三、泄漏检测

### 3.1 智能指针追踪

```cpp
// 引用计数追踪

template<typename T>
class TrackedPtr {
public:
    explicit TrackedPtr(T* ptr = nullptr)
        : ptr_(ptr), refCount_(new std::atomic<int>(1)) {
        trackAcquire();
    }

    ~TrackedPtr() {
        trackRelease();
        if (--(*refCount_) == 0) {
            delete ptr_;
            delete refCount_;
        }
    }

    TrackedPtr(const TrackedPtr& other)
        : ptr_(other.ptr_), refCount_(other.refCount_) {
        ++(*refCount_);
        trackAcquire();
    }

private:
    void trackAcquire() {
        if (ptr_) {
            MemoryTracker::instance().recordAcquire(
                typeid(T).name(), ptr_
            );
        }
    }

    void trackRelease() {
        if (ptr_) {
            MemoryTracker::instance().recordRelease(
                typeid(T).name(), ptr_
            );
        }
    }

    T* ptr_;
    std::atomic<int>* refCount_;
};
```

---

## 四、报警系统

### 4.1 阈值报警

```cpp
// 内存报警系统

class MemoryAlerter {
public:
    struct Threshold {
        size_t rss;           // RSS 阈值
        size_t heap;          // 堆阈值
        size_t growthRate;    // 增长速率阈值
    };

    void setThreshold(const Threshold& threshold) {
        threshold_ = threshold;
    }

    void check(const HeapSampler::Snapshot& snapshot) {
        // 检查 RSS
        if (snapshot.rss > threshold_.rss) {
            alert("RSS exceeded", snapshot.rss, threshold_.rss);
        }

        // 检查堆
        if (snapshot.heap > threshold_.heap) {
            alert("Heap exceeded", snapshot.heap, threshold_.heap);
        }

        // 检查增长速率
        if (lastSnapshot_) {
            size_t growth = snapshot.heap - lastSnapshot_->heap;
            uint64_t timeDiff = snapshot.timestamp - lastSnapshot_->timestamp;
            size_t rate = growth * 1000 / std::max(timeDiff, uint64_t(1));

            if (rate > threshold_.growthRate) {
                alert("Growth rate exceeded", rate, threshold_.growthRate);
            }
        }

        lastSnapshot_ = snapshot;
    }

private:
    void alert(const std::string& type, size_t value, size_t threshold) {
        std::string msg = "ALERT: " + type +
                         " (value=" + std::to_string(value) +
                         ", threshold=" + std::to_string(threshold) + ")";

        std::cout << msg << "\n";

        // 发送到监控系统
        sendToMonitoring(msg);

        // 记录日志
        logWarning(msg);
    }

    Threshold threshold_;
    std::unique_ptr<HeapSampler::Snapshot> lastSnapshot_;
};
```

---

## 五、可视化

### 5.1 内存火焰图

```bash
# 生成内存分配火焰图

# 1. 使用 perf 采样
perf record -e malloc,malloc_return -p $(pidof server) -g -- sleep 60

# 2. 转换为火焰图格式
perf script | FlameGraph/stackcollapse-perf.pl | \
    sed 's/malloc//g' | \
    FlameGraph/flamegraph.pl --color=mem --title="Memory Allocations" \
    > memory-flamegraph.svg
```

---

## 六、最佳实践

| 实践 | 说明 |
|------|------|
| **定期采样** | 避免性能影响 |
| **生产环境监控** | 实时告警 |
| **泄漏检测** | 定期检查 |
| **趋势分析** | 预测问题 |
| **快照对比** | 找出差异 |

---

## 七、总结

```
内存监控 = 分配追踪 + 泄漏检测 + 阈值告警 + 可视化分析
- 追踪所有分配
- 检测内存泄漏
- 设置合理阈值
- 及时发现问题
```

---

## 参考资料

- [Valgrind Memcheck](https://valgrind.org/docs/manual/mc.html)
- [AddressSanitizer](https://github.com/google/sanitizers/wiki/AddressSanitizer)
- [jemalloc Stats](http://jemalloc.net/jemalloc.3.html)
