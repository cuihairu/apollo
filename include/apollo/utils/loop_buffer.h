#pragma once

#include <vector>
#include <memory>
#include <cstring>
#include <stdexcept>
#include <type_traits>

namespace apollo {
namespace utils {

/**
 * @brief 环形缓冲区
 *
 * 固定大小的循环队列，支持连续读写
 */
template<typename T>
class LoopBuffer {
public:
    /**
     * @brief 构造函数
     *
     * @param capacity 容量
     */
    explicit LoopBuffer(size_t capacity)
        : capacity_(capacity), data_(capacity) {
    }

    /**
     * @brief 构造函数
     *
     * @param capacity 容量
     * @param initialValue 初始值
     */
    LoopBuffer(size_t capacity, const T& initialValue)
        : capacity_(capacity), data_(capacity, initialValue) {
    }

    ~LoopBuffer() = default;

    // 禁止拷贝
    LoopBuffer(const LoopBuffer&) = delete;
    LoopBuffer& operator=(const LoopBuffer&) = delete;

    // 支持移动
    LoopBuffer(LoopBuffer&&) = default;
    LoopBuffer& operator=(LoopBuffer&&) = default;

    /**
     * @brief 写入数据
     *
     * @param data 数据指针
     * @param size 数据数量
     * @return 实际写入的数量
     */
    size_t write(const T* data, size_t size) {
        if (data == nullptr || size == 0) {
            return 0;
        }

        size_t available = availableWrite();
        size_t toWrite = (size < available) ? size : available;

        if (toWrite == 0) {
            return 0;
        }

        size_t writePos = writePos_ % capacity_;
        size_t contiguous = capacity_ - writePos;
        size_t firstChunk = (toWrite < contiguous) ? toWrite : contiguous;

        // 第一段
        std::copy(data, data + firstChunk, data_.begin() + writePos);

        // 第二段（环绕）
        if (toWrite > firstChunk) {
            std::copy(data + firstChunk, data + toWrite, data_.begin());
        }

        writePos_ += toWrite;
        return toWrite;
    }

    /**
     * @brief 写入单个元素
     */
    bool push(const T& item) {
        if (availableWrite() == 0) {
            return false;
        }
        data_[writePos_ % capacity_] = item;
        ++writePos_;
        return true;
    }

    /**
     * @brief 移动写入单个元素
     */
    bool push(T&& item) {
        if (availableWrite() == 0) {
            return false;
        }
        data_[writePos_ % capacity_] = std::move(item);
        ++writePos_;
        return true;
    }

    /**
     * @brief 读取数据
     *
     * @param buffer 输出缓冲区
     * @param size 要读取的数量
     * @return 实际读取的数量
     */
    size_t read(T* buffer, size_t size) {
        if (buffer == nullptr || size == 0) {
            return 0;
        }

        size_t available = availableRead();
        size_t toRead = (size < available) ? size : available;

        if (toRead == 0) {
            return 0;
        }

        size_t readPos = readPos_ % capacity_;
        size_t contiguous = capacity_ - readPos;
        size_t firstChunk = (toRead < contiguous) ? toRead : contiguous;

        // 第一段
        std::copy(data_.begin() + readPos, data_.begin() + readPos + firstChunk, buffer);

        // 第二段（环绕）
        if (toRead > firstChunk) {
            std::copy(data_.begin(), data_.begin() + (toRead - firstChunk), buffer + firstChunk);
        }

        readPos_ += toRead;
        return toRead;
    }

    /**
     * @brief 读取单个元素
     */
    bool pop(T& item) {
        if (availableRead() == 0) {
            return false;
        }
        item = std::move(data_[readPos_ % capacity_]);
        ++readPos_;
        return true;
    }

    /**
     * @brief 查看首个元素（不移除）
     */
    T* front() {
        if (availableRead() == 0) {
            return nullptr;
        }
        return &data_[readPos_ % capacity_];
    }

    /**
     * @brief 查看首个元素（不移除）
     */
    const T* front() const {
        if (availableRead() == 0) {
            return nullptr;
        }
        return &data_[readPos_ % capacity_];
    }

    /**
     * @brief 获取可读数量
     */
    size_t availableRead() const {
        return writePos_ - readPos_;
    }

    /**
     * @brief 获取可写数量
     */
    size_t availableWrite() const {
        return capacity_ - (writePos_ - readPos_);
    }

    /**
     * @brief 检查是否为空
     */
    bool isEmpty() const {
        return writePos_ == readPos_;
    }

    /**
     * @brief 检查是否已满
     */
    bool isFull() const {
        return (writePos_ - readPos_) == capacity_;
    }

    /**
     * @brief 获取容量
     */
    size_t capacity() const {
        return capacity_;
    }

    /**
     * @brief 清空缓冲区
     */
    void clear() {
        readPos_ = 0;
        writePos_ = 0;
    }

    /**
     * @brief 直接访问连续可读区域
     *
     * @param start 起始位置指针
     * @param size 可读大小
     */
    void getReadRegion(const T** start, size_t& size) const {
        size_t avail = availableRead();
        if (avail == 0) {
            *start = nullptr;
            size = 0;
            return;
        }

        size_t readPos = readPos_ % capacity_;
        size_t contiguous = capacity_ - readPos;
        size = (avail < contiguous) ? avail : contiguous;
        *start = &data_[readPos];
    }

    /**
     * @brief 跳过指定数量的数据
     */
    void skip(size_t size) {
        size_t avail = availableRead();
        size_t toSkip = (size < avail) ? size : avail;
        readPos_ += toSkip;
    }

    /**
     * @brief 提交已写入的数据（用于直接写入）
     */
    void commitWrite(size_t size) {
        size_t avail = availableWrite();
        size_t toCommit = (size < avail) ? size : avail;
        writePos_ += toCommit;
    }

    /**
     * @brief 获取写入区域
     */
    void getWriteRegion(T** start, size_t& size) {
        size_t avail = availableWrite();
        if (avail == 0) {
            *start = nullptr;
            size = 0;
            return;
        }

        size_t writePos = writePos_ % capacity_;
        size_t contiguous = capacity_ - writePos;
        size = (avail < contiguous) ? avail : contiguous;
        *start = &data_[writePos];
    }

private:
    size_t capacity_;
    std::vector<T> data_;
    size_t readPos_ = 0;
    size_t writePos_ = 0;
};

//==============================================================================
// ByteLoopBuffer - 字节环形缓冲区特化
//==============================================================================

/**
 * @brief 字节环形缓冲区
 *
 * 优化的字节操作版本
 */
class ByteLoopBuffer {
public:
    explicit ByteLoopBuffer(size_t capacity)
        : capacity_(capacity), data_(capacity) {
    }

    size_t write(const void* data, size_t size);
    size_t read(void* buffer, size_t size);
    size_t peek(void* buffer, size_t size) const;
    void skip(size_t size);
    void clear();

    size_t availableRead() const { return writePos_ - readPos_; }
    size_t availableWrite() const { return capacity_ - (writePos_ - readPos_); }
    bool isEmpty() const { return writePos_ == readPos_; }
    bool isFull() const { return (writePos_ - readPos_) == capacity_; }
    size_t capacity() const { return capacity_; }

private:
    size_t capacity_;
    std::vector<uint8_t> data_;
    size_t readPos_ = 0;
    size_t writePos_ = 0;
};

inline size_t ByteLoopBuffer::write(const void* data, size_t size) {
    if (data == nullptr || size == 0) {
        return 0;
    }

    const uint8_t* src = static_cast<const uint8_t*>(data);
    size_t available = availableWrite();
    size_t toWrite = (size < available) ? size : available;

    if (toWrite == 0) {
        return 0;
    }

    size_t writePos = writePos_ % capacity_;
    size_t contiguous = capacity_ - writePos;
    size_t firstChunk = (toWrite < contiguous) ? toWrite : contiguous;

    std::memcpy(data_.data() + writePos, src, firstChunk);

    if (toWrite > firstChunk) {
        std::memcpy(data_.data(), src + firstChunk, toWrite - firstChunk);
    }

    writePos_ += toWrite;
    return toWrite;
}

inline size_t ByteLoopBuffer::read(void* buffer, size_t size) {
    size_t read = peek(buffer, size);
    skip(read);
    return read;
}

inline size_t ByteLoopBuffer::peek(void* buffer, size_t size) const {
    if (buffer == nullptr || size == 0) {
        return 0;
    }

    uint8_t* dst = static_cast<uint8_t*>(buffer);
    size_t available = availableRead();
    size_t toRead = (size < available) ? size : available;

    if (toRead == 0) {
        return 0;
    }

    size_t readPos = readPos_ % capacity_;
    size_t contiguous = capacity_ - readPos;
    size_t firstChunk = (toRead < contiguous) ? toRead : contiguous;

    std::memcpy(dst, data_.data() + readPos, firstChunk);

    if (toRead > firstChunk) {
        std::memcpy(dst + firstChunk, data_.data(), toRead - firstChunk);
    }

    return toRead;
}

inline void ByteLoopBuffer::skip(size_t size) {
    size_t avail = availableRead();
    size_t toSkip = (size < avail) ? size : avail;
    readPos_ += toSkip;
}

inline void ByteLoopBuffer::clear() {
    readPos_ = 0;
    writePos_ = 0;
}

} // namespace utils
} // namespace apollo
