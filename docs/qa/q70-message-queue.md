# Q70: 如何设计高性能的消息队列？

## 问题分析

本题考察对消息队列设计的理解：
- 无锁队列实现
- 批量处理
- 优先级队列
- 内存管理

---

## 一、消息队列架构

### 1.1 队列类型

```
┌─────────────────────────────────────────────────────────────┐
│                    消息队列分类                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  无锁队列 (Lock-Free):                                      │
│  ├── 基于原子操作                                           │
│  ├── 无互斥锁                                               │
│  ├── 高并发性能                                             │
│  └── 适用: 生产-消费模式                                     │
│                                                             │
│  有界队列 (Bounded):                                        │
│  ├── 固定容量                                               │
│  ├── 环形缓冲区                                             │
│  ├── 零分配                                                 │
│  └── 适用: 高频消息                                          │
│                                                             │
│  优先级队列 (Priority):                                     │
│  ├── 按优先级排序                                           │
│  ├── 堆实现                                                 │
│  └── 适用: 重要消息优先                                      │
│                                                             │
│  批量队列 (Batch):                                          │
│  ├── 批量处理                                               │
│  ├── 减少锁竞争                                             │
│  └── 适用: 批量操作                                          │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 设计考虑

```
┌─────────────────────────────────────────────────────────────┐
│                    设计权衡                                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  无锁 vs 有锁:                                               │
│  ├── 无锁: 高并发、复杂                                      │
│  └── 有锁: 简单、低并发                                       │
│                                                             │
│  有界 vs 无界:                                               │
│  ├── 有界: 内存可控、可能阻塞                                │
│  └── 无界: 不阻塞、可能 OOM                                  │
│                                                             │
│  单生产者 vs 多生产者:                                       │
│  ├── 单生产者: 简单实现                                      │
│  └── 多生产者: 需要同步                                      │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、无锁队列实现

### 2.1 单生产者单消费者 (SPSC)

```cpp
// 无锁 SPSC 队列

template<typename T, size_t Size>
class SPSCQueue {
public:
    static_assert(Size && !(Size & (Size - 1)), "Size must be power of 2");

    SPSCQueue() : readPos_(0), writePos_(0) {}

    // 生产者调用
    bool push(const T& item) {
        size_t writePos = writePos_.load(std::memory_order_relaxed);
        size_t nextPos = (writePos + 1) & (Size - 1);

        // 检查是否已满
        if (nextPos == readPos_.load(std::memory_order_acquire)) {
            return false;
        }

        buffer_[writePos] = item;

        // 确保数据写入后再更新写指针
        writePos_.store(nextPos, std::memory_order_release);
        return true;
    }

    bool push(T&& item) {
        size_t writePos = writePos_.load(std::memory_order_relaxed);
        size_t nextPos = (writePos + 1) & (Size - 1);

        if (nextPos == readPos_.load(std::memory_order_acquire)) {
            return false;
        }

        buffer_[writePos] = std::move(item);

        writePos_.store(nextPos, std::memory_order_release);
        return true;
    }

    // 消费者调用
    bool pop(T& item) {
        size_t readPos = readPos_.load(std::memory_order_relaxed);

        // 检查是否为空
        if (readPos == writePos_.load(std::memory_order_acquire)) {
            return false;
        }

        item = std::move(buffer_[readPos]);

        // 更新读指针
        readPos_.store((readPos + 1) & (Size - 1), std::memory_order_release);
        return true;
    }

    bool empty() const {
        return readPos_.load(std::memory_order_acquire) ==
               writePos_.load(std::memory_order_acquire);
    }

private:
    std::array<T, Size> buffer_;
    std::atomic<size_t> readPos_;   // 只由消费者修改
    std::atomic<size_t> writePos_;  // 只由生产者修改
};
```

### 2.2 多生产者多消费者 (MPMC)

```cpp
// 无锁 MPMC 队列

template<typename T>
class MPMCQueue {
public:
    explicit MPMCQueue(size_t capacity)
        : capacity_(capacity), mask_(capacity - 1) {
        assert(capacity && !(capacity & (capacity - 1)));  // 必须是 2 的幂

        // 分配对齐的节点数组
        nodes_ = static_cast<Node*>(std::aligned_alloc(
            alignof(Node), sizeof(Node) * capacity
        ));

        for (size_t i = 0; i < capacity; ++i) {
            new (&nodes_[i]) Node();
            nodes_[i].sequence.store(i, std::memory_order_relaxed);
        }

        enqueuePos_.store(0, std::memory_order_relaxed);
        dequeuePos_.store(0, std::memory_order_relaxed);
    }

    ~MPMCQueue() {
        for (size_t i = 0; i < capacity_; ++i) {
            nodes_[i].~Node();
        }
        std::free(nodes_);
    }

    bool push(T&& item) {
        Node* node;

        size_t pos = enqueuePos_.load(std::memory_order_relaxed);
        while (true) {
            node = &nodes_[pos & mask_];
            size_t seq = node->sequence.load(std::memory_order_acquire);

            intptr_t diff = (intptr_t)seq - (intptr_t)pos;

            if (diff == 0) {
                // 尝试获取这个位置
                if (enqueuePos_.compare_exchange_weak(pos, pos + 1,
                        std::memory_order_relaxed,
                        std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                // 队列满
                return false;
            } else {
                pos = enqueuePos_.load(std::memory_order_relaxed);
            }
        }

        // 写入数据
        node->data = std::move(item);

        // 更新序列号，通知消费者
        node->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        Node* node;

        size_t pos = dequeuePos_.load(std::memory_order_relaxed);
        while (true) {
            node = &nodes_[pos & mask_];
            size_t seq = node->sequence.load(std::memory_order_acquire);

            intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);

            if (diff == 0) {
                if (dequeuePos_.compare_exchange_weak(pos, pos + 1,
                        std::memory_order_relaxed,
                        std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                // 队列空
                return false;
            } else {
                pos = dequeuePos_.load(std::memory_order_relaxed);
            }
        }

        // 读取数据
        item = std::move(node->data);

        // 更新序列号，通知生产者
        node->sequence.store(pos + mask_ + 1, std::memory_order_release);
        return true;
    }

private:
    struct Node {
        T data;
        std::atomic<size_t> sequence;
    };

    Node* nodes_;
    size_t capacity_;
    size_t mask_;
    std::atomic<size_t> enqueuePos_;
    std::atomic<size_t> dequeuePos_;
};
```

---

## 三、批量队列

### 3.1 批量处理队列

```cpp
// 批量处理消息队列

template<typename T, size_t BatchSize = 64>
class BatchMessageQueue {
public:
    // 批量弹出消息
    size_t popBatch(std::vector<T>& out) {
        out.clear();
        out.reserve(BatchSize);

        std::unique_lock<std::mutex> lock(mutex_);

        size_t count = std::min({BatchSize, queue_.size()});
        for (size_t i = 0; i < count; ++i) {
            out.push_back(std::move(queue_.front()));
            queue_.pop();
        }

        return count;
    }

    // 批量推入消息
    void pushBatch(const std::vector<T>& items) {
        std::unique_lock<std::mutex> lock(mutex_);

        for (const auto& item : items) {
            queue_.push(item);
        }

        cond_.notify_one();
    }

    void push(const T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_.push(item);
        cond_.notify_one();
    }

private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
};
```

---

## 四、优先级队列

### 4.1 基于堆的优先级队列

```cpp
// 优先级消息队列

template<typename T, typename Priority = int>
class PriorityQueue {
public:
    void push(const T& item, Priority priority) {
        std::lock_guard<std::mutex> lock(mutex_);

        items_.emplace_back(item, priority);
        std::push_heap(items_.begin(), items_.end(), compare_);
    }

    bool pop(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (items_.empty()) {
            return false;
        }

        std::pop_heap(items_.begin(), items_.end(), compare_);
        item = items_.back().item;
        items_.pop_back();

        return true;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return items_.empty();
    }

private:
    struct Item {
        T item;
        Priority priority;
    };

    struct Compare {
        bool operator()(const Item& a, const Item& b) const {
            return a.priority < b.priority;  // 大值优先
        }
    };

    std::vector<Item> items_;
    mutable std::mutex mutex_;
    Compare compare_;
};
```

---

## 五、游戏消息队列

### 5.1 KBEngine 风格消息队列

```cpp
// 游戏服务器消息队列

class GameMessageQueue {
public:
    enum class Priority {
        LOW = 0,
        NORMAL = 1,
        HIGH = 2,
        URGENT = 3
    };

    // 发送消息
    bool send(uint64_t playerId, Message msg, Priority priority = Priority::NORMAL) {
        Envelope envelope;
        envelope.playerId = playerId;
        envelope.message = std::move(msg);
        envelope.timestamp = getCurrentTime();
        envelope.priority = priority;

        return queues_[static_cast<int>(priority)].push(std::move(envelope));
    }

    // 接收消息 (按优先级)
    bool receive(Envelope& out) {
        // 先检查高优先级队列
        for (int i = static_cast<int>(Priority::URGENT); i >= 0; --i) {
            if (queues_[i].pop(out)) {
                return true;
            }
        }
        return false;
    }

    struct Envelope {
        uint64_t playerId;
        Message message;
        uint64_t timestamp;
        Priority priority;
    };

private:
    std::array<MPMCQueue<Envelope>, 4> queues_;
};
```

---

## 六、性能对比

### 6.1 队列实现对比

| 实现 | 吞吐量 | 延迟 | 适用场景 |
|------|--------|------|----------|
| **std::queue + mutex** | 1M ops/s | 低 | 通用 |
| **SPSC 无锁** | 10M ops/s | 极低 | 1:1 通信 |
| **MPMC 无锁** | 5M ops/s | 低 | 多对多 |
| **批量队列** | 8M ops/s | 中 | 批量处理 |

### 6.2 使用建议

```
┌─────────────────────────────────────────────────────────────┐
│                    队列选择指南                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  单生产者单消费者:                                            │
│  └── 使用 SPSC 无锁队列 (最快)                               │
│                                                             │
│  多生产者多消费者:                                            │
│  └── 使用 MPMC 无锁队列                                      │
│                                                             │
│  需要优先级:                                                  │
│  └── 使用多队列 + 优先级轮询                                   │
│                                                             │
│  批量处理:                                                    │
│  └── 使用批量队列                                            │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 七、最佳实践

| 实践 | 说明 |
|------|------|
| **无锁优先** | 高并发场景 |
| **批量处理** | 减少锁竞争 |
| **优先级分离** | 重要消息优先 |
| **容量控制** | 避免无限增长 |
| **监控统计** | 跟踪队列长度 |

---

## 八、总结

```
高性能消息队列 = 无锁设计 + 批量处理 + 优先级 + 监控
- SPSC/MPMC 无锁
- 批量减少操作
- 优先级保证时效
- 容量控制防溢出
```

---

## 参考资料

- [Lock-Free Programming](https://preshing.com/20120612/an-introduction-to-lock-free-programming/)
- [Boost.Lockfree](https://www.boost.org/doc/libs/release/libs/lockfree/)
- [Disruptor Pattern](https://lmax-exchange.github.io/disruptor/)
