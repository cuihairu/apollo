# Q59: 如何减少锁竞争？

## 问题分析

本题考察对并发控制和锁优化的理解：
- 锁竞争的原因和影响
- 无锁编程技术
- Actor 模型在游戏服务器中的应用
- KBEngine 的并发设计

---

## 一、锁竞争问题

### 1.1 锁竞争的危害

```
┌─────────────────────────────────────────────────────────────┐
│                    锁竞争的危害                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  场景：多线程竞争同一把锁                                 │
│                                                             │
│  线程 1 ──┐                                              │
│  线程 2 ──┤                                              │
│  线程 3 ──┤  争夺锁                                       │
│  线程 4 ──┤                                              │
│  线程 5 ──┘                                              │
│         │                                                │
│         ▼                                                │
│  ┌─────────┐                                              │
│  │  锁     │ 只有 1 个线程能获得锁                         │
│  └─────────┘ 其他线程等待                                │
│                                                             │
│  问题：                                                    │
│  ├── CPU 浪费 - 等待的线程空转                            │
│  ├── 串行执行 - 并发能力下降                              │
│  ├── 上下文切换 - 频繁切换开销                             │
│  └── 死锁风险 - 设计不当会死锁                            │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 锁竞争检测

```cpp
// ThreadSanitizer 检测锁竞争

// 编译选项
g++ -fsanitize=thread -g -O2 game-server.cpp -o game-server

// 运行时会检测数据竞争
./game-server

// 输出示例:
# WARNING: ThreadSanitizer: data race on addr
#   Read of size 4 at 0x7f1234567890 by thread T1:
#     #0 0x7f1234567890 in operator int
#   Previous write of size 4 at 0x7f1234567890 by thread T2:
#     #1 0x7f1234567890 in operator int
```

---

## 二、减少锁竞争的方法

### 2.1 减少锁的范围

```cpp
// 不好的做法：大粒度锁

class EntityManager {
    std::mutex mutex_;  // 整个实体管理器一把锁
    std::unordered_map<EntityID, Entity*> entities_;

    Entity* getEntity(EntityID id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entities_.find(id);
        return it != entities_.end() ? it->second : nullptr;
    }

    void addEntity(Entity* entity) {
        std::lock_guard<std::mutex> lock(mutex_);
        entities_[entity->id()] = entity;
    }
};

// 问题：每次操作都要锁住整个 map
```

```cpp
// 好的做法：细粒度锁

class EntityManager {
    std::array<std::shared_mutex, 256> mutexes_;  // 分段锁

    Entity* getEntity(EntityID id) {
        size_t bucket = id % mutexes_.size();
        std::shared_lock<std::shared_mutex> lock(mutexes_[bucket]);

        auto it = entities_[bucket].find(id);
        return it != entities_[bucket].end() ? it->second : nullptr;
    }

    void addEntity(Entity* entity) {
        size_t bucket = entity->id() % mutexes_.size();
        std::unique_lock<std::shared_mutex> lock(mutexes_[bucket]);

        entities_[bucket][entity->id()] = entity;
    }

private:
    std::array<std::unordered_map<EntityID, Entity*>, 256> entities_;
};
```

### 2.2 读写锁

```cpp
// 读写锁使用

class ConfigManager {
    mutable std::shared_mutex mutex_;
    std::string config_;

    // 读取操作 - 可以并发
    std::string getConfig() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return config_;
    }

    // 写入操作 - 独占
    void setConfig(const std::string& config) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        config_ = config;
    }

    // 多个读线程可以同时访问
    // 但写线程会阻塞所有读写
};
```

---

## 三、无锁编程

### 3.1 原子操作

```cpp
// 原子操作示例

#include <atomic>

class LockFreeCounter {
public:
    LockFreeCounter() : value_(0) {}

    // 原子递增
    uint64_t increment() {
        return value_.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    // 原子读取
    uint64_t get() const {
        return value_.load(std::memory_order_relaxed);
    }

    // CAS (Compare-And-Swap)
    bool compareExchange(uint64_t& expected, uint64_t desired) {
        return value_.compare_exchange_weak(
            expected,
            desired,
            std::memory_order_acq_rel
        );
    }

private:
    std::atomic<uint64_t> value_;
};
```

### 3.2 无锁队列

```cpp
// 无锁队列 (单生产者单消费者)

template<typename T>
class LockFreeQueue {
public:
    LockFreeQueue(size_t capacity)
        : buffer_(new Node[capacity]), capacity_(capacity) {
        // 初始化哨兵节点
        Node* sentinel = new Node();
        head_.store(sentinel);
        tail_.store(sentinel);
    }

    void enqueue(T value) {
        Node* node = new Node(value);

        // 将新节点链接到尾部
        Node* prev = tail_.load(std::memory_order_relaxed);
        prev->next_.store(node, std::memory_order_release);

        // 更新尾部
        tail_.store(node, std::memory_order_release);
    }

    bool dequeue(T& outValue) {
        Node* head = head_.load(std::memory_order_acquire);
        Node* next = head->next_.load(std::memory_order_acquire);

        if (next == nullptr) {
            return false;  // 队列空
        }

        // 更新头部
        head_.store(next, std::memory_order_release);

        outValue = next->value;
        delete head;

        return true;
    }

private:
    struct Node {
        T value;
        std::atomic<Node*> next;
    };

    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;
    Node* buffer_;  // 用于内存管理
    size_t capacity_;
};
```

### 3.3 RCU (Read-Copy-Update)

```cpp
// RCU 模式：读无锁，写复制的无锁数据结构

template<typename T>
class RCUList {
public:
    // 读取 - 无锁
    T* find(std::function<bool(const T&)> predicate) {
        VersionedList* list = getReadableList();

        for (const auto& item : list->items) {
            if (predicate(item.data)) {
                return item.data;
            }
        }
        return nullptr;
    }

    // 写入 - 复制修改
    void add(const T& item) {
        auto* newList = new VersionedList();

        // 复制旧数据
        VersionedList* oldList = getReadableList();
        for (const auto& i : oldList->items) {
            newList->items.push_back({
                i.data, i.data  // 深拷贝
            });
        }

        // 添加新项
        newList->items.push_back({item});

        // 发布新版本
        publishList(newList);

        // 旧版本会在后续被回收
    }

private:
    struct VersionedList {
        std::vector<RCUItem> items;
        uint64_t version;
    };

    std::atomic<VersionedList*> currentList_;
    std::vector<VersionedList*> oldLists_;
};
```

---

## 四、Actor 模型

### 4.1 Actor 模型原理

```
┌─────────────────────────────────────────────────────────────┐
│                    Actor 模型                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  传统共享内存并发：                                         │
│  ┌─────────────────────────────────────────────────┐       │
│  │  线程1 ──┐                                        │       │
│  │  线程2 ──┼── 共享内存 ─ 锁竞争                     │       │
│  │  线程3 ──┘                                        │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  Actor 模型 (消息传递)：                                    │
│  ┌─────────────────────────────────────────────────┐       │
│  │  Actor1      Actor2      Actor3                    │       │
│  │    │            │           │                    │       │
│  │    └────────────┴───────────┘                    │       │
│  │           消息队列 (Mailbox)                      │       │
│  │                                                   │       │
│  │  特点：                                            │       │
│  │  ├── 每个 Actor 有独立队列                         │       │
│  │  ├── 串行处理消息                                  │       │
│  │  ├── 无锁竞争                                      │       │
│  │  └── 易于扩展                                      │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 Actor 实现

```cpp
// Actor 模型实现

class Actor {
public:
    // 发送消息到 Actor
    template<typename T>
    void send(EntityID targetId, const T& message) {
        Actor* target = getActor(targetId);
        if (target) {
            target->receive(message);
        }
    }

    // 接收消息
    template<typename T>
    void receive(const T& message) {
        // 将消息加入队列
        std::unique_lock<std::mutex> lock(queueMutex_);
        messageQueue_.push(Message::create(message));
    }

    // 处理消息（在 Actor 线程中调用）
    void processMessages() {
        while (running_) {
            Message* msg = nullptr;

            {
                std::unique_lock<std::mutex> lock(queueMutex_);
                if (!messageQueue_.empty()) {
                    msg = messageQueue_.front();
                    messageQueue_.pop();
                }
            }

            if (msg) {
                handleMessage(msg);
                delete msg;
            } else {
                // 队列空，等待
                std::unique_lock<std::mutex> lock(queueMutex_);
                queueCV_.wait(lock);
            }
        }
    }

protected:
    // 子类实现消息处理
    virtual void handleMessage(Message* msg) = 0;

private:
    std::mutex queueMutex_;
    std::condition_variable queueCV_;
    std::queue<Message*> messageQueue_;
    bool running_ = true;
};
```

### 4.3 Actor 线程池

```cpp
// Actor 线程池

class ActorSystem {
public:
    ActorSystem(size_t numThreads = std::thread::hardware_concurrency())
        : running_(true) {

        // 创建工作线程
        for (size_t i = 0; i < numThreads; ++i) {
            workers_.emplace_back([this]() {
                workerThread();
            });
        }
    }

    ~ActorSystem() {
        running_ = false;
        for (auto& thread : workers_) {
            thread.join();
        }
    }

    // 注册 Actor
    void registerActor(EntityID id, Actor* actor) {
        std::lock_guard<std::mutex> lock(actorsMutex_);
        actors_[id] = actor;
    }

    // 分配消息到 Actor
    void dispatch(EntityID targetId, Message* msg) {
        Actor* actor = getActor(targetId);
        if (actor) {
            actor->receive(msg);
            notifyActor(targetId);  // 唤醒 Actor 线程
        }
    }

private:
    void workerThread() {
        while (running_) {
            // 等待有消息的 Actor
            std::unique_lock<std::mutex> lock(readyMutex_);
            readyCV_.wait(lock, [this] {
                return !readyActors_.empty() || !running_;
            });

            if (!running_) break;

            // 处理所有就绪的 Actor
            for (EntityID actorId : readyActors_) {
                Actor* actor = getActor(actorId);
                if (actor) {
                    actor->processMessages();
                }
            }

            readyActors_.clear();
        }
    }

    void notifyActor(EntityID actorId) {
        std::lock_guard<std::mutex> lock(readyMutex_);
        readyActors_.insert(actorId);
        readyCV_.notify_one();
    }

    Actor* getActor(EntityID id) {
        std::lock_guard<std::mutex> lock(actorsMutex_);
        auto it = actors_.find(id);
        return it != actors_.end() ? it->second : nullptr;
    }

    std::unordered_map<EntityID, Actor*> actors_;
    std::mutex actorsMutex_;

    std::unordered_set<EntityID> readyActors_;
    std::mutex readyMutex_;
    std::condition_variable readyCV_;

    std::vector<std::thread> workers_;
    bool running_;
};
```

---

## 五、KBEngine 并发设计

### 5.1 KBEngine 单线程模型

```
┌─────────────────────────────────────────────────────────────┐
│                  KBEngine 单线程设计                           │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  设计理念：                                                │
│  ├── 每个 App 单线程处理消息                              │
│  ├── 通过事件驱动 (poller)                               │
│  ├── 避免锁竞争                                          │
│  └── 简化开发                                            │
│                                                             │
│  架构：                                                    │
│  ┌─────────────────────────────────────────────────┐       │
│  │  BaseApp/CellApp (单线程)                         │       │
│  │  ┌─────────────────────────────────────┐         │       │
│  │  │  Poller (epoll/kqueue)             │         │       │
│  │  │  ┌───────────────────────────────────┐ │       │       │
│  │  │  │  事件循环 (processLoop)        │ │       │       │
│  │  │  │  ├─► 处理网络事件            │ │       │       │
│  │  │  │  ├─► 处理定时器              │ │       │       │
│  │  │  │  ├─► 处理实体逻辑            │ │       │       │
│  │  │  │  └───────────────────────────────┘ │       │       │
│  │  │  └─────────────────────────────────────┘         │       │
│  │  └─────────────────────────────────────────┘         │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  优势：                                                  │
│  ├── 无锁竞争                                             │
│  ├── 缓存友好                                             │
│  ├── 调试简单                                             │
│  └── 开发效率高                                           │
│                                                             │
│  限制：                                                  │
│  ├── 单核 CPU 利用率低                                    │
│  ├── 需要多进程扩展                                        │
│  └── 大量计算会阻塞                                     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 5.2 KBEngine 多线程扩展

```cpp
// KBEngine 多线程支持

// src/lib/thread/threadpool.h

class ThreadPool {
public:
    // 创建线程池
    ThreadPool(size_t numThreads) {
        for (size_t i = 0; i < numThreads; ++i) {
            threads_.emplace_back([this]() {
                workerThread();
            });
        }
    }

    // 添加任务
    template<typename F>
    auto execute(F&& task) -> std::future<decltype(task())> {
        auto pTask = std::make_shared<std::packaged_task<F>>(task);
        auto future = pTask->get_future();

        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            taskQueue_.push(pTask);
        }

        queueCV_.notify_one();
        return future;
    }

private:
    void workerThread() {
        while (running_) {
            std::shared_ptr<Task> task;

            {
                std::unique_lock<std::mutex> lock(queueMutex_);
                queueCV_.wait(lock, [this] {
                    return !taskQueue_.empty() || !running_;
                });

                if (!running_) break;

                if (!taskQueue_.empty()) {
                    task = taskQueue_.front();
                    taskQueue_.pop();
                }
            }

            if (task) {
                task->execute();
            }
        }
    }

    std::vector<std::thread> threads_;
    std::queue<std::shared_ptr<Task>> taskQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCV_;
    bool running_ = true;
};
```

---

## 六、最佳实践

### 6.1 并发设计原则

```
┌─────────────────────────────────────────────────────────────┐
│                减少锁竞争的设计原则                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 避免共享状态                                          │
│     ├── 每个线程独立数据                                  │
│     ├── 使用线程本地存储 (TLS)                             │
│     └── 消除共享需求                                     │
│                                                             │
│  2. 最小化锁范围                                           │
│     ├── 只锁必要数据                                       │
│     ├── 减小锁持有时间                                     │
│     └── 使用分段锁/读写锁                                  │
│                                                             │
│  3. 使用无锁数据结构                                       │
│     ├── 原子操作 (atomic)                                  │
│     ├── 无锁队列                                           │
│     └── RCU                                                │
│                                                             │
│  4. 消息传递并发                                           │
│     ├── Actor 模型                                          │
│     ├── CSP 模型                                            │
│     └── 消息队列                                           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 6.2 工具推荐

```cpp
// 并发分析工具

// 1. ThreadSanitizer
// 编译: -fsanitize=thread -g
// 检测: 数据竞争、死锁等

// 2. Helgrind (Valgrind 工具)
// 运行: valgrind --tool=helgrind ./server
// 功能: 锁错误检测、死锁检测

// 3. perf lock
// 运行: perf lock ./server
// 功能: 锁竞争分析

// 4. Intel VTune
// 功能: 并发分析、热点识别
```

---

## 七、总结

### 锁优化技术总结

| 技术 | 复杂度 | 效果 | 适用场景 |
|------|--------|------|----------|
| **分段锁** | 中 | 中 | 哈表/Map |
| **读写锁** | 低 | 中 | 读多写少 |
| **无锁队列** | 高 | 高 | SPS/MPMC |
| **RCU** | 高 | 高 | 读多写少 |
| **Actor** | 高 | 高 | 消息传递 |

### 最佳实践

```
1. 优先避免锁
   - 使用线程本地存储
   - 采用无锁数据结构
   - 消除共享状态

2. 无法避免时减小锁粒度
   - 缩小锁范围
   - 使用读写锁
   - 减少持有时间

3. 考虑消息传递
   - Actor 模型
   - CSP 模型
   - 消息队列

4. 工具检测
   - ThreadSanitizer
   - Helgrind
   - perf lock
```

---

## 参考资料

- [KBEngine GitHub - ThreadPool](https://github.com/kkengine/kbengine/tree/master/kbe/src/lib/thread)
- [Lock-Free Programming](https://www.kernel.org/doc/Documentation/core-api/lockdep.html)
- [Actor Model 论文](https://www.semanticscholar.org/paper/5369310/)
