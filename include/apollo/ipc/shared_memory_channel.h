#pragma once

#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <vector>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif

// FlatBuffers 头文件 (可选)
#if __has_include("flatbuffers/flatbuffers.h")
    #include "flatbuffers/flatbuffers.h"
    #define APOLLO_HAS_FLATBUFFERS 1
#endif

namespace apollo {
namespace ipc {

//==============================================================================
// 共享内存环形缓冲区 - 配合 FlatBuffers 零拷贝访问
//==============================================================================

class SharedMemoryRingBuffer {
public:
    struct Header {
        std::atomic<size_t> writePos{0};    // 写位置
        std::atomic<size_t> readPos{0};     // 读位置
        std::atomic<size_t> count{0};       // 消息计数
        size_t capacity{0};                 // 容量
        uint32_t magic{0};                  // 魔数 (验证)
    };

    struct MessageBlock {
        uint32_t size;        // 数据大小
        uint32_t type;        // 消息类型
        uint64_t timestamp;   // 时间戳
        // 后面跟实际数据
    };

    SharedMemoryRingBuffer(const std::string& name, size_t capacity);
    ~SharedMemoryRingBuffer();

    // 创建/连接
    bool create();
    bool open();
    void close();
    bool isValid() const { return data_ != nullptr; }

    // 写入 (返回写入位置的指针，可直接写 FlatBuffers)
    void* allocate(size_t size, uint32_t type = 0);
    void commit(size_t size);  // 提交写入

    // 读取 (返回数据指针，可直接作为 FlatBuffers 访问)
    struct ReadResult {
        const void* data;
        size_t size;
        uint32_t type;
        uint64_t timestamp;
    };
    bool read(ReadResult& result);
    void consume();  // 消费完成后调用

    // 状态
    size_t available() const;      // 可读字节数
    size_t writable() const;       // 可写字节数
    bool isEmpty() const;
    bool isFull() const;
    size_t capacity() const { return header_ ? header_->capacity : 0; }
    size_t count() const { return header_ ? header_->count.load() : 0; }

    // 原始访问
    void* data() { return data_; }
    const void* data() const { return data_; }

private:
    std::string name_;
    size_t capacity_;
    size_t totalSize_;  // 包括 header

#ifdef _WIN32
    HANDLE handle_{nullptr};
#else
    int fd_{-1};
#endif

    Header* header_{nullptr};
    uint8_t* data_{nullptr};
    bool owner_{false};  // 是否是创建者
};

//==============================================================================
// FlatBuffers 专用适配器
//==============================================================================

#ifdef APOLLO_HAS_FLATBUFFERS

class FlatBufferChannel {
public:
    FlatBufferChannel(const std::string& name, size_t capacity = 1024 * 1024);
    ~FlatBufferChannel();

    // 创建服务端 (创建共享内存)
    bool createServer();

    // 连接服务端 (打开共享内存)
    bool connect();

    // 断开
    void disconnect();

    //======================================================================
    // 发送 FlatBuffers 消息 (零拷贝)
    //======================================================================

    // 方式1: 直接在共享内存中构建 FlatBuffers
    template<typename BuilderFunc>
    bool send(BuilderFunc builder) {
        auto* buf = ringBuffer_->allocate(sizeof(flatbuffers::uoffset_t), 0);
        if (!buf) return false;

        // 在共享内存中直接构建
        flatbuffers::FlatBufferBuilder fbb;
        builder(fbb);
        auto size = fbb.GetSize();
        auto ptr = fbb.GetBufferPointer();

        std::memcpy(buf, ptr, size);
        ringBuffer_->commit(size + sizeof(flatbuffers::uoffset_t));
        return true;
    }

    // 方式2: 发送已构建的 FlatBuffer
    bool send(const void* flatbufferData, size_t size, uint32_t type = 0);

    // 方式3: 使用 Builder 直接构建
    template<typename T>
    bool send(const T& root, uint32_t type = 0);

    //======================================================================
    // 接收 FlatBuffers 消息 (零拷贝访问)
    //======================================================================

    // 读取并返回 FlatBuffers 数据指针 (无需拷贝！)
    struct Message {
        const uint8_t* data;      // 直接指向共享内存中的 FlatBuffer
        size_t size;
        uint32_t type;
        uint64_t timestamp;
    };

    bool receive(Message& msg);
    void consume();  // 消费完成后调用

    // 类型安全的接收
    template<typename T>
    const T* receiveAs() {
        Message msg;
        if (!receive(msg)) return nullptr;
        return flatbuffers::GetRoot<T>(msg.data);
    }

    //======================================================================
    // 迭代器模式 (遍历所有消息)
    //======================================================================

    template<typename Func>
    void forEach(Func func) {
        while (!ringBuffer_->isEmpty()) {
            SharedMemoryRingBuffer::ReadResult result;
            if (ringBuffer_->read(result)) {
                func(result.data, result.size, result.type);
                ringBuffer_->consume();
            } else {
                break;
            }
        }
    }

    //======================================================================
    // 状态
    //======================================================================

    size_t availableCount() const { return ringBuffer_->count(); }
    size_t availableBytes() const { return ringBuffer_->available(); }
    bool isConnected() const { return ringBuffer_->isValid(); }

private:
    std::string name_;
    std::unique_ptr<SharedMemoryRingBuffer> ringBuffer_;
    bool server_{false};
};

#endif // APOLLO_HAS_FLATBUFFERS

//==============================================================================
// 示例：游戏服务器场景
//==============================================================================

#if 0  // 示例代码 (不编译)

// 定义 FlatBuffers schema
namespace game {

// player.fbs
struct PlayerState {
    id: uint64_t;
    x: float;
    y: float;
    z: float;
    hp: int32_t;
    mp: int32_t;
    state: uint8_t;
}

// 使用示例
void example() {
    // 服务端 (战斗进程)
    FlatBufferChannel server("battle_shm", 2 * 1024 * 1024);
    server.createServer();

    // 客户端 (世界进程)
    FlatBufferChannel client("battle_shm");
    client.connect();

    // 发送玩家状态更新 (零拷贝！)
    flatbuffers::FlatBufferBuilder fbb;
    auto player = CreatePlayerState(fbb,
        12345,      // id
        100.0f,     // x
        200.0f,     // y
        300.0f,     // z
        100,        // hp
        50,         // mp
        1           // state
    );
    fbb.Finish(player);

    client.send(fbb.GetBufferPointer(), fbb.GetSize(), 1);

    // 接收 (零拷贝！直接在共享内存中访问)
    FlatBufferChannel::Message msg;
    if (client.receive(msg)) {
        auto playerState = flatbuffers::GetRoot<PlayerState>(msg.data);
        std::cout << "Player " << playerState->id()
                  << " at (" << playerState->x() << ", " << playerState->y() << ")\n";
        client.consume();  // 完成读取
    }
}

} // namespace game

#endif

} // namespace ipc
} // namespace apollo
