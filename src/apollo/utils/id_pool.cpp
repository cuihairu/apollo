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
    bitmap_.resize(bitmapSize, 0);

    // 初始化空闲链表
    uint32_t nodeCount = (size_ < 1000) ? size_ : 1000;  // 限制初始节点数
    nodes_.reserve(nodeCount);

    for (uint32_t i = 0; i < size_; ++i) {
        if (i < nodeCount) {
            auto node = std::make_unique<FreeNode>();
            node->id = minId_ + i;
            node->next = (i + 1 < size_) ? i + 1 : UINT32_MAX;

            if (i == 0) {
                freeList_ = node.get();
            } else {
                nodes_[i - 1]->next = node.get();
            }

            nodes_.push_back(std::move(node));
        }
    }

    allocatedCount_ = 0;
}

uint32_t IdPool::allocate() {
    if (freeList_ == nullptr) {
        return UINT32_MAX;  // 池已满
    }

    uint32_t id = freeList_->id;
    freeList_ = reinterpret_cast<FreeNode*>(freeList_->next);

    setBit(id - minId_);
    ++allocatedCount_;

    return id;
}

void IdPool::release(uint32_t id) {
    if (id < minId_ || id > maxId_) {
        return;  // 无效 ID
    }

    uint32_t index = id - minId_;

    if (!getBit(index)) {
        return;  // 未分配
    }

    clearBit(index);
    --allocatedCount_;

    // 加入空闲链表
    auto node = std::make_unique<FreeNode>();
    node->id = id;
    node->next = reinterpret_cast<uintptr_t>(freeList_);
    nodes_.push_back(std::move(node));
    freeList_ = nodes_.back().get();
}

bool IdPool::isValid(uint32_t id) const {
    return id >= minId_ && id <= maxId_ && getBit(id - minId_);
}

bool IdPool::isAllocated(uint32_t id) const {
    if (id < minId_ || id > maxId_) {
        return false;
    }
    return getBit(id - minId_);
}

void IdPool::reset() {
    std::fill(bitmap_.begin(), bitmap_.end(), 0);
    nodes_.clear();
    freeList_ = nullptr;
    allocatedCount_ = 0;

    // 重新初始化
    uint32_t nodeCount = (size_ < 1000) ? size_ : 1000;
    nodes_.reserve(nodeCount);

    for (uint32_t i = 0; i < size_; ++i) {
        if (i < nodeCount) {
            auto node = std::make_unique<FreeNode>();
            node->id = minId_ + i;
            node->next = (i + 1 < size_) ? i + 1 : UINT32_MAX;

            if (i == 0) {
                freeList_ = node.get();
            } else {
                nodes_[i - 1]->next = node.get();
            }

            nodes_.push_back(std::move(node));
        }
    }
}

bool IdPool::reserve(uint32_t id) {
    if (id < minId_ || id > maxId_) {
        return false;
    }

    uint32_t index = id - minId_;

    if (getBit(index)) {
        return false;  // 已分配
    }

    // 从空闲链表中移除该 ID
    FreeNode** prev = &freeList_;
    for (FreeNode* curr = freeList_; curr != nullptr; curr = reinterpret_cast<FreeNode*>(curr->next)) {
        if (curr->id == id) {
            *prev = reinterpret_cast<FreeNode*>(curr->next);
            setBit(index);
            ++allocatedCount_;
            return true;
        }
        prev = reinterpret_cast<FreeNode**>(&curr->next);
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
