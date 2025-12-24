#pragma once

#include <functional>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <future>
#include <memory>

namespace apollo {
namespace utils {

/**
 * @brief 线程池
 *
 * 高效的任务执行池，支持任意函数对象和返回值
 */
class ThreadPool {
public:
    using Task = std::function<void()>;

    /**
     * @brief 构造函数
     *
     * @param numThreads 线程数量，0表示使用硬件并发数
     */
    explicit ThreadPool(size_t numThreads = 0);

    /**
     * @brief 析构函数
     *
     * 等待所有任务完成后销毁
     */
    ~ThreadPool();

    // 禁止拷贝和移动
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    /**
     * @brief 提交任务
     *
     * @param f 可调用对象
     * @param args 参数
     * @return std::future 用于获取返回值
     */
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
                throw std::runtime_error("submit on stopped ThreadPool");
            }
            tasks_.emplace([task]() { (*task)(); });
        }

        condition_.notify_one();
        return result;
    }

    /**
     * @brief 提交任务（无返回值版本）
     */
    void enqueue(Task task);

    /**
     * @brief 获取线程数量
     */
    size_t getThreadCount() const { return threads_.size(); }

    /**
     * @brief 获取待处理任务数量
     */
    size_t getPendingTaskCount() const;

    /**
     * @brief 等待所有任务完成
     */
    void waitForAll();

    /**
     * @brief 停止线程池
     *
     * 不再接受新任务，等待当前任务完成后退出
     */
    void stop();

    /**
     * @brief 立即停止线程池
     *
     * 放弃所有待处理任务
     */
    void stopNow();

    /**
     * @brief 检查是否已停止
     */
    bool isStopped() const { return stop_; }

private:
    void workerThread();

    std::vector<std::thread> threads_;
    std::queue<Task> tasks_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_{false};
    std::atomic<size_t> activeWorkers_{0};
    std::condition_variable finished_;
};

//==============================================================================
// 实现
//==============================================================================

inline ThreadPool::ThreadPool(size_t numThreads) {
    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) {
            numThreads = 4;  // 默认值
        }
    }

    for (size_t i = 0; i < numThreads; ++i) {
        threads_.emplace_back(&ThreadPool::workerThread, this);
    }
}

inline ThreadPool::~ThreadPool() {
    stop();
}

inline void ThreadPool::enqueue(Task task) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (stop_) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }
        tasks_.push(std::move(task));
    }
    condition_.notify_one();
}

inline size_t ThreadPool::getPendingTaskCount() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return tasks_.size();
}

inline void ThreadPool::waitForAll() {
    std::unique_lock<std::mutex> lock(mutex_);
    finished_.wait(lock, [this] {
        return tasks_.empty() && activeWorkers_ == 0;
    });
}

inline void ThreadPool::stop() {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        stop_ = true;
    }
    condition_.notify_all();

    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    threads_.clear();
}

inline void ThreadPool::stopNow() {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        stop_ = true;
        // 清空任务队列
        while (!tasks_.empty()) {
            tasks_.pop();
        }
    }
    condition_.notify_all();

    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    threads_.clear();
}

inline void ThreadPool::workerThread() {
    while (true) {
        Task task;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this] {
                return stop_ || !tasks_.empty();
            });

            if (stop_ && tasks_.empty()) {
                return;
            }

            if (!tasks_.empty()) {
                task = std::move(tasks_.front());
                tasks_.pop();
                ++activeWorkers_;
            }
        }

        if (task) {
            task();
            {
                std::unique_lock<std::mutex> lock(mutex_);
                --activeWorkers_;
            }
            finished_.notify_one();
        }
    }
}

} // namespace utils
} // namespace apollo
