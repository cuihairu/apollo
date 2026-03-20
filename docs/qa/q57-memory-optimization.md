# Q57: 如何优化内存使用？

## 问题分析

本题考察对内存优化的理解：
- 内存分配策略
- 内存池技术
- 对象生命周期管理
- 内存泄漏检测
- 缓存优化

---

## 一、内存优化策略

### 1.1 优化层次

```
┌─────────────────────────────────────────────────────────────┐
│                    内存优化层次                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 设计层 (Design Layer)                                   │
│  ├── 避免过度设计                                           │
│  ├── 选择合适的容器                                         │
│  ├── 减少不必要的拷贝                                       │
│  └── 延迟初始化                                             │
│                          │                                  │
│  2. 分配层 (Allocation Layer)                               │
│  ├── 内存池                                                 │
│  ├── 对象池                                                 │
│  ├── 区域分配器 (Arena Allocator)                           │
│  └── 自定义分配器                                           │
│                          │                                  │
│  3. 缓存层 (Cache Layer)                                    │
│  ├── LRU 缓存                                              │
│  ├── 对象复用                                               │
│  ├── 字符串驻留                                             │
│  └── 资源共享                                               │
│                          │                                  │
│  4. 监控层 (Monitoring Layer)                               │
│  ├── 内存统计                                               │
│  ├── 泄漏检测                                               │
│  ├── 性能分析                                               │
│  └── 报警机制                                               │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 内存占用分类

```
┌─────────────────────────────────────────────────────────────┐
│                    内存占用分类                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  固定内存 (Fixed Memory):                                   │
│  ├── 代码段 (Text Segment)                                  │
│  ├── 全局/静态变量                                          │
│  └── 常量数据                                               │
│                                                             │
│  堆内存 (Heap Memory):                                      │
│  ├── 动态分配对象                                           │
│  ├── 容器元素                                               │
│  └── 游戏实体                                               │
│                                                             │
│  栈内存 (Stack Memory):                                     │
│  ├── 局部变量                                               │
│  ├── 函数参数                                               │
│  └── 返回地址                                               │
│                                                             │
│  系统开销 (System Overhead):                                │
│  ├── 内存分配器元数据                                       │
│  ├── 对象对齐填充                                           │
│  └── 虚函数表指针                                           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、内存优化技术

### 2.1 内存池实现

```cpp
// KBEngine 风格的内存池实现
// 参考: src/lib/helpers/objectpool.h

template<typename T, size_t BlockSize = 1024>
class MemoryPool {
public:
    MemoryPool() : head_(nullptr) {
        allocateBlock();
    }

    ~MemoryPool() {
        // 释放所有块
        for (void* block : blocks_) {
            ::free(block);
        }
    }

    // 分配内存
    T* allocate() {
        // 如果没有可用节点，分配新块
        if (!head_) {
            allocateBlock();
        }

        // 从链表取出节点
        Node* node = head_;
        head_ = node->next;

        // 调用构造函数
        T* obj = reinterpret_cast<T*>(node);
        new(obj) T();

        return obj;
    }

    // 释放内存
    void deallocate(T* obj) {
        if (!obj) return;

        // 调用析构函数
        obj->~T();

        // 放回空闲链表
        Node* node = reinterpret_cast<Node*>(obj);
        node->next = head_;
        head_ = node;
    }

    // 获取统计信息
    size_t getTotalBlocks() const { return blocks_.size(); }
    size_t getBlockSize() const { return BlockSize; }

private:
    struct Node {
        Node* next;
    };

    void allocateBlock() {
        // 分配新块
        void* block = ::malloc(sizeof(T) * BlockSize);
        blocks_.push_back(block);

        // 构建空闲链表
        char* p = static_cast<char*>(block);
        for (size_t i = 0; i < BlockSize; ++i) {
            Node* node = reinterpret_cast<Node*>(p + i * sizeof(T));
            node->next = head_;
            head_ = node;
        }
    }

    Node* head_;
    std::vector<void*> blocks_;
};
```

### 2.2 区域分配器 (Arena Allocator)

```cpp
// 区域分配器 - 适合临时对象分配

class ArenaAllocator {
public:
    ArenaAllocator(size_t initialSize = 4096)
        : head_(nullptr), current_(nullptr), remaining_(0) {
        allocateBlock(initialSize);
    }

    ~ArenaAllocator() {
        for (Block* block : blocks_) {
            ::free(block->data);
            delete block;
        }
    }

    // 分配内存 (不调用构造函数)
    void* allocate(size_t size, size_t alignment = 8) {
        // 对齐
        uintptr_t current = reinterpret_cast<uintptr_t>(current_);
        uintptr_t aligned = (current + alignment - 1) & ~(alignment - 1);
        size_t padding = aligned - current;

        // 检查是否有足够空间
        if (padding + size > remaining_) {
            // 分配新块 (按需扩容)
            size_t newSize = blocks_.empty() ? 4096 : remaining_ * 2;
            while (newSize < size) {
                newSize *= 2;
            }
            allocateBlock(newSize);

            current = reinterpret_cast<uintptr_t>(current_);
            aligned = current;
            padding = 0;
        }

        // 分配
        void* ptr = reinterpret_cast<void*>(aligned);
        current_ = static_cast<char*>(current_) + padding + size;
        remaining_ -= padding + size;

        return ptr;
    }

    // 重置 (释放所有分配，但保留内存块)
    void reset() {
        if (blocks_.empty()) return;

        current_ = blocks_[0]->data;
        remaining_ = blocks_[0]->size;
    }

private:
    struct Block {
        char* data;
        size_t size;
    };

    void allocateBlock(size_t size) {
        char* data = static_cast<char*>(::malloc(size));
        Block* block = new Block{data, size};
        blocks_.push_back(block);

        current_ = data;
        remaining_ = size;
    }

    std::vector<Block*> blocks_;
    char* current_;
    size_t remaining_;
};
```

### 2.3 字符串驻留

```cpp
// 字符串驻留池 - 减少重复字符串内存

class StringInterner {
public:
    // 驻留字符串
    const std::string* intern(const std::string& str) {
        auto it = string_set_.find(str);
        if (it != string_set_.end()) {
            return &*it;
        }

        auto [it2, _] = string_set_.insert(str);
        return &*it2;
    }

    const std::string* intern(const char* str) {
        return intern(std::string(str));
    }

    // 获取统计信息
    size_t getUniqueCount() const { return string_set_.size(); }
    size_t getTotalSaved() const {
        size_t total = 0;
        for (const auto& str : string_set_) {
            total += str.capacity();
        }
        return total;
    }

    // 清理不用的字符串 (引用计数版本)
    void garbageCollect() {
        // 需要配合引用计数实现
    }

private:
    // 使用 unordered_set 存储唯一字符串
    std::unordered_set<std::string> string_set_;
};
```

---

## 三、KBEngine 内存管理

### 3.1 KBEngine 对象池

KBEngine 使用了多种对象池技术：

```python
# KBEngine Python 脚本中的对象池模式
# scripts/common/object_pool.py

class ObjectPool:
    """KBEngine 对象池实现"""

    def __init__(self, factory, initSize=10):
        self.factory = factory
        self.pool = []
        self.lock = threading.Lock()

        # 预分配对象
        for _ in range(initSize):
            self.pool.append(self.factory())

    def acquire(self):
        """获取对象"""
        with self.lock:
            if self.pool:
                return self.pool.pop()
            return self.factory()

    def release(self, obj):
        """归还对象"""
        with self.lock:
            self.pool.append(obj)

    def clear(self):
        """清空对象池"""
        with self.lock:
            self.pool.clear()
```

### 3.2 KBEngine 内存分配器

```cpp
// KBEngine C++ 内存分配器
// src/lib/helpers/allocator.h

namespace KBEngine {

// 固定大小分配器
template<size_t SIZE, size_t ALIGNMENT = 8>
class FixedAllocator {
public:
    void* allocate() {
        if (freeList_) {
            void* ptr = freeList_;
            freeList_ = *static_cast<void**>(freeList_);
            return ptr;
        }

        // 从系统分配
        return ::malloc(SIZE);
    }

    void deallocate(void* ptr) {
        if (!ptr) return;

        // 放回空闲列表
        *static_cast<void**>(ptr) = freeList_;
        freeList_ = ptr;
    }

private:
    void* freeList_ = nullptr;
};

// 小对象分配器 (适配不同大小)
class SmallObjectAllocator {
public:
    static constexpr size_t SIZE_THRESHOLDS[] = {
        8, 16, 32, 64, 128, 256, 512, 1024
    };

    void* allocate(size_t size) {
        // 找到合适的分配器
        for (size_t threshold : SIZE_THRESHOLDS) {
            if (size <= threshold) {
                return getAllocator(threshold)->allocate();
            }
        }

        // 大对象直接分配
        return ::malloc(size);
    }

    void deallocate(void* ptr, size_t size) {
        for (size_t threshold : SIZE_THRESHOLDS) {
            if (size <= threshold) {
                getAllocator(threshold)->deallocate(ptr);
                return;
            }
        }

        ::free(ptr);
    }

private:
    std::array<std::unique_ptr<FixedAllocator<8>>, 8> allocators_;
};

} // namespace KBEngine
```

---

## 四、内存泄漏检测

### 4.1 引用计数追踪

```cpp
// 引用计数追踪器

class RefCounted {
public:
    RefCounted() : refCount_(0) {
        DEBUG_ADD_REF(this);
    }

    virtual ~RefCounted() {
        if (refCount_ > 0) {
            ERROR("Object deleted with non-zero ref count: {}", refCount_);
        }
        DEBUG_REMOVE_REF(this);
    }

    void addRef() {
        ++refCount_;
    }

    void release() {
        if (--refCount_ == 0) {
            delete this;
        }
    }

    int getRefCount() const { return refCount_; }

private:
    std::atomic<int> refCount_;
};

// 智能指针包装
template<typename T>
class SmartPtr {
public:
    SmartPtr() : ptr_(nullptr) {}

    SmartPtr(T* ptr) : ptr_(ptr) {
        if (ptr_) ptr_->addRef();
    }

    SmartPtr(const SmartPtr& other) : ptr_(other.ptr_) {
        if (ptr_) ptr_->addRef();
    }

    ~SmartPtr() {
        if (ptr_) ptr_->release();
    }

    SmartPtr& operator=(const SmartPtr& other) {
        if (other.ptr_) other.ptr_->addRef();
        if (ptr_) ptr_->release();
        ptr_ = other.ptr_;
        return *this;
    }

    T* operator->() { return ptr_; }
    const T* operator->() const { return ptr_; }

    T* get() { return ptr_; }

private:
    T* ptr_;
};
```

### 4.2 内存统计

```cpp
// 内存统计系统

class MemoryStats {
public:
    static MemoryStats& instance() {
        static MemoryStats inst;
        return inst;
    }

    // 记录分配
    void recordAllocation(const char* type, size_t size, void* ptr) {
        std::lock_guard<std::mutex> lock(mutex_);

        allocations_[ptr] = {type, size};
        statsByType_[type].count++;
        statsByType_[type].totalSize += size;

        totalAllocations_++;
        totalAllocated_ += size;

        // 峰值统计
        if (currentAllocated_ > peakAllocated_) {
            peakAllocated_ = currentAllocated_;
        }
        currentAllocated_ += size;
    }

    // 记录释放
    void recordDeallocation(void* ptr) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = allocations_.find(ptr);
        if (it != allocations_.end()) {
            const auto& info = it->second;
            statsByType_[info.type].count--;
            statsByType_[info.type].totalSize -= info.size;

            currentAllocated_ -= info.size;
            allocations_.erase(it);
        }
    }

    // 获取统计报告
    void report() {
        INFO("=== Memory Statistics ===");
        INFO("Total Allocations: {}", totalAllocations_);
        INFO("Current Allocated: {} bytes", currentAllocated_);
        INFO("Peak Allocated: {} bytes", peakAllocated_);

        INFO("\n--- By Type ---");
        for (const auto& [type, stat] : statsByType_) {
            INFO("{}: {} allocations, {} bytes",
                 type, stat.count, stat.totalSize);
        }

        // 检测泄漏
        if (!allocations_.empty()) {
            WARN("\n--- Potential Leaks ---");
            for (const auto& [ptr, info] : allocations_) {
                WARN("Leaked: {} at {} ({} bytes)", info.type, ptr, info.size);
            }
        }
    }

private:
    struct AllocationInfo {
        const char* type;
        size_t size;
    };

    struct TypeStats {
        size_t count = 0;
        size_t totalSize = 0;
    };

    std::mutex mutex_;
    std::unordered_map<void*, AllocationInfo> allocations_;
    std::unordered_map<const char*, TypeStats> statsByType_;

    size_t totalAllocations_ = 0;
    size_t totalAllocated_ = 0;
    size_t currentAllocated_ = 0;
    size_t peakAllocated_ = 0;
};

// 内存追踪宏
#define DEBUG_NEW new(__FILE__, __LINE__)
#define DEBUG_DELETE delete
```

---

## 五、优化对比

### 5.1 内存分配方式对比

| 方式 | 优点 | 缺点 | 适用场景 |
|------|------|------|----------|
| **系统 malloc** | 简单、通用 | 慢、碎片化 | 大对象、不频繁分配 |
| **内存池** | 快、无碎片 | 预分配开销 | 固定大小对象 |
| **对象池** | 复用对象、少分配 | 需要重置状态 | 短生命周期对象 |
| **Arena** | 极快、批量释放 | 无法单独释放 | 临时对象、帧数据 |
| **智能指针** | 自动管理 | 有性能开销 | 共享所有权 |

### 5.2 容器选择对比

| 容器 | 内存开销 | 随机访问 | 插入删除 | 适用场景 |
|------|----------|----------|----------|----------|
| **vector** | 低 | O(1) | 尾部 O(1) | 连续存储、顺序访问 |
| **deque** | 中 | O(1) | 头尾 O(1) | 两端操作 |
| **list** | 高 (指针) | O(n) | O(1) | 频繁中间插入 |
| **map** | 高 (树节点) | O(log n) | O(log n) | 有序查找 |
| **unordered_map** | 高 (桶) | O(1) 均摊 | O(1) 均摊 | 快速查找 |

---

## 六、最佳实践

### 6.1 内存优化建议

| 实践 | 说明 | 示例 |
|------|------|------|
| **预分配** | 避免频繁分配 | `vector.reserve(n)` |
| **对象池** | 复用对象 | 子弹、特效 |
| **延迟初始化** | 按需创建 | `std::optional` |
| **移动语义** | 避免拷贝 | `std::move` |
| **小对象优化** | 内联分配 | SBO (Small Buffer Optimization) |
| **字符串视图** | 避免拷贝 | `std::string_view` |
| **紧凑布局** | 减少填充 | 按大小排序成员 |
| **位域** | 压缩布尔值 | `bool flags : 1;` |

### 6.2 KBEngine 内存优化技巧

```cpp
// 1. 使用共享指针减少拷贝
// KBEngine 大量使用共享指针管理实体生命周期

// 2. 消息包内存复用
// src/lib/network/bundle.h
// Bundle 使用固定大小缓冲区，避免频繁分配

// 3. 定时器内存池
// KBEngine 的定时器系统使用对象池

// 4. 实体属性延迟加载
// EntityDef 属性按需初始化
```

---

## 七、总结

### 内存优化核心

```
内存优化 = 减少分配 + 复用内存 + 及时释放 + 持续监控
- 内存池/对象池减少分配开销
- 智能指针/引用计数管理生命周期
- 内存监控检测泄漏
- 选择合适的容器和数据结构
```

---

## 参考资料

- [KBEngine GitHub - Memory Management](https://github.com/kbengine/kbengine/tree/master/src/lib/helpers)
- [Game Engine Architecture - Memory](https://www.gameenginebook.com/)
- [C++ Memory Management Best Practices](https://en.cppreference.com/w/cpp/memory)
