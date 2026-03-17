#include "apollo/base/id_pool.hpp"

#include <algorithm>

namespace apollo::base {

IdPool::IdPool(uint32_t min_id, uint32_t max_id) {
    init(min_id, max_id);
}

IdPool::IdPool(uint32_t size) {
    if (size > 0) {
        init(0, size - 1);
    }
}

uint32_t IdPool::usage_percent() const {
    if (size_ == 0) {
        return 0;
    }
    return (allocated_count_ * 100U) / size_;
}

void IdPool::init(uint32_t min_id, uint32_t max_id) {
    if (max_id < min_id) {
        max_id = min_id;
    }

    min_id_ = min_id;
    max_id_ = max_id;
    size_ = max_id - min_id + 1;

    const size_t bitmap_size = (size_ + 63U) / 64U;
    bitmap_.assign(bitmap_size, 0);

    nodes_.clear();
    nodes_.reserve(size_);
    for (uint32_t i = 0; i < size_; ++i) {
        auto node = std::make_unique<FreeNode>();
        node->id = min_id_ + i;
        nodes_.push_back(std::move(node));
    }

    for (uint32_t i = 0; i + 1 < size_; ++i) {
        nodes_[i]->next = nodes_[i + 1].get();
    }
    free_list_ = size_ > 0 ? nodes_[0].get() : nullptr;
    allocated_count_ = 0;
}

uint32_t IdPool::allocate() {
    if (free_list_ == nullptr) {
        return UINT32_MAX;
    }

    FreeNode* node = free_list_;
    free_list_ = node->next;
    node->next = nullptr;

    const uint32_t id = node->id;
    set_bit(id - min_id_);
    ++allocated_count_;
    return id;
}

void IdPool::release(uint32_t id) {
    if (!is_allocated(id)) {
        return;
    }

    const uint32_t index = id - min_id_;
    clear_bit(index);
    --allocated_count_;

    FreeNode* node = nodes_[index].get();
    node->next = free_list_;
    free_list_ = node;
}

bool IdPool::is_valid(uint32_t id) const {
    if (size_ == 0 || id < min_id_ || id > max_id_) {
        return false;
    }
    return !get_bit(id - min_id_);
}

bool IdPool::is_allocated(uint32_t id) const {
    if (size_ == 0 || id < min_id_ || id > max_id_) {
        return false;
    }
    return get_bit(id - min_id_);
}

void IdPool::reset() {
    std::fill(bitmap_.begin(), bitmap_.end(), 0);
    allocated_count_ = 0;
    for (uint32_t i = 0; i < size_; ++i) {
        nodes_[i]->id = min_id_ + i;
        nodes_[i]->next = (i + 1 < size_) ? nodes_[i + 1].get() : nullptr;
    }
    free_list_ = size_ > 0 ? nodes_[0].get() : nullptr;
}

bool IdPool::reserve(uint32_t id) {
    if (size_ == 0 || id < min_id_ || id > max_id_) {
        return false;
    }

    const uint32_t index = id - min_id_;
    if (get_bit(index)) {
        return false;
    }

    FreeNode* previous = nullptr;
    for (FreeNode* current = free_list_; current != nullptr; previous = current, current = current->next) {
        if (current->id != id) {
            continue;
        }

        if (previous != nullptr) {
            previous->next = current->next;
        } else {
            free_list_ = current->next;
        }

        current->next = nullptr;
        set_bit(index);
        ++allocated_count_;
        return true;
    }

    return false;
}

bool IdPool::get_bit(uint32_t index) const {
    const uint32_t word = index / 64U;
    const uint32_t bit = index % 64U;
    return (bitmap_[word] & (1ULL << bit)) != 0;
}

void IdPool::set_bit(uint32_t index) {
    const uint32_t word = index / 64U;
    const uint32_t bit = index % 64U;
    bitmap_[word] |= (1ULL << bit);
}

void IdPool::clear_bit(uint32_t index) {
    const uint32_t word = index / 64U;
    const uint32_t bit = index % 64U;
    bitmap_[word] &= ~(1ULL << bit);
}

} // namespace apollo::base
