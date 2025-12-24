#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <mutex>
#include <atomic>
#include <memory>

namespace apollo {
namespace utils {

//==============================================================================
// MemoryPool - 固定大小内存池
//==============================================================================

/**
 * @brief 固定大小内存池
 *
 * 用于高效分配固定大小的内存块
 * 避免频繁的 malloc/free 调用
 */
class MemoryPool {
public:
    /**
     * @brief 构造函数
     *
     * @param blockSize 块大小（字节）
     * @param initialBlocks 初始块数量
     * @param growSize 扩容时的块数量
     */
    MemoryPool(size_t blockSize, size_t initialBlocks = 16, size_t growSize = 16)
        : blockSize_(blockSize)
        , growSize_(growSize)
        , allocatedCount_(0)
        , freeCount_(0) {
        expand(initialBlocks);
    }

    ~MemoryPool() {
        // 释放所有块
        for (auto& chunk : chunks_) {
            ::operator delete(chunk.data);
        }
    }

    // 禁止拷贝和移动
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    MemoryPool(MemoryPool&&) = delete;
    MemoryPool& operator=(MemoryPool&&) = delete;

    /**
     * @brief 分配内存块
     *
     * @return 内存块指针，如果失败返回 nullptr
     */
    void* allocate() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (freeList_ == nullptr) {
            if (!expand(growSize_)) {
                return nullptr;  // 扩容失败
            }
        }

        void* block = freeList_;
        freeList_ = *static_cast<void**>(freeList_);
        ++allocatedCount_;
        --freeCount_;
        return block;
    }

    /**
     * @brief 释放内存块
     *
     * @param ptr 要释放的指针
     */
    void deallocate(void* ptr) {
        if (ptr == nullptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        *static_cast<void**>(ptr) = freeList_;
        freeList_ = ptr;
        --allocatedCount_;
        ++freeCount_;
    }

    /**
     * @brief 获取块大小
     */
    size_t getBlockSize() const {
        return blockSize_;
    }

    /**
     * @brief 获取已分配块数量
     */
    size_t getAllocatedCount() const {
        return allocatedCount_;
    }

    /**
     * @brief 获取空闲块数量
     */
    size_t getFreeCount() const {
        return freeCount_;
    }

    /**
     * @brief 获取总块数量
     */
    size_t getTotalCount() const {
        return allocatedCount_ + freeCount_;
    }

private:
    struct Chunk {
        void* data;
        size_t size;
    };

    bool expand(size_t blockCount) {
        size_t chunkSize = blockSize_ * blockCount;
        void* chunk = ::operator new(chunkSize, std::nothrow_t{});

        if (chunk == nullptr) {
            return false;
        }

        // 将新块加入空闲链表
        char* ptr = static_cast<char*>(chunk);
        for (size_t i = 0; i < blockCount; ++i) {
            *reinterpret_cast<void**>(ptr) = freeList_;
            freeList_ = ptr;
            ptr += blockSize_;
        }

        chunks_.push_back({chunk, chunkSize});
        freeCount_ += blockCount;
        return true;
    }

    size_t blockSize_;
    size_t growSize_;
    std::atomic<size_t> allocatedCount_;
    std::atomic<size_t> freeCount_;
    void* freeList_ = nullptr;
    std::vector<Chunk> chunks_;
    std::mutex mutex_;
};

//==============================================================================
// PoolAllocator - STL 分配器适配器
//==============================================================================

/**
 * @brief 内存池分配器适配器
 *
 * 可以与 STL 容器配合使用
 */
template<typename T>
class PoolAllocator {
public:
    using value_type = T;

    explicit PoolAllocator(MemoryPool& pool) : pool_(&pool) {}

    template<typename U>
    PoolAllocator(const PoolAllocator<U>& other) noexcept : pool_(other.pool_) {}

    T* allocate(size_t n) {
        if (n != 1 || sizeof(T) > pool_->getBlockSize()) {
            throw std::bad_alloc();
        }
        return static_cast<T*>(pool_->allocate());
    }

    void deallocate(T* ptr, size_t) noexcept {
        pool_->deallocate(ptr);
    }

    template<typename U>
    friend class PoolAllocator;

private:
    MemoryPool* pool_;
};

template<typename T, typename U>
bool operator==(const PoolAllocator<T>& lhs, const PoolAllocator<U>& rhs) noexcept {
    return lhs.pool_ == rhs.pool_;
}

template<typename T, typename U>
bool operator!=(const PoolAllocator<T>& lhs, const PoolAllocator<U>& rhs) noexcept {
    return !(lhs == rhs);
}

//==============================================================================
// ArenaAllocator - 区域分配器
//==============================================================================

/**
 * @brief 区域分配器
 *
 * 一次性分配大块内存，批量释放
 * 适用于临时对象分配场景
 */
class ArenaAllocator {
public:
    /**
     * @brief 构造函数
     *
     * @param initialSize 初始区域大小
     * @param growSize 扩容大小
     */
    explicit ArenaAllocator(size_t initialSize = 4096, size_t growSize = 4096)
        : growSize_(growSize)
        , currentPtr_(nullptr)
        , currentEnd_(nullptr)
        , usedSize_(0) {
        allocateBlock(initialSize);
    }

    ~ArenaAllocator() {
        for (auto& block : blocks_) {
            ::operator delete(block.data);
        }
    }

    // 禁止拷贝和移动
    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;

    /**
     * @brief 分配内存
     *
     * @param size 字节数
     * @param alignment 对齐要求
     * @return 内存指针
     */
    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) {
        // 对齐当前指针
        uintptr_t ptr = reinterpret_cast<uintptr_t>(currentPtr_);
        uintptr_t aligned = (ptr + alignment - 1) & ~(alignment - 1);
        size_t padding = aligned - ptr;

        if (currentPtr_ + padding + size > currentEnd_) {
            // 需要分配新块
            size_t blockSize = (size < growSize_) ? growSize_ : size + alignment;
            if (!allocateBlock(blockSize)) {
                return nullptr;
            }
            return allocate(size, alignment);
        }

        currentPtr_ += padding;
        void* result = currentPtr_;
        currentPtr_ += size;
        usedSize_ += padding + size;
        return result;
    }

    /**
     * @brief 重置分配器
     *
     * 释放所有已分配内存（但保留底层块）
     */
    void reset() {
        if (blocks_.empty()) {
            return;
        }

        // 重置到第一个块
        currentPtr_ = static_cast<char*>(blocks_[0].data);
        currentEnd_ = currentPtr_ + blocks_[0].size;
        usedSize_ = 0;
    }

    /**
     * @brief 获取已使用内存大小
     */
    size_t getUsedSize() const {
        return usedSize_;
    }

    /**
     * @brief 获取总分配内存大小
     */
    size_t getTotalSize() const {
        size_t total = 0;
        for (const auto& block : blocks_) {
            total += block.size;
        }
        return total;
    }

private:
    struct Block {
        void* data;
        size_t size;
    };

    bool allocateBlock(size_t size) {
        void* block = ::operator new(size, std::nothrow_t{});
        if (block == nullptr) {
            return false;
        }

        blocks_.push_back({block, size});
        currentPtr_ = static_cast<char*>(block);
        currentEnd_ = currentPtr_ + size;
        return true;
    }

    size_t growSize_;
    char* currentPtr_;
    char* currentEnd_;
    size_t usedSize_;
    std::vector<Block> blocks_;
};

} // namespace utils
} // namespace apollo
