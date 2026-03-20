# Q75: 线程池如何设计？任务如何调度？

## 问题分析

本题考察对线程池设计的理解：
- 线程池架构
- 任务队列设计
- 调度策略
- 动态扩展

---

## 一、线程池架构

### 1.1 基本组成

```
┌─────────────────────────────────────────────────────────────┐
│                    线程池架构                                  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────────────────────────────────────────┐       │
│  │              任务队列 (Task Queue)                │       │
│  │  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐      │       │
│  │  │Task │ │Task │ │Task │ │Task │ │ ... │      │       │
│  │  └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘ └─────┘      │       │
│  └─────┼────────┼────────┼────────┼────────────────┘       │
│        │        │        │        │                        │
│        ▼        ▼        ▼        ▼                        │
│  ┌─────────────────────────────────────────────────┐       │
│  │              工作线程 (Worker Threads)            │       │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐            │       │
│  │  │Thread 1 │ │Thread 2 │ │Thread N │            │       │
│  │  │  idle   │ │running  │ │  idle   │            │       │
│  │  └─────────┘ └─────────┘ └─────────┘            │       │
│  └─────────────────────────────────────────────────┘       │
│                          │                                  │
│  ┌───────────────────────┼───────────────────────┐       │
│  │                       │                       │       │
│  ▼                       ▼                       ▼       │
│  [任务完成]              [新任务加入]            [线程退出]  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 任务类型

```
┌─────────────────────────────────────────────────────────────┐
│                    任务类型                                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  普通任务 (Normal Task):                                    │
│  ├── 一次性执行                                              │
│  ├── 无优先级                                                │
│  └── 示例: 数据库查询                                        │
│                                                             │
│  定时任务 (Timed Task):                                     │
│  ├── 延迟执行                                                │
│  ├── 周期执行                                                │
│  └── 示例: 心跳检测                                          │
│                                                             │
│  优先级任务 (Priority Task):                                │
│  ├── 按优先级调度                                            │
│  ├── 高优先级先执行                                          │
│  └── 示例: 紧急消息                                          │
│                                                             │
│  异步任务 (Async Task):                                     │
│  ├── 带返回值                                                │
│  ├── Future/Promise                                         │
│  └── 示例: RPC 调用                                         │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、线程池实现

### 2.1 基础线程池

```cpp
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>

class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount)
        : stop_(false) {

        // 创建工作线程
        for (size_t i = 0; i < threadCount; ++i) {
            workers_.emplace_back([this] {
                workerLoop();
            });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            stop_ = true;
        }
        condition_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    // 提交任务
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result_t<F, Args...>> {

        using ReturnType = typename std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<ReturnType> result = task->get_future();

        {
            std::unique_lock<std::mutex> lock(mutex_);

            if (stop_) {
                throw std::runtime_error("ThreadPool is stopped");
            }

            tasks_.emplace([task]() {
                (*task)();
            });
        }

        condition_.notify_one();
        return result;
    }

private:
    void workerLoop() {
        while (true) {
            Task task;

            {
                std::unique_lock<std::mutex> lock(mutex_);

                // 等待任务或停止信号
                condition_.wait(lock, [this] {
                    return stop_ || !tasks_.empty();
                });

                if (stop_ && tasks_.empty()) {
                    return;
                }

                task = std::move(tasks_.front());
                tasks_.pop();
            }

            // 执行任务 (解锁状态)
            task();
        }
    }

    using Task = std::function<void()>;

    std::vector<std::thread> workers_;
    std::queue<Task> tasks_;

    std::mutex mutex_;
    std::condition_variable condition_;
    bool stop_;
};
```

### 2.2 优先级线程池

```cpp
// 优先级线程池

class PriorityThreadPool {
public:
    enum class Priority {
        LOW = 0,
        NORMAL = 1,
        HIGH = 2,
        URGENT = 3
    };

    explicit PriorityThreadPool(size_t threadCount) {
        for (size_t i = 0; i < threadCount; ++i) {
            workers_.emplace_back([this] { workerLoop(); });
        }
    }

    template<typename F, typename... Args>
    auto submit(Priority priority, F&& f, Args&&... args)
        -> std::future<typename std::invoke_result_t<F, Args...>> {

        using ReturnType = typename std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<ReturnType> result = task->get_future();

        {
            std::unique_lock<std::mutex> lock(mutex_);

            TaskItem item;
            item.priority = priority;
            item.sequence = nextSequence_++;
            item.task = [task]() { (*task)(); };

            tasks_.push(std::move(item));
        }

        condition_.notify_one();
        return result;
    }

private:
    struct TaskItem {
        Priority priority;
        uint64_t sequence;  // 同优先级内的顺序
        std::function<void()> task;

        bool operator<(const TaskItem& other) const {
            if (priority != other.priority) {
                return static_cast<int>(priority) <
                       static_cast<int>(other.priority);
            }
            return sequence > other.sequence;  // 小的先执行
        }
    };

    void workerLoop() {
        while (true) {
            TaskItem item;

            {
                std::unique_lock<std::mutex> lock(mutex_);

                condition_.wait(lock, [this] {
                    return stop_ || !tasks_.empty();
                });

                if (stop_ && tasks_.empty()) {
                    return;
                }

                item = std::move(const_cast<TaskItem&>(tasks_.top()));
                tasks_.pop();
            }

            item.task();
        }
    }

    std::vector<std::thread> workers_;
    std::priority_queue<TaskItem> tasks_;

    std::mutex mutex_;
    std::condition_variable condition_;
    bool stop_ = false;
    uint64_t nextSequence_ = 0;
};
```

---

## 三、任务调度

### 3.1 工作窃取 (Work Stealing)

```cpp
// 工作窃取线程池

class WorkStealingThreadPool {
public:
    explicit WorkStealingThreadPool(size_t threadCount)
        : threadCount_(threadCount) {

        for (size_t i = 0; i < threadCount; ++i) {
            queues_.emplace_back(std::make_unique<WorkStealingQueue>());
        }

        for (size_t i = 0; i < threadCount; ++i) {
            workers_.emplace_back([this, i] {
                workerLoop(i);
            });
        }
    }

    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result_t<F, Args...>> {

        using ReturnType = typename std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<ReturnType> result = task->get_future();

        // 提交到当前线程的队列
        size_t queueId = getCurrentThreadId() % threadCount_;
        queues_[queueId]->push([task]() { (*task)(); });

        return result;
    }

private:
    class WorkStealingQueue {
    public:
        void push(std::function<void()> task) {
            std::lock_guard<std::mutex> lock(mutex_);
            tasks_.push(std::move(task));
        }

        bool pop(std::function<void()>& task) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (tasks_.empty()) return false;
            task = std::move(tasks_.front());
            tasks_.pop();
            return true;
        }

        bool steal(std::function<void()>& task) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (tasks_.empty()) return false;
            task = std::move(tasks_.back());
            tasks_.pop_back();
            return true;
        }

        bool empty() const {
            return tasks_.empty();
        }

    private:
        std::deque<std::function<void()>> tasks_;
        mutable std::mutex mutex_;
    };

    void workerLoop(size_t threadId) {
        setCurrentThreadId(threadId);

        while (true) {
            std::function<void()> task;

            // 1. 从自己的队列取
            if (queues_[threadId]->pop(task)) {
                task();
                continue;
            }

            // 2. 尝试从其他队列窃取
            bool stolen = false;
            for (size_t i = 0; i < threadCount_; ++i) {
                size_t targetId = (threadId + i) % threadCount_;
                if (targetId != threadId && queues_[targetId]->steal(task)) {
                    stolen = true;
                    break;
                }
            }

            if (stolen) {
                task();
            } else {
                // 3. 没有任务，等待
                std::this_thread::yield();
            }
        }
    }

    static size_t& getCurrentThreadId() {
        static thread_local size_t id = 0;
        return id;
    }

    static void setCurrentThreadId(size_t id) {
        getCurrentThreadId() = id;
    }

    size_t threadCount_;
    std::vector<std::unique_ptr<WorkStealingQueue>> queues_;
    std::vector<std::thread> workers_;
};
```

### 3.2 动态线程池

```cpp
// 动态扩展的线程池

class DynamicThreadPool {
public:
    DynamicThreadPool(size_t minThreads, size_t maxThreads)
        : minThreads_(minThreads),
          maxThreads_(maxThreads),
          idleThreads_(0),
          stop_(false) {

        for (size_t i = 0; i < minThreads_; ++i) {
            addWorker();
        }

        // 监控线程
        monitorThread_ = std::thread([this] { monitorLoop(); });
    }

    ~DynamicThreadPool() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            stop_ = true;
        }
        condition_.notify_all();

        if (monitorThread_.joinable()) {
            monitorThread_.join();
        }

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result_t<F, Args...>> {

        using ReturnType = typename std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<ReturnType> result = task->get_future();

        {
            std::unique_lock<std::mutex> lock(mutex_);

            tasks_.emplace([task]() { (*task)(); });

            // 检查是否需要增加线程
            if (idleThreads_ == 0 && workers_.size() < maxThreads_) {
                addWorker();
            }
        }

        condition_.notify_one();
        return result;
    }

private:
    void addWorker() {
        workers_.emplace_back([this] {
            workerLoop();
        });
    }

    void workerLoop() {
        while (true) {
            Task task;

            {
                std::unique_lock<std::mutex> lock(mutex_);

                ++idleThreads_;

                condition_.wait(lock, [this] {
                    return stop_ || !tasks_.empty();
                });

                --idleThreads_;

                if (stop_ && tasks_.empty()) {
                    return;
                }

                task = std::move(tasks_.front());
                tasks_.pop();
            }

            task();
        }
    }

    void monitorLoop() {
        while (!stop_) {
            std::this_thread::sleep_for(std::chrono::seconds(5));

            std::unique_lock<std::mutex> lock(mutex_);

            // 如果空闲线程太多，移除一些
            while (idleThreads_ > minThreads_ && workers_.size() > minThreads_) {
                // 实际实现需要更复杂的机制
                // 这里只是示意
                break;
            }
        }
    }

    using Task = std::function<void()>;

    size_t minThreads_;
    size_t maxThreads_;
    size_t idleThreads_;

    std::vector<std::thread> workers_;
    std::queue<Task> tasks_;

    std::mutex mutex_;
    std::condition_variable condition_;
    bool stop_;

    std::thread monitorThread_;
};
```

---

## 四、KBEngine 线程池

### 4.1 KBEngine 线程模型

```python
# KBEngine 的线程池实现

"""
KBEngine 线程模型:

1. 主线程: 事件循环
2. DB 线程: 数据库操作
3. 备份线程: 数据快照
4. 网络线程: 消息收发

内部使用线程池处理:
- 数据库查询
- 文件读写
- 定时任务
"""

# KBEngine 风格的线程池
class KBEngineThreadPool:
    def __init__(self, name, size):
        self.name = name
        self.size = size
        self.queue = []
        self.workers = []

        for i in range(size):
            worker = threading.Thread(
                target=self._workerLoop,
                name=f"{name}-{i}",
                daemon=True
            )
            worker.start()
            self.workers.append(worker)

    def submit(self, task):
        """提交任务"""
        self.queue.append(task)
        # KBEngine 内部通过条件变量通知

    def _workerLoop(self):
        """工作线程循环"""
        while True:
            if self.queue:
                task = self.queue.pop(0)
                try:
                    task()
                except Exception as e:
                    KBEngine.error(f"Task error: {e}")
            else:
                time.sleep(0.01)
```

---

## 五、最佳实践

### 5.1 线程池配置

| 场景 | 线程数 | 说明 |
|------|--------|------|
| **CPU 密集** | CPU 核心数 | 避免频繁切换 |
| **IO 密集** | CPU 核心数 × 2 | 等待时其他线程可运行 |
| **混合型** | CPU 核心数 + 50 | 平衡考虑 |

### 5.2 使用建议

```
线程池使用建议:

1. 避免任务阻塞
2. 合理设置线程数
3. 使用工作窃取提高吞吐
4. 动态调整应对负载
5. 优雅关闭保证任务完成
```

---

## 六、总结

```
线程池 = 任务队列 + 工作线程 + 调度策略 + 动态调整
- 复用线程减少开销
- 任务队列缓冲
- 工作窃取平衡负载
- 动态调整应对流量
```

---

## 参考资料

- [C++ Thread Pool Implementation](https://github.com/progschj/ThreadPool)
- [Java ThreadPoolExecutor](https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/ThreadPoolExecutor.html)
- [Work Stealing Queue](https://www.dre.vanderbilt.edu/~schmidt/POSA/POSA2/)
