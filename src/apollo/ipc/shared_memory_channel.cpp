/**
 * @file shared_memory_channel.cpp
 * @brief 共享内存 + FlatBuffers 零拷贝 IPC 实现
 */

#include "apollo/ipc/shared_memory_channel.h"
#include <cstring>
#include <iostream>

namespace apollo {
namespace ipc {

//==============================================================================
// 常量
//==============================================================================

namespace {
    constexpr uint32_t SHM_MAGIC = 0x41504C53;  // "APLS" - Apollo Shared Memory
    constexpr size_t ALIGNMENT = 64;            // 缓存行对齐
}

//==============================================================================
// SharedMemoryRingBuffer 实现
//==============================================================================

SharedMemoryRingBuffer::SharedMemoryRingBuffer(const std::string& name, size_t capacity)
    : name_(name)
    , capacity_(capacity)
    , totalSize_(sizeof(Header) + capacity) {
    // 对齐到缓存行
    totalSize_ = (totalSize_ + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

SharedMemoryRingBuffer::~SharedMemoryRingBuffer() {
    close();
}

bool SharedMemoryRingBuffer::create() {
#ifdef _WIN32
    // Windows: CreateFileMapping
    handle_ = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        static_cast<DWORD>(totalSize_),
        name_.c_str()
    );

    if (!handle_) {
        std::cerr << "Failed to create file mapping: " << GetLastError() << "\n";
        return false;
    }

    void* ptr = MapViewOfFile(
        handle_,
        FILE_MAP_ALL_ACCESS,
        0, 0,
        totalSize_
    );

    if (!ptr) {
        std::cerr << "Failed to map view: " << GetLastError() << "\n";
        CloseHandle(handle_);
        handle_ = nullptr;
        return false;
    }

    // 初始化 Header
    header_ = static_cast<Header*>(ptr);
    new (header_) Header{
        .writePos = 0,
        .readPos = 0,
        .count = 0,
        .capacity = capacity_,
        .magic = SHM_MAGIC
    };

    data_ = reinterpret_cast<uint8_t*>(header_ + 1);
    owner_ = true;

#else
    // Linux/Unix: shm_open + mmap
    fd_ = shm_open(name_.c_str(), O_CREAT | O_RDWR, 0666);
    if (fd_ < 0) {
        std::cerr << "Failed to open shared memory: " << strerror(errno) << "\n";
        return false;
    }

    // 设置大小
    if (ftruncate(fd_, totalSize_) < 0) {
        std::cerr << "Failed to set size: " << strerror(errno) << "\n";
        close(fd_);
        fd_ = -1;
        return false;
    }

    // 映射内存
    void* ptr = mmap(nullptr, totalSize_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (ptr == MAP_FAILED) {
        std::cerr << "Failed to mmap: " << strerror(errno) << "\n";
        close(fd_);
        fd_ = -1;
        return false;
    }

    // 初始化 Header
    header_ = static_cast<Header*>(ptr);
    new (header_) Header{
        .writePos = 0,
        .readPos = 0,
        .count = 0,
        .capacity = capacity_,
        .magic = SHM_MAGIC
    };

    data_ = reinterpret_cast<uint8_t*>(header_ + 1);
    owner_ = true;
#endif

    return true;
}

bool SharedMemoryRingBuffer::open() {
#ifdef _WIN32
    handle_ = OpenFileMappingA(
        FILE_MAP_ALL_ACCESS,
        FALSE,
        name_.c_str()
    );

    if (!handle_) {
        return false;
    }

    void* ptr = MapViewOfFile(handle_, FILE_MAP_ALL_ACCESS, 0, 0, totalSize_);
    if (!ptr) {
        CloseHandle(handle_);
        handle_ = nullptr;
        return false;
    }

    header_ = static_cast<Header*>(ptr);
    if (header_->magic != SHM_MAGIC) {
        std::cerr << "Invalid shared memory magic\n";
        UnmapViewOfFile(ptr);
        CloseHandle(handle_);
        handle_ = nullptr;
        return false;
    }

    data_ = reinterpret_cast<uint8_t*>(header_ + 1);
    owner_ = false;

#else
    fd_ = shm_open(name_.c_str(), O_RDWR, 0666);
    if (fd_ < 0) {
        return false;
    }

    void* ptr = mmap(nullptr, totalSize_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (ptr == MAP_FAILED) {
        close(fd_);
        fd_ = -1;
        return false;
    }

    header_ = static_cast<Header*>(ptr);
    if (header_->magic != SHM_MAGIC) {
        std::cerr << "Invalid shared memory magic\n";
        munmap(ptr, totalSize_);
        close(fd_);
        fd_ = -1;
        return false;
    }

    data_ = reinterpret_cast<uint8_t*>(header_ + 1);
    owner_ = false;
#endif

    return true;
}

void SharedMemoryRingBuffer::close() {
    if (data_) {
#ifdef _WIN32
        UnmapViewOfFile(header_);
#else
        munmap(header_, totalSize_);
#endif
        data_ = nullptr;
        header_ = nullptr;
    }

#ifdef _WIN32
    if (handle_) {
        CloseHandle(handle_);
        handle_ = nullptr;
    }
#else
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }

    // 如果是创建者，删除共享内存
    if (owner_) {
        shm_unlink(name_.c_str());
    }
#endif
}

void* SharedMemoryRingBuffer::allocate(size_t size, uint32_t type) {
    size_t totalSize = sizeof(MessageBlock) + size;
    size_t writePos = header_->writePos.load(std::memory_order_relaxed);
    size_t readPos = header_->readPos.load(std::memory_order_acquire);

    // 检查空间
    size_t available = (writePos >= readPos)
        ? (capacity_ - writePos + readPos)
        : (readPos - writePos);

    if (available < totalSize) {
        return nullptr;  // 空间不足
    }

    // 处理环绕
    if (writePos + totalSize > capacity_) {
        // 不够空间，回绕到开头
        if (readPos > 0) {
            // 在末尾写入一个跳过标记
            auto* skip = reinterpret_cast<MessageBlock*>(data_ + writePos);
            skip->size = capacity_ - writePos;
            skip->type = 0xFFFFFFFF;  // 跳过标记
            header_->writePos.store(0, std::memory_order_release);
            writePos = 0;

            // 再次检查空间
            if (readPos <= totalSize) {
                return nullptr;  // 开头空间也不够
            }
        } else {
            return nullptr;
        }
    }

    auto* block = reinterpret_cast<MessageBlock*>(data_ + writePos);
    block->size = size;
    block->type = type;
    block->timestamp = std::chrono::steady_clock::now().time_since_epoch().count();

    return block + 1;  // 返回数据区域指针
}

void SharedMemoryRingBuffer::commit(size_t size) {
    size_t totalSize = sizeof(MessageBlock) + size;
    size_t writePos = header_->writePos.load(std::memory_order_relaxed);
    size_t newWritePos = writePos + totalSize;

    if (newWritePos >= capacity_) {
        newWritePos = totalSize;  // 回绕后
    }

    header_->writePos.store(newWritePos, std::memory_order_release);
    header_->count.fetch_add(1, std::memory_order_relaxed);
}

bool SharedMemoryRingBuffer::read(ReadResult& result) {
    size_t readPos = header_->readPos.load(std::memory_order_relaxed);
    size_t writePos = header_->writePos.load(std::memory_order_acquire);

    if (readPos == writePos) {
        return false;  // 空
    }

    auto* block = reinterpret_cast<MessageBlock*>(data_ + readPos);

    // 检查跳过标记
    if (block->type == 0xFFFFFFFF) {
        header_->readPos.store(0, std::memory_order_release);
        return read(result);  // 递归重试
    }

    result.data = block + 1;
    result.size = block->size;
    result.type = block->type;
    result.timestamp = block->timestamp;

    return true;
}

void SharedMemoryRingBuffer::consume() {
    size_t readPos = header_->readPos.load(std::memory_order_relaxed);
    auto* block = reinterpret_cast<MessageBlock*>(data_ + readPos);

    if (block->type == 0xFFFFFFFF) {
        header_->readPos.store(0, std::memory_order_release);
    } else {
        size_t totalSize = sizeof(MessageBlock) + block->size;
        size_t newReadPos = readPos + totalSize;
        if (newReadPos >= capacity_) {
            newReadPos = 0;
        }
        header_->readPos.store(newReadPos, std::memory_order_release);
    }

    header_->count.fetch_sub(1, std::memory_order_relaxed);
}

size_t SharedMemoryRingBuffer::available() const {
    size_t writePos = header_->writePos.load(std::memory_order_acquire);
    size_t readPos = header_->readPos.load(std::memory_order_acquire);

    return (writePos >= readPos)
        ? (writePos - readPos)
        : (capacity_ - readPos + writePos);
}

size_t SharedMemoryRingBuffer::writable() const {
    size_t writePos = header_->writePos.load(std::memory_order_acquire);
    size_t readPos = header_->readPos.load(std::memory_order_acquire);

    return (writePos >= readPos)
        ? (capacity_ - writePos + readPos)
        : (readPos - writePos);
}

bool SharedMemoryRingBuffer::isEmpty() const {
    return header_->readPos.load(std::memory_order_acquire) ==
           header_->writePos.load(std::memory_order_acquire);
}

bool SharedMemoryRingBuffer::isFull() const {
    return writable() < sizeof(MessageBlock) + 64;
}

//==============================================================================
// FlatBufferChannel 实现
//==============================================================================

#ifdef APOLLO_HAS_FLATBUFFERS

FlatBufferChannel::FlatBufferChannel(const std::string& name, size_t capacity)
    : name_(name)
    , ringBuffer_(std::make_unique<SharedMemoryRingBuffer>(name, capacity)) {
}

FlatBufferChannel::~FlatBufferChannel() {
    disconnect();
}

bool FlatBufferChannel::createServer() {
    if (!ringBuffer_->create()) {
        return false;
    }
    server_ = true;
    return true;
}

bool FlatBufferChannel::connect() {
    return ringBuffer_->open();
}

void FlatBufferChannel::disconnect() {
    ringBuffer_->close();
}

bool FlatBufferChannel::send(const void* flatbufferData, size_t size, uint32_t type) {
    void* buf = ringBuffer_->allocate(size, type);
    if (!buf) return false;

    std::memcpy(buf, flatbufferData, size);
    ringBuffer_->commit(size);
    return true;
}

bool FlatBufferChannel::receive(Message& msg) {
    SharedMemoryRingBuffer::ReadResult result;
    if (!ringBuffer_->read(result)) {
        return false;
    }

    msg.data = static_cast<const uint8_t*>(result.data);
    msg.size = result.size;
    msg.type = result.type;
    msg.timestamp = result.timestamp;

    return true;
}

void FlatBufferChannel::consume() {
    ringBuffer_->consume();
}

#endif // APOLLO_HAS_FLATBUFFERS

} // namespace ipc
} // namespace apollo
