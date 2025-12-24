/**
 * @file distributed_lock.cpp
 * @brief 分布式锁实现
 */

#include "apollo/core/distributed_lock.h"
#include "apollo/utils/time.h"
#include "apollo/utils/random.h"
#include <thread>
#include <sstream>
#include <iomanip>

namespace apollo {
namespace core {

//==============================================================================
// RedisDistributedLock 实现
//==============================================================================

RedisDistributedLock::RedisDistributedLock(void* redisClient,
                                           const std::string& key,
                                           const std::string& value)
    : redisClient_(redisClient)
    , key_(key)
    , value_(value.empty() ? LockHelper::generateLockValue() : value)
    , locked_(false)
    , renewRunning_(false)
    , lockExpiryTime_(0) {
}

RedisDistributedLock::~RedisDistributedLock() {
    unlock();
}

bool RedisDistributedLock::tryLock() {
    return acquireLock(options_.expiryMs);
}

bool RedisDistributedLock::lock(int timeoutMs) {
    uint64_t deadline = timeoutMs > 0
        ? Time::now() + timeoutMs
        : UINT64_MAX;

    int retries = 0;
    while (true) {
        if (acquireLock(options_.expiryMs)) {
            if (options_.autoRenew) {
                startAutoRenew();
            }
            return true;
        }

        // 检查超时
        if (Time::now() >= deadline) {
            return false;
        }

        // 检查重试次数
        if (options_.maxRetries >= 0 && ++retries > options_.maxRetries) {
            return false;
        }

        // 等待重试
        std::this_thread::sleep_for(std::chrono::milliseconds(options_.retryIntervalMs));
    }
}

bool RedisDistributedLock::unlock() {
    stopAutoRenew();
    return releaseLock();
}

bool RedisDistributedLock::renew(uint64_t addTimeMs) {
    // 实际实现需要调用 Redis EXPIRE 命令
    // 这里简化处理
    lockExpiryTime_ = Time::now() + addTimeMs;
    return true;
}

int64_t RedisDistributedLock::getRemainingTime() const {
    if (!locked_) {
        return -1;
    }
    int64_t remaining = static_cast<int64_t>(lockExpiryTime_) - static_cast<int64_t>(Time::now());
    return remaining > 0 ? remaining : 0;
}

bool RedisDistributedLock::acquireLock(uint64_t expiryMs) {
    // 实际实现使用 SET key value NX PX 命令
    // SET key value NX PX milliseconds
    // 这里模拟成功获取锁

    // 模拟：90% 的成功率
    utils::Random rng;
    bool success = rng.nextDouble() > 0.1;

    if (success) {
        locked_ = true;
        lockExpiryTime_ = Time::now() + expiryMs;
    }

    return success;
}

bool RedisDistributedLock::releaseLock() {
    if (!locked_) {
        return false;
    }

    // 实际实现需要使用 Lua 脚本确保只删除自己持有的锁
    // if redis.call("get", KEYS[1]) == ARGV[1] then
    //     return redis.call("del", KEYS[1])
    // else
    //     return 0
    // end

    locked_ = false;
    lockExpiryTime_ = 0;

    return true;
}

void RedisDistributedLock::startAutoRenew() {
    if (renewRunning_) {
        return;
    }

    renewRunning_ = true;
    renewThread_ = std::thread(&RedisDistributedLock::autoRenewLoop, this);
}

void RedisDistributedLock::stopAutoRenew() {
    renewRunning_ = false;

    if (renewThread_.joinable()) {
        renewThread_.join();
    }
}

void RedisDistributedLock::autoRenewLoop() {
    while (renewRunning_ && locked_) {
        // 在锁过期前一半时间时续期
        int64_t remaining = getRemainingTime();
        if (remaining > 0 && remaining < static_cast<int64_t>(options_.expiryMs / 2)) {
            renew(options_.expiryMs);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(options_.expiryMs / 4));
    }
}

//==============================================================================
// DistributedLockFactory 实现
//==============================================================================

DistributedLockFactory& DistributedLockFactory::instance() {
    static DistributedLockFactory instance;
    return instance;
}

std::unique_ptr<IDistributedLock> DistributedLockFactory::createLock(const std::string& name) {
    return createLock(name, DistributedLockOptions());
}

std::unique_ptr<IDistributedLock> DistributedLockFactory::createLock(
    const std::string& name,
    const DistributedLockOptions& options) {

    std::string key = LockHelper::makeLockKey(name);
    auto lock = std::make_unique<RedisDistributedLock>(redisClient_, key);

    auto* redisLock = static_cast<RedisDistributedLock*>(lock.get());
    redisLock->setOptions(options);

    return lock;
}

void DistributedLockFactory::setRedisClient(void* redisClient) {
    redisClient_ = redisClient;
}

//==============================================================================
// LockGuard 实现
//==============================================================================

LockGuard::LockGuard(std::unique_ptr<IDistributedLock> lock)
    : lock_(std::move(lock)), owns_(false) {

    if (lock_) {
        owns_ = lock_->lock();
    }
}

LockGuard::~LockGuard() {
    if (owns_ && lock_) {
        lock_->unlock();
    }
}

//==============================================================================
// TryLockGuard 实现
//==============================================================================

TryLockGuard::TryLockGuard(std::unique_ptr<IDistributedLock> lock)
    : lock_(std::move(lock)), owns_(false) {

    if (lock_) {
        owns_ = lock_->tryLock();
    }
}

TryLockGuard::~TryLockGuard() {
    if (owns_ && lock_) {
        lock_->unlock();
    }
}

//==============================================================================
// LockHelper 实现
//==============================================================================

namespace LockHelper {

std::string generateLockValue() {
    // 生成唯一标识：机器ID + 进程ID + 线程ID + 时间戳 + 随机数
    std::ostringstream oss;

    uint64_t now = Time::now();
    utils::Random rng;

    oss << std::hex << std::setfill('0')
        << std::setw(16) << now
        << "-"
        << std::setw(8) << static_cast<uint32_t>(rng.next())
        << "-"
        << std::setw(8) << std::hash<std::thread::id>{}(std::this_thread::get_id());

    return oss.str();
}

std::string makeLockKey(const std::string& name, const std::string& prefix) {
    return prefix + name;
}

//==============================================================================
// RwLock 实现
//==============================================================================

RwLock::RwLock(const std::string& name)
    : readLockName_(name + ":read")
    , writeLockName_(name + ":write") {
}

bool RwLock::lockRead(int timeoutMs) {
    // 读锁可以通过计数器实现
    // 简化实现：直接返回成功
    return true;
}

bool RwLock::tryLockRead() {
    return lockRead(0);
}

void RwLock::unlockRead() {
    // 释放读锁
}

bool RwLock::lockWrite(int timeoutMs) {
    // 写锁需要等待所有读锁释放
    return true;
}

bool RwLock::tryLockWrite() {
    return lockWrite(0);
}

void RwLock::unlockWrite() {
    // 释放写锁
}

//==============================================================================
// Semaphore 实现
//==============================================================================

Semaphore::Semaphore(const std::string& name, int permits)
    : name_(name), maxPermits_(permits) {
}

bool Semaphore::acquire(int timeoutMs) {
    // 实际实现使用 Redis 计数器
    return true;
}

bool Semaphore::tryAcquire() {
    return acquire(0);
}

void Semaphore::release() {
    // 增加许可数
}

int Semaphore::availablePermits() const {
    // 实际实现从 Redis 获取
    return maxPermits_;
}

//==============================================================================
// CountDownLatch 实现
//==============================================================================

CountDownLatch::CountDownLatch(int count)
    : count_(count) {
}

bool CountDownLatch::await(int timeoutMs) {
    std::unique_lock<std::mutex> lock(mutex_);

    if (timeoutMs > 0) {
        return cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                           [this] { return count_ == 0; });
    } else {
        cv_.wait(lock, [this] { return count_ == 0; });
        return true;
    }
}

void CountDownLatch::countDown() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (count_ > 0) {
        --count_;
        if (count_ == 0) {
            cv_.notify_all();
        }
    }
}

int CountDownLatch::getCount() const {
    return count_;
}

} // namespace LockHelper

} // namespace core
} // namespace apollo
