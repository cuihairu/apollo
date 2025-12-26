#pragma once

#include "apollo/ipc/channel.h"
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    using socket_t = SOCKET;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <fcntl.h>
    #include <unistd.h>
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    using socket_t = int;
#endif

namespace apollo {
namespace ipc {

//==============================================================================
// Socket 传输层基类
//==============================================================================

class SocketTransport : public ITransport {
public:
    SocketTransport(const ChannelConfig& config);
    ~SocketTransport() override;

    // ITransport 接口
    bool connect(const std::string& address) override;
    bool disconnect() override;
    bool isConnected() const override;

    bool send(const void* data, size_t size) override;
    size_t receive(void* buffer, size_t size) override;

    void sendAsync(std::vector<uint8_t> data,
                   std::function<void(bool)> callback) override;
    void receiveAsync(std::function<void(std::vector<uint8_t>)> callback) override;

    ChannelState getState() const override { return state_; }
    std::string getLastError() const override { return lastError_; }
    int getFd() const override { return static_cast<int>(socket_); }

    // 容量查询
    size_t getAvailable() const override;
    size_t getWritable() const override;

    // Socket 特定方法
    bool setNonBlocking(bool enable);
    bool setTcpNoDelay(bool enable);
    bool setKeepAlive(bool enable, uint32_t idleSec = 60, uint32_t intervalSec = 5);

    // 获取地址信息
    std::string getLocalAddress() const;
    std::string getRemoteAddress() const;
    uint16_t getLocalPort() const;
    uint16_t getRemotePort() const;

protected:
    bool createSocket();
    void closeSocket();
    bool setSocketOptions();

    // 接收完整消息 (处理分包)
    bool receiveMessage(std::vector<uint8_t>& buffer);

    ChannelConfig config_;
    socket_t socket_{INVALID_SOCKET};
    std::atomic<ChannelState> state_{ChannelState::Disconnected};
    std::string lastError_;

    // 发送/接收缓冲
    std::queue<std::vector<uint8_t>> sendQueue_;
    std::mutex sendMutex_;

    // 异步 I/O 线程
    std::thread ioThread_;
    std::atomic<bool> ioRunning_{false};
    void ioThreadFunc();

    // 消息分帧
    struct MessageHeader {
        uint32_t magic;      // 魔数
        uint32_t size;       // 消息体大小
        uint32_t type;       // 消息类型
        uint32_t sequence;   // 序列号
    };
    static constexpr uint32_t MESSAGE_MAGIC = 0x4D534747;  // "MSGG"
};

//==============================================================================
// TCP Socket 传输 (跨服务器/局域网)
//==============================================================================

class TcpSocketTransport : public SocketTransport {
public:
    TcpSocketTransport(const ChannelConfig& config);

    // 作为客户端连接
    bool connect(const std::string& address) override;

    // 作为服务器
    bool bind(const std::string& address, uint16_t port);
    bool listen(int backlog = 128);
    std::unique_ptr<ITransport> accept();

    // TCP 特定
    bool setLinger(bool enable, int seconds = 0);
    bool setReuseAddr(bool enable);
    bool setReusePort(bool enable);
};

//==============================================================================
// Unix Socket 传输 (本机，Linux/Mac)
//==============================================================================

#if !defined(_WIN32)

class UnixSocketTransport : public SocketTransport {
public:
    UnixSocketTransport(const ChannelConfig& config);

    bool connect(const std::string& path) override;

    // 作为服务器
    bool bind(const std::string& path);
    bool listen(int backlog = 128);
    std::unique_ptr<ITransport> accept();

    // Unix Socket 特定
    bool setPassCred(bool enable);
};

#endif // !_WIN32

//==============================================================================
// UDP Socket 传输 (无连接，高性能)
//==============================================================================

class UdpSocketTransport : public ITransport {
public:
    UdpSocketTransport(const ChannelConfig& config);
    ~UdpSocketTransport() override;

    // 连接 (设置默认目标)
    bool connect(const std::string& address) override;
    bool disconnect() override;
    bool isConnected() const override;

    // 发送/接收
    bool send(const void* data, size_t size) override;
    size_t receive(void* buffer, size_t size) override;

    // UDP 特定 - 发送到指定地址
    bool sendTo(const void* data, size_t size, const std::string& address, uint16_t port);
    ssize_t receiveFrom(void* buffer, size_t size, std::string& sourceAddr, uint16_t& sourcePort);

    void sendAsync(std::vector<uint8_t> data,
                   std::function<void(bool)> callback) override;
    void receiveAsync(std::function<void(std::vector<uint8_t>)> callback) override;

    ChannelState getState() const override { return state_; }
    std::string getLastError() const override { return lastError_; }
    int getFd() const override { return static_cast<int>(socket_); }

    size_t getAvailable() const override;
    size_t getWritable() const override;

    // 多播支持
    bool joinMulticastGroup(const std::string& group);
    bool leaveMulticastGroup(const std::string& group);
    bool setMulticastTTL(int ttl);
    bool setMulticastInterface(const std::string& iface);

    // 广播支持
    bool setBroadcast(bool enable);

private:
    bool createSocket();
    void closeSocket();
    bool setSocketOptions();

    ChannelConfig config_;
    socket_t socket_{INVALID_SOCKET};
    std::atomic<ChannelState> state_{ChannelState::Disconnected};
    std::string lastError_;

    struct sockaddr_in multicastAddr_;
    bool isMulticast_{false};
};

//==============================================================================
// 命名管道传输 (Windows 本机)
//==============================================================================

#ifdef _WIN32

class NamedPipeTransport : public ITransport {
public:
    NamedPipeTransport(const ChannelConfig& config);
    ~NamedPipeTransport() override;

    bool connect(const std::string& name) override;
    bool disconnect() override;
    bool isConnected() const override;

    bool send(const void* data, size_t size) override;
    size_t receive(void* buffer, size_t size) override;

    void sendAsync(std::vector<uint8_t> data,
                   std::function<void(bool)> callback) override;
    void receiveAsync(std::function<void(std::vector<uint8_t>)> callback) override;

    ChannelState getState() const override { return state_; }
    std::string getLastError() const override { return lastError_; }
    int getFd() const override { return -1; }  // Windows 不支持

    size_t getAvailable() const override;
    size_t getWritable() const override;

    // 作为服务器
    bool createServer(const std::string& name, int maxInstances = PIPE_UNLIMITED_INSTANCES);
    bool waitForConnection();
    HANDLE disconnectEvent() { return disconnectEvent_; }

private:
    ChannelConfig config_;
    HANDLE handle_{INVALID_HANDLE_VALUE};
    HANDLE disconnectEvent_{nullptr};
    std::atomic<ChannelState> state_{ChannelState::Disconnected};
    std::string lastError_;
    std::wstring pipeName_;
};

#endif // _WIN32

//==============================================================================
// 传输工厂
//==============================================================================

class TransportFactory {
public:
    static std::unique_ptr<ITransport> create(TransportType type,
                                               const ChannelConfig& config);

    // 自动选择最佳传输
    static std::unique_ptr<ITransport> createAuto(const ChannelConfig& config);

    // 检测传输类型可用性
    static bool isAvailable(TransportType type);
};

//==============================================================================
// 便捷函数
//==============================================================================

// 初始化 Socket 库 (Windows 需要 WSAStartup)
class SocketInitializer {
public:
    SocketInitializer();
    ~SocketInitializer();

    bool isInitialized() const { return initialized_; }
    static SocketInitializer& instance();

private:
    bool initialized_{false};
    #ifdef _WIN32
    WSADATA wsaData_;
    #endif
};

} // namespace ipc
} // namespace apollo
