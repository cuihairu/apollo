#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

namespace apollo::base {

class ThreadPool {
public:
    using Task = std::function<void()>;

    explicit ThreadPool(size_t num_threads = 0) {
        if (num_threads == 0) {
            num_threads = std::thread::hardware_concurrency();
            if (num_threads == 0) {
                num_threads = 4;
            }
        }

        for (size_t i = 0; i < num_threads; ++i) {
            threads_.emplace_back(&ThreadPool::worker_thread, this);
        }
    }

    ~ThreadPool() {
        stop();
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using ReturnType = std::invoke_result_t<F, Args...>;

        auto task_ptr = std::make_shared<std::packaged_task<ReturnType()>>(
            [f = std::forward<F>(f), tuple = std::make_tuple(std::forward<Args>(args)...)]() mutable {
                return std::apply([f](Args&&... args) -> ReturnType {
                    return std::invoke(f, std::forward<Args>(args)...);
                }, tuple);
            }
        );

        auto result = task_ptr->get_future();
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (stop_) {
                throw std::runtime_error("submit on stopped ThreadPool");
            }
            tasks_.emplace([task_ptr]() { (*task_ptr)(); });
        }
        condition_.notify_one();
        return result;
    }

    void enqueue(Task task) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (stop_) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            tasks_.push(std::move(task));
        }
        condition_.notify_one();
    }

    size_t thread_count() const {
        return threads_.size();
    }

    size_t pending_task_count() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return tasks_.size();
    }

    void wait_for_all() {
        std::unique_lock<std::mutex> lock(mutex_);
        finished_.wait(lock, [this] {
            return tasks_.empty() && active_workers_ == 0;
        });
    }

    void stop() {
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

    void stop_now() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            stop_ = true;
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

    bool is_stopped() const {
        return stop_;
    }

private:
    void worker_thread() {
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
                    ++active_workers_;
                }
            }

            if (task) {
                try {
                    task();
                } catch (...) {
                }

                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    --active_workers_;
                }
                finished_.notify_one();
            }
        }
    }

    std::vector<std::thread> threads_;
    std::queue<Task> tasks_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::condition_variable finished_;
    std::atomic<bool> stop_{false};
    std::atomic<size_t> active_workers_{0};
};

} // namespace apollo::base
