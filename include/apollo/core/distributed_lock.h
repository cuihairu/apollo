#pragma once

#include <string>
#include <chrono>
#include <functional>
#include <memory>

namespace apollo {
namespace core {

/// 分布式锁接口
class IDistributedLock {
public:
    virtual ~IDistributedLock() = default;

    /// 尝试获取锁
    virtual bool tryLock() = 0;

    /// 获取锁（阻塞直到成功或超时）
    virtual bool lock(int timeoutMs = 0) = 0;

    /// 释放锁
    virtual bool unlock() = 0;

    /// 延长锁的过期时间
    virtual bool renew(uint64_t addTimeMs) = 0;

    /// 检查是否持有锁
    virtual bool isLocked() const = 0;

    /// 获取锁的剩余时间（毫秒）
    virtual int64_t getRemainingTime() const = 0;
};

/// 分布式锁选项
struct DistributedLockOptions {
    uint64_t expiryMs = 30000;           // 锁过期时间（毫秒）
    uint64_t retryIntervalMs = 100;      // 重试间隔
    int maxRetries = -1;                 // 最大重试次数（-1=无限）
    bool autoRenew = false;              // 自动续期
};

/// Redis 分布式锁实现（Redlock 算法简化版）
class RedisDistributedLock : public IDistributedLock {
public:
    /**
     * @brief 构造函数
     *
     * @param redisClient Redis 客户端（实际使用时传入真实的客户端）
     * @param key 锁的键名
     * @param value 锁的值（用于标识持有者，通常是唯一标识）
     */
    RedisDistributedLock(void* redisClient, const std::string& key,
                         const std::string& value = "");

    ~RedisDistributedLock() override;

    /// 设置选项
    void setOptions(const DistributedLockOptions& options) {
        options_ = options;
    }

    // IDistributedLock 实现
    bool tryLock() override;
    bool lock(int timeoutMs = 0) override;
    bool unlock() override;
    bool renew(uint64_t addTimeMs) override;
    bool isLocked() const override { return locked_; }
    int64_t getRemainingTime() const override;

private:
    bool acquireLock(uint64_t expiryMs);
    bool releaseLock();
    void startAutoRenew();
    void stopAutoRenew();
    void autoRenewLoop();

    void* redisClient_;  // 实际使用时是 RedisClient*
    std::string key_;
    std::string value_;
    DistributedLockOptions options_;
    std::atomic<bool> locked_;
    std::thread renewThread_;
    std::atomic<bool> renewRunning_;
    uint64_t lockExpiryTime_;
};

/// 分布式锁工厂
class DistributedLockFactory {
public:
    static DistributedLockFactory& instance();

    /**
     * @brief 创建锁
     *
     * @param name 锁名称
     * @return 锁对象
     */
    std::unique_ptr<IDistributedLock> createLock(const std::string& name);

    /**
     * @brief 创建带选项的锁
     */
    std::unique_ptr<IDistributedLock> createLock(
        const std::string& name,
        const DistributedLockOptions& options);

    /**
     * @brief 设置 Redis 客户端
     */
    void setRedisClient(void* redisClient);

private:
    DistributedLockFactory() = default;
    ~DistributedLockFactory() = default;

    void* redisClient_;
};

/// 锁守卫（RAII）
class LockGuard {
public:
    LockGuard(std::unique_ptr<IDistributedLock> lock);
    ~LockGuard();

    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;

    /// 检查是否获取到锁
    bool ownsLock() const { return owns_; }

private:
    std::unique_ptr<IDistributedLock> lock_;
    bool owns_;
};

/// 尝试锁守卫（可选获取锁）
class TryLockGuard {
public:
    TryLockGuard(std::unique_ptr<IDistributedLock> lock);
    ~TryLockGuard();

    TryLockGuard(const TryLockGuard&) = delete;
    TryLockGuard& operator=(const TryLockGuard&) = delete;

    /// 检查是否获取到锁
    bool ownsLock() const { return owns_; }

    explicit operator bool() const { return owns_; }

private:
    std::unique_ptr<IDistributedLock> lock_;
    bool owns_;
};

//==============================================================================
// 辅助函数
//==============================================================================

namespace LockHelper {

/// 生成唯一的锁值
std::string generateLockValue();

/// 生成锁的完整键名
std::string makeLockKey(const std::string& name,
                       const std::string& prefix = "lock:");

/// 读写锁（基于分布式锁实现）
class RwLock {
public:
    explicit RwLock(const std::string& name);

    /// 读锁
    bool lockRead(int timeoutMs = 0);
    bool tryLockRead();
    void unlockRead();

    /// 写锁
    bool lockWrite(int timeoutMs = 0);
    bool tryLockWrite();
    void unlockWrite();

private:
    std::string readLockName_;
    std::string writeLockName_;
};

/// 信号量（基于 Redis 实现）
class Semaphore {
public:
    Semaphore(const std::string& name, int permits);

    /// 获取许可
    bool acquire(int timeoutMs = 0);
    bool tryAcquire();

    /// 释放许可
    void release();

    /// 获取当前可用许可数
    int availablePermits() const;

private:
    std::string name_;
    int maxPermits_;
};

/// 倒计时门栓
class CountDownLatch {
public:
    explicit CountDownLatch(int count);

    /// 等待计数归零
    bool await(int timeoutMs = 0);

    /// 减少计数
    void countDown();

    /// 获取当前计数
    int getCount() const;

private:
    std::atomic<int> count_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

} // namespace LockHelper

} // namespace core
} // namespace apollo
