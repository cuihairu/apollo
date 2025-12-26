#pragma once

#include "apollo/ipc/channel.h"
#include <atomic>
#include <memory>
#include <functional>
#include <vector>
#include <unordered_map>

// 平台检测
#if defined(_WIN32)
    #define APOLLO_IO_IOCP
    #include <windows.h>
#elif defined(__linux__)
    #if defined(__has_include)
        #if __has_include(<linux/io_uring.h>)
            #define APOLLO_IO_URING
            #include <linux/io_uring.h>
        #else
            #define APOLLO_IO_EPOLL
            #include <sys/epoll.h>
        #endif
    #else
        #define APOLLO_IO_EPOLL
        #include <sys/epoll.h>
    #endif
#elif defined(__APPLE__) || defined(__FreeBSD__)
    #define APOLLO_IO_KQUEUE
    #include <sys/event.h>
#endif

namespace apollo {
namespace ipc {

//==============================================================================
// IO 操作类型
//==============================================================================

enum class IoOp : uint8_t {
    Accept,
    Connect,
    Read,
    Write,
    Close,
    Timeout
};

//==============================================================================
// IO 事件
//==============================================================================

struct IoEvent {
    void* userData;              // 用户数据
    int64_t result;              // 操作结果 (字节数 或 错误码)
    uint32_t error;              // 错误代码
    IoOp operation;              // 操作类型

    // 隐式转换为 bool (检查是否成功)
    explicit operator bool() const { return error == 0; }
    bool isError() const { return error != 0; }
};

//==============================================================================
// IO 操作完成回调
//==============================================================================

using IoCallback = std::function<void(const IoEvent&)>;

//==============================================================================
// 异步 IO 操作
//==============================================================================

struct AsyncOp {
    void* userData = nullptr;
    IoCallback callback;
    int fd = -1;
    std::vector<uint8_t> buffer;  // 写数据
    size_t offset = 0;
    uint32_t flags = 0;
};

//==============================================================================
// IO 复用抽象接口
//==============================================================================

class IoMultiplexer {
public:
    virtual ~IoMultiplexer() = default;

    // 启动/停止
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;

    // 注册文件描述符
    virtual bool registerFd(int fd, uint32_t events) = 0;
    virtual bool unregisterFd(int fd) = 0;
    virtual bool modifyFd(int fd, uint32_t events) = 0;

    // 异步操作
    virtual bool postRead(int fd, void* buffer, size_t size,
                          AsyncOp::IoCallback callback, void* userData = nullptr) = 0;
    virtual bool postWrite(int fd, const void* data, size_t size,
                           AsyncOp::IoCallback callback, void* userData = nullptr) = 0;
    virtual bool postAccept(int listenFd,
                            AsyncOp::IoCallback callback, void* userData = nullptr) = 0;
    virtual bool postConnect(int fd, const std::string& address,
                             uint16_t port,
                             AsyncOp::IoCallback callback, void* userData = nullptr) = 0;

    // 定时器
    virtual uint64_t scheduleTimer(uint32_t delayMs,
                                   std::function<void()> callback) = 0;
    virtual bool cancelTimer(uint64_t timerId) = 0;

    // 事件循环
    virtual void runOnce(uint32_t timeoutMs = 0) = 0;
    virtual void run() = 0;

    // 统计
    virtual size_t getPendingOps() const = 0;
    virtual size_t getProcessedOps() const = 0;

    // 工厂方法
    static std::unique_ptr<IoMultiplexer> create(size_t maxEvents = 1024);
    static const char* getBackendName();
};

//==============================================================================
// IOCP 实现 (Windows)
//==============================================================================

#ifdef APOLLO_IO_IOCP

class IocpMultiplexer : public IoMultiplexer {
public:
    explicit IocpMultiplexer(size_t maxEvents = 1024);
    ~IocpMultiplexer() override;

    bool start() override;
    void stop() override;
    bool isRunning() const override { return running_; }

    bool registerFd(int fd, uint32_t events) override;
    bool unregisterFd(int fd) override;
    bool modifyFd(int fd, uint32_t events) override;

    bool postRead(int fd, void* buffer, size_t size,
                  AsyncOp::IoCallback callback, void* userData = nullptr) override;
    bool postWrite(int fd, const void* data, size_t size,
                   AsyncOp::IoCallback callback, void* userData = nullptr) override;
    bool postAccept(int listenFd,
                    AsyncOp::IoCallback callback, void* userData = nullptr) override;
    bool postConnect(int fd, const std::string& address, uint16_t port,
                     AsyncOp::IoCallback callback, void* userData = nullptr) override;

    uint64_t scheduleTimer(uint32_t delayMs, std::function<void()> callback) override;
    bool cancelTimer(uint64_t timerId) override;

    void runOnce(uint32_t timeoutMs = 0) override;
    void run() override;

    size_t getPendingOps() const override { return pendingOps_; }
    size_t getProcessedOps() const override { return processedOps_; }

    // IOCP 特定
    HANDLE getIocpHandle() const { return iocpHandle_; }

private:
    struct IocpOperation {
        OVERLAPPED overlapped{};
        AsyncOp asyncOp;
        std::vector<uint8_t> buffer;
        int fd;
        bool accept;
        SOCKET acceptSocket;
        char acceptAddrBuf[sizeof(SOCKADDR_IN) + 16];
    };

    bool processCompletion(DWORD bytesTransferred, ULONG_PTR completionKey, LPOVERLAPPED overlapped);
    IocpOperation* createOp(int fd, const AsyncOp& asyncOp);

    HANDLE iocpHandle_{nullptr};
    HANDLE wakeupEvent_{nullptr};
    std::atomic<bool> running_{false};
    size_t maxEvents_;

    std::atomic<size_t> pendingOps_{0};
    std::atomic<size_t> processedOps_{0};

    // 定时器线程
    std::thread timerThread_;
    std::mutex timerMutex_;
    std::unordered_map<uint64_t, std::function<void()>> timers_;
    std::atomic<uint64_t> nextTimerId_{1};
};

#endif // APOLLO_IO_IOCP

//==============================================================================
// io_uring 实现 (Linux 5.1+)
//==============================================================================

#ifdef APOLLO_IO_URING

class IoUringMultiplexer : public IoMultiplexer {
public:
    explicit IoUringMultiplexer(size_t queueDepth = 1024, size_t entries = 256);
    ~IoUringMultiplexer() override;

    bool start() override;
    void stop() override;
    bool isRunning() const override { return running_; }

    bool registerFd(int fd, uint32_t events) override;
    bool unregisterFd(int fd) override;
    bool modifyFd(int fd, uint32_t events) override;

    bool postRead(int fd, void* buffer, size_t size,
                  AsyncOp::IoCallback callback, void* userData = nullptr) override;
    bool postWrite(int fd, const void* data, size_t size,
                   AsyncOp::IoCallback callback, void* userData = nullptr) override;
    bool postAccept(int listenFd,
                    AsyncOp::IoCallback callback, void* userData = nullptr) override;
    bool postConnect(int fd, const std::string& address, uint16_t port,
                     AsyncOp::IoCallback callback, void* userData = nullptr) override;

    uint64_t scheduleTimer(uint32_t delayMs, std::function<void()> callback) override;
    bool cancelTimer(uint64_t timerId) override;

    void runOnce(uint32_t timeoutMs = 0) override;
    void run() override;

    size_t getPendingOps() const override { return pendingOps_; }
    size_t getProcessedOps() const override { return processedOps_; }

    // io_uring 特定
    struct io_uring* getRing() const { return ring_.get(); }

private:
    struct RingDeleter {
        void operator()(struct io_uring* ring) const;
    };

    std::unique_ptr<struct io_uring, RingDeleter> ring_;
    std::atomic<bool> running_{false};
    size_t queueDepth_;

    std::atomic<size_t> pendingOps_{0};
    std::atomic<size_t> processedOps_{0};

    // 定时器支持 (io_uring 原生)
    std::mutex timerMutex_;
    std::unordered_map<uint64_t, std::function<void()>> timers_;
    std::atomic<uint64_t> nextTimerId_{1};
};

#endif // APOLLO_IO_URING

//==============================================================================
// epoll 实现 (Linux fallback)
//==============================================================================

#ifdef APOLLO_IO_EPOLL

class EpollMultiplexer : public IoMultiplexer {
public:
    explicit EpollMultiplexer(size_t maxEvents = 1024);
    ~EpollMultiplexer() override;

    bool start() override;
    void stop() override;
    bool isRunning() const override { return running_; }

    bool registerFd(int fd, uint32_t events) override;
    bool unregisterFd(int fd) override;
    bool modifyFd(int fd, uint32_t events) override;

    bool postRead(int fd, void* buffer, size_t size,
                  AsyncOp::IoCallback callback, void* userData = nullptr) override;
    bool postWrite(int fd, const void* data, size_t size,
                   AsyncOp::IoCallback callback, void* userData = nullptr) override;
    bool postAccept(int listenFd,
                    AsyncOp::IoCallback callback, void* userData = nullptr) override;
    bool postConnect(int fd, const std::string& address, uint16_t port,
                     AsyncOp::IoCallback callback, void* userData = nullptr) override;

    uint64_t scheduleTimer(uint32_t delayMs, std::function<void()> callback) override;
    bool cancelTimer(uint64_t timerId) override;

    void runOnce(uint32_t timeoutMs = 0) override;
    void run() override;

    size_t getPendingOps() const override { return pendingOps_; }
    size_t getProcessedOps() const override { return processedOps_; }

    // epoll 特定
    int getEpollFd() const { return epollFd_; }

private:
    struct FdContext {
        int fd;
        uint32_t events;
        std::function<void(int, uint32_t)> callback;
        std::vector<uint8_t> readBuffer;
        std::queue<std::vector<uint8_t>> writeQueue;
    };

    bool processEvents(int count);
    void handleRead(FdContext* ctx);
    void handleWrite(FdContext* ctx);
    void handleAccept(FdContext* ctx);

    int epollFd_{-1};
    int wakeupFd_{-1};          // eventfd 用于唤醒
    std::vector<struct epoll_event> events_;
    std::atomic<bool> running_{false};
    size_t maxEvents_;

    std::atomic<size_t> pendingOps_{0};
    std::atomic<size_t> processedOps_{0};

    std::unordered_map<int, FdContext> fdContexts_;
    std::mutex fdMutex_;

    // 定时器支持
    std::thread timerThread_;
    std::mutex timerMutex_;
    std::unordered_map<uint64_t, std::function<void()>> timers_;
    std::atomic<uint64_t> nextTimerId_{1};
    std::atomic<bool> timerRunning_{false};
};

#endif // APOLLO_IO_EPOLL

//==============================================================================
// kqueue 实现 (macOS/BSD)
//==============================================================================

#ifdef APOLLO_IO_KQUEUE

class KqueueMultiplexer : public IoMultiplexer {
public:
    explicit KqueueMultiplexer(size_t maxEvents = 1024);
    ~KqueueMultiplexer() override;

    bool start() override;
    void stop() override;
    bool isRunning() const override { return running_; }

    bool registerFd(int fd, uint32_t events) override;
    bool unregisterFd(int fd) override;
    bool modifyFd(int fd, uint32_t events) override;

    bool postRead(int fd, void* buffer, size_t size,
                  AsyncOp::IoCallback callback, void* userData = nullptr) override;
    bool postWrite(int fd, const void* data, size_t size,
                   AsyncOp::IoCallback callback, void* userData = nullptr) override;
    bool postAccept(int listenFd,
                    AsyncOp::IoCallback callback, void* userData = nullptr) override;
    bool postConnect(int fd, const std::string& address, uint16_t port,
                     AsyncOp::IoCallback callback, void* userData = nullptr) override;

    uint64_t scheduleTimer(uint32_t delayMs, std::function<void()> callback) override;
    bool cancelTimer(uint64_t timerId) override;

    void runOnce(uint32_t timeoutMs = 0) override;
    void run() override;

    size_t getPendingOps() const override { return pendingOps_; }
    size_t getProcessedOps() const override { return processedOps_; }

    // kqueue 特定
    int getKqueueFd() const { return kqFd_; }

private:
    struct FdContext {
        int fd;
        uint16_t filter;      // EVFILT_READ or EVFILT_WRITE
        std::function<void(int, uint16_t)> callback;
        std::vector<uint8_t> readBuffer;
        std::queue<std::vector<uint8_t>> writeQueue;
    };

    int kqFd_{-1};
    std::vector<struct kevent> events_;
    std::atomic<bool> running_{false};
    size_t maxEvents_;

    std::atomic<size_t> pendingOps_{0};
    std::atomic<size_t> processedOps_{0};

    std::unordered_map<int, FdContext> fdContexts_;
    std::mutex fdMutex_;
};

#endif // APOLLO_IO_KQUEUE

//==============================================================================
// 基于 IoMultiplexer 的异步 Channel
//==============================================================================

class AsyncChannel : public Channel {
public:
    AsyncChannel(std::unique_ptr<IoMultiplexer> multiplexer,
                 const ChannelConfig& config);
    ~AsyncChannel() override;

    // 连接管理
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    ChannelState getState() const override { return state_; }

    bool bind() override;
    bool listen() override;
    std::unique_ptr<Channel> accept() override;

    // 发送消息
    SendResult send(const Message& msg) override;
    SendResult send(const BinaryMessage& msg) override;
    SendResult send(const TextMessage& msg) override;
    SendResult sendRaw(const void* data, size_t size) override;

    void sendAsync(const Message& msg,
                   std::function<void(SendResult)> callback) override;
    SendResult trySend(const Message& msg) override;

    // 接收消息
    Message receive() override;
    bool receive(Message& msg) override;
    bool tryReceive(Message& msg) override;
    void receiveAsync(std::function<void(Message)> callback) override;
    bool receive(Message& msg, uint32_t timeoutMs) override;

    // 订阅模式
    void subscribe(uint32_t msgType, MessageCallback callback) override;
    void unsubscribe(uint32_t msgType) override;
    void subscribeAll(MessageCallback callback) override;
    void unsubscribeAll() override;

    void startMessageLoop() override;
    void stopMessageLoop() override;
    bool isMessageLoopRunning() const override;

    // Request-Reply
    std::future<Message> request(const Message& req, uint32_t timeoutMs) override;
    void setRequestHandler(std::function<Message(const Message&)> handler) override;

    // 背压控制
    void setBackpressureStrategy(BackpressureStrategy strategy) override;
    BackpressureStrategy getBackpressureStrategy() const override;
    bool isHighWatermark() const override;
    bool isLowWatermark() const override;
    size_t getPendingMessages() const override;
    size_t getPendingBytes() const override;
    void onHighWatermark(WatermarkCallback callback) override;
    void onLowWatermark(WatermarkCallback callback) override;

    // 自动重连
    void enableAutoReconnect(bool enable) override;
    bool isAutoReconnectEnabled() const override;
    bool reconnect() override;
    int getReconnectCount() const override;
    int getRemainingRetries() const override;
    uint32_t getNextReconnectDelay() const override;
    void setReconnectConfig(const ReconnectConfig& config) override;
    ReconnectConfig getReconnectConfig() const override;

    // 属性
    std::string getName() const override { return config_.name; }
    TransportType getTransportType() const override { return config_.transport; }
    ChannelMode getMode() const override { return config_.mode; }
    ChannelConfig getConfig() const override { return config_; }

    // 统计
    size_t getSentBytes() const override { return sentBytes_; }
    size_t getReceivedBytes() const override { return receivedBytes_; }
    size_t getSentMessages() const override { return sentMessages_; }
    size_t getReceivedMessages() const override { return receivedMessages_; }
    size_t getDroppedMessages() const override { return droppedMessages_; }

    // 事件回调
    void onStateChange(StateCallback callback) override;
    void onError(ErrorCallback callback) override;
    void onClose(CloseCallback callback) override;

    // 获取底层 multiplexer
    IoMultiplexer* getMultiplexer() const { return multiplexer_.get(); }
    int getFd() const { return fd_; }

private:
    // 连接管理
    bool doConnect();
    bool doBind();
    void doAccept(std::unique_ptr<AsyncChannel> peer, SOCKET sock);
    void onConnected();
    void onDisconnected();
    void onError(const std::string& error);

    // 发送/接收处理
    void handleRead(const IoEvent& event);
    void handleWrite(const IoEvent& event);
    void processMessage(const void* data, size_t size);

    // 重连逻辑
    void startReconnectThread();
    void stopReconnectThread();
    void reconnectThreadFunc();
    uint32_t calculateReconnectDelay() const;

    // 心跳和健康检查
    void startHeartbeat();
    void stopHeartbeat();
    void sendHeartbeat();
    void checkHealth();

    std::unique_ptr<IoMultiplexer> multiplexer_;
    ChannelConfig config_;

    int fd_{-1};
    std::atomic<ChannelState> state_{ChannelState::Disconnected};

    // 统计
    std::atomic<size_t> sentBytes_{0};
    std::atomic<size_t> receivedBytes_{0};
    std::atomic<size_t> sentMessages_{0};
    std::atomic<size_t> receivedMessages_{0};
    std::atomic<size_t> droppedMessages_{0};

    // 接收缓冲
    std::vector<uint8_t> recvBuffer_;
    size_t recvOffset_{0};
    size_t recvNeeded_{0};

    // 订阅
    std::unordered_map<uint32_t, MessageCallback> subscriptions_;
    std::mutex subscriptionMutex_;

    // 回调
    StateCallback stateCallback_;
    ErrorCallback errorCallback_;
    CloseCallback closeCallback_;

    // 重连
    std::thread reconnectThread_;
    std::atomic<bool> reconnectRunning_{false};
    std::atomic<int> reconnectCount_{0};
    ReconnectConfig reconnectConfig_;

    // 心跳
    std::thread heartbeatThread_;
    std::atomic<bool> heartbeatRunning_{false};
    std::atomic<uint64_t> lastReceiveTime_{0};

    // 背压
    std::unique_ptr<BackpressureManager> backpressure_;
    WatermarkCallback highWatermarkCallback_;
    WatermarkCallback lowWatermarkCallback_;

    // 请求-响应
    std::unordered_map<uint64_t, std::promise<Message>> pendingRequests_;
    std::mutex requestMutex_;
    std::atomic<uint64_t> nextRequestId_{1};

    // 消息循环
    std::thread messageLoopThread_;
    std::atomic<bool> messageLoopRunning_{false};
};

} // namespace ipc
} // namespace apollo
