#pragma once

#include "apollo/net/net.h"
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <unordered_map>
#include <functional>

namespace apollo {
namespace net {
namespace adapters {

/**
 * @brief 原生网络后端标识
 */
constexpr const char* NATIVE_BACKEND = "native";

// ========== 平台相关定义 ==========

#ifdef _WIN32
    #include <winsock2.h>
    #include <mswsock.h>
    #include <ws2tcpip.h>

    #ifndef SOCKET_TYPE
    typedef SOCKET socket_t;
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define SOCKET_ERROR_VALUE SOCKET_ERROR
    #endif

    using socklen_t = int;
    using ssize_t = SSIZE_T;

#else // Linux/macOS
    #include <sys/socket.h>
    #include <sys/epoll.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <errno.h>
    #include <string.h>

    typedef int socket_t;
    #define INVALID_SOCKET_VALUE -1
    #define SOCKET_ERROR_VALUE -1
    #define closesocket close
    #define SD_BOTH SHUT_RDWR

#endif // _WIN32

// ========== 网络错误码 ==========

enum class NativeErrorCode : int32_t {
    Success = 0,
    InvalidSocket = -1,
    BindFailed = -2,
    ListenFailed = -3,
    AcceptFailed = -4,
    ConnectFailed = -5,
    SendFailed = -6,
    RecvFailed = -7,
    WouldBlock = -100,      // 非阻塞模式下操作需要重试
    ConnectionClosed = -101,
    ConnectionReset = -102,
};

/**
 * @brief 获取错误描述
 */
inline const char* getErrorString(NativeErrorCode code) {
    switch (code) {
        case NativeErrorCode::Success: return "Success";
        case NativeErrorCode::InvalidSocket: return "Invalid socket";
        case NativeErrorCode::BindFailed: return "Bind failed";
        case NativeErrorCode::ListenFailed: return "Listen failed";
        case NativeErrorCode::AcceptFailed: return "Accept failed";
        case NativeErrorCode::ConnectFailed: return "Connect failed";
        case NativeErrorCode::SendFailed: return "Send failed";
        case NativeErrorCode::RecvFailed: return "Receive failed";
        case NativeErrorCode::WouldBlock: return "Operation would block";
        case NativeErrorCode::ConnectionClosed: return "Connection closed";
        case NativeErrorCode::ConnectionReset: return "Connection reset";
        default: return "Unknown error";
    }
}

/**
 * @brief 获取最后一个网络错误
 */
inline int getLastNetError() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

// ========== 套接字工具函数 ==========

/**
 * @brief 初始化网络库
 */
inline bool initNetwork() {
#ifdef _WIN32
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
#else
    return true; // Linux 不需要初始化
#endif
}

/**
 * @brief 清理网络库
 */
inline void cleanupNetwork() {
#ifdef _WIN32
    WSACleanup();
#endif
}

/**
 * @brief 创建非阻塞 TCP 套接字
 */
inline socket_t createNonBlockingSocket() {
    socket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET_VALUE) {
        return INVALID_SOCKET_VALUE;
    }

#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif

    // 设置 SO_LINGER
    struct linger linger;
    linger.l_onoff = 1;
    linger.l_linger = 0;
    setsockopt(sock, SOL_SOCKET, SO_LINGER, (char*)&linger, sizeof(linger));

    // 禁用 Nagle 算法
    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

    return sock;
}

/**
 * @brief 设置套接字选项
 */
inline bool setSocketOptions(socket_t sock, bool noDelay = true,
                             int recvBufSize = 0, int sendBufSize = 0) {
    if (sock == INVALID_SOCKET_VALUE) return false;

    // 禁用 Nagle 算法
    if (noDelay) {
        int flag = 1;
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
    }

    // 设置接收缓冲区
    if (recvBufSize > 0) {
        setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&recvBufSize, sizeof(recvBufSize));
    }

    // 设置发送缓冲区
    if (sendBufSize > 0) {
        setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&sendBufSize, sizeof(sendBufSize));
    }

    return true;
}

// ========== 缓冲区管理 ==========

/**
 * @brief 网络缓冲区
 */
class NetBuffer {
public:
    static constexpr size_t DEFAULT_SIZE = 8192;
    static constexpr size_t MAX_SIZE = 64 * 1024;

    NetBuffer() : readPos_(0), writePos_(0) {
        buffer_.resize(DEFAULT_SIZE);
    }

    explicit NetBuffer(size_t size) : readPos_(0), writePos_(0) {
        buffer_.resize(size);
    }

    ~NetBuffer() = default;

    // 写入数据
    bool write(const char* data, size_t length) {
        ensureSpace(length);

        if (writePos_ + length <= buffer_.size()) {
            std::memcpy(buffer_.data() + writePos_, data, length);
            writePos_ += length;
        } else {
            // 需要绕回到开头
            size_t firstPart = buffer_.size() - writePos_;
            std::memcpy(buffer_.data() + writePos_, data, firstPart);
            size_t secondPart = length - firstPart;
            std::memcpy(buffer_.data(), data + firstPart, secondPart);
            writePos_ = secondPart;
        }
        return true;
    }

    // 读取数据
    size_t read(char* data, size_t length) {
        size_t readable = readableBytes();
        if (length > readable) length = readable;

        if (readPos_ + length <= buffer_.size()) {
            std::memcpy(data, buffer_.data() + readPos_, length);
            readPos_ += length;
        } else {
            size_t firstPart = buffer_.size() - readPos_;
            std::memcpy(data, buffer_.data() + readPos_, firstPart);
            size_t secondPart = length - firstPart;
            std::memcpy(data + firstPart, buffer_.data(), secondPart);
            readPos_ = secondPart;
        }

        return length;
    }

    // 查看数据(不移动读指针)
    size_t peek(char* data, size_t length) const {
        size_t readable = readableBytes();
        if (length > readable) length = readable;

        if (readPos_ + length <= buffer_.size()) {
            std::memcpy(data, buffer_.data() + readPos_, length);
        } else {
            size_t firstPart = buffer_.size() - readPos_;
            std::memcpy(data, buffer_.data() + readPos_, firstPart);
            size_t secondPart = length - firstPart;
            std::memcpy(data + firstPart, buffer_.data(), secondPart);
        }

        return length;
    }

    // 跳过指定字节数
    void skip(size_t length) {
        size_t readable = readableBytes();
        if (length > readable) length = readable;
        readPos_ = (readPos_ + length) % buffer_.size();
    }

    // 可读字节数
    size_t readableBytes() const {
        if (writePos_ >= readPos_) {
            return writePos_ - readPos_;
        }
        return buffer_.size() - readPos_ + writePos_;
    }

    // 可写字节数
    size_t writableBytes() const {
        return buffer_.size() - readableBytes() - 1;
    }

    // 确保有足够空间
    void ensureSpace(size_t required) {
        if (writableBytes() >= required) return;

        // 尝试移动数据到开头
        compact();

        // 如果还是不够，扩容
        if (writableBytes() < required) {
            size_t newSize = buffer_.size() * 2;
            while (newSize - readableBytes() - 1 < required) {
                newSize *= 2;
            }
            if (newSize > MAX_SIZE) {
                newSize = MAX_SIZE;
            }
            resize(newSize);
        }
    }

    // 重置缓冲区
    void reset() {
        readPos_ = 0;
        writePos_ = 0;
    }

    // 缓冲区大小
    size_t capacity() const { return buffer_.size(); }

private:
    void compact() {
        if (readPos_ == 0) return;

        size_t readable = readableBytes();
        if (readable == 0) {
            readPos_ = 0;
            writePos_ = 0;
            return;
        }

        if (writePos_ > readPos_) {
            std::memmove(buffer_.data(), buffer_.data() + readPos_, readable);
        } else {
            std::vector<char> temp(readable);
            peek(temp.data(), readable);
            std::memcpy(buffer_.data(), temp.data(), readable);
        }

        readPos_ = 0;
        writePos_ = readable;
    }

    void resize(size_t newSize) {
        std::vector<char> newBuffer(newSize);
        size_t readable = readableBytes();
        peek(newBuffer.data(), readable);
        buffer_ = std::move(newBuffer);
        readPos_ = 0;
        writePos_ = readable;
    }

    std::vector<char> buffer_;
    size_t readPos_;
    size_t writePos_;
};

// ========== 前向声明 ==========

class NativeConnection;
class NativeListener;
class NativeNetworkManager;

/**
 * @brief 数据包处理器回调
 */
using PacketHandler = std::function<void(NativeConnection*, const char*, uint32_t)>;

/**
 * @brief 连接事件回调
 */
struct ConnectionCallbacks {
    std::function<void(NativeConnection*)> onConnected;
    std::function<void(NativeConnection*)> onDisconnected;
    std::function<void(NativeConnection*, const char*, uint32_t)> onData;
    std::function<void(NativeConnection*, int32_t)> onError;

    ConnectionCallbacks() = default;
};

/**
 * @brief 原生网络连接实现
 */
class NativeConnection : public Connection {
public:
    NativeConnection(socket_t sock, uint64_t connId,
                     const std::string& remoteAddr, uint16_t remotePort);
    ~NativeConnection() override;

    // Connection 接口实现
    bool isConnected() const override;
    ConnectionState getState() const override;
    int32_t send(const char* data, uint32_t length) override;
    bool sendAsync(const char* data, uint32_t length) override;
    void disconnect(const char* reason) override;
    void close() override;

    const std::string& getRemoteAddress() const override;
    uint16_t getRemotePort() const override;
    const std::string& getLocalAddress() const override;
    uint16_t getLocalPort() const override;
    uint64_t getConnectionId() const override;

    void* getUserData() const override;
    void setUserData(void* data) override;

    uint64_t getLastActiveTime() const override;
    uint64_t getConnectTime() const override;
    uint64_t getSentBytes() const override;
    uint64_t getReceivedBytes() const override;

    // 原生接口
    socket_t getSocket() const { return socket_; }
    NetBuffer& getRecvBuffer() { return recvBuffer_; }
    NetBuffer& getSendBuffer() { return sendBuffer_; }

    void setPacketHandler(PacketHandler handler) { packetHandler_ = std::move(handler); }
    void setConnectionCallbacks(const ConnectionCallbacks& callbacks) { callbacks_ = callbacks; }

    bool handleRecv();
    bool handleSend();

    void markForClose() { shouldClose_ = true; }
    bool shouldClose() const { return shouldClose_; }

    void updateActiveTime() { lastActiveTime_ = getCurrentTimeMs(); }

private:
    socket_t socket_;
    uint64_t connectionId_;
    std::string remoteAddress_;
    std::string localAddress_;
    uint16_t remotePort_;
    uint16_t localPort_;

    void* userData_;
    uint64_t connectTime_;
    uint64_t lastActiveTime_;
    uint64_t sentBytes_;
    uint64_t receivedBytes_;

    NetBuffer recvBuffer_;
    NetBuffer sendBuffer_;
    std::mutex sendMutex_;
    std::queue<std::vector<char>> sendQueue_;

    PacketHandler packetHandler_;
    ConnectionCallbacks callbacks_;

    std::atomic<bool> shouldClose_;
    ConnectionState state_;
};

/**
 * @brief 原生会话实现
 */
class NativeSession : public Session {
public:
    explicit NativeSession(NativeConnection* conn);
    ~NativeSession() override;

    // Session 接口实现
    void onEstablish(Connection* connection) override;
    void onTerminate(TerminateReason reason) override;
    uint32_t onRecv(const char* data, uint32_t length) override;
    void onError(int32_t errorCode, const char* errorMsg) override;

    uint64_t getSessionId() const override;
    Connection* getConnection() const override;
    void setConnection(Connection* connection) override;

    bool isActive() const override;
    void updateHeartbeat() override;
    uint64_t getLastHeartbeat() const override;
    bool isTimeout(uint64_t currentTime, uint64_t timeoutMs) const override;

protected:
    virtual void handleEstablish() {}
    virtual void handleTerminate(TerminateReason reason) {}
    virtual uint32_t handlePacket(const char* data, uint32_t length) { return length; }
    virtual void handleError(int32_t errorCode, const char* errorMsg) {}

private:
    static uint64_t generateSessionId();

    NativeConnection* connection_;
    uint64_t sessionId_;
    uint64_t lastHeartbeat_;
    bool active_;
};

/**
 * @brief 原生数据包解析器
 */
class NativePacketParser {
public:
    struct ParseResult {
        bool isValid;
        const char* data;
        uint32_t length;

        static ParseResult valid(const char* d, uint32_t l) {
            return {true, d, l};
        }
        static ParseResult invalid() {
            return {false, nullptr, 0};
        }
        static ParseResult needMore() {
            return {false, nullptr, 0};
        }
    };

    NativePacketParser() = default;
    virtual ~NativePacketParser() = default;

    /**
     * @brief 解析数据包
     * @return >0 表示完整包长度，0 表示需要更多数据，<0 表示错误
     */
    virtual int32_t parse(const char* data, uint32_t length) = 0;

    /**
     * @brief 获取包头大小
     */
    virtual uint32_t getHeaderSize() const = 0;
};

/**
 * @brief 默认长度前缀数据包解析器
 */
class LengthPrefixPacketParser : public NativePacketParser {
public:
    explicit LengthPrefixPacketParser(uint32_t maxPacketSize = 64 * 1024)
        : maxPacketSize_(maxPacketSize) {}

    int32_t parse(const char* data, uint32_t length) override {
        if (length < sizeof(uint32_t)) return 0; // 需要更多数据

        uint32_t packetLen = *reinterpret_cast<const uint32_t*>(data);

        if (packetLen > maxPacketSize_) {
            return -1; // 包过大，错误
        }

        if (length < sizeof(uint32_t) + packetLen) {
            return 0; // 数据不完整
        }

        return static_cast<int32_t>(sizeof(uint32_t) + packetLen);
    }

    uint32_t getHeaderSize() const override { return sizeof(uint32_t); }

private:
    uint32_t maxPacketSize_;
};

/**
 * @brief 原生会话工厂
 */
class NativeSessionFactory {
public:
    virtual ~NativeSessionFactory() = default;
    virtual NativeSession* createSession(NativeConnection* connection) = 0;
};

/**
 * @brief 通用会话工厂模板
 */
template <typename SessionType>
class TemplateNativeSessionFactory : public NativeSessionFactory {
public:
    NativeSession* createSession(NativeConnection* connection) override {
        return new SessionType(connection);
    }
};

/**
 * @brief 原生监听器实现
 */
class NativeListener : public Listener {
public:
    NativeListener();
    ~NativeListener() override;

    // Listener 接口实现
    bool start(const std::string& ip, uint16_t port) override;
    bool stop() override;
    bool isRunning() const override;

    const std::string& getListenAddress() const override { return listenAddress_; }
    uint16_t getListenPort() const override { return listenPort_; }

    uint32_t getConnectionCount() const override;
    uint32_t getMaxConnections() const override { return maxConnections_; }
    void setMaxConnections(uint32_t maxConn) override { maxConnections_ = maxConn; }

    void setSessionFactory(SessionFactoryPtr factory) override;
    void setPacketParser(PacketParserPtr parser) override;
    void setCallbacks(const ListenerCallbacks& callbacks) override;

    bool disconnectConnection(uint64_t connectionId, const char* reason) override;
    uint32_t disconnectAll(const char* reason) override;
    ConnectionPtr getConnection(uint64_t connectionId) const override;

    void setNoDelay(bool enable) override { noDelay_ = enable; }
    void setRecvBufferSize(int32_t size) override { recvBufferSize_ = size; }
    void setSendBufferSize(int32_t size) override { sendBufferSize_ = size; }

    // 原生接口
    socket_t getListenSocket() const { return listenSocket_; }
    bool handleAccept();

    void setNativeSessionFactory(NativeSessionFactory* factory) {
        nativeSessionFactory_ = factory;
    }

    void onConnectionClosed(NativeConnection* conn);

private:
    socket_t listenSocket_;
    std::string listenAddress_;
    uint16_t listenPort_;

    uint32_t maxConnections_;
    std::atomic<uint32_t> currentConnections_;
    std::unordered_map<uint64_t, std::shared_ptr<NativeConnection>> connections_;
    mutable std::mutex connectionsMutex_;

    SessionFactoryPtr apolloSessionFactory_;
    NativeSessionFactory* nativeSessionFactory_;
    PacketParserPtr packetParser_;
    std::shared_ptr<NativePacketParser> nativePacketParser_;
    ListenerCallbacks callbacks_;

    bool noDelay_;
    int32_t recvBufferSize_;
    int32_t sendBufferSize_;
    bool running_;

    std::atomic<uint64_t> nextConnectionId_;
};

/**
 * @brief 原生连接器实现
 */
class NativeConnector : public Connector {
public:
    NativeConnector();
    ~NativeConnector() override;

    // Connector 接口实现
    int32_t connect(const ConnectConfig& config) override;
    int32_t connect(const std::string& host, uint16_t port) override;
    int32_t reconnect() override;
    void disconnect() override;
    void stop() override;

    ConnectorState getState() const override { return state_; }
    ConnectionPtr getConnection() const override { return connection_; }
    const ConnectConfig& getConfig() const override { return config_; }
    void updateConfig(const ConnectConfig& config) override { config_ = config; }

    void setCallbacks(const ConnectorCallbacks& callbacks) override;
    void setPacketParser(PacketParserPtr parser) override;

    bool isConnected() const override;
    bool isConnecting() const override;

    // 原生接口
    bool handleConnect();
    socket_t getSocket() const { return socket_; }

private:
    void setState(ConnectorState state) { state_ = state; }
    void setConnection(std::shared_ptr<NativeConnection> conn) {
        connection_ = std::move(conn);
    }

    socket_t socket_;
    std::shared_ptr<NativeConnection> connection_;
    PacketParserPtr packetParser_;
    std::shared_ptr<NativePacketParser> nativePacketParser_;
    ConnectorCallbacks callbacks_;

    ConnectConfig config_;
    ConnectorState state_;
    std::atomic<uint64_t> nextConnectionId_;
};

/**
 * @brief 原生网络管理器实现
 */
class NativeNetworkManager : public NetworkManager {
public:
    NativeNetworkManager();
    ~NativeNetworkManager() override;

    // NetworkManager 接口实现
    bool initialize(const NetworkConfig& config) override;
    bool start() override;
    void stop() override;
    bool isRunning() const override { return running_; }
    void update() override;

    ListenerPtr createListener(const std::string& name) override;
    ListenerPtr getListener(const std::string& name) const override;
    bool removeListener(const std::string& name) override;
    void removeAllListeners() override;

    ConnectorPtr createConnector(const std::string& name) override;
    ConnectorPtr getConnector(const std::string& name) const override;
    bool removeConnector(const std::string& name) override;
    void removeAllConnectors() override;

    ConnectionPtr getConnection(uint64_t connectionId) const override;
    bool disconnectConnection(uint64_t connectionId, const char* reason) override;
    uint32_t broadcast(const char* data, uint32_t length,
                       const std::vector<uint64_t>& excludeIds) override;

    NetworkStats getStats() const override;
    void resetStats() override;
    const NetworkConfig& getConfig() const override { return config_; }

    // 原生接口
    void addConnection(std::shared_ptr<NativeConnection> conn);
    void removeConnection(uint64_t connectionId);
    std::shared_ptr<NativeConnection> getNativeConnection(uint64_t connectionId) const;

private:
    // IO 线程
    void ioThreadProc();
    bool processEvents(int timeoutMs);

    NetworkConfig config_;
    mutable NetworkStats stats_;

    std::unordered_map<std::string, std::shared_ptr<NativeListener>> listeners_;
    std::unordered_map<std::string, std::shared_ptr<NativeConnector>> connectors_;
    std::unordered_map<uint64_t, std::shared_ptr<NativeConnection>> connections_;
    mutable std::mutex mutex_;

    std::vector<std::thread> ioThreads_;
    std::atomic<bool> running_;
    uint32_t threadCount_;

#ifdef _WIN32
    HANDLE ioPort_;
    std::vector<HANDLE> events_;
#else
    int epollFd_;
#endif
};

/**
 * @brief Native 适配器入口
 */
class NativeAdapter {
public:
    static NetworkManagerPtr createManager();
    static bool initialize();
    static void shutdown();
    static std::string getVersion();
};

} // namespace adapters
} // namespace net
} // namespace apollo
