#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace apollo::base {

class IdPool {
public:
    IdPool() = default;
    IdPool(uint32_t min_id, uint32_t max_id);
    explicit IdPool(uint32_t size);

    ~IdPool() = default;

    IdPool(const IdPool&) = delete;
    IdPool& operator=(const IdPool&) = delete;
    IdPool(IdPool&&) noexcept = default;
    IdPool& operator=(IdPool&&) noexcept = default;

    uint32_t allocate();
    void release(uint32_t id);
    bool is_valid(uint32_t id) const;
    bool is_allocated(uint32_t id) const;
    uint32_t allocated_count() const { return allocated_count_; }
    uint32_t capacity() const { return size_; }
    bool is_full() const { return allocated_count_ == size_; }
    bool is_empty() const { return allocated_count_ == 0; }
    uint32_t usage_percent() const;
    void reset();
    bool reserve(uint32_t id);

private:
    struct FreeNode {
        uint32_t id = 0;
        FreeNode* next = nullptr;
    };

    void init(uint32_t min_id, uint32_t max_id);
    bool get_bit(uint32_t index) const;
    void set_bit(uint32_t index);
    void clear_bit(uint32_t index);

    uint32_t min_id_ = 0;
    uint32_t max_id_ = 0;
    uint32_t size_ = 0;
    uint32_t allocated_count_ = 0;
    std::vector<uint64_t> bitmap_;
    FreeNode* free_list_ = nullptr;
    std::vector<std::unique_ptr<FreeNode>> nodes_;
};

template <typename T>
class ObjectPool {
public:
    explicit ObjectPool(uint32_t capacity)
        : min_id_(0), max_id_(capacity > 0 ? capacity - 1 : 0), pool_(capacity) {
        for (uint32_t i = 0; i < capacity; ++i) {
            pool_[i].next = (i + 1 < capacity) ? i + 1 : UINT32_MAX;
        }
        free_head_ = capacity > 0 ? 0 : UINT32_MAX;
        free_count_ = capacity;
    }

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    uint32_t allocate(T object) {
        if (free_count_ == 0 || free_head_ == UINT32_MAX) {
            return UINT32_MAX;
        }

        const uint32_t id = free_head_;
        auto& node = pool_[id];
        free_head_ = node.next;
        node.next = UINT32_MAX;
        node.object = std::move(object);
        --free_count_;
        return id + min_id_;
    }

    T* get(uint32_t id) {
        if (id < min_id_ || id > max_id_) {
            return nullptr;
        }

        auto& node = pool_[id - min_id_];
        if (node.next != UINT32_MAX) {
            return nullptr;
        }
        return &node.object;
    }

    std::unique_ptr<T> release(uint32_t id) {
        if (id < min_id_ || id > max_id_) {
            return nullptr;
        }

        const uint32_t index = id - min_id_;
        auto& node = pool_[index];
        if (node.next != UINT32_MAX) {
            return nullptr;
        }

        auto object = std::make_unique<T>(std::move(node.object));
        node.next = free_head_;
        free_head_ = index;
        ++free_count_;
        return object;
    }

    bool is_allocated(uint32_t id) const {
        if (id < min_id_ || id > max_id_) {
            return false;
        }
        return pool_[id - min_id_].next == UINT32_MAX;
    }

    uint32_t allocated_count() const {
        return capacity() - free_count_;
    }

    uint32_t capacity() const {
        return max_id_ >= min_id_ ? (max_id_ - min_id_ + 1) : 0;
    }

private:
    struct Node {
        T object{};
        uint32_t next = UINT32_MAX;
    };

    uint32_t min_id_ = 0;
    uint32_t max_id_ = 0;
    std::vector<Node> pool_;
    uint32_t free_head_ = UINT32_MAX;
    uint32_t free_count_ = 0;
};

} // namespace apollo::base
