#pragma once

#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <thread>

namespace apollo {
namespace utils {

//==============================================================================
// SpinLock - 自旋锁
//==============================================================================

/**
 * @brief 自旋锁
 *
 * 适用于短临界区的轻量级锁
 */
class SpinLock {
public:
    SpinLock() = default;
    ~SpinLock() = default;

    SpinLock(const SpinLock&) = delete;
    SpinLock& operator=(const SpinLock&) = delete;

    void lock() {
        while (flag_.test_and_set(std::memory_order_acquire)) {
            // 自旋等待
            std::this_thread::yield();
        }
    }

    bool tryLock() {
        return !flag_.test_and_set(std::memory_order_acquire);
    }

    void unlock() {
        flag_.clear(std::memory_order_release);
    }

private:
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

//==============================================================================
// ScopedLock - RAII 锁守卫
//==============================================================================

/**
 * @brief 自旋锁的 RAII 守卫
 */
class ScopedSpinLock {
public:
    explicit ScopedSpinLock(SpinLock& lock) : lock_(lock) {
        lock_.lock();
    }

    ~ScopedSpinLock() {
        lock_.unlock();
    }

    ScopedSpinLock(const ScopedSpinLock&) = delete;
    ScopedSpinLock& operator=(const ScopedSpinLock&) = delete;

private:
    SpinLock& lock_;
};

//==============================================================================
// RwLock - 读写锁
//==============================================================================

/**
 * @brief 读写锁
 *
 * 多个读者或单个写者
 */
class RwLock {
public:
    RwLock() = default;
    ~RwLock() = default;

    RwLock(const RwLock&) = delete;
    RwLock& operator=(const RwLock&) = delete;

    void lockRead() {
        std::shared_lock<std::shared_mutex> lock(mutex_);
    }

    void lockWrite() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
    }

    bool tryLockRead() {
        // 使用 try_lock_shared 需要自定义实现
        mutex_.lock_shared();
        return true;
    }

    bool tryLockWrite() {
        return mutex_.try_lock();
    }

    void unlock() {
        mutex_.unlock();
    }

private:
    std::shared_mutex mutex_;
};

/**
 * @brief 读锁的 RAII 守卫
 */
class ReadLockGuard {
public:
    explicit ReadLockGuard(std::shared_mutex& mutex) : mutex_(mutex) {
        mutex_.lock_shared();
    }

    ~ReadLockGuard() {
        mutex_.unlock_shared();
    }

    ReadLockGuard(const ReadLockGuard&) = delete;
    ReadLockGuard& operator=(const ReadLockGuard&) = delete;

private:
    std::shared_mutex& mutex_;
};

/**
 * @brief 写锁的 RAII 守卫
 */
class WriteLockGuard {
public:
    explicit WriteLockGuard(std::shared_mutex& mutex) : mutex_(mutex) {
        mutex_.lock();
    }

    ~WriteLockGuard() {
        mutex_.unlock();
    }

    WriteLockGuard(const WriteLockGuard&) = delete;
    WriteLockGuard& operator=(const WriteLockGuard&) = delete;

private:
    std::shared_mutex& mutex_;
};

//==============================================================================
// AtomicFlag - 轻量级原子标志
//==============================================================================

/**
 * @brief 原子标志
 *
 * 用于简单的线程间信号
 */
class AtomicFlag {
public:
    AtomicFlag() = default;

    void set() { flag_.store(true, std::memory_order_release); }
    void clear() { flag_.store(false, std::memory_order_release); }
    bool test() const { return flag_.load(std::memory_order_acquire); }
    bool testAndSet() { return flag_.test_and_set(std::memory_order_acq_rel); }

private:
    std::atomic<bool> flag_{false};
};

//==============================================================================
// SeqLock - 顺序锁
//==============================================================================

/**
 * @brief 顺序锁
 *
 * 允许读者无锁访问，适用于读多写少场景
 */
class SeqLock {
public:
    SeqLock() = default;

    // 写者：获取锁
    void lock() {
        uint32_t expected;
        do {
            expected = sequence_.load(std::memory_order_acquire) & ~1u;
            if (expected & 1u) {
                // 有其他写者在操作，等待
                std::this_thread::yield();
                continue;
            }
        } while (!sequence_.compare_exchange_weak(
            expected, expected + 1,
            std::memory_order_acq_rel,
            std::memory_order_acquire
        ));
    }

    // 写者：释放锁
    void unlock() {
        sequence_.fetch_add(1, std::memory_order_release);
    }

    // 读者：开始读取
    uint32_t readBegin() const {
        return sequence_.load(std::memory_order_acquire);
    }

    // 读者：验证读取是否有效
    bool readRetry(uint32_t version) const {
        return version != sequence_.load(std::memory_order_acquire);
    }

    // 检查是否在写入
    bool isWriting() const {
        return sequence_.load(std::memory_order_acquire) & 1u;
    }

private:
    std::atomic<uint32_t> sequence_{0};
};

} // namespace utils
} // namespace apollo
