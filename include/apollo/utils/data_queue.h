#pragma once

#include <atomic>
#include <memory>

namespace apollo {
namespace utils {

//==============================================================================
// LockFreeQueue - 无锁队列（单生产者单消费者）
//==============================================================================

/**
 * @brief 无锁队列
 *
 * 基于 Michael & Scott 算法的无锁队列
 * 支持多生产者多消费者
 */
template<typename T>
class LockFreeQueue {
    struct Node {
        std::atomic<T*> data;
        std::atomic<Node*> next;

        Node() : data(nullptr), next(nullptr) {}
        explicit Node(T* value) : data(value), next(nullptr) {}
    };

public:
    LockFreeQueue() {
        Node* dummy = new Node(nullptr);
        head_.store(dummy);
        tail_.store(dummy);
    }

    ~LockFreeQueue() {
        while (Node* head = head_.load()) {
            head_.store(head->next);
            delete head;
        }
    }

    // 禁止拷贝和移动
    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;
    LockFreeQueue(LockFreeQueue&&) = delete;
    LockFreeQueue& operator=(LockFreeQueue&&) = delete;

    /**
     * @brief 入队
     *
     * @param value 值指针
     */
    void enqueue(T* value) {
        Node* node = new Node(value);

        while (true) {
            Node* tail = tail_.load();
            Node* next = tail->next.load();

            if (tail == tail_.load()) {
                if (next == nullptr) {
                    if (tail->next.compare_exchange_weak(next, node)) {
                        tail_.compare_exchange_strong(tail, node);
                        return;
                    }
                } else {
                    tail_.compare_exchange_strong(tail, next);
                }
            }
        }
    }

    /**
     * @brief 入队（值版本）
     */
    void enqueue(const T& value) {
        enqueue(new T(value));
    }

    /**
     * @brief 入队（移动版本）
     */
    void enqueue(T&& value) {
        enqueue(new T(std::move(value)));
    }

    /**
     * @brief 出队
     *
     * @return 值指针，如果队列为空返回 nullptr
     */
    T* dequeue() {
        while (true) {
            Node* head = head_.load();
            Node* tail = tail_.load();
            Node* next = head->next.load();

            if (head == head_.load()) {
                if (head == tail) {
                    if (next == nullptr) {
                        return nullptr;  // 队列为空
                    }
                    tail_.compare_exchange_strong(tail, next);
                } else {
                    T* value = next->data.load();
                    if (head_.compare_exchange_weak(head, next)) {
                        delete head;
                        return value;
                    }
                }
            }
        }
    }

    /**
     * @brief 检查是否为空
     *
     * 注意：在并发环境下此值仅供参考
     */
    bool isEmpty() const {
        Node* head = head_.load();
        Node* tail = tail_.load();
        return (head == tail) && (head->next.load() == nullptr);
    }

private:
    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;
};

//==============================================================================
// SPSCQueue - 单生产者单消费者队列
//==============================================================================

/**
 * @brief 单生产者单消费者无界队列
 *
 * 更高效的 SPSC 实现，无需原子操作
 */
template<typename T, size_t Size = 1024>
class SPSCQueue {
public:
    SPSCQueue() : readIndex_(0), writeIndex_(0) {
        // 确保大小是2的幂
        static_assert((Size & (Size - 1)) == 0, "Size must be power of 2");
    }

    ~SPSCQueue() = default;

    // 禁止拷贝和移动
    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;

    /**
     * @brief 写入（生产者调用）
     */
    bool push(const T& item) {
        size_t currentWrite = writeIndex_;
        size_t nextWrite = (currentWrite + 1) & (Size - 1);

        if (nextWrite == readIndex_) {
            return false;  // 队列已满
        }

        data_[currentWrite] = item;
        writeIndex_ = nextWrite;
        return true;
    }

    /**
     * @brief 写入（移动版本）
     */
    bool push(T&& item) {
        size_t currentWrite = writeIndex_;
        size_t nextWrite = (currentWrite + 1) & (Size - 1);

        if (nextWrite == readIndex_) {
            return false;
        }

        data_[currentWrite] = std::move(item);
        writeIndex_ = nextWrite;
        return true;
    }

    /**
     * @brief 读取（消费者调用）
     */
    bool pop(T& item) {
        size_t currentRead = readIndex_;

        if (currentRead == writeIndex_) {
            return false;  // 队列为空
        }

        item = std::move(data_[currentRead]);
        readIndex_ = (currentRead + 1) & (Size - 1);
        return true;
    }

    /**
     * @brief 检查是否为空
     */
    bool isEmpty() const {
        return readIndex_ == writeIndex_;
    }

    /**
     * @brief 检查是否已满
     */
    bool isFull() const {
        size_t nextWrite = (writeIndex_ + 1) & (Size - 1);
        return nextWrite == readIndex_;
    }

    /**
     * @brief 获取大小
     */
    size_t size() const {
        return (writeIndex_ - readIndex_) & (Size - 1);
    }

    /**
     * @brief 获取容量
     */
    static constexpr size_t capacity() {
        return Size - 1;
    }

private:
    alignas(64) T data_[Size];
    alignas(64) size_t readIndex_;   // 缓存行对齐
    alignas(64) size_t writeIndex_;  // 缓存行对齐
};

//==============================================================================
// MPSCQueue - 多生产者单消费者队列
//==============================================================================

/**
 * @brief 多生产者单消费者队列
 *
 * 使用链表实现，消费者端无锁
 */
template<typename T>
class MPSCQueue {
    struct Node {
        T data;
        Node* next;

        template<typename U>
        explicit Node(U&& value) : data(std::forward<U>(value)), next(nullptr) {}
    };

public:
    MPSCQueue() {
        Node* dummy = new Node(T());
        dummy->next = nullptr;
        head_ = dummy;
        tail_.store(dummy);
    }

    ~MPSCQueue() {
        while (Node* head = head_) {
            head_ = head->next;
            delete head;
        }
    }

    // 禁止拷贝和移动
    MPSCQueue(const MPSCQueue&) = delete;
    MPSCQueue& operator=(const MPSCQueue&) = delete;

    /**
     * @brief 入队（多生产者线程安全）
     */
    template<typename U>
    void enqueue(U&& value) {
        Node* node = new Node(std::forward<U>(value));
        Node* prev = tail_.exchange(node, std::memory_order_acq_rel);
        prev->next = node;
    }

    /**
     * @brief 出队（仅单消费者线程调用）
     */
    bool dequeue(T& out) {
        Node* head = head_;
        Node* next = head->next;

        if (next == nullptr) {
            return false;  // 队列为空
        }

        out = std::move(next->data);
        head_ = next;
        delete head;
        return true;
    }

    /**
     * @brief 检查是否为空
     */
    bool isEmpty() const {
        return head_->next == nullptr;
    }

private:
    Node* head_;                          // 仅消费者访问
    std::atomic<Node*> tail_;             // 多生产者访问
};

} // namespace utils
} // namespace apollo
