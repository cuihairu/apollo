#pragma once

#include "apollo/ipc/transport.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <string>
#include <chrono>

namespace apollo {
namespace ipc {

//==============================================================================
// 前向声明
//==============================================================================

class ITransport;
class Channel;
struct ChannelConfig;

//==============================================================================
// 背压策略
//==============================================================================

enum class BackpressureStrategy : uint8_t {
    Drop,       // 丢弃新消息
    Buffer,     // 缓冲所有消息
    Block,      // 阻塞发送者
    Fail,       // 返回失败
    Signal      // 触发信号
};

//==============================================================================
// 背压配置 - 支持双重水位线（消息数量 + 字节大小）
//==============================================================================

struct BackpressureConfig {
    BackpressureStrategy strategy = BackpressureStrategy::Buffer;
    
    // 新增：基于消息数量的水位线（类似 Go Channel）
    size_t lowMessageCount = 32;       // 低水位：32条消息
    size_t highMessageCount = 128;     // 高水位：128条消息
    size_t maxMessageCount = 256;      // 最大消息数
    
    // 新增：基于字节大小的水位线（防止 OOM）
    size_t lowByteCount = 32 * 1024;   // 低水位：32KB
    size_t highByteCount = 128 * 1024; // 高水位：128KB
    size_t maxByteCount = 256 * 1024;  // 最大字节数
    
    // 阻塞配置
    uint32_t blockTimeoutMs = 30000;
    
    // 回调
    std::function<void()> onHighWatermark;
    std::function<void()> onLowWatermark;
    
    // 兼容旧版本：已弃用的字段
    [[deprecated("Use lowMessageCount and lowByteCount instead")]]
    size_t lowWatermark = 32 * 1024;
    
    [[deprecated("Use highMessageCount and highByteCount instead")]]
    size_t highWatermark = 128 * 1024;
    
    [[deprecated("Use maxMessageCount and maxByteCount instead")]]
    size_t bufferSize = 256 * 1024;
};
//==============================================================================
// 背压管理器 - 双重水位线实现
//==============================================================================

class BackpressureManager {
public:
    explicit BackpressureManager(const BackpressureConfig& config);
    ~BackpressureManager() = default;
    
    // 配置
    const BackpressureConfig& getConfig() const { return config_; }
    void setConfig(const BackpressureConfig& config) { config_ = config; }
    
    // 新增：消息数量追踪
    void addMessage(size_t messageSize);
    void removeMessage(size_t messageSize);
    size_t getPendingCount() const { return pendingCount_.load(std::memory_order_acquire); }
    size_t getPendingSize() const { return pendingSize_.load(std::memory_order_acquire); }
    
    // 水位线判断 - 双重条件
    bool isHighWatermark() const;
    bool isLowWatermark() const;
    
    // 发送检查
    bool canSend(size_t pendingSize) const;
    
    // 应用背压策略
    SendResult applyBackpressure(size_t messageSize);
    
    // 等待可用空间
    bool waitForAvailable(size_t requiredSize, uint32_t timeoutMs);
    
    // 重置
    void reset();
    
private:
    void updateWatermark(size_t currentSize);
    
    BackpressureConfig config_;
    
    // 新增：消息数量计数器
    std::atomic<size_t> pendingCount_{0};
    
    // 原有：字节大小计数器
    std::atomic<size_t> pendingSize_{0};
    
    std::atomic<bool> highWatermarkTriggered_{false};
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};
//==============================================================================
// 重连策略
//==============================================================================

enum class ReconnectStrategy : uint8_t {
    FixedDelay,     // 固定延迟
    ExponentialBackoff,  // 指数退避
    LinearBackoff   // 线性退避
};

//==============================================================================
// 重连配置
//==============================================================================

struct ReconnectConfig {
    bool enable = false;
    ReconnectStrategy strategy = ReconnectStrategy::ExponentialBackoff;
    uint32_t initialDelayMs = 1000;
    uint32_t maxDelayMs = 30000;
    int maxRetries = -1;  // -1 表示无限重试
    uint32_t retryIntervalMs = 2000;
    
    // 心跳检测
    bool enableHeartbeat = false;
    uint32_t heartbeatIntervalMs = 30000;
    uint32_t heartbeatTimeoutMs = 5000;
    
    // 健康检查
    bool enableHealthCheck = false;
    uint32_t healthCheckIntervalMs = 60000;
    uint32_t healthCheckTimeoutMs = 5000;
    
    // 回调
    std::function<void()> onReconnecting;
    std::function<void()> onReconnected;
    std::function<void(int)> onReconnectFailed;
    std::function<void()> onGiveUp;
};
//==============================================================================
// Channel 配置
//==============================================================================

enum class ChannelMode : uint8_t {
    Client,
    Server
};

struct ChannelConfig {
    // 基本信息
    std::string name;
    TransportType transport = TransportType::Tcp;
    ChannelMode mode = ChannelMode::Client;
    
    // 网络配置
    std::string host = "127.0.0.1";
    uint16_t port = 8080;
    std::string path;  // 用于 Unix Socket / 共享内存
    
    // 缓冲区配置
    size_t sendBufferSize = 64 * 1024;
    size_t recvBufferSize = 64 * 1024;
    size_t maxMessageSize = 16 * 1024 * 1024;
    size_t sharedMemorySize = 1024 * 1024;
    
    // 超时配置
    uint32_t connectTimeoutMs = 5000;
    uint32_t sendTimeoutMs = 5000;
    uint32_t recvTimeoutMs = 30000;
    
    // 背压配置
    BackpressureConfig backpressure;
    
    // 重连配置
    ReconnectConfig reconnect;
    
    // TCP 选项
    bool enableNagle = true;
    bool enableKeepAlive = true;
    uint32_t keepAliveIdleSec = 60;
    uint32_t keepAliveIntervalSec = 5;
    uint32_t keepAliveCount = 3;
    
    // 其他
    bool blocking = false;
};
//==============================================================================
// Channel ״̬
//==============================================================================

enum class ChannelState : uint8_t {
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
    Error
};

inline const char* toString(ChannelState state) {
    switch (state) {
        case ChannelState::Disconnected: return "Disconnected";
        case ChannelState::Connecting: return "Connecting";
        case ChannelState::Connected: return "Connected";
        case ChannelState::Disconnecting: return "Disconnecting";
        case ChannelState::Error: return "Error";
        default: return "Unknown";
    }
}

//==============================================================================
// 消息类型
//==============================================================================

enum class MessageType : uint32_t {
    Text = 1,
    Binary = 2,
    Request = 3,
    Response = 4,
    Heartbeat = 5,
    Custom = 100
};
//==============================================================================
// 消息基类
//==============================================================================

class Message {
public:
    virtual ~Message() = default;
    
    virtual MessageType getType() const = 0;
    virtual std::vector<uint8_t> serialize() const = 0;
    virtual bool deserialize(const std::vector<uint8_t>& data) = 0;
    virtual std::shared_ptr<Message> clone() const = 0;
};

//==============================================================================
// 文本消息
//==============================================================================

class TextMessage : public Message {
public:
    TextMessage() = default;
    explicit TextMessage(std::string text) : text_(std::move(text)) {}
    
    MessageType getType() const override { return MessageType::Text; }
    
    std::vector<uint8_t> serialize() const override {
        return std::vector<uint8_t>(text_.begin(), text_.end());
    }
    
    bool deserialize(const std::vector<uint8_t>& data) override {
        text_.assign(data.begin(), data.end());
        return true;
    }
    
    std::shared_ptr<Message> clone() const override {
        return std::make_shared<TextMessage>(text_);
    }
    
    const std::string& getText() const { return text_; }
    void setText(std::string text) { text_ = std::move(text); }
    
private:
    std::string text_;
};
//==============================================================================
// 二进制消息
//==============================================================================

class BinaryMessage : public Message {
public:
    BinaryMessage() = default;
    explicit BinaryMessage(std::vector<uint8_t> data) : data_(std::move(data)) {}
    
    MessageType getType() const override { return MessageType::Binary; }
    
    std::vector<uint8_t> serialize() const override {
        return data_;
    }
    
    bool deserialize(const std::vector<uint8_t>& data) override {
        data_ = data;
        return true;
    }
    
    std::shared_ptr<Message> clone() const override {
        return std::make_shared<BinaryMessage>(data_);
    }
    
    const std::vector<uint8_t>& getData() const { return data_; }
    void setData(std::vector<uint8_t> data) { data_ = std::move(data); }
    size_t getSize() const { return data_.size(); }
    
private:
    std::vector<uint8_t> data_;
};
//==============================================================================
// 发送结果
//==============================================================================

enum class SendResult : uint8_t {
    Success,
    Failed,
    TimedOut,
    Dropped,
    BufferFull,
    NotConnected
};

//==============================================================================
// 回调类型
//==============================================================================

using MessageCallback = std::function<void(const Message&)>;
using StateCallback = std::function<void(ChannelState)>;
using ErrorCallback = std::function<void(const std::string&)>;
using CloseCallback = std::function<void()>;
using WatermarkCallback = std::function<void()>;
//==============================================================================
// Channel 接口
//==============================================================================

class Channel : public std::enable_shared_from_this<Channel> {
public:
    virtual ~Channel() = default;
    
    // 工厂方法
    static std::unique_ptr<Channel> create(const std::string& name, const ChannelConfig& config);
    static std::unique_ptr<Channel> createClient(const std::string& host, uint16_t port);
    static std::unique_ptr<Channel> createServer(const std::string& host, uint16_t port);
    
    // 连接管理
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual ChannelState getState() const = 0;
    
    virtual bool bind() = 0;
    virtual bool listen() = 0;
    virtual std::unique_ptr<Channel> accept() = 0;
    
    // 发送消息
    virtual SendResult send(const Message& msg) = 0;
    virtual SendResult send(const BinaryMessage& msg) = 0;
    virtual SendResult send(const TextMessage& msg) = 0;
    virtual SendResult sendRaw(const void* data, size_t size) = 0;
    
    virtual void sendAsync(const Message& msg, std::function<void(SendResult)> callback) = 0;
    virtual SendResult trySend(const Message& msg) = 0;
    // 接收消息
    virtual Message receive() = 0;
    virtual bool receive(Message& msg) = 0;
    virtual bool tryReceive(Message& msg) = 0;
    virtual void receiveAsync(std::function<void(Message)> callback) = 0;
    virtual bool receive(Message& msg, uint32_t timeoutMs) = 0;
    
    // 订阅模式
    virtual void subscribe(uint32_t msgType, MessageCallback callback) = 0;
    virtual void unsubscribe(uint32_t msgType) = 0;
    virtual void subscribeAll(MessageCallback callback) = 0;
    virtual void unsubscribeAll() = 0;
    
    virtual void startMessageLoop() = 0;
    virtual void stopMessageLoop() = 0;
    virtual bool isMessageLoopRunning() const = 0;
    
    // Request-Reply ģʽ
    virtual std::future<Message> request(const Message& req, uint32_t timeoutMs) = 0;
    virtual void setRequestHandler(std::function<Message(const Message&)> handler) = 0;
    // 背压控制
    virtual void setBackpressureStrategy(BackpressureStrategy strategy) = 0;
    virtual BackpressureStrategy getBackpressureStrategy() const = 0;
    virtual bool isHighWatermark() const = 0;
    virtual bool isLowWatermark() const = 0;
    virtual size_t getPendingMessages() const = 0;
    virtual size_t getPendingBytes() const = 0;
    virtual void onHighWatermark(WatermarkCallback callback) = 0;
    virtual void onLowWatermark(WatermarkCallback callback) = 0;
    
    // 自动重连
    virtual void enableAutoReconnect(bool enable) = 0;
    virtual bool isAutoReconnectEnabled() const = 0;
    virtual bool reconnect() = 0;
    virtual int getReconnectCount() const = 0;
    virtual int getRemainingRetries() const = 0;
    virtual uint32_t getNextReconnectDelay() const = 0;
    virtual void setReconnectConfig(const ReconnectConfig& config) = 0;
    virtual ReconnectConfig getReconnectConfig() const = 0;
    // 属性
    virtual std::string getName() const = 0;
    virtual TransportType getTransportType() const = 0;
    virtual ChannelMode getMode() const = 0;
    virtual ChannelConfig getConfig() const = 0;
    
    // 统计
    virtual size_t getSentBytes() const = 0;
    virtual size_t getReceivedBytes() const = 0;
    virtual size_t getSentMessages() const = 0;
    virtual size_t getReceivedMessages() const = 0;
    virtual size_t getDroppedMessages() const = 0;
    
    // 事件回调
    virtual void onStateChange(StateCallback callback) = 0;
    virtual void onError(ErrorCallback callback) = 0;
    virtual void onClose(CloseCallback callback) = 0;
    
    // 获取传输层
    virtual ITransport* getTransport() const = 0;
};
//==============================================================================
// ChannelBuilder - 流式构建器
//==============================================================================

class ChannelBuilder {
public:
    ChannelBuilder();
    ~ChannelBuilder() = default;
    
    // 基本信息
    ChannelBuilder& name(const std::string& name);
    ChannelBuilder& transport(TransportType type);
    ChannelBuilder& mode(ChannelMode mode);
    
    // 网络配置
    ChannelBuilder& host(const std::string& host);
    ChannelBuilder& port(uint16_t port);
    ChannelBuilder& path(const std::string& path);
    
    // 缓冲区配置
    ChannelBuilder& bufferSize(size_t size);
    ChannelBuilder& maxMessageSize(size_t size);
    ChannelBuilder& sharedMemorySize(size_t size);
    
    // 背压配置
    ChannelBuilder& backpressure(BackpressureStrategy strategy);
    ChannelBuilder& watermarks(size_t low, size_t high);
    ChannelBuilder& bufferSize(size_t low, size_t high, size_t max);
    ChannelBuilder& blockTimeout(uint32_t ms);
    // 超时配置
    ChannelBuilder& connectTimeout(uint32_t ms);
    ChannelBuilder& sendTimeout(uint32_t ms);
    ChannelBuilder& recvTimeout(uint32_t ms);
    ChannelBuilder& blocking(bool enabled);
    ChannelBuilder& nagle(bool enabled);
    
    // 回调
    ChannelBuilder& onHighWatermark(std::function<void()> callback);
    ChannelBuilder& onLowWatermark(std::function<void()> callback);
    
    // 重连配置
    ChannelBuilder& autoReconnect(bool enable);
    ChannelBuilder& reconnectStrategy(ReconnectStrategy strategy);
    ChannelBuilder& reconnectDelay(uint32_t initialMs, uint32_t maxMs);
    ChannelBuilder& maxRetries(int maxRetries);
    ChannelBuilder& heartbeat(uint32_t intervalMs, uint32_t timeoutMs);
    ChannelBuilder& healthCheck(uint32_t intervalMs, uint32_t timeoutMs);
    ChannelBuilder& onReconnecting(std::function<void()> callback);
    ChannelBuilder& onReconnected(std::function<void()> callback);
    ChannelBuilder& onReconnectFailed(std::function<void(int)> callback);
    ChannelBuilder& onGiveUp(std::function<void()> callback);
    // 构建
    std::unique_ptr<Channel> build();
    std::unique_ptr<Channel> connect();
    
private:
    std::string name_;
    ChannelConfig config_;
};
//==============================================================================
// 便捷工厂函数
//==============================================================================

inline std::unique_ptr<Channel> createTcpClient(const std::string& host, uint16_t port) {
    return Channel::createClient(host, port);
}

inline std::unique_ptr<Channel> createTcpServer(const std::string& host, uint16_t port) {
    return Channel::createServer(host, port);
}
//==============================================================================
// 辅助函数 - 发送结果转字符串
//==============================================================================

inline const char* toString(SendResult result) {
    switch (result) {
        case SendResult::Success: return "Success";
        case SendResult::Failed: return "Failed";
        case SendResult::TimedOut: return "TimedOut";
        case SendResult::Dropped: return "Dropped";
        case SendResult::BufferFull: return "BufferFull";
        case SendResult::NotConnected: return "NotConnected";
        default: return "Unknown";
    }
}
//==============================================================================
// 辅助函数 - 背压策略转字符串
//==============================================================================

inline const char* toString(BackpressureStrategy strategy) {
    switch (strategy) {
        case BackpressureStrategy::Drop: return "Drop";
        case BackpressureStrategy::Buffer: return "Buffer";
        case BackpressureStrategy::Block: return "Block";
        case BackpressureStrategy::Fail: return "Fail";
        case BackpressureStrategy::Signal: return "Signal";
        default: return "Unknown";
    }
}

} // namespace ipc
} // namespace apollo

//==============================================================================
// 服务发现集成
//==============================================================================

#include "apollo/ipc/service_discovery.h"

// ChannelConfig 扩展：服务发现配置
// 在 ChannelConfig 中添加以下字段:
//
// std::string serviceName;                           // 服务名称（用于服务发现）
// std::shared_ptr<IServiceDiscovery> discovery;      // 服务发现客户端
// LoadBalanceStrategy loadBalance;                  // 负载均衡策略
// bool enableServiceDiscovery = false;              // 是否启用服务发现

// ChannelBuilder 扩展方法
/*
ChannelBuilder& serviceName(const std::string& name);
ChannelBuilder& discovery(std::shared_ptr<IServiceDiscovery> discovery);
ChannelBuilder& loadBalance(LoadBalanceStrategy strategy);
*/

// 使用示例:
/*
// 1. 使用服务发现连接
auto discovery = ServiceDiscoveryFactory::create(
    ServiceDiscoveryFactory::Type::SQLite,
    {.dbPath = "./services.db"}
);

auto channel = ChannelBuilder()
    .name("user-service-client")
    .serviceName("user-service")           // 通过服务名连接而非 IP:Port
    .discovery(discovery)
    .loadBalance(LoadBalanceStrategy::RoundRobin)
    .connect();

// 2. 服务端注册服务
auto serverDiscovery = ServiceDiscoveryFactory::create(
    ServiceDiscoveryFactory::Type::SQLite
);

serverDiscovery->start();

ServiceEndpoint endpoint;
endpoint.id = "user-service-1";
endpoint.host = "0.0.0.0";
endpoint.port = 8080;
endpoint.metadata = {{"version", "1.0.0"}, {"region", "us-east"}};

serverDiscovery->registerService("user-service", endpoint);

// 3. 使用 Redis 服务发现
auto redisDiscovery = ServiceDiscoveryFactory::create(
    ServiceDiscoveryFactory::Type::Redis,
    {.redisHost = "localhost", .redisPort = 6379}
);

auto channel = ChannelBuilder()
    .serviceName("order-service")
    .discovery(redisDiscovery)
    .loadBalance(LoadBalanceStrategy::Weighted)
    .connect();
*/

