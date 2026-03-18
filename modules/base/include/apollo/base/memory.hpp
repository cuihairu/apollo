#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace apollo::base {

// Simple buffer for byte operations
class ByteBuffer {
public:
    explicit ByteBuffer(size_t capacity = 1024)
        : buffer_(capacity), size_(0) {}

    size_t capacity() const { return buffer_.size(); }
    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    const uint8_t* data() const { return buffer_.data(); }
    uint8_t* data() { return buffer_.data(); }

    void clear() {
        size_ = 0;
    }

    void reserve(size_t new_capacity) {
        if (new_capacity > buffer_.size()) {
            buffer_.resize(new_capacity);
        }
    }

    void resize(size_t new_size) {
        reserve(new_size);
        size_ = new_size;
    }

    bool write(const void* data, size_t len) {
        if (size_ + len > buffer_.size()) {
            return false;
        }
        std::memcpy(buffer_.data() + size_, data, len);
        size_ += len;
        return true;
    }

    bool write_at(size_t offset, const void* data, size_t len) {
        if (offset + len > buffer_.size()) {
            return false;
        }
        std::memcpy(buffer_.data() + offset, data, len);
        if (offset + len > size_) {
            size_ = offset + len;
        }
        return true;
    }

    bool read(void* out, size_t len, size_t& offset) const {
        if (offset + len > size_) {
            return false;
        }
        std::memcpy(out, buffer_.data() + offset, len);
        offset += len;
        return true;
    }

    bool append(const ByteBuffer& other) {
        return write(other.data(), other.size());
    }

    bool append(const std::vector<uint8_t>& data) {
        return write(data.data(), data.size());
    }

    std::vector<uint8_t> to_vector() const {
        return std::vector<uint8_t>(buffer_.begin(), buffer_.begin() + size_);
    }

private:
    std::vector<uint8_t> buffer_;
    size_t size_;
};

// Simple memory pool for fixed-size allocations
template <size_t BlockSize, size_t BlockCount = 1024>
class FixedMemoryPool {
public:
    static_assert(BlockSize >= sizeof(void*), "BlockSize must be at least sizeof(void*)");

    FixedMemoryPool() : free_list_(nullptr), allocated_count_(0) {
        for (size_t i = 0; i < BlockCount; ++i) {
            auto* block = reinterpret_cast<Block*>(&blocks_[i]);
            block->next = free_list_;
            free_list_ = block;
        }
    }

    ~FixedMemoryPool() = default;

    void* allocate() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!free_list_) {
            return nullptr;  // Pool exhausted
        }

        auto* block = free_list_;
        free_list_ = free_list_->next;
        ++allocated_count_;
        return block;
    }

    void deallocate(void* ptr) {
        if (!ptr) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        auto* block = reinterpret_cast<Block*>(ptr);
        block->next = free_list_;
        free_list_ = block;
        --allocated_count_;
    }

    size_t allocated_count() const {
        return allocated_count_.load(std::memory_order_relaxed);
    }

    size_t capacity() const {
        return BlockCount;
    }

    size_t block_size() const {
        return BlockSize;
    }

private:
    struct Block {
        Block* next = nullptr;
        alignas(std::max_align_t) uint8_t data[BlockSize];
    };

    std::mutex mutex_;
    Block* free_list_;
    std::atomic<size_t> allocated_count_;
    std::array<Block, BlockCount> blocks_;
};

// Arena allocator for temporary allocations
class ArenaAllocator {
public:
    explicit ArenaAllocator(size_t initial_capacity = 4096)
        : blocks_(), current_block_(nullptr), current_offset_(0), current_capacity_(0) {
        allocate_block(initial_capacity);
    }

    ~ArenaAllocator() {
        for (auto& block : blocks_) {
            ::operator delete(block.data, block.size);
        }
    }

    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) {
        size_t aligned_offset = (current_offset_ + alignment - 1) & ~(alignment - 1);

        if (aligned_offset + size > current_capacity_) {
            // Need a new block
            const size_t new_capacity = std::max(current_capacity_ * 2, size + alignment);
            allocate_block(new_capacity);
            current_offset_ = 0;
            aligned_offset = 0;
        }

        void* ptr = static_cast<uint8_t*>(current_block_) + aligned_offset;
        current_offset_ = aligned_offset + size;
        return ptr;
    }

    template <typename T, typename... Args>
    T* construct(Args&&... args) {
        void* ptr = allocate(sizeof(T), alignof(T));
        return new (ptr) T(std::forward<Args>(args)...);
    }

    void reset() {
        // Reset to first block
        if (!blocks_.empty()) {
            current_block_ = blocks_[0].data;
            current_capacity_ = blocks_[0].size;
            current_offset_ = 0;
        }
    }

    size_t total_capacity() const {
        size_t total = 0;
        for (const auto& block : blocks_) {
            total += block.size;
        }
        return total;
    }

    size_t current_usage() const {
        size_t usage = 0;
        for (size_t i = 0; i < blocks_.size() - 1; ++i) {
            usage += blocks_[i].size;
        }
        usage += current_offset_;
        return usage;
    }

private:
    struct BlockInfo {
        void* data = nullptr;
        size_t size = 0;
    };

    void allocate_block(size_t capacity) {
        void* data = ::operator new(capacity);
        blocks_.push_back({data, capacity});
        current_block_ = data;
        current_capacity_ = capacity;
        current_offset_ = 0;
    }

    std::vector<BlockInfo> blocks_;
    void* current_block_;
    size_t current_offset_;
    size_t current_capacity_;
};

} // namespace apollo::base
