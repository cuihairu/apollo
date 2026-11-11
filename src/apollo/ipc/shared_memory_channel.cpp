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
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    // 映射内存
    void* ptr = mmap(nullptr, totalSize_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (ptr == MAP_FAILED) {
        std::cerr << "Failed to mmap: " << strerror(errno) << "\n";
        ::close(fd_);
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
    // 容量不校验则读写两端按各自 size 寻址 → OOB（audit P0#4②）
    if (header_->capacity != capacity_) {
        std::cerr << "Shared memory capacity mismatch: " << header_->capacity
                  << " != " << capacity_ << "\n";
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
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    header_ = static_cast<Header*>(ptr);
    if (header_->magic != SHM_MAGIC) {
        std::cerr << "Invalid shared memory magic\n";
        munmap(ptr, totalSize_);
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    // 容量不校验则读写两端按各自 size 寻址 → OOB（audit P0#4②）
    if (header_->capacity != capacity_) {
        std::cerr << "Shared memory capacity mismatch: " << header_->capacity
                  << " != " << capacity_ << "\n";
        munmap(ptr, totalSize_);
        ::close(fd_);
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
        ::close(fd_);
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

    // 检查空间。writePos == readPos 是歧义几何：count==0 为空环（全空）；
    // count>0 为回绕悬挂（exact-fill 提交回 0 后写端爬升追平读端），未读
    // [readPos,capacity)∪[0,writePos) 覆盖整环，可用空间为 0——原式
    // `capacity - writePos + readPos` 误判整环空闲，会覆盖读端 read()-
    // consume() 窗口内的零拷贝消息（压测实测撕裂）。
    size_t available;
    if (writePos > readPos) {
        available = capacity_ - writePos + readPos;  // 未读 [readPos, writePos)
    } else if (writePos < readPos) {
        available = readPos - writePos;              // 未读尾部 [readPos, capacity)
    } else {
        available = (header_->count.load(std::memory_order_acquire) == 0)
            ? capacity_ : 0;
    }

    if (available < totalSize) {
        return nullptr;  // 空间不足
    }

    // 处理环绕
    if (writePos + totalSize > capacity_) {
        // 回绕前提：读端已完全排空（count==0，此时 readPos 恰在尾前沿）。
        // count==0 是下界，快照必精确。若旧一圈尚有未读就回绕爬升
        // [0,readPos)，writePos 单变量无法同时表示爬升前沿与旧尾前沿：
        // 写端爬到 writePos==readPos 时 available 误判整环空闲，会直接
        // 覆盖旧圈未读/读端 read()-consume() 窗口内的零拷贝消息（压测
        // 实测撕裂）。未排空时拒绝分配，写端背压等待读端追平。
        if (header_->count.load(std::memory_order_acquire) != 0) {
            return nullptr;
        }
        // 不够空间，回绕到开头
        if (readPos > 0) {
            if (capacity_ - writePos >= sizeof(MessageBlock)) {
                // 在末尾写入一个跳过标记
                auto* skip = reinterpret_cast<MessageBlock*>(data_ + writePos);
                skip->size = capacity_ - writePos;
                skip->type = 0xFFFFFFFF;  // 跳过标记
            }
            // 残尾不足一块时放不下跳过标记：读端对 <sizeof(MessageBlock)
            // 残尾统一跳回 0（read/consume），故这里静默回绕即可，不能把
            // 16 字节标记写出 data 区界外
            header_->writePos.store(0, std::memory_order_release);
            writePos = 0;

            // 再次检查空间：块 [0,totalSize) 不得与未读区 [readPos,capacity)
            // 重叠，即需 totalSize <= readPos；readPos == totalSize 恰好接界
            // 合法（audit P0#4③ 边界差一：原 `<=` 误拒合法接界）
            if (readPos < totalSize) {
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

    // 恰好写满到 capacity：回绕到 0 而非 totalSize。totalSize 只适用于
    // allocate 已回绕、消息写在 [0,totalSize) 的情形；消息写在尾部且恰好
    // 顶满时置 totalSize 会让写端跳到 totalSize、读端（consume 的 >= 判定）
    // 跳到 0，两端约定分裂——读端会重读 [0,totalSize) 的上一圈陈旧消息并
    // 冲过写端前沿，撕裂后续消息（审计 P0#4③ 同源，实测复现）。
    if (newWritePos >= capacity_) {
        newWritePos = 0;
    }

    header_->writePos.store(newWritePos, std::memory_order_release);
    // release：read() 以 count acquire 判空，须与块数据写入同步
    header_->count.fetch_add(1, std::memory_order_release);
}

bool SharedMemoryRingBuffer::write(const void* data, size_t size, uint32_t type) {
    // audit P0#4①：allocate+memcpy+commit 非原子——并发 send 重叠分配、
    // 撕裂发布。全程持写锁串行化；writePos 在 memcpy 完成后才 release
    // 发布，读端 acquire 读到即完整块。
    std::lock_guard<std::mutex> lock(writeMutex_);
    void* buf = allocate(size, type);
    if (!buf) {
        return false;
    }
    std::memcpy(buf, data, size);
    commit(size);
    return true;
}

bool SharedMemoryRingBuffer::read(ReadResult& result) {
    // 空/非空以 count_ 为准（audit P0#4③ 同类）：回绕后 writePos 折回与
    // readPos 数值重合（例：readPos=writePos=2056 且消息 B 在 [0,2056) 未读），
    // `readPos == writePos` 判空会把已发布消息永久误判为空丢失。count_ 由
    // commit/consume 维护，跳过标记不计数，可无歧义区分。
    if (header_->count.load(std::memory_order_acquire) == 0) {
        return false;  // 空
    }
    size_t readPos = header_->readPos.load(std::memory_order_relaxed);
    size_t writePos = header_->writePos.load(std::memory_order_acquire);

    // 残尾 < sizeof(MessageBlock) 时不可能有完整消息（每块至少 16 字节头），
    // 跳回 0 继续读；否则 readPos 落在 capacity 处会读到界外幻影块
    if (capacity_ - readPos < sizeof(MessageBlock)) {
        header_->readPos.store(0, std::memory_order_release);
        return read(result);
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
    // 与 read() 同一残尾规则：<16 字节残尾无完整块，跳回 0
    if (capacity_ - readPos < sizeof(MessageBlock)) {
        header_->readPos.store(0, std::memory_order_release);
        header_->count.fetch_sub(1, std::memory_order_relaxed);
        return;
    }
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
    // 同 read()：回绕后 readPos/writePos 数值重合不代表空，以 count_ 为准
    return header_->count.load(std::memory_order_acquire) == 0;
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
    // audit P0#4①：改走 write()，allocate+memcpy+commit 持 writeMutex_ 原子
    return ringBuffer_->write(flatbufferData, size, type);
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
