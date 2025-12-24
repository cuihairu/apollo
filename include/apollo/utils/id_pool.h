#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <functional>

namespace apollo {
namespace utils {

/**
 * @brief ID 池
 *
 * 高效的 ID 分配和回收管理
 * 用于游戏服务器中管理玩家ID、房间ID等有限资源
 */
class IdPool {
public:
    IdPool() = default;

    /**
     * @brief 构造函数
     *
     * @param minId 最小ID（包含）
     * @param maxId 最大ID（包含）
     */
    IdPool(uint32_t minId, uint32_t maxId);

    /**
     * @brief 构造函数
     *
     * @param size ID 范围大小（从0开始）
     */
    explicit IdPool(uint32_t size);

    ~IdPool() = default;

    // 禁止拷贝
    IdPool(const IdPool&) = delete;
    IdPool& operator=(const IdPool&) = delete;

    // 允许移动
    IdPool(IdPool&&) noexcept = default;
    IdPool& operator=(IdPool&&) noexcept = default;

    /**
     * @brief 分配一个ID
     *
     * @return 分配的ID，如果池已满返回 UINT32_MAX
     */
    uint32_t allocate();

    /**
     * @brief 释放一个ID
     *
     * @param id 要释放的ID
     */
    void release(uint32_t id);

    /**
     * @brief 检查ID是否可用
     *
     * @param id ID值
     * @return 如果ID在范围内且未分配返回true
     */
    bool isValid(uint32_t id) const;

    /**
     * @brief 检查ID是否已分配
     *
     * @param id ID值
     * @return 如果ID已分配返回true
     */
    bool isAllocated(uint32_t id) const;

    /**
     * @brief 获取已分配的ID数量
     */
    uint32_t getAllocatedCount() const { return allocatedCount_; }

    /**
     * @brief 获取总容量
     */
    uint32_t capacity() const { return size_; }

    /**
     * @brief 检查是否已满
     */
    bool isFull() const { return allocatedCount_ == size_; }

    /**
     * @brief 检查是否为空
     */
    bool isEmpty() const { return allocatedCount_ == 0; }

    /**
     * @brief 获取使用率（百分比）
     */
    uint32_t getUsage() const {
        if (size_ == 0) return 0;
        return (allocatedCount_ * 100) / size_;
    }

    /**
     * @brief 重置池
     */
    void reset();

    /**
     * @brief 预分配一个ID（标记为已占用）
     *
     * @param id 要预分配的ID
     * @return 成功返回true
     */
    bool reserve(uint32_t id);

private:
    uint32_t minId_ = 0;
    uint32_t maxId_ = 0;
    uint32_t size_ = 0;
    uint32_t allocatedCount_ = 0;

    // 使用位图跟踪分配状态（更高效）
    std::vector<uint64_t> bitmap_;

    struct FreeNode {
        uint32_t id;
        FreeNode* next;
    };

    FreeNode* freeList_ = nullptr;
    std::vector<std::unique_ptr<FreeNode>> nodes_;

    void init(uint32_t minId, uint32_t maxId);
    bool getBit(uint32_t id) const;
    void setBit(uint32_t id);
    void clearBit(uint32_t id);
};

/**
 * @brief 泛型 ID 池
 *
 * 可以为每个 ID 关联一个对象
 */
template<typename T>
class ObjectPool {
public:
    explicit ObjectPool(uint32_t capacity)
        : minId_(0), maxId_(capacity - 1), pool_(capacity) {
        for (uint32_t i = 0; i < capacity; ++i) {
            pool_[i].next = (i + 1 < capacity) ? i + 1 : UINT32_MAX;
        }
        freeHead_ = 0;
        freeCount_ = capacity;
    }

    ~ObjectPool() = default;

    // 禁止拷贝
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    /**
     * @brief 分配一个对象
     *
     * @param obj 关联的对象
     * @return 分配的ID，如果池已满返回 UINT32_MAX
     */
    uint32_t allocate(T obj) {
        if (freeCount_ == 0) {
            return UINT32_MAX;
        }

        uint32_t id = freeHead_;
        auto& node = pool_[id];
        freeHead_ = node.next;
        node.next = UINT32_MAX;  // 标记为已使用
        node.object = std::move(obj);
        --freeCount_;

        return id + minId_;
    }

    /**
     * @brief 获取对象（不分配）
     *
     * @param id ID值
     * @return 对象指针，如果ID无效或未分配返回nullptr
     */
    T* get(uint32_t id) {
        if (id < minId_ || id > maxId_) {
            return nullptr;
        }

        uint32_t index = id - minId_;
        if (pool_[index].next != UINT32_MAX) {
            return nullptr;  // 未分配
        }

        return &pool_[index].object;
    }

    /**
     * @brief 释放一个对象
     *
     * @param id 要释放的ID
     * @return 释放的对象，如果ID无效返回nullptr
     */
    std::unique_ptr<T> release(uint32_t id) {
        if (id < minId_ || id > maxId_) {
            return nullptr;
        }

        uint32_t index = id - minId_;
        if (pool_[index].next != UINT32_MAX) {
            return nullptr;  // 未分配
        }

        auto& node = pool_[index];
        auto obj = std::make_unique<T>(std::move(node.object));

        // 加入空闲链表
        node.next = freeHead_;
        freeHead_ = index;
        ++freeCount_;

        return obj;
    }

    /**
     * @brief 检查ID是否已分配
     */
    bool isAllocated(uint32_t id) const {
        if (id < minId_ || id > maxId_) {
            return false;
        }
        return pool_[id - minId_].next == UINT32_MAX;
    }

    /**
     * @brief 获取已分配数量
     */
    uint32_t getAllocatedCount() const {
        return (maxId_ - minId_ + 1) - freeCount_;
    }

    /**
     * @brief 获取容量
     */
    uint32_t capacity() const {
        return maxId_ - minId_ + 1;
    }

    /**
     * @brief 检查是否已满
     */
    bool isFull() const {
        return freeCount_ == 0;
    }

    /**
     * @brief 检查是否为空
     */
    bool isEmpty() const {
        return freeCount_ == (maxId_ - minId_ + 1);
    }

private:
    struct Node {
        T object;
        uint32_t next;
    };

    uint32_t minId_;
    uint32_t maxId_;
    std::vector<Node> pool_;
    uint32_t freeHead_;
    uint32_t freeCount_;
};

} // namespace utils
} // namespace apollo
