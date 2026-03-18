#pragma once

#include "net_common.hpp"
#include <vector>
#include <queue>
#include <cstring>

// Platform specific headers are already included in net_common.hpp

namespace apollo::net {

/// 缓冲区大小
constexpr size_t NET_BUFFER_SIZE = 64 * 1024;

/// 网络Socket封装类
class Socket {
public:
    Socket();
    explicit Socket(socket_t sockfd, const NetAddress& addr);
    ~Socket();

    // 禁止拷贝，允许移动
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    /// 创建TCP Socket
    bool CreateTCP();

    /// 创建UDP Socket
    bool CreateUDP();

    /// 绑定地址
    bool Bind(const NetAddress& addr);

    /// 监听连接
    bool Listen(int backlog = 128);

    /// 接受连接
    std::unique_ptr<Socket> Accept();

    /// 连接到服务器
    bool Connect(const NetAddress& addr);

    /// 发送数据
    NetError Send(const void* data, size_t len, size_t& sent);

    /// 接收数据
    NetError Receive(void* buffer, size_t len, size_t& received);

    /// 发送数据（完整发送）
    bool SendAll(const void* data, size_t len);

    /// 接收数据（完整接收）
    bool ReceiveAll(void* buffer, size_t len);

    /// 关闭Socket
    void Close();

    /// 设置非阻塞模式
    bool SetNonBlocking(bool nonBlocking);

    /// 设置TCP_NODELAY
    bool SetTcpNoDelay(bool noDelay);

    /// 设置SO_REUSEADDR
    bool SetReuseAddr(bool reuse);

    /// 设置接收缓冲区大小
    bool SetRecvBufferSize(size_t size);

    /// 设置发送缓冲区大小
    bool SetSendBufferSize(size_t size);

    /// 获取本地地址
    NetAddress GetLocalAddress() const;

    /// 获取远程地址
    NetAddress GetRemoteAddress() const;

    /// 获取Socket文件描述符
    socket_t GetFd() const { return sockfd_; }

    /// 是否有效
    bool IsValid() const { return sockfd_ != INVALID_SOCKET; }

    /// 是否已连接（仅对TCP有效）
    bool IsConnected() const { return connected_; }

    /// 获取错误码
    int GetLastError() const;

    /// 获取错误信息
    std::string GetErrorString() const;

private:
    socket_t sockfd_ = INVALID_SOCKET;
    NetAddress localAddr_;
    NetAddress remoteAddr_;
    bool connected_ = false;
};

/// 数据缓冲区
class Buffer {
public:
    Buffer() = default;
    explicit Buffer(size_t capacity) : buffer_(capacity) {}

    /// 写入数据
    void Write(const void* data, size_t len) {
        if (writePos_ + len > buffer_.size()) {
            buffer_.resize(writePos_ + len);
        }
        std::memcpy(buffer_.data() + writePos_, data, len);
        writePos_ += len;
    }

    /// 读取数据
    size_t Read(void* data, size_t len) {
        size_t readable = GetReadableSize();
        if (len > readable) {
            len = readable;
        }
        if (len > 0) {
            std::memcpy(data, buffer_.data() + readPos_, len);
            readPos_ += len;

            // 如果所有数据都被读取了，重置指针
            if (readPos_ == writePos_) {
                readPos_ = 0;
                writePos_ = 0;
            }
        }
        return len;
    }

    /// 查看数据（不移动读取位置）
    const void* Peek(size_t offset = 0) const {
        return buffer_.data() + readPos_ + offset;
    }

    /// 丢弃数据
    void Discard(size_t len) {
        size_t readable = GetReadableSize();
        if (len > readable) {
            len = readable;
        }
        readPos_ += len;

        if (readPos_ == writePos_) {
            readPos_ = 0;
            writePos_ = 0;
        }
    }

    /// 压缩缓冲区（将未读取的数据移到开头）
    void Compact() {
        if (readPos_ > 0) {
            size_t readable = GetReadableSize();
            if (readable > 0) {
                std::memmove(buffer_.data(), buffer_.data() + readPos_, readable);
            }
            readPos_ = 0;
            writePos_ = readable;
        }
    }

    /// 获取可读字节数
    size_t GetReadableSize() const {
        return writePos_ - readPos_;
    }

    /// 当前可读数据指针（从 readPos_ 开始）
    const void* Data() const {
        return buffer_.empty() ? nullptr : (buffer_.data() + readPos_);
    }

    /// 当前可读数据长度（同 GetReadableSize）
    size_t Size() const {
        return GetReadableSize();
    }

    /// 获取可写字节数
    size_t GetWritableSize() const {
        return buffer_.size() - writePos_;
    }

    /// 确保有足够的可写空间
    void EnsureWritable(size_t needed) {
        if (GetWritableSize() < needed) {
            Compact();
            if (GetWritableSize() < needed) {
                buffer_.resize(writePos_ + needed);
            }
        }
    }

    /// 清空缓冲区
    void Clear() {
        readPos_ = 0;
        writePos_ = 0;
    }

    /// 获取缓冲区容量
    size_t GetCapacity() const {
        return buffer_.size();
    }

    /// 获取数据指针
    const char* GetData() const {
        return buffer_.data();
    }

private:
    std::vector<char> buffer_;
    size_t readPos_ = 0;
    size_t writePos_ = 0;
};

}  // namespace apollo::net
