#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <memory>
#include <atomic>

namespace apollo {

/// 线程池
class ThreadPool {
public:
    /// 构造线程池
    /// @param numThreads 线程数量，0表示使用硬件线程数
    explicit ThreadPool(size_t numThreads = 0);

    /// 析构函数，等待所有任务完成
    ~ThreadPool();

    /// 禁止拷贝
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /// 提交任务
    template<typename F, typename... Args>
    auto Submit(F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>;

    /// 获取线程数量
    size_t GetThreadCount() const { return threads_.size(); }

    /// 获取待处理任务数
    size_t GetPendingTaskCount() const;

    /// 等待所有任务完成
    void WaitForAllTasks();

    /// 停止线程池（立即返回，不等待任务完成）
    void Stop();

private:
    void WorkerThread();

    std::vector<std::thread> threads_;                   // 工作线程
    std::queue<std::function<void()>> tasks_;           // 任务队列
    mutable std::mutex queueMutex_;                   // 任务队列互斥锁
    std::condition_variable condition_;                 // 条件变量
    std::atomic<bool> stop_{false};                    // 停止标志
    std::atomic<size_t> activeThreads_{0};              // 活跃线程数
    std::condition_variable finished_;                  // 任务完成条件变量
};

template<typename F, typename... Args>
auto ThreadPool::Submit(F&& f, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type> {

    using ReturnType = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<ReturnType> result = task->get_future();

    {
        std::unique_lock<std::mutex> lock(queueMutex_);

        if (stop_) {
            throw std::runtime_error("ThreadPool is stopped");
        }

        tasks_.emplace([task]() { (*task)(); });
    }

    condition_.notify_one();
    return result;
}

}  // namespace apollo