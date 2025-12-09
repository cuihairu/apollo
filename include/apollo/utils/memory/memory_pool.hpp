#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <cstddef>
#include <atomic>
#include <functional>
#include <stack>
#include <array>

namespace apollo {

/// 对象池 - 用于重用对象，减少内存分配
template<typename T>
class ObjectPool {
public:
    /// 构造函数
    /// @param initialSize 初始对象数量
    /// @param maxSize 最大对象数量
    explicit ObjectPool(size_t initialSize = 16, size_t maxSize = 256)
        : maxSize_(maxSize) {
        for (size_t i = 0; i < initialSize; ++i) {
            pool_.push_back(std::make_unique<T>());
        }
    }

    /// 获取对象
    std::unique_ptr<T, std::function<void(T*)>> Acquire() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!pool_.empty()) {
            auto obj = std::move(pool_.back());
            pool_.pop_back();

            // 自定义删除器，将对象归还到池中
            return std::unique_ptr<T, std::function<void(T*)>>(
                obj.release(),
                [this](T* ptr) {
                    std::lock_guard<std::mutex> lock(mutex_);
                    if (pool_.size() < maxSize_) {
                        pool_.push_back(std::unique_ptr<T>(ptr));
                    } else {
                        delete ptr;
                    }
                }
            );
        }

        // 池为空，创建新对象
        return std::unique_ptr<T, std::function<void(T*)>>(
            new T(),
            [this](T* ptr) {
                std::lock_guard<std::mutex> lock(mutex_);
                if (pool_.size() < maxSize_) {
                    pool_.push_back(std::unique_ptr<T>(ptr));
                } else {
                    delete ptr;
                }
            }
        );
    }

    /// 获取池中可用对象数量
    size_t GetAvailableCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_.size();
    }

    /// 清理池
    void Clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.clear();
    }

private:
    std::vector<std::unique_ptr<T>> pool_;
    mutable std::mutex mutex_;
    size_t maxSize_;
};

/// 内存块池 - 固定大小内存块管理
class MemoryPool {
public:
    /// 构造函数
    /// @param blockSize 内存块大小
    /// @param initialBlocks 初始内存块数量
    /// @param maxBlocks 最大内存块数量
    explicit MemoryPool(size_t blockSize, size_t initialBlocks = 16, size_t maxBlocks = 256)
        : blockSize_(blockSize), maxBlocks_(maxBlocks) {

        // 分配大块内存
        size_t chunkSize = blockSize_ * initialBlocks;
        chunk_ = std::make_unique<char[]>(chunkSize);

        // 初始化空闲列表
        for (size_t i = 0; i < initialBlocks; ++i) {
            auto* block = chunk_.get() + i * blockSize_;
            freeBlocks_.push(block);
        }
    }

    /// 分配内存块
    void* Allocate() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (freeBlocks_.empty()) {
            if (allocatedBlocks_ >= maxBlocks_) {
                return nullptr;  // 达到最大限制
            }

            // 分配新的内存块
            auto* block = new char[blockSize_];
            allocatedBlocks_++;
            return block;
        }

        auto* block = freeBlocks_.top();
        freeBlocks_.pop();
        return block;
    }

    /// 释放内存块
    void Deallocate(void* ptr) {
        if (!ptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        // 检查是否是池内内存
        char* p = static_cast<char*>(ptr);
        if (p >= chunk_.get() && p < chunk_.get() + blockSize_ * initialBlocks_) {
            freeBlocks_.push(p);
        } else {
            delete[] static_cast<char*>(ptr);
            allocatedBlocks_--;
        }
    }

    /// 获取内存块大小
    size_t GetBlockSize() const { return blockSize_; }

    /// 获取空闲内存块数量
    size_t GetFreeBlockCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return freeBlocks_.size();
    }

private:
    size_t blockSize_;
    size_t initialBlocks_;
    size_t maxBlocks_;
    std::unique_ptr<char[]> chunk_;
    std::stack<void*> freeBlocks_;
    std::atomic<size_t> allocatedBlocks_{0};
    mutable std::mutex mutex_;
};

/// 内存池管理器 - 管理多种大小的内存池
class MemoryPoolManager {
public:
    /// 获取单例
    static MemoryPoolManager& Instance() {
        static MemoryPoolManager instance;
        return instance;
    }

    /// 分配内存
    void* Allocate(size_t size) {
        // 调整到合适的块大小
        size_t adjustedSize = AdjustSize(size);
        size_t index = GetPoolIndex(adjustedSize);

        std::lock_guard<std::mutex> lock(mutex_);

        auto& pool = pools_[index];
        if (!pool) {
            // 创建新的内存池
            size_t initialBlocks = 16;
            size_t maxBlocks = 256;
            pool = std::make_unique<MemoryPool>(adjustedSize, initialBlocks, maxBlocks);
        }

        return pool->Allocate();
    }

    /// 释放内存
    void Deallocate(void* ptr, size_t size) {
        if (!ptr) {
            return;
        }

        size_t adjustedSize = AdjustSize(size);
        size_t index = GetPoolIndex(adjustedSize);

        std::lock_guard<std::mutex> lock(mutex_);

        auto& pool = pools_[index];
        if (pool) {
            pool->Deallocate(ptr);
        }
    }

    /// 获取统计信息
    struct PoolStats {
        size_t totalAllocated;
        size_t totalDeallocated;
        size_t currentUsage;
        size_t poolCount;
    };

    PoolStats GetStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        PoolStats stats{};
        stats.poolCount = pools_.size();
        return stats;
    }

private:
    MemoryPoolManager() = default;
    ~MemoryPoolManager() = default;

    // 调整大小到最近的2的幂
    size_t AdjustSize(size_t size) {
        if (size <= 8) return 8;
        if (size <= 16) return 16;
        if (size <= 32) return 32;
        if (size <= 64) return 64;
        if (size <= 128) return 128;
        if (size <= 256) return 256;
        if (size <= 512) return 512;
        if (size <= 1024) return 1024;
        if (size <= 2048) return 2048;
        if (size <= 4096) return 4096;
        return ((size + 4095) / 4096) * 4096;
    }

    // 获取池索引
    size_t GetPoolIndex(size_t size) {
        switch (size) {
            case 8:   return 0;
            case 16:  return 1;
            case 32:  return 2;
            case 64:  return 3;
            case 128: return 4;
            case 256: return 5;
            case 512: return 6;
            case 1024:return 7;
            case 2048:return 8;
            case 4096:return 9;
            default: return 10;
        }
    }

    mutable std::mutex mutex_;
    std::array<std::unique_ptr<MemoryPool>, 11> pools_;
};

}  // namespace apollo

/// 自定义分配器 - 使用内存池
template<typename T>
struct PoolAllocator {
    using value_type = T;

    PoolAllocator() noexcept {}

    template<typename U>
    PoolAllocator(const PoolAllocator<U>&) noexcept {}

    T* allocate(size_t n) {
        if (n == 1) {
            return static_cast<T*>(
                apollo::MemoryPoolManager::Instance().Allocate(sizeof(T))
            );
        }
        return new T[n];
    }

    void deallocate(T* ptr, size_t n) noexcept {
        if (n == 1) {
            apollo::MemoryPoolManager::Instance().Deallocate(ptr, sizeof(T));
        } else {
            delete[] ptr;
        }
    }

    template<typename U>
    bool operator==(const PoolAllocator<U>&) const noexcept {
        return true;
    }

    template<typename U>
    bool operator!=(const PoolAllocator<U>&) const noexcept {
        return false;
    }
};