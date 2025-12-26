/**
 * @file min_heap.h
 * @brief 最小堆模板实现
 *
 * 最小堆是一种二叉堆数据结构，支持：
 * - O(1) 获取最小元素
 * - O(log n) 插入元素
 * - O(log n) 删除最小元素
 * - O(log n) 更新元素（ decrease-key）
 *
 * 适用场景：
 * - 定时任务调度
 * - 优先级队列
 * - Dijkstra 最短路径算法
 * - 技能释放优先级管理
 */

#pragma once

#include <vector>
#include <functional>
#include <stdexcept>
#include <algorithm>
#include <cstdint>
#include <type_traits>

namespace apollo {
namespace utils {

//==============================================================================
// 最小堆实现
//==============================================================================

/**
 * @brief 最小堆模板
 *
 * @tparam T 元素类型
 * @tparam Compare 比较器（默认为 std::less，即最小堆）
 *
 * 特性：
 * - 支持自定义比较器（可配置为最大堆）
 * - 支持元素更新（通过索引）
 * - 支持删除任意元素
 * - 异常安全保证
 */
template <
    typename T,
    typename Compare = std::less<T>,
    typename Allocator = std::allocator<T>
>
class MinHeap {
public:
    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using allocator_type = Allocator;

    //==========================================================================
    // 构造函数
    //==========================================================================

    MinHeap() = default;

    /**
     * @brief 构造函数（指定初始容量）
     */
    explicit MinHeap(size_type capacity, const Allocator& alloc = Allocator())
        : data_(alloc) {
        data_.reserve(capacity);
    }

    /**
     * @brief 构造函数（指定比较器）
     */
    explicit MinHeap(const Compare& comp, const Allocator& alloc = Allocator())
        : data_(alloc)
        , comp_(comp) {
    }

    /**
     * @brief 范围构造函数
     */
    template<typename InputIt>
    MinHeap(InputIt first, InputIt last, const Allocator& alloc = Allocator())
        : data_(first, last, alloc) {
        heapify();
    }

    /**
     * @brief 初始化列表构造
     */
    MinHeap(std::initializer_list<T> init, const Allocator& alloc = Allocator())
        : data_(init, alloc) {
        heapify();
    }

    //==========================================================================
    // 容量操作
    //==========================================================================

    /**
     * @brief 检查是否为空
     */
    bool empty() const noexcept {
        return data_.empty();
    }

    /**
     * @brief 获取元素数量
     */
    size_type size() const noexcept {
        return data_.size();
    }

    /**
     * @brief 获取容量
     */
    size_type capacity() const noexcept {
        return data_.capacity();
    }

    /**
     * @brief 预留空间
     */
    void reserve(size_type newCapacity) {
        data_.reserve(newCapacity);
    }

    /**
     * @brief 收缩容量
     */
    void shrink_to_fit() {
        data_.shrink_to_fit();
    }

    //==========================================================================
    // 元素访问
    //==========================================================================

    /**
     * @brief 获取最小元素（堆顶）
     * @throw std::out_of_range 如果堆为空
     */
    const_reference peek() const {
        if (empty()) {
            throw std::out_of_range("MinHeap::peek() - heap is empty");
        }
        return data_.front();
    }

    /**
     * @brief 获取最小元素（堆顶），不抛出版本
     * @return 指向堆顶的指针，如果为空返回 nullptr
     */
    const T* try_peek() const noexcept {
        if (empty()) {
            return nullptr;
        }
        return &data_.front();
    }

    //==========================================================================
    // 修改操作
    //==========================================================================

    /**
     * @brief 插入元素
     * @param value 要插入的值
     *
     * 时间复杂度：O(log n)
     */
    void push(const T& value) {
        data_.push_back(value);
        sift_up(data_.size() - 1);
    }

    /**
     * @brief 插入元素（移动语义）
     */
    void push(T&& value) {
        data_.push_back(std::move(value));
        sift_up(data_.size() - 1);
    }

    /**
     * @brief 原位构造元素
     */
    template<typename... Args>
    void emplace(Args&&... args) {
        data_.emplace_back(std::forward<Args>(args)...);
        sift_up(data_.size() - 1);
    }

    /**
     * @brief 弹出最小元素
     * @throw std::out_of_range 如果堆为空
     *
     * 时间复杂度：O(log n)
     */
    T pop() {
        if (empty()) {
            throw std::out_of_range("MinHeap::pop() - heap is empty");
        }

        T result = std::move(data_.front());

        if (data_.size() > 1) {
            data_.front() = std::move(data_.back());
        }

        data_.pop_back();

        if (!empty()) {
            sift_down(0);
        }

        return result;
    }

    /**
     * @brief 尝试弹出最小元素（不抛出版本）
     * @return 如果成功返回弹出的值，否则返回默认构造的 T
     */
    T try_pop() noexcept {
        if (empty()) {
            return T{};
        }

        T result = std::move(data_.front());

        if (data_.size() > 1) {
            data_.front() = std::move(data_.back());
        }

        data_.pop_back();

        if (!empty()) {
            sift_down(0);
        }

        return result;
    }

    /**
     * @brief 替换堆顶元素
     *
     * 相比 pop + push，此操作只需要一次 sift_down
     */
    T replace(const T& value) {
        if (empty()) {
            push(value);
            return T{};
        }

        T oldTop = std::move(data_.front());
        data_.front() = value;
        sift_down(0);
        return oldTop;
    }

    T replace(T&& value) {
        if (empty()) {
            push(std::move(value));
            return T{};
        }

        T oldTop = std::move(data_.front());
        data_.front() = std::move(value);
        sift_down(0);
        return oldTop;
    }

    /**
     * @brief 清空堆
     */
    void clear() noexcept {
        data_.clear();
    }

    //==========================================================================
    // 堆操作
    //==========================================================================

    /**
     * @brief 将现有数组调整为堆
     */
    void heapify() {
        // 从最后一个非叶子节点开始，向下调整
        for (size_type i = (data_.size() / 2); i > 0; --i) {
            sift_down(i - 1);
        }
    }

    /**
     * @brief 更新指定索引的元素值
     *
     * 当元素的键值减少时，向上调整
     * 当元素的键值增加时，向下调整
     */
    void update(size_type index, const T& newValue) {
        if (index >= data_.size()) {
            throw std::out_of_range("MinHeap::update() - index out of range");
        }

        bool should_go_up = comp_(newValue, data_[index]);
        data_[index] = newValue;

        if (should_go_up) {
            sift_up(index);
        } else {
            sift_down(index);
        }
    }

    /**
     * @brief 删除指定索引的元素
     */
    void erase(size_type index) {
        if (index >= data_.size()) {
            throw std::out_of_range("MinHeap::erase() - index out of range");
        }

        // 将最后一个元素移到要删除的位置
        if (index != data_.size() - 1) {
            data_[index] = std::move(data_.back());
            data_.pop_back();

            // 决定向上还是向下调整
            size_type parent = (index - 1) / 2;
            if (index > 0 && comp_(data_[index], data_[parent])) {
                sift_up(index);
            } else {
                sift_down(index);
            }
        } else {
            data_.pop_back();
        }
    }

    //==========================================================================
    // 迭代器支持
    //==========================================================================

    /**
     * @brief 获取底层数据的迭代器
     * 注意：修改容器会使迭代器失效
     */
    const T* data() const noexcept {
        return data_.data();
    }

    //==========================================================================
    // 比较器访问
    //==========================================================================

    const Compare& get_compare() const noexcept {
        return comp_;
    }

    void set_compare(const Compare& comp) noexcept {
        comp_ = comp;
        heapify();  // 比较器改变后需要重建堆
    }

private:
    //==========================================================================
    // 内部辅助函数
    //==========================================================================

    /**
     * @brief 向上调整（ sift-up / swim）
     */
    void sift_up(size_type index) {
        while (index > 0) {
            size_type parent = (index - 1) / 2;

            if (!comp_(data_[index], data_[parent])) {
                break;
            }

            // 交换父子节点
            using std::swap;
            swap(data_[index], data_[parent]);
            index = parent;
        }
    }

    /**
     * @brief 向下调整（ sift-down / sink）
     */
    void sift_down(size_type index) {
        const size_type size = data_.size();

        while (true) {
            size_type left = 2 * index + 1;
            size_type right = 2 * index + 2;
            size_type smallest = index;

            // 找到父节点和两个子节点中的最小值
            if (left < size && comp_(data_[left], data_[smallest])) {
                smallest = left;
            }

            if (right < size && comp_(data_[right], data_[smallest])) {
                smallest = right;
            }

            if (smallest == index) {
                break;  // 堆性质满足
            }

            // 交换
            using std::swap;
            swap(data_[index], data_[smallest]);
            index = smallest;
        }
    }

    std::vector<T, Allocator> data_;
    Compare comp_;
};

//==============================================================================
// 专用别名
//==============================================================================

/**
 * @brief 最大堆（使用 std::greater）
 */
template<typename T, typename Allocator = std::allocator<T>>
using MaxHeap = MinHeap<T, std::greater<T>, Allocator>;

//==============================================================================
// 基于索引的最小堆（支持高效删除）
//==============================================================================

/**
 * @brief 可索引最小堆
 *
 * 与普通最小堆的区别：
 * - 每个元素维护一个索引，用于快速查找和删除
 * - 支持通过外部 ID 直接删除元素
 *
 * 适用场景：
 * - 定时器管理（需要通过 timer_id 删除）
 * - 任务调度（需要通过 task_id 取消）
 */
template <
    typename T,
    typename IdType = uint64_t,
    typename Compare = std::less<T>
>
class IndexedHeap {
public:
    using value_type = T;
    using id_type = IdType;
    using size_type = size_t;

    /**
     * @brief 堆节点（包含值和外部索引）
     */
    struct Node {
        T value;
        IdType id;
        size_type heapIndex;  // 在堆数组中的位置

        Node() = default;
        Node(const T& v, IdType i) : value(v), id(i), heapIndex(0) {}
        Node(T&& v, IdType i) : value(std::move(v)), id(i), heapIndex(0) {}
    };

    IndexedHeap() = default;

    /**
     * @brief 检查是否为空
     */
    bool empty() const noexcept {
        return heap_.empty();
    }

    /**
     * @brief 获取元素数量
     */
    size_type size() const noexcept {
        return heap_.size();
    }

    /**
     * @brief 插入元素
     * @param id 外部 ID（用于后续删除）
     * @param value 值
     */
    void push(IdType id, const T& value) {
        if (idToIndex_.find(id) != idToIndex_.end()) {
            return;  // ID 已存在
        }

        size_type index = heap_.size();
        heap_.emplace_back(value, id);
        heap_.back().heapIndex = index;
        idToIndex_[id] = index;

        sift_up(index);
    }

    void push(IdType id, T&& value) {
        if (idToIndex_.find(id) != idToIndex_.end()) {
            return;
        }

        size_type index = heap_.size();
        heap_.emplace_back(std::move(value), id);
        heap_.back().heapIndex = index;
        idToIndex_[id] = index;

        sift_up(index);
    }

    /**
     * @brief 弹出最小元素
     */
    std::pair<IdType, T> pop() {
        if (empty()) {
            throw std::out_of_range("IndexedHeap::pop() - heap is empty");
        }

        Node top = heap_.front();
        idToIndex_.erase(top.id);

        if (heap_.size() > 1) {
            heap_.front() = std::move(heap_.back());
            heap_.front().heapIndex = 0;
            heap_.pop_back();
            sift_down(0);
        } else {
            heap_.pop_back();
        }

        return {top.id, std::move(top.value)};
    }

    /**
     * @brief 通过 ID 删除元素
     * @return 是否成功删除
     */
    bool erase(IdType id) {
        auto it = idToIndex_.find(id);
        if (it == idToIndex_.end()) {
            return false;
        }

        size_type index = it->second;
        erase_at_index(index);
        return true;
    }

    /**
     * @brief 通过 ID 更新元素值
     */
    bool update(IdType id, const T& newValue) {
        auto it = idToIndex_.find(id);
        if (it == idToIndex_.end()) {
            return false;
        }

        size_type index = it->second;
        bool should_go_up = comp_(newValue, heap_[index].value);
        heap_[index].value = newValue;

        if (should_go_up) {
            sift_up(index);
        } else {
            sift_down(index);
        }

        return true;
    }

    /**
     * @brief 检查 ID 是否存在
     */
    bool contains(IdType id) const {
        return idToIndex_.find(id) != idToIndex_.end();
    }

    /**
     * @brief 获取堆顶元素
     */
    const T& peek() const {
        if (empty()) {
            throw std::out_of_range("IndexedHeap::peek() - heap is empty");
        }
        return heap_.front().value;
    }

    /**
     * @brief 清空
     */
    void clear() {
        heap_.clear();
        idToIndex_.clear();
    }

private:
    void sift_up(size_type index) {
        while (index > 0) {
            size_type parent = (index - 1) / 2;

            if (!comp_(heap_[index].value, heap_[parent].value)) {
                break;
            }

            swap_nodes(index, parent);
            index = parent;
        }
    }

    void sift_down(size_type index) {
        const size_type size = heap_.size();

        while (true) {
            size_type left = 2 * index + 1;
            size_type right = 2 * index + 2;
            size_type smallest = index;

            if (left < size && comp_(heap_[left].value, heap_[smallest].value)) {
                smallest = left;
            }

            if (right < size && comp_(heap_[right].value, heap_[smallest].value)) {
                smallest = right;
            }

            if (smallest == index) {
                break;
            }

            swap_nodes(index, smallest);
            index = smallest;
        }
    }

    void erase_at_index(size_type index) {
        if (index >= heap_.size()) {
            return;
        }

        IdType id = heap_[index].id;
        idToIndex_.erase(id);

        if (index == heap_.size() - 1) {
            heap_.pop_back();
            return;
        }

        heap_[index] = std::move(heap_.back());
        heap_[index].heapIndex = index;
        idToIndex_[heap_[index].id] = index;
        heap_.pop_back();

        // 决定向上还是向下调整
        size_type parent = (index > 0) ? (index - 1) / 2 : 0;
        if (index > 0 && comp_(heap_[index].value, heap_[parent].value)) {
            sift_up(index);
        } else {
            sift_down(index);
        }
    }

    void swap_nodes(size_type i, size_type j) {
        using std::swap;
        swap(heap_[i], heap_[j]);
        heap_[i].heapIndex = i;
        heap_[j].heapIndex = j;
        idToIndex_[heap_[i].id] = i;
        idToIndex_[heap_[j].id] = j;
    }

    std::vector<Node> heap_;
    std::unordered_map<IdType, size_type> idToIndex_;
    Compare comp_;
};

} // namespace utils
} // namespace apollo
