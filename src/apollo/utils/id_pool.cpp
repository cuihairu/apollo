/**
 * @file id_pool.cpp
 * @brief ID 池实现
 */

#include "apollo/utils/id_pool.h"
#include <algorithm>

namespace apollo {
namespace utils {

//==============================================================================
// IdPool 实现
//==============================================================================

IdPool::IdPool(uint32_t minId, uint32_t maxId)
    : minId_(minId), maxId_(maxId)
{
    init(minId, maxId);
}

IdPool::IdPool(uint32_t size)
    : minId_(0), maxId_(size > 0 ? size - 1 : 0)
{
    if (size > 0) {
        init(0, size - 1);
    }
}

void IdPool::init(uint32_t minId, uint32_t maxId) {
    if (maxId < minId) {
        maxId = minId;
    }

    minId_ = minId;
    maxId_ = maxId;
    size_ = maxId - minId + 1;

    // 计算需要的 bitmap 大小（每个 uint64_t 存储 64 位）
    size_t bitmapSize = (size_ + 63) / 64;
    bitmap_.assign(bitmapSize, 0);

    nodes_.clear();
    nodes_.reserve(size_);
    for (uint32_t i = 0; i < size_; ++i) {
        auto node = std::make_unique<FreeNode>();
        node->id = minId_ + i;
        node->next = nullptr;
        nodes_.push_back(std::move(node));
    }

    for (uint32_t i = 0; i + 1 < size_; ++i) {
        nodes_[i]->next = nodes_[i + 1].get();
    }
    freeList_ = (size_ > 0) ? nodes_[0].get() : nullptr;

    allocatedCount_ = 0;
}

uint32_t IdPool::allocate() {
    if (freeList_ == nullptr) {
        return UINT32_MAX;  // 池已满
    }

    FreeNode* node = freeList_;
    freeList_ = node->next;
    node->next = nullptr;
    uint32_t id = node->id;

    setBit(id - minId_);
    ++allocatedCount_;

    return id;
}

void IdPool::release(uint32_t id) {
    if (size_ == 0) {
        return;
    }
    if (id < minId_ || id > maxId_) {
        return;  // 无效 ID
    }

    uint32_t index = id - minId_;

    if (!getBit(index)) {
        return;  // 未分配
    }

    clearBit(index);
    --allocatedCount_;

    // 加入空闲链表（复用节点）
    FreeNode* node = nodes_[index].get();
    node->next = freeList_;
    freeList_ = node;
}

bool IdPool::isValid(uint32_t id) const {
    if (size_ == 0) {
        return false;
    }
    if (id < minId_ || id > maxId_) {
        return false;
    }
    return !getBit(id - minId_);
}

bool IdPool::isAllocated(uint32_t id) const {
    if (size_ == 0) {
        return false;
    }
    if (id < minId_ || id > maxId_) {
        return false;
    }
    return getBit(id - minId_);
}

void IdPool::reset() {
    std::fill(bitmap_.begin(), bitmap_.end(), 0);
    allocatedCount_ = 0;

    for (uint32_t i = 0; i < size_; ++i) {
        nodes_[i]->id = minId_ + i;
        nodes_[i]->next = (i + 1 < size_) ? nodes_[i + 1].get() : nullptr;
    }
    freeList_ = (size_ > 0) ? nodes_[0].get() : nullptr;
}

bool IdPool::reserve(uint32_t id) {
    if (size_ == 0) {
        return false;
    }
    if (id < minId_ || id > maxId_) {
        return false;
    }

    uint32_t index = id - minId_;

    if (getBit(index)) {
        return false;  // 已分配
    }

    // 从空闲链表中移除该 ID
    FreeNode* prev = nullptr;
    for (FreeNode* curr = freeList_; curr != nullptr; prev = curr, curr = curr->next) {
        if (curr->id == id) {
            if (prev) {
                prev->next = curr->next;
            } else {
                freeList_ = curr->next;
            }
            curr->next = nullptr;
            setBit(index);
            ++allocatedCount_;
            return true;
        }
    }

    return false;
}

bool IdPool::getBit(uint32_t index) const {
    uint32_t word = index / 64;
    uint32_t bit = index % 64;
    return (bitmap_[word] & (1ULL << bit)) != 0;
}

void IdPool::setBit(uint32_t index) {
    uint32_t word = index / 64;
    uint32_t bit = index % 64;
    bitmap_[word] |= (1ULL << bit);
}

void IdPool::clearBit(uint32_t index) {
    uint32_t word = index / 64;
    uint32_t bit = index % 64;
    bitmap_[word] &= ~(1ULL << bit);
}

} // namespace utils
} // namespace apollo
