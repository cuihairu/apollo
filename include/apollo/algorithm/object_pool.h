/**
 * @file object_pool.h
 * @brief 高性能对象池实现
 *
 * 对象池用于重用对象，减少频繁的内存分配/释放开销。
 *
 * 适用场景：
 * - 定时器对象复用
 * - 网络包缓冲区
 * - 游戏实体（子弹、特效等）
 * - 临时事件对象
 *
 * 特性：
 * - 线程安全（可选）
 * - 自动扩容
 * - 对象构造/析构控制
 * - 统计信息
 */

#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <stack>
#include <queue>
#include <mutex>
#include <atomic>
#include <functional>
#include <type_traits>
#include <initializer_list>

namespace apollo {
namespace utils {

//==============================================================================
// 对象池配置
//==============================================================================

/**
 * @brief 对象池配置
 */
struct ObjectPoolConfig {
    size_t initialCapacity = 16;      // 初始容量
    size_t maxCapacity = 1024;        // 最大容量
    size_t growthFactor = 2;          // 扩容因子
    bool threadSafe = true;           // 是否线程安全
    bool enableStats = false;         // 启用统计
};

//==============================================================================
// 基础对象池
//==============================================================================

/**
 * @brief 通用对象池
 *
 * @tparam T 对象类型
 * @tparam ThreadSafe 是否线程安全
 */
template <
    typename T,
    bool ThreadSafe = true
>
class ObjectPool {
public:
    using ValueType = T;
    using Pointer = std::unique_ptr<T, std::function<void(T*)>>;

    //==========================================================================
    // 构造函数
    //==========================================================================

    /**
     * @brief 默认构造
     */
    ObjectPool()
        : ObjectPool(ObjectPoolConfig{}) {
    }

    /**
     * @brief 使用配置构造
     */
    explicit ObjectPool(const ObjectPoolConfig& config)
        : config_(config)
        , allocCount_(0)
        , freeCount_(0)
        , hitCount_(0)
        , missCount_(0) {

        // 预分配对象
        for (size_t i = 0; i < config_.initialCapacity; ++i) {
            free_.push(createObject());
        }
        freeCount_.store(free_.size(), std::memory_order_relaxed);
    }

    /**
     * @brief 析构函数
     */
    ~ObjectPool() {
        clear();
    }

    //==========================================================================
    // 禁止拷贝和移动
    //==========================================================================

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    ObjectPool(ObjectPool&&) noexcept = delete;
    ObjectPool& operator=(ObjectPool&&) noexcept = delete;

    //==========================================================================
    // 对象获取
    //==========================================================================

    /**
     * @brief 获取一个对象（使用智能指针自动归还）
     *
     * 使用示例：
     * @code
     * auto obj = pool.acquire();
     * obj->doSomething();
     * // obj 离开作用域时自动归还
     * @endcode
     */
    Pointer acquire() {
        T* raw = allocate();

        // 创建自定义删除器，自动归还对象到池
        return Pointer(raw, [this](T* ptr) {
            deallocate(ptr);
        });
    }

    /**
     * @brief 获取原始指针（需要手动归还）
     */
    T* allocate() {
        T* obj = nullptr;

        if constexpr (ThreadSafe) {
            std::lock_guard lock(mutex_);
            obj = allocateImpl();
        } else {
            obj = allocateImpl();
        }

        if (obj) {
            hitCount_.fetch_add(1, std::memory_order_relaxed);
        } else {
            missCount_.fetch_add(1, std::memory_order_relaxed);
        }

        return obj;
    }

    //==========================================================================
    // 对象归还
    //==========================================================================

    /**
     * @brief 归还对象到池
     */
    void deallocate(T* obj) {
        if (!obj) {
            return;
        }

        // 可选：重置对象状态
        if constexpr (std::is_same_v<T, void>) {
            // 无法重置 void
        } else if constexpr (requires { obj->reset(); }) {
            obj->reset();
        }

        if constexpr (ThreadSafe) {
            std::lock_guard lock(mutex_);
            deallocateImpl(obj);
        } else {
            deallocateImpl(obj);
        }
    }

    //==========================================================================
    // 容量管理
    //==========================================================================

    /**
     * @brief 清空对象池
     */
    void clear() {
        if constexpr (ThreadSafe) {
            std::lock_guard lock(mutex_);
            clearImpl();
        } else {
            clearImpl();
        }
    }

    /**
     * @brief 预分配更多对象
     */
    void reserve(size_type count) {
        if constexpr (ThreadSafe) {
            std::lock_guard lock(mutex_);
            reserveImpl(count);
        } else {
            reserveImpl(count);
        }
    }

    /**
     * @brief 收缩到当前使用量
     */
    void shrink() {
        if constexpr (ThreadSafe) {
            std::lock_guard lock(mutex_);
            shrinkImpl();
        } else {
            shrinkImpl();
        }
    }

    //==========================================================================
    // 状态查询
    //==========================================================================

    /**
     * @brief 获取当前空闲对象数量
     */
    size_type freeCount() const {
        if constexpr (ThreadSafe) {
            return freeCount_.load(std::memory_order_acquire);
        } else {
            return freeCount_.load(std::memory_order_relaxed);
        }
    }

    /**
     * @brief 获取总分配对象数量
     */
    size_type allocCount() const {
        return allocCount_.load(std::memory_order_relaxed);
    }

    /**
     * @brief 获取命中率
     */
    double hitRate() const {
        size_type hits = hitCount_.load(std::memory_order_relaxed);
        size_type misses = missCount_.load(std::memory_order_relaxed);
        size_type total = hits + misses;

        return (total > 0) ? static_cast<double>(hits) / total : 0.0;
    }

    /**
     * @brief 统计信息
     */
    struct Stats {
        size_type totalAllocated = 0;
        size_type currentlyFree = 0;
        size_type currentlyInUse = 0;
        size_type cacheHits = 0;
        size_type cacheMisses = 0;
        double hitRate = 0.0;
    };

    Stats getStats() const {
        Stats stats;
        stats.totalAllocated = allocCount_.load(std::memory_order_relaxed);
        stats.currentlyFree = freeCount_.load(std::memory_order_acquire);
        stats.currentlyInUse = stats.totalAllocated - stats.currentlyFree;
        stats.cacheHits = hitCount_.load(std::memory_order_relaxed);
        stats.cacheMisses = missCount_.load(std::memory_order_relaxed);
        stats.hitRate = hitRate();
        return stats;
    }

private:
    using size_type = size_t;

    //==========================================================================
    // 内部实现（不加锁）
    //==========================================================================

    T* allocateImpl() {
        // 尝试从空闲队列获取
        if (!free_.empty()) {
            T* obj = free_.top();
            free_.pop();
            freeCount_.store(free_.size(), std::memory_order_relaxed);
            return obj;
        }

        // 检查是否可以扩容
        if (allocCount_.load(std::memory_order_relaxed) < config_.maxCapacity) {
            T* obj = createObject();
            allocCount_.fetch_add(1, std::memory_order_relaxed);
            return obj;
        }

        // 达到最大容量，返回 nullptr
        return nullptr;
    }

    void deallocateImpl(T* obj) {
        if (free_.size() < config_.maxCapacity) {
            free_.push(obj);
            freeCount_.store(free_.size(), std::memory_order_relaxed);
        } else {
            // 超过最大容量，直接删除
            delete obj;
            allocCount_.fetch_sub(1, std::memory_order_relaxed);
        }
    }

    void clearImpl() {
        while (!free_.empty()) {
            delete free_.top();
            free_.pop();
        }
        free_.clear();
        allocCount_.store(0, std::memory_order_relaxed);
        freeCount_.store(0, std::memory_order_relaxed);
    }

    void reserveImpl(size_type count) {
        size_type currentFree = free_.size();
        if (count <= currentFree) {
            return;
        }

        size_type toAdd = std::min(count - currentFree,
                                   config_.maxCapacity - allocCount_.load(std::memory_order_relaxed));

        for (size_type i = 0; i < toAdd; ++i) {
            free_.push(createObject());
            allocCount_.fetch_add(1, std::memory_order_relaxed);
        }

        freeCount_.store(free_.size(), std::memory_order_relaxed);
    }

    void shrinkImpl() {
        size_type targetSize = allocCount_.load(std::memory_order_relaxed) / 2;

        while (free_.size() > targetSize) {
            delete free_.top();
            free_.pop();
            allocCount_.fetch_sub(1, std::memory_order_relaxed);
        }

        freeCount_.store(free_.size(), std::memory_order_relaxed);
    }

    /**
     * @brief 创建新对象
     */
    T* createObject() {
        return new T();
    }

    ObjectPoolConfig config_;
    std::stack<T*> free_;

    mutable std::conditional_t<ThreadSafe, std::mutex, struct EmptyMutex> mutex_;

    std::atomic<size_type> allocCount_;
    std::atomic<size_type> freeCount_;
    std::atomic<size_type> hitCount_;
    std::atomic<size_type> missCount_;
};

//==============================================================================
// 空互斥锁（用于单线程版本）
//==============================================================================

template<>
struct ObjectPool<void, false>::EmptyMutex {
    void lock() const noexcept {}
    void unlock() const noexcept {}
};

//==============================================================================
// 专用对象池（带自定义构造函数）
//==============================================================================

/**
 * @brief 支持自定义构造函数的对象池
 *
 * @tparam T 对象类型
 * @tparam ThreadSafe 是否线程安全
 */
template <
    typename T,
    bool ThreadSafe = true
>
class CustomObjectPool {
public:
    using Factory = std::function<T*()>;
    using Resetter = std::function<void(T*)>;

    /**
     * @brief 构造函数
     * @param factory 对象工厂函数
     * @param resetter 对象重置函数
     * @param config 池配置
     */
    CustomObjectPool(
        Factory factory,
        Resetter resetter = nullptr,
        const ObjectPoolConfig& config = ObjectPoolConfig{})
        : factory_(std::move(factory))
        , resetter_(std::move(resetter))
        , config_(config) {

        for (size_t i = 0; i < config_.initialCapacity; ++i) {
            free_.push(factory_());
        }
    }

    /**
     * @brief 析构函数
     */
    ~CustomObjectPool() {
        clear();
    }

    /**
     * @brief 获取对象
     */
    std::unique_ptr<T, std::function<void(T*)>> acquire() {
        T* raw = allocate();

        return std::unique_ptr<T, std::function<void(T*)>>(
            raw,
            [this](T* ptr) { deallocate(ptr); }
        );
    }

    T* allocate() {
        T* obj = nullptr;

        if constexpr (ThreadSafe) {
            std::lock_guard lock(mutex_);
            obj = allocateImpl();
        } else {
            obj = allocateImpl();
        }

        return obj;
    }

    void deallocate(T* obj) {
        if (!obj) {
            return;
        }

        if (resetter_) {
            resetter_(obj);
        }

        if constexpr (ThreadSafe) {
            std::lock_guard lock(mutex_);
            deallocateImpl(obj);
        } else {
            deallocateImpl(obj);
        }
    }

    void clear() {
        if constexpr (ThreadSafe) {
            std::lock_guard lock(mutex_);
            clearImpl();
        } else {
            clearImpl();
        }
    }

private:
    using size_type = size_t;

    T* allocateImpl() {
        if (!free_.empty()) {
            T* obj = free_.top();
            free_.pop();
            return obj;
        }

        if (allocCount_ < config_.maxCapacity) {
            ++allocCount_;
            return factory_();
        }

        return nullptr;
    }

    void deallocateImpl(T* obj) {
        if (free_.size() < config_.maxCapacity) {
            free_.push(obj);
        } else {
            delete obj;
            --allocCount_;
        }
    }

    void clearImpl() {
        while (!free_.empty()) {
            delete free_.top();
            free_.pop();
        }
        allocCount_ = 0;
    }

    Factory factory_;
    Resetter resetter_;
    ObjectPoolConfig config_;
    std::stack<T*> free_;
    mutable std::conditional_t<ThreadSafe, std::mutex, struct EmptyMutex> mutex_;
    size_type allocCount_ = 0;
};

//==============================================================================
// 定长对象池（无锁、高性能）
//==============================================================================

/**
 * @brief 定长环形对象池
 *
 * 适用于容量固定、性能要求极高的场景。
 * 使用环形缓冲区实现，无锁（仅适用于单生产者单消费者或单线程场景）。
 *
 * @tparam T 对象类型
 * @tparam Capacity 固定容量（必须是 2 的幂）
 */
template <
    typename T,
    size_t Capacity
>
class FixedObjectPool {
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of 2");

public:
    static constexpr size_t capacity = Capacity;

    FixedObjectPool() : head_(0), tail_(0) {
        // 预构造所有对象
        for (size_t i = 0; i < Capacity; ++i) {
            free_[i] = new (&storage_[i]) T();
        }
    }

    ~FixedObjectPool() {
        // 析构所有对象
        for (size_t i = 0; i < Capacity; ++i) {
            if (free_[i]) {
                free_[i]->~T();
            }
        }
    }

    /**
     * @brief 获取对象
     */
    T* allocate() {
        size_t h = head_.load(std::memory_order_acquire);
        size_t t = tail_.load(std::memory_order_acquire);

        if (h - t >= Capacity) {
            return nullptr;  // 池已空
        }

        T* obj = free_[h & (Capacity - 1)];
        head_.store(h + 1, std::memory_order_release);
        return obj;
    }

    /**
     * @brief 归还对象
     */
    void deallocate(T* obj) {
        size_t t = tail_.load(std::memory_order_acquire);
        size_t h = head_.load(std::memory_order_acquire);

        if (h - t >= Capacity) {
            return;  // 池已满，丢弃
        }

        free_[t & (Capacity - 1)] = obj;
        tail_.store(t + 1, std::memory_order_release);
    }

    /**
     * @brief 获取可用对象数量
     */
    size_t available() const {
        size_t h = head_.load(std::memory_order_acquire);
        size_t t = tail_.load(std::memory_order_acquire);
        return h - t;
    }

private:
    using Storage = std::aligned_storage_t<sizeof(T), alignof(T)>;

    Storage storage_[Capacity];
    T* free_[Capacity];
    std::atomic<size_t> head_;  // 分配位置
    std::atomic<size_t> tail_;  // 释放位置
};

//==============================================================================
// 辅助别名
//==============================================================================

// 线程安全版本
template<typename T>
using ThreadPool = ObjectPool<T, true>;

// 单线程版本
template<typename T>
using LocalPool = ObjectPool<T, false>;

} // namespace utils
} // namespace apollo
