# Q79: 如何设计线程安全的容器？

## 问题分析

本题考察对线程安全容器的理解：
- 线程安全设计策略
- 常见容器实现
- 细粒度锁 vs 粗粒度锁
- 无锁容器

---

## 一、设计策略

### 1.1 策略对比

```
┌─────────────────────────────────────────────────────────────┐
│                    线程安全容器策略                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 粗粒度锁 (Coarse-grained Locking):                      │
│  ├── 单个锁保护整个容器                                      │
│  ├── 实现简单                                               │
│  ├── 并发度低                                               │
│  └── 适用: 小容器、低并发                                    │
│                                                             │
│  2. 细粒度锁 (Fine-grained Locking):                        │
│  ├── 多个锁分别保护不同部分                                  │
│  ├── 实现复杂                                               │
│  ├── 并发度高                                               │
│  └── 适用: 大容器、高并发                                    │
│                                                             │
│  3. 分段锁 (Striped Locking):                               │
│  ├── 按哈希分段加锁                                          │
│  ├── 平衡实现和性能                                          │
│  └── 适用: 哈希表                                           │
│                                                             │
│  4. 无锁 (Lock-free):                                      │
│  ├── 原子操作                                               │
│  ├── 实现最复杂                                             │
│  ├── 性能最好                                               │
│  └── 适用: 特定场景                                         │
│                                                             │
│  5. 副本-写-复制 (Copy-on-Write):                           │
│  ├── 读不加锁                                               │
│  ├── 写时复制                                               │
│  └── 适用: 读多写少                                         │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 API 设计

```cpp
// 线程安全容器 API 设计

template<typename T>
class ThreadSafeQueue {
public:
    // 基本操作
    bool push(const T& value);           // 入队
    bool pop(T& value);                   // 出队

    // 批量操作
    size_t pushBatch(const std::vector<T>& values);
    size_t popBatch(std::vector<T>& values, size_t max);

    // 状态查询
    bool empty() const;
    size_t size() const;  // 可能是近似值

    // 等待操作
    bool waitAndPop(T& value);            // 等待直到有元素
    bool waitFor(T& value,
                std::chrono::milliseconds timeout);

    // 交换操作
    bool swap(std::vector<T>& other);

    // 迭代器 (不支持，因为不安全)
    // 迭代需要快照方式
    std::vector<T> snapshot() const;
};
```

---

## 二、粗粒度锁容器

### 2.1 线程安全队列

```cpp
// 粗粒度锁的线程安全队列

template<typename T>
class ThreadSafeQueue {
public:
    void push(const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(value);
        condition_.notify_one();
    }

    void push(T&& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(value));
        condition_.notify_one();
    }

    bool pop(T& value) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (queue_.empty()) {
            return false;
        }

        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    bool waitAndPop(T& value) {
        std::unique_lock<std::mutex> lock(mutex_);

        condition_.wait(lock, [this] {
            return !queue_.empty() || stop_;
        });

        if (queue_.empty() && stop_) {
            return false;
        }

        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    bool waitFor(T& value, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (condition_.wait_for(lock, timeout, [this] {
            return !queue_.empty() || stop_;
        })) {
            if (queue_.empty() && stop_) {
                return false;
            }

            value = std::move(queue_.front());
            queue_.pop();
            return true;
        }

        return false;
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }
        condition_.notify_all();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::queue<T> queue_;
    bool stop_ = false;
};
```

### 2.2 线程安全哈希表

```cpp
// 粗粒度锁的哈希表

template<typename K, typename V>
class ThreadSafeHashMap {
public:
    bool insert(const K& key, const V& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto result = map_.emplace(key, value);
        return result.second;
    }

    bool find(const K& key, V& value) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = map_.find(key);
        if (it != map_.end()) {
            value = it->second;
            return true;
        }
        return false;
    }

    bool erase(const K& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return map_.erase(key) > 0;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return map_.size();
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<K, V> map_;
};
```

---

## 三、细粒度锁容器

### 3.1 分段哈希表

```cpp
// 分段锁的哈希表

template<typename K, typename V, size_t N = 16>
class StripedHashMap {
public:
    StripedHashMap() : buckets_(N) {}

    bool insert(const K& key, const V& value) {
        size_t stripe = getStripe(key);
        std::lock_guard<std::mutex> lock(mutexes_[stripe]);

        auto& bucket = buckets_[stripe];
        auto result = bucket.emplace(key, value);
        return result.second;
    }

    bool find(const K& key, V& value) const {
        size_t stripe = getStripe(key);
        std::lock_guard<std::mutex> lock(mutexes_[stripe]);

        const auto& bucket = buckets_[stripe];
        auto it = bucket.find(key);
        if (it != bucket.end()) {
            value = it->second;
            return true;
        }
        return false;
    }

    bool erase(const K& key) {
        size_t stripe = getStripe(key);
        std::lock_guard<std::mutex> lock(mutexes_[stripe]);

        auto& bucket = buckets_[stripe];
        return bucket.erase(key) > 0;
    }

    // 获取所有元素的快照
    std::vector<std::pair<K, V>> snapshot() const {
        std::vector<std::pair<K, V>> result;

        for (size_t i = 0; i < N; ++i) {
            std::lock_guard<std::mutex> lock(mutexes_[i]);
            for (const auto& kv : buckets_[i]) {
                result.push_back(kv);
            }
        }

        return result;
    }

private:
    size_t getStripe(const K& key) const {
        return std::hash<K>{}(key) % N;
    }

    std::array<std::unordered_map<K, V>, N> buckets_;
    mutable std::array<std::mutex, N> mutexes_;
};
```

### 3.2 并发链表

```cpp
// 基于节点锁的并发链表

template<typename T>
class ConcurrentLinkedList {
public:
    struct Node {
        T data;
        std::mutex mutex;
        Node* next;

        Node(const T& value) : data(value), next(nullptr) {}
    };

    bool insert(const T& value) {
        Node* newNode = new Node(value);

        // 锁定头节点
        head_.mutex.lock();
        Node* prev = &head_;
        Node* curr = head_.next;

        if (curr) {
            curr->mutex.lock();
        }

        while (curr && curr->data < value) {
            prev->mutex.unlock();
            prev = curr;
            curr = curr->next;

            if (curr) {
                curr->mutex.lock();
            }
        }

        // 插入新节点
        newNode->next = curr;
        prev->next = newNode;

        // 释放锁
        if (curr) {
            curr->mutex.unlock();
        }
        prev->mutex.unlock();

        return true;
    }

    bool find(const T& value) const {
        // 需要可变 mutex (mutable)
        // 实现类似 insert
        return false;
    }

    bool erase(const T& value) {
        // 实现类似 insert，需要锁定前驱节点
        return false;
    }

private:
    mutable Node head_;  // 哨兵节点
};
```

---

## 四、读写分离容器

### 4.1 Copy-on-Write

```cpp
// 写时复制的容器

template<typename T>
class CowVector {
public:
    // 读操作不加锁 (使用 shared_ptr 的原子引用计数)
    const T& at(size_t index) const {
        return data_->at(index);
    }

    size_t size() const {
        return data_->size();
    }

    // 写操作创建副本
    void push_back(const T& value) {
        auto newData = std::make_shared<std::vector<T>>(*data_);
        newData->push_back(value);
        data_.store(newData);
    }

    void set(size_t index, const T& value) {
        auto newData = std::make_shared<std::vector<T>>(*data_);
        newData->at(index) = value;
        data_.store(newData);
    }

    // 获取快照
    std::shared_ptr<const std::vector<T>> snapshot() const {
        return data_.load();
    }

private:
    std::atomic<std::shared_ptr<std::vector<T>>> data_{
        std::make_shared<std::vector<T>>()
    };
};
```

### 4.2 读写锁哈希表

```cpp
// 读写锁的哈希表

template<typename K, typename V>
class RwLockHashMap {
public:
    bool insert(const K& key, const V& value) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto result = map_.emplace(key, value);
        return result.second;
    }

    bool find(const K& key, V& value) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = map_.find(key);
        if (it != map_.end()) {
            value = it->second;
            return true;
        }
        return false;
    }

    bool erase(const K& key) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        return map_.erase(key) > 0;
    }

    // 允许并发的只读迭代
    std::vector<V> values() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<V> result;
        result.reserve(map_.size());

        for (const auto& kv : map_) {
            result.push_back(kv.second);
        }

        return result;
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<K, V> map_;
};
```

---

## 五、无锁容器

### 5.1 无锁栈

```cpp
// 无锁栈 (使用 hazard pointer)

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

        // 延迟删除 (需要 hazard pointer 或 epoch-based reclamation)
        reclaimLater(oldTop);

        return item;
    }

private:
    struct Node {
        T* data;
        Node* next;
    };

    void reclaimLater(Node* node) {
        // 简化实现，实际需要更复杂的内存回收机制
        delete node;
    }

    std::atomic<Node*> top_{nullptr};
};
```

---

## 六、性能对比

### 6.1 容器实现对比

| 实现方式 | 读性能 | 写性能 | 内存开销 | 实现复杂度 |
|---------|--------|--------|----------|------------|
| **粗粒度锁** | 中 | 中 | 低 | 简单 |
| **细粒度锁** | 高 | 高 | 中 | 中等 |
| **分段锁** | 高 | 高 | 中 | 中等 |
| **读写锁** | 高 | 中 | 低 | 简单 |
| **Copy-on-Write** | 极高 | 低 | 高 | 简单 |
| **无锁** | 极高 | 高 | 低 | 复杂 |

### 6.2 使用建议

```
选择建议:

- 低并发: 粗粒度锁
- 高并发读多写少: 读写锁或 COW
- 高并发读写均衡: 分段锁
- 极致性能: 无锁 (需谨慎)
```

---

## 七、KBEngine 容器设计

### 7.1 KBEngine 线程安全策略

```python
# KBEngine 容器设计

"""
KBEngine 容器策略:

1. 单线程访问容器
   - 每个进程内单线程事件循环
   - 无需线程安全容器

2. 实体字典
   - entities = {}  # 单线程访问
   - 查找时直接 dict[key]

3. 邮件队列
   - 每个实体独立消息队列
   - 单线程串行处理

4. 跨进程通信
   - 消息传递而非共享状态
   - 序列化传输
"""

# KBEngine 风格实现
class EntityManager:
    def __init__(self):
        # 单线程访问，无需锁
        self.entities = {}
        self.next_id = 1

    def create_entity(self, entity_type):
        # 单线程执行，安全
        entity_id = self.next_id
        self.next_id += 1

        entity = entity_type(entity_id)
        self.entities[entity_id] = entity
        return entity

    def get_entity(self, entity_id):
        return self.entities.get(entity_id)

    def destroy_entity(self, entity_id):
        if entity_id in self.entities:
            del self.entities[entity_id]
```

---

## 八、总结

### 线程安全容器设计

```
线程安全容器 = 选择合适策略 + 细心实现 + 充分测试
- 粗粒度: 简单场景
- 分段锁: 哈希表
- 读写锁: 读多写少
- 无锁: 高性能高复杂

KBEngine: 单线程模型避免问题
```

---

## 参考资料

- [Intel TBB Concurrent Containers](https://www.intel.com/content/www/us/en/developer/tools/oneapi/threadscheduler.html)
- [Concurrency Kit](http://concurrencykit.org/)
- [C++ Thread Safe Containers](https://en.cppreference.com/w/cpp/container)
