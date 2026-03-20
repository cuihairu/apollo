# Q78: 如何处理竞态条件？

## 问题分析

本题考察对竞态条件的理解：
- 竞态条件产生原因
- 检测方法
- 防护策略
- 线程安全设计

---

## 一、竞态条件基础

### 1.1 定义

```
┌─────────────────────────────────────────────────────────────┐
│                    竞态条件定义                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  竞态条件 (Race Condition):                                 │
│                                                             │
│  系统的输出依赖于事件发生的相对时序，                        │
│  当多个线程/进程同时访问共享资源时，                         │
│  执行顺序的不确定性导致错误结果。                            │
│                                                             │
│  示例:                                                      │
│  ┌─────────────────────────────────────────────────┐       │
│  │  int counter = 0;                                │       │
│  │                                                  │       │
│  │  // Thread 1:                                     │       │
│  │  counter = counter + 1;  // 读取、加1、写回        │       │
│  │                                                  │       │
│  │  // Thread 2:                                     │       │
│  │  counter = counter + 1;  // 可能被中断！           │       │
│  │                                                  │       │
│  │  结果: counter 可能是 1 而不是 2                   │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 常见类型

| 类型 | 描述 | 示例 |
|------|------|------|
| **数据竞争** | 多线程同时读写共享数据 | 全局变量未加锁 |
| **检查-时-竞争** | 检查和使用之间状态改变 | 单例模式 |
| **死锁** | 相互等待对方持有的锁 | A 等待 B，B 等待 A |
| **活锁** | 相互让步导致无法进展 | 礼让式重试 |
| **饥饿** | 某线程长时间得不到资源 | 低优先级线程 |

---

## 二、竞态条件示例

### 2.1 检查-时-竞争

```cpp
// ❌ 检查-时-竞争示例

class Singleton {
public:
    static Singleton* getInstance() {
        // 线程 A 检查
        if (instance_ == nullptr) {
            // 线程 A 被中断...

            // 线程 B 也检查，发现为空
            // 线程 B 创建实例
            // 线程 B 完成赋值

            // 线程 A 恢复，又创建一个实例！
            instance_ = new Singleton();
        }
        return instance_;
    }

private:
    static Singleton* instance_;
};

// ✅ 解决方案 1: 静态局部变量
class Singleton {
public:
    static Singleton& getInstance() {
        // C++11 保证线程安全
        static Singleton instance;
        return instance;
    }
};

// ✅ 解决方案 2: 双重检查锁定
class Singleton {
public:
    static Singleton* getInstance() {
        if (instance_ == nullptr) {  // 第一次检查
            std::lock_guard<std::mutex> lock(mutex_);
            if (instance_ == nullptr) {  // 第二次检查
                instance_ = new Singleton();
            }
        }
        return instance_;
    }

private:
    static std::mutex mutex_;
    static Singleton* instance_;
};
```

### 2.2 延迟初始化竞争

```cpp
// ❌ 延迟初始化竞争

class Cache {
public:
    Value* get(const Key& key) {
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            // 竞态！多个线程可能同时创建
            Value* v = loadValue(key);
            cache_[key] = v;
            return v;
        }
        return it->second;
    }

private:
    std::unordered_map<Key, Value*> cache_;
};

// ✅ 解决方案: 使用 std::call_once
class Cache {
public:
    Value* get(const Key& key) {
        // 先检查是否已存在（无锁快速路径）
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            auto it = cache_.find(key);
            if (it != cache_.end()) {
                return it->second;
            }
        }

        // 需要初始化
        std::unique_lock<std::shared_mutex> lock(mutex_);

        // 再次检查（可能其他线程已初始化）
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            return it->second;
        }

        // 初始化
        Value* v = loadValue(key);
        cache_[key] = v;
        return v;
    }

private:
    std::unordered_map<Key, Value*> cache_;
    std::shared_mutex mutex_;  // 读写锁
};
```

### 2.3 迭代器失效

```cpp
// ❌ 迭代器失效竞态

class PlayerManager {
public:
    void update() {
        // 线程 A: 遍历
        for (auto& player : players_) {
            player->update();
        }
    }

    void addPlayer(Player* player) {
        // 线程 B: 添加
        players_.push_back(player);
        // 可能导致线程 A 的迭代器失效！
    }

private:
    std::vector<Player*> players_;
};

// ✅ 解决方案: 使用版本控制或快照
class PlayerManager {
public:
    void update() {
        // 获取当前版本
        size_t version = getVersion();
        auto snapshot = getPlayersSnapshot(version);

        for (auto* player : snapshot) {
            player->update();
        }
    }

    void addPlayer(Player* player) {
        std::lock_guard<std::mutex> lock(mutex_);
        players_.push_back(player);
        version_++;  // 增加版本号
    }

private:
    std::vector<Player*> getPlayersSnapshot(size_t version) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        if (version != version_) {
            // 版本变了，重新获取
            // 实际可以使用读写拷贝技巧
        }
        return players_;
    }

    size_t getVersion() const {
        return version_;
    }

    std::vector<Player*> players_;
    std::shared_mutex mutex_;
    size_t version_ = 0;
};
```

---

## 三、检测工具

### 3.1 ThreadSanitizer

```bash
# 使用 ThreadSanitizer 检测竞态

# 编译时加上 -fsanitize=thread 标志
g++ -fsanitize=thread -g -O2 your_program.cpp

# 运行程序
./a.out

# ThreadSanitizer 会报告潜在的竞态条件
```

```cpp
// ThreadSanitizer 检测示例

int global = 0;

void thread1() {
    global = 1;  // 数据竞争
}

void thread2() {
    global = 2;  // 数据竞争
}

// ThreadSanitizer 输出:
// WARNING: ThreadSanitizer: data race on vptr
//   Write of size 4 at 0x... by thread T1:
//     #0 thread1()
//   Previous write of size 4 at 0x... by thread T2:
//     #0 thread2()
```

### 3.2 静态分析

```cpp
// 使用静态分析工具检测

// Clang Thread Safety Analysis
class Mutex {
public:
    void lock() __attribute__((exclusive_lock_function()));
    void unlock() __attribute__((unlock_function()));
};

class Data {
public:
    void process() {
        mu_.lock();  // 获取锁
        data_++;     // 操作数据
        mu_.unlock(); // 释放锁
    }

private:
    Mutex mu_;
    int data_ __attribute__((guarded_by(mu_)));
};
```

---

## 四、防护策略

### 4.1 互斥锁

```cpp
// 使用互斥锁保护共享数据

class ThreadSafeCounter {
public:
    void increment() {
        std::lock_guard<std::mutex> lock(mutex_);
        ++value_;
    }

    int get() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return value_;
    }

private:
    mutable std::mutex mutex_;
    int value_ = 0;
};
```

### 4.2 原子操作

```cpp
// 使用原子操作

class AtomicCounter {
public:
    void increment() {
        // 原子递增，无需锁
        value_.fetch_add(1, std::memory_order_relaxed);
    }

    int get() const {
        return value_.load(std::memory_order_relaxed);
    }

private:
    std::atomic<int> value_{0};
};
```

### 4.3 读写锁

```cpp
// 读写锁分离读写操作

class ThreadSafeCache {
public:
    Value* get(const Key& key) {
        // 读操作可以并发
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = cache_.find(key);
        return it != cache_.end() ? it->second : nullptr;
    }

    void put(const Key& key, Value* value) {
        // 写操作独占访问
        std::unique_lock<std::shared_mutex> lock(mutex_);
        cache_[key] = value;
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<Key, Value*> cache_;
};
```

### 4.4 线程局部存储

```cpp
// 使用 thread_local 避免共享

class PerThreadCache {
public:
    Value* get(const Key& key) {
        // 每个线程独立的缓存，无需加锁
        auto& cache = getThreadLocalCache();
        auto it = cache.find(key);
        if (it != cache.end()) {
            return it->second;
        }

        Value* v = loadValue(key);
        cache[key] = v;
        return v;
    }

private:
    std::unordered_map<Key, Value*>& getThreadLocalCache() {
        static thread_local std::unordered_map<Key, Value*> cache;
        return cache;
    }
};
```

---

## 五、KBEngine 避免竞态

### 5.1 单线程模型

```python
# KBEngine 使用单线程事件循环避免竞态

"""
KBEngine 的竞态避免策略:

1. 每个进程内单线程事件循环
   - 串行处理所有消息
   - 避免进程内竞态

2. 实体单线程绑定
   - 每个实体只在一个线程处理
   - 状态变更串行化

3. 邮箱消息传递
   - 进程间异步通信
   - 避免共享状态
"""

# KBEngine 风格实现
class Entity(KBEngine.Entity):
    def __init__(self):
        KBEngine.Entity.__init__(self)
        self.counter = 0  # 单线程访问，无需锁

    def onAttack(self, attackerId):
        # 串行执行，无竞态
        self.counter += 1
        self.processDamage(attackerId)

    def processDamage(self, attackerId):
        attacker = KBEngine.getEntity(attackerId)
        if attacker:
            self.hp -= attacker.attack
```

### 5.2 Actor 模型

```cpp
// Actor 模型避免竞态

class Actor {
public:
    // 发送消息 (异步)
    void send(Message msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        mailbox_.push(std::move(msg));
    }

    // 处理消息 (单线程执行)
    void process() {
        std::queue<Message> messages;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            messages.swap(mailbox_);
        }

        while (!messages.empty()) {
            handleMessage(messages.front());
            messages.pop();
        }
    }

protected:
    virtual void handleMessage(const Message& msg) = 0;

private:
    std::mutex mutex_;
    std::queue<Message> mailbox_;
};
```

---

## 六、最佳实践

### 6.1 避免竞态建议

| 实践 | 说明 |
|------|------|
| **最小化共享** | 尽量减少共享数据 |
| **使用原子操作** | 简单计数等用原子类型 |
| **锁保护** | 复杂操作用锁 |
| **单线程模型** | KBEngine 方式 |
| **消息传递** | Actor 模型 |

### 6.2 编码检查清单

```
竞态检查清单:

□ 共享变量是否有保护?
□ 检查-时-操作是否原子?
□ 迭代器是否可能失效?
□ 延迟初始化是否安全?
□ 是否使用了 ThreadSanitizer?
```

---

## 七、总结

### 竞态条件防护

```
竞态防护 = 最小共享 + 同步机制 + 单线程模型 + 检测工具
- 能不共享就不共享
- 共享就用锁/原子操作
- KBEngine 选择单线程事件循环
- Actor 模型消息传递
```

---

## 参考资料

- [ThreadSanitizer](https://github.com/google/sanitizers/wiki/ThreadSanitizerCppManual)
- [C++ Concurrency in Action](https://www.manning.com/books/c-plus-plus-concurrency-in-action)
- [KBEngine Thread Safety](https://kbengine.github.io/docs/)
