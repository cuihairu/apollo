# Q72: 如何避免死锁？

## 问题分析

本题考察对死锁处理的理解：
- 死锁产生原因
- 检测方法
- 预防策略
- KBEngine 的解决方案

---

## 一、死锁基础

### 1.1 死锁四个必要条件

```
┌─────────────────────────────────────────────────────────────┐
│                    死锁四条件                                  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 互斥 (Mutual Exclusion):                                │
│     至少一个资源不能被共享                                   │
│                                                             │
│  2. 持有并等待 (Hold and Wait):                             │
│     进程持有至少一个资源，同时等待获取其他资源                 │
│                                                             │
│  3. 不可抢占 (No Preemption):                               │
│     资源不能被强制抢占，只能自愿释放                          │
│                                                             │
│  4. 循环等待 (Circular Wait):                               │
│     存在进程等待链 P0→P1→P2→...→P0                          │
│                                                             │
│  四个条件同时满足才会死锁！                                   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 死锁示例

```cpp
// ❌ 死锁示例

class Account {
public:
    void transfer(Account& to, int amount) {
        // 转账需要锁定两个账户

        // 线程 A: transfer(acc1, acc2, 100)
        std::lock_guard<std::mutex> lock1(mutex_);  // 锁定 acc1
        // ... 此时上下文切换 ...

        // 线程 B: transfer(acc2, acc1, 50)
        std::lock_guard<std::mutex> lock2(to.mutex_);  // 锁定 acc2

        // A 等待 acc2，B 等待 acc1 → 死锁！
        std::lock_guard<std::mutex> lock3(to.mutex_);  // A 等待这个
        std::lock_guard<std::mutex> lock4(mutex_);      // B 等待这个

        balance_ -= amount;
        to.balance_ += amount;
    }

private:
    std::mutex mutex_;
    int balance_;
};
```

---

## 二、死锁预防

### 2.1 锁顺序约定

```cpp
// ✅ 方案 1: 统一锁顺序

class Account {
public:
    // 每个账户有唯一 ID
    Account(int id, int balance) : id_(id), balance_(balance) {}

    void transfer(Account& to, int amount) {
        // 始终按 ID 顺序加锁
        if (id_ < to.id_) {
            std::lock(mutex_, to.mutex_);
            std::lock_guard<std::mutex> lock1(mutex_, std::adopt_lock);
            std::lock_guard<std::mutex> lock2(to.mutex_, std::adopt_lock);

            doTransfer(to, amount);
        } else {
            std::lock(to.mutex_, mutex_);
            std::lock_guard<std::mutex> lock1(to.mutex_, std::adopt_lock);
            std::lock_guard<std::mutex> lock2(mutex_, std::adopt_lock);

            doTransfer(to, amount);
        }
    }

private:
    void doTransfer(Account& to, int amount) {
        balance_ -= amount;
        to.balance_ += amount;
    }

    int id_;
    int balance_;
    std::mutex mutex_;
};
```

### 2.2 std::lock (同时锁定)

```cpp
// ✅ 方案 2: 使用 std::lock 避免死锁

class Account {
public:
    void transfer(Account& to, int amount) {
        // std::lock 使用 deadlock avoidance 算法
        std::lock(mutex_, to.mutex_);

        // adopt_lock 表示已锁定，只需管理 RAII
        std::lock_guard<std::mutex> lock1(mutex_, std::adopt_lock);
        std::lock_guard<std::mutex> lock2(to.mutex_, std::adopt_lock);

        balance_ -= amount;
        to.balance_ += amount;
    }

private:
    int balance_;
    std::mutex mutex_;
};
```

### 2.3 try_lock 超时

```cpp
// ✅ 方案 3: 带超时的尝试锁定

class Account {
public:
    void transfer(Account& to, int amount) {
        while (true) {
            std::unique_lock<std::mutex> lock1(mutex_, std::defer_lock);
            std::unique_lock<std::mutex> lock2(to.mutex_, std::defer_lock);

            // 尝试同时锁定，有超时
            if (lock1.try_lock_for(std::chrono::milliseconds(100))) {
                if (lock2.try_lock_for(std::chrono::milliseconds(100))) {
                    // 成功获取两个锁
                    balance_ -= amount;
                    to.balance_ += amount;
                    return;
                }
                // 只获取到第一个锁，释放重试
                lock1.unlock();
            }

            // 随机退避避免活锁
            std::this_thread::sleep_for(
                std::chrono::microseconds(rand() % 1000)
            );
        }
    }

private:
    int balance_;
    std::timed_mutex mutex_;
};
```

### 2.4 锁层次结构

```cpp
// ✅ 方案 4: 锁层次

enum class LockLevel {
    DATABASE = 0,
    ENTITY = 1,
    INVENTORY = 2,
    ITEM = 3
};

class HierarchicalLock {
public:
    void lock(LockLevel level) {
        // 只能按从低到高的顺序获取锁
        if (level <= currentLevel_) {
            throw std::runtime_error("Invalid lock order");
        }

        locks_[static_cast<int>(level)].lock();
        currentLevel_ = level;
    }

    void unlock(LockLevel level) {
        locks_[static_cast<int>(level)].unlock();
        currentLevel_ = static_cast<LockLevel>(static_cast<int>(level) - 1);
    }

private:
    std::array<std::mutex, 4> locks_;
    LockLevel currentLevel_ = LockLevel::DATABASE;
};

// 使用: 始终按层次加锁
void updatePlayerInventory(Player* player) {
    lock(LockLevel::ENTITY);
    lock(LockLevel::INVENTORY);
    // ... 操作
    unlock(LockLevel::INVENTORY);
    unlock(LockLevel::ENTITY);
}
```

---

## 三、死锁检测

### 3.1 超时检测

```cpp
// 死锁检测器

class DeadlockDetector {
public:
    struct LockInfo {
        std::thread::id threadId;
        void* mutexAddr;
        std::string lockName;
        uint64_t timestamp;
    };

    // 试图获取锁时记录
    void beforeLock(void* mutex, const std::string& name) {
        std::lock_guard<std::mutex> lock(detectorMutex_);

        LockInfo info;
        info.threadId = std::this_thread::get_id();
        info.mutexAddr = mutex;
        info.lockName = name;
        info.timestamp = getCurrentTime();

        waiting_[info.threadId] = info;

        // 检查循环等待
        if (detectCycle()) {
            ERROR("Deadlock detected!");
            printWaitingGraph();
        }
    }

    // 获取锁后记录
    void afterLock(void* mutex) {
        std::lock_guard<std::mutex> lock(detectorMutex_);

        auto threadId = std::this_thread::get_id();
        waiting_.erase(threadId);

        held_[threadId].push_back(mutex);
    }

    // 释放锁
    void unlock(void* mutex) {
        std::lock_guard<std::mutex> lock(detectorMutex_);

        auto threadId = std::this_thread::get_id();
        auto& locks = held_[threadId];

        locks.erase(
            std::remove(locks.begin(), locks.end(), mutex),
            locks.end()
        );
    }

private:
    bool detectCycle() {
        // 使用 DFS 检测循环等待
        std::unordered_set<std::thread::id> visited;

        for (const auto& [thread, info] : waiting_) {
            if (dfsCycle(thread, thread, visited)) {
                return true;
            }
        }
        return false;
    }

    bool dfsCycle(const std::thread::id& start,
                  const std::thread::id& current,
                  std::unordered_set<std::thread::id>& visited) {
        if (visited.count(current)) {
            return current == start;
        }

        visited.insert(current);

        // 当前线程等待的锁被谁持有？
        auto it = waiting_.find(current);
        if (it != waiting_.end()) {
            void* mutex = it->second.mutexAddr;

            // 找到持有这个锁的线程
            for (const auto& [thread, locks] : held_) {
                if (std::find(locks.begin(), locks.end(), mutex) != locks.end()) {
                    if (dfsCycle(start, thread, visited)) {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    void printWaitingGraph() {
        INFO("=== Waiting Graph ===");
        for (const auto& [thread, info] : waiting_) {
            INFO("Thread {} waiting for {} at {}",
                 thread, info.lockName, info.mutexAddr);
        }
    }

    std::mutex detectorMutex_;
    std::unordered_map<std::thread::id, LockInfo> waiting_;
    std::unordered_map<std::thread::id, std::vector<void*>> held_;
};
```

---

## 四、KBEngine 死锁避免

### 4.1 KBEngine Actor 模型

```python
# KBEngine 使用 Actor 模型避免死锁

"""
KBEngine 死锁避免策略:

1. 单线程事件循环:
   - 每个 BaseApp/CellApp 内部单线程处理
   - 避免了进程内锁竞争

2. 消息传递:
   - 进程间通过消息通信
   - 异步发送，不阻塞

3. 无共享状态:
   - 实体分布在不同进程
   - 避免跨进程锁

4. 邮箱队列:
   - 每个实体有消息队列
   - 串行处理消息
"""

# KBEngine 风格的 Actor
class Entity(KBEngine.Entity):
    def onAttack(self, attackerId):
        # 单线程处理，无锁
        attacker = KBEngine.getEntity(attackerId)
        if attacker:
            # 直接处理，不用担心死锁
            self.hp -= attacker.attack
            if self.hp <= 0:
                self.onDeath()

    # 跨进程通信异步进行
    def requestOtherEntityData(self, entityId):
        # 发送消息到其他进程
        self.otherBase.reqEntityData(entityId, self._onDataReceived)

    def _onDataReceived(self, data):
        # 回调处理，异步非阻塞
        self.processData(data)
```

### 4.2 KBEngine 锁使用规范

```cpp
// KBEngine C++ 锁规范

namespace KBEngine {

/**
 * 锁使用规范:
 *
 * 1. 锁的粒度尽可能小
 * 2. 持有时间尽可能短
 * 3. 避免嵌套锁
 * 4. 统一锁顺序
 * 5. 使用 RAII 管理锁
 */

class EntityLock {
public:
    // 使用 RAII 自动管理锁
    class Guard {
    public:
        Guard(EntityLock& lock) : lock_(lock) {
            lock_.lock();
        }

        ~Guard() {
            lock_.unlock();
        }

    private:
        EntityLock& lock_;
    };

    void lock() {
        mutex_.lock();
        // 记录锁获取 (调试用)
        if (debugEnabled_) {
            debugLock_.lock();
            lockOwner_ = std::this_thread::get_id();
            debugLock_.unlock();
        }
    }

    void unlock() {
        if (debugEnabled_) {
            debugLock_.lock();
            lockOwner_ = std::thread::id();
            debugLock_.unlock();
        }
        mutex_.unlock();
    }

    bool isLockedByCurrentThread() const {
        if (!debugEnabled_) return true;
        return lockOwner_ == std::this_thread::get_id();
    }

private:
    std::mutex mutex_;
    std::mutex debugLock_;
    std::thread::id lockOwner_;
    bool debugEnabled_ = false;
};

}
```

---

## 五、最佳实践

### 5.1 避免死锁准则

| 准则 | 说明 |
|------|------|
| **固定顺序** | 所有线程按相同顺序获取锁 |
| **限时等待** | 使用 try_lock_for |
| **减少锁** | 缩小临界区 |
| **避免嵌套** | 不要持有锁再等待 |
| **及早释放** | 用完立即释放 |

### 5.2 代码审查检查清单

```
死锁检查清单:

□ 是否持有一个锁时等待另一个锁？
□ 是否有循环等待的可能？
□ 锁的获取顺序是否一致？
□ 是否有超时机制？
□ 异常路径是否会释放锁？
□ 是否有锁的层次结构？
```

---

## 六、总结

### 死锁预防核心

```
死锁避免 = 预防设计 + 检测机制 + 良好实践
- 统一锁顺序
- 使用 std::lock
- 超时机制
- Actor 模型 (KBEngine)
```

---

## 参考资料

- [C++ Concurrency in Action](https://www.manning.com/books/c-plus-plus-concurrency-in-action)
- [Deadlock Prevention](https://en.wikipedia.org/wiki/Deadlock_prevention)
- [KBEngine Thread Safety](https://kbengine.github.io/docs/)
