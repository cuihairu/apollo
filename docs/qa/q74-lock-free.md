# Q74: 如何设计无锁数据结构？

## 问题分析

本题考察对无锁编程的理解：
- 原子操作
- CAS (Compare-And-Swap)
- ABA 问题
- 常见无锁结构

---

## 一、无锁基础

### 1.1 原子操作

```
┌─────────────────────────────────────────────────────────────┐
│                    原子操作类型                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  内存序 (Memory Order):                                     │
│  ├── relaxed: 无序                                          │
│  ├── acquire: 获取语义 (读屏障)                              │
│  ├── release: 释放语义 (写屏障)                              │
│  ├── acq_rel: 两者兼有                                      │
│  └── seq_cst: 顺序一致性                                    │
│                                                             │
│  原子操作:                                                   │
│  ├── load/store: 读写                                        │
│  ├── exchange: 交换                                         │
│  ├── compare_exchange: CAS                                  │
│  ├── fetch_add: 加法并返回旧值                               │
│  └── fetch_sub: 减法并返回旧值                               │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 CAS 原理

```cpp
// Compare-And-Swap (CAS)

template<typename T>
bool compareAndSwap(std::atomic<T>* ptr, T expected, T desired) {
    // 原子地: 如果 *ptr == expected，则 *ptr = desired
    // 返回是否成功
    return ptr->compare_exchange_weak(expected, desired);
}

// 使用示例: 无锁计数器
class LockFreeCounter {
public:
    void increment() {
        int oldValue = value_.load(std::memory_order_relaxed);
        int newValue;

        do {
            newValue = oldValue + 1;
            // 如果 value_ 仍是 oldValue，则更新为 newValue
            // 否则重试
        } while (!value_.compare_exchange_weak(
            oldValue, newValue,
            std::memory_order_release,
            std::memory_order_relaxed
        ));
    }

    int get() const {
        return value_.load(std::memory_order_acquire);
    }

private:
    std::atomic<int> value_{0};
};
```

---

## 二、ABA 问题

### 2.1 ABA 问题说明

```
┌─────────────────────────────────────────────────────────────┐
│                    ABA 问题                                   │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  初始状态:                                                   │
│  ┌─────────┐                                               │
│  │ Stack   │  Top → A → B → C                              │
│  └─────────┘                                               │
│                                                             │
│  线程 1: pop A                                              │
│  - 读取 Top = A                                             │
│  - (被挂起)                                                 │
│                                                             │
│  线程 2:                                                    │
│  - pop A, pop B                                            │
│  - push D, push A                                          │
│  - 现在 Top → A → D (A 回来了！)                           │
│                                                             │
│  线程 1 恢复:                                               │
│  - CAS(Top, A, B)                                          │
│  - 成功！(因为 Top 还是 A)                                 │
│  - 但 B 已经不在栈中了！                                     │
│  - 结果: B 丢失                                            │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 解决方案: 版本号

```cpp
// 带版本号的指针解决 ABA

template<typename T>
class VersionedPointer {
public:
    struct Pointer {
        T* ptr;
        uint64_t version;
    };

    bool compareAndSwap(Pointer& expected, const Pointer& desired) {
        // CAS 时同时比较指针和版本号
        return value_.compare_exchange_weak(
            expected, desired,
            std::memory_order_release,
            std::memory_order_acquire
        );
    }

private:
    std::atomic<Pointer> value_;
};

// 使用: 无锁栈
template<typename T>
class LockFreeStack {
public:
    void push(T* item) {
        Node* node = new Node{item};

        Node* oldTop = top_.load(std::memory_order_acquire);
        do {
            node->next = oldTop;
        } while (!top_.compare_exchange_weak(
            oldTop, node,
            std::memory_order_release,
            std::memory_order_acquire
        ));
    }

    T* pop() {
        Node* oldTop = top_.load(std::memory_order_acquire);
        Node* newTop;

        do {
            if (oldTop == nullptr) {
                return nullptr;
            }
            newTop = oldTop->next;
        } while (!top_.compare_exchange_weak(
            oldTop, newTop,
            std::memory_order_release,
            std::memory_order_acquire
        ));

        T* item = oldTop->data;

        // 延迟删除 (避免 ABA)
        reclaimLater(oldTop);

        return item;
    }

private:
    struct Node {
        T* data;
        Node* next;
    };

    void reclaimLater(Node* node) {
        // 添加到待删除列表
        // 使用 hazard pointer 或 epoch-based reclamation
    }

    std::atomic<Node*> top_{nullptr};
};
```

---

## 三、无锁队列

### 3.1 MPMC 无锁队列

```cpp
// 多生产者多消费者无锁队列

template<typename T>
class LockFreeQueue {
public:
    explicit LockFreeQueue(size_t capacity) {
        // 容量必须是 2 的幂
        assert(capacity && !(capacity & (capacity - 1)));
        capacity_ = capacity;
        mask_ = capacity - 1;

        // 分配对齐的节点
        nodes_ = static_cast<Node*>(
            aligned_alloc(alignof(Node), sizeof(Node) * capacity)
        );

        for (size_t i = 0; i < capacity; ++i) {
            nodes_[i].sequence.store(i, std::memory_order_relaxed);
        }

        enqueuePos_.store(0, std::memory_order_relaxed);
        dequeuePos_.store(0, std::memory_order_relaxed);
    }

    ~LockFreeQueue() {
        for (size_t i = 0; i < capacity_; ++i) {
            nodes_[i].~Node();
        }
        free(nodes_);
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
                if (enqueuePos_.compare_exchange_weak(
                    pos, pos + 1,
                    std::memory_order_relaxed,
                    std::memory_order_relaxed
                )) {
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

        // 更新序列号
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
                if (dequeuePos_.compare_exchange_weak(
                    pos, pos + 1,
                    std::memory_order_relaxed,
                    std::memory_order_relaxed
                )) {
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

        // 更新序列号
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

## 四、Hazard Pointer

### 4.1 内存回收方案

```cpp
// Hazard Pointer - 解决无锁结构内存回收

class HazardPointer {
public:
    class Holder {
    public:
        Holder(void** ptr) : ptr_(ptr) {
            *ptr_ = nullptr;  // 初始化
        }

        ~Holder() {
            *ptr_ = nullptr;  // 清除
        }

        void protect(void* ptr) {
            *ptr_ = ptr;
        }

    private:
        void** ptr_;
    };

    // 尝试回收节点
    void reclaim(void* node) {
        // 检查是否有 hazard pointer 指向
        for (int i = 0; i < MAX_THREADS; ++i) {
            void* hazard = hazardPointers_[i].load(std::memory_order_acquire);
            if (hazard == node) {
                // 有线程在使用，延迟回收
                toRetract_.push_back(node);
                return;
            }
        }

        // 安全删除
        deleteNode(node);

        // 尝试回收延迟的节点
        retract();
    }

    void* getHazardPointer(int threadId) {
        return &hazardPointers_[threadId % MAX_THREADS];
    }

private:
    void retract() {
        // 定期尝试回收延迟节点
        // ...
    }

    void deleteNode(void* node) {
        // 根据类型删除
        // ...
    }

    static constexpr int MAX_THREADS = 64;
    std::array<std::atomic<void*>, MAX_THREADS> hazardPointers_;
    std::vector<void*> toRetract_;
};
```

---

## 五、无锁数据结构应用

### 5.1 游戏服务器应用

```cpp
// 无锁结构在游戏中的应用

class LockFreeGameServer {
public:
    // 1. 无锁玩家列表
    void addPlayer(Player* player) {
        playerList_.push(player);
    }

    Player* getNextPlayer() {
        Player* player;
        if (playerList_.pop(player)) {
            return player;
        }
        return nullptr;
    }

    // 2. 无锁事件队列
    void postEvent(const Event& event) {
        eventQueue_.push(event);
    }

    void processEvents() {
        Event event;
        while (eventQueue_.pop(event)) {
            handleEvent(event);
        }
    }

    // 3. 无锁引用计数
    void addRef(Entity* entity) {
        entity->refCount_.fetch_add(1, std::memory_order_relaxed);
    }

    void releaseRef(Entity* entity) {
        if (entity->refCount_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            reclaimEntity(entity);
        }
    }

private:
    LockFreeStack<Player> playerList_;
    LockFreeQueue<Event> eventQueue_;
};
```

---

## 六、KBEngine 无锁实践

### 6.1 KBEngine 中的无锁使用

```python
# KBEngine 使用单线程事件循环避免锁

"""
KBEngine 的无锁策略:

1. 每个进程内单线程事件循环:
   - 避免了进程内锁竞争
   - 消息串行处理

2. 进程间异步消息:
   - 无需等待响应
   - 避免死锁

3. 实体邮件箱:
   - 每个实体独立队列
   - 自然并发
"""

# KBEngine 风格实现
class EntityMailbox:
    """实体邮件箱 - 无锁队列"""

    def __init__(self, entityId):
        self.entityId = entityId
        self.messages = []  # 单线程访问，无需锁

    def post(self, message):
        """发送消息 (单线程)"""
        self.messages.append(message)

    def process(self):
        """处理消息 (单线程)"""
        while self.messages:
            msg = self.messages.pop(0)
            self.handleMessage(msg)
```

---

## 七、总结

### 无锁编程要点

```
无锁数据结构 = 原子操作 + CAS + 内存序 + ABA 解决
- 理解内存序
- 注意 ABA 问题
- 使用 Hazard Pointer
- 调试验证复杂度高
- KBEngine 选择 Actor 模型替代
```

---

## 参考资料

- [C++ Concurrency in Action - Lock-Free Data Structures](https://www.manning.com/books/c-plus-plus-concurrency-in-action)
- [Hazard Pointers](https://www.boost.org/doc/libs/release/libs/smart_ptr/doc/hazard_pointer_explained.html)
- [Lock-Free Programming](https://preshing.com/20120612/an-introduction-to-lock-free-programming/)
