#include "apollo/thread_pool.hpp"
#include <stdexcept>

namespace apollo {

ThreadPool::ThreadPool(size_t numThreads) {
    // 默认使用硬件线程数
    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
    }

    for (size_t i = 0; i < numThreads; ++i) {
        threads_.emplace_back(&ThreadPool::WorkerThread, this);
    }
}

ThreadPool::~ThreadPool() {
    Stop();
}

size_t ThreadPool::GetPendingTaskCount() const {
    std::unique_lock<std::mutex> lock(queueMutex_);
    return tasks_.size();
}

void ThreadPool::WaitForAllTasks() {
    std::unique_lock<std::mutex> lock(queueMutex_);
    finished_.wait(lock, [this]() {
        return tasks_.empty() && activeThreads_.load() == 0;
    });
}

void ThreadPool::Stop() {
    if (stop_) {
        return;
    }

    stop_ = true;
    condition_.notify_all();

    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    threads_.clear();
}

void ThreadPool::WorkerThread() {
    while (true) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            condition_.wait(lock, [this]() {
                return stop_ || !tasks_.empty();
            });

            if (stop_ && tasks_.empty()) {
                return;
            }

            if (!tasks_.empty()) {
                task = std::move(tasks_.front());
                tasks_.pop();
                ++activeThreads_;
            }
        }

        if (task) {
            try {
                task();
            } catch (const std::exception& e) {
                // 这里可以记录错误日志
                // std::cerr << "ThreadPool task exception: " << e.what() << std::endl;
            } catch (...) {
                // std::cerr << "ThreadPool unknown exception" << std::endl;
            }

            std::unique_lock<std::mutex> lock(queueMutex_);
            --activeThreads_;
            if (tasks_.empty() && activeThreads_.load() == 0) {
                finished_.notify_all();
            }
        }
    }
}

}  // namespace apollo