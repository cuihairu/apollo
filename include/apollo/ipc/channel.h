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
// Ç°ÏòÉùÃ÷
//==============================================================================

class ITransport;
class Channel;
struct ChannelConfig;

//==============================================================================
// ±³Ñ¹²ßÂÔ
//==============================================================================

enum class BackpressureStrategy : uint8_t {
    Drop,       // ¶ªÆúĞÂÏûÏ¢
    Buffer,     // »º³åËùÓĞÏûÏ¢
    Block,      // ×èÈû·¢ËÍÕß
    Fail,       // ·µ»ØÊ§°Ü
    Signal      // ´¥·¢ĞÅºÅ
};

//==============================================================================
// ±³Ñ¹ÅäÖÃ - Ö§³ÖË«ÖØË®Î»Ïß£¨ÏûÏ¢ÊıÁ¿ + ×Ö½Ú´óĞ¡£©
//==============================================================================

struct BackpressureConfig {
    BackpressureStrategy strategy = BackpressureStrategy::Buffer;
    
    // ĞÂÔö£º»ùÓÚÏûÏ¢ÊıÁ¿µÄË®Î»Ïß£¨ÀàËÆ Go Channel£©
    size_t lowMessageCount = 32;       // µÍË®Î»£º32ÌõÏûÏ¢
    size_t highMessageCount = 128;     // ¸ßË®Î»£º128ÌõÏûÏ¢
    size_t maxMessageCount = 256;      // ×î´óÏûÏ¢Êı
    
    // ĞÂÔö£º»ùÓÚ×Ö½Ú´óĞ¡µÄË®Î»Ïß£¨·ÀÖ¹ OOM£©
    size_t lowByteCount = 32 * 1024;   // µÍË®Î»£º32KB
    size_t highByteCount = 128 * 1024; // ¸ßË®Î»£º128KB
    size_t maxByteCount = 256 * 1024;  // ×î´ó×Ö½ÚÊı
    
    // ×èÈûÅäÖÃ
    uint32_t blockTimeoutMs = 30000;
    
    // »Øµ÷
    std::function<void()> onHighWatermark;
    std::function<void()> onLowWatermark;
    
    // ¼æÈİ¾É°æ±¾£ºÒÑÆúÓÃµÄ×Ö¶Î
    [[deprecated("Use lowMessageCount and lowByteCount instead")]]
    size_t lowWatermark = 32 * 1024;
    
    [[deprecated("Use highMessageCount and highByteCount instead")]]
    size_t highWatermark = 128 * 1024;
    
    [[deprecated("Use maxMessageCount and maxByteCount instead")]]
    size_t bufferSize = 256 * 1024;
};
//==============================================================================
// ±³Ñ¹¹ÜÀíÆ÷ - Ë«ÖØË®Î»ÏßÊµÏÖ
//==============================================================================

class BackpressureManager {
public:
    explicit BackpressureManager(const BackpressureConfig& config);
    ~BackpressureManager() = default;
    
    // ÅäÖÃ
    const BackpressureConfig& getConfig() const { return config_; }
    void setConfig(const BackpressureConfig& config) { config_ = config; }
    
    // ĞÂÔö£ºÏûÏ¢ÊıÁ¿×·×Ù
    void addMessage(size_t messageSize);
    void removeMessage(size_t messageSize);
    size_t getPendingCount() const { return pendingCount_.load(std::memory_order_acquire); }
    size_t getPendingSize() const { return pendingSize_.load(std::memory_order_acquire); }
    
    // Ë®Î»ÏßÅĞ¶Ï - Ë«ÖØÌõ¼ş
    bool isHighWatermark() const;
    bool isLowWatermark() const;
    
    // ·¢ËÍ¼ì²é
    bool canSend(size_t pendingSize) const;
    
    // Ó¦ÓÃ±³Ñ¹²ßÂÔ
    SendResult applyBackpressure(size_t messageSize);
    
    // µÈ´ı¿ÉÓÃ¿Õ¼ä
    bool waitForAvailable(size_t requiredSize, uint32_t timeoutMs);
    
    // ÖØÖÃ
    void reset();
    
private:
    void updateWatermark(size_t currentSize);
    
    BackpressureConfig config_;
    
    // ĞÂÔö£ºÏûÏ¢ÊıÁ¿¼ÆÊıÆ÷
    std::atomic<size_t> pendingCount_{0};
    
    // Ô­ÓĞ£º×Ö½Ú´óĞ¡¼ÆÊıÆ÷
    std::atomic<size_t> pendingSize_{0};
    
    std::atomic<bool> highWatermarkTriggered_{false};
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};
//==============================================================================
// ÖØÁ¬²ßÂÔ
//==============================================================================

enum class ReconnectStrategy : uint8_t {
    FixedDelay,     // ¹Ì¶¨ÑÓ³Ù
    ExponentialBackoff,  // Ö¸ÊıÍË±Ü
    LinearBackoff   // ÏßĞÔÍË±Ü
};

//==============================================================================
// ÖØÁ¬ÅäÖÃ
//==============================================================================

struct ReconnectConfig {
    bool enable = false;
    ReconnectStrategy strategy = ReconnectStrategy::ExponentialBackoff;
    uint32_t initialDelayMs = 1000;
    uint32_t maxDelayMs = 30000;
    int maxRetries = -1;  // -1 ±íÊ¾ÎŞÏŞÖØÊÔ
    uint32_t retryIntervalMs = 2000;
    
    // ĞÄÌø¼ì²â
    bool enableHeartbeat = false;
    uint32_t heartbeatIntervalMs = 30000;
    uint32_t heartbeatTimeoutMs = 5000;
    
    // ½¡¿µ¼ì²é
    bool enableHealthCheck = false;
    uint32_t healthCheckIntervalMs = 60000;
    uint32_t healthCheckTimeoutMs = 5000;
    
    // »Øµ÷
    std::function<void()> onReconnecting;
    std::function<void()> onReconnected;
    std::function<void(int)> onReconnectFailed;
    std::function<void()> onGiveUp;
};
//==============================================================================
// Channel ÅäÖÃ
//==============================================================================

enum class ChannelMode : uint8_t {
    Client,
    Server
};

struct ChannelConfig {
    // »ù±¾ĞÅÏ¢
    std::string name;
    TransportType transport = TransportType::Tcp;
    ChannelMode mode = ChannelMode::Client;
    
    // ÍøÂçÅäÖÃ
    std::string host = "127.0.0.1";
    uint16_t port = 8080;
    std::string path;  // ÓÃÓÚ Unix Socket / ¹²ÏíÄÚ´æ
    
    // »º³åÇøÅäÖÃ
    size_t sendBufferSize = 64 * 1024;
    size_t recvBufferSize = 64 * 1024;
    size_t maxMessageSize = 16 * 1024 * 1024;
    size_t sharedMemorySize = 1024 * 1024;
    
    // ³¬Ê±ÅäÖÃ
    uint32_t connectTimeoutMs = 5000;
    uint32_t sendTimeoutMs = 5000;
    uint32_t recvTimeoutMs = 30000;
    
    // ±³Ñ¹ÅäÖÃ
    BackpressureConfig backpressure;
    
    // ÖØÁ¬ÅäÖÃ
    ReconnectConfig reconnect;
    
    // TCP Ñ¡Ïî
    bool enableNagle = true;
    bool enableKeepAlive = true;
    uint32_t keepAliveIdleSec = 60;
    uint32_t keepAliveIntervalSec = 5;
    uint32_t keepAliveCount = 3;
    
    // ÆäËû
    bool blocking = false;
};
//==============================================================================
// Channel ×´Ì¬
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
// ÏûÏ¢ÀàĞÍ
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
// ÏûÏ¢»ùÀà
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
// ÎÄ±¾ÏûÏ¢
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
// ¶ş½øÖÆÏûÏ¢
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
// ·¢ËÍ½á¹û
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
// »Øµ÷ÀàĞÍ
//==============================================================================

using MessageCallback = std::function<void(const Message&)>;
using StateCallback = std::function<void(ChannelState)>;
using ErrorCallback = std::function<void(const std::string&)>;
using CloseCallback = std::function<void()>;
using WatermarkCallback = std::function<void()>;
//==============================================================================
// Channel ½Ó¿Ú
//==============================================================================

class Channel : public std::enable_shared_from_this<Channel> {
public:
    virtual ~Channel() = default;
    
    // ¹¤³§·½·¨
    static std::unique_ptr<Channel> create(const std::string& name, const ChannelConfig& config);
    static std::unique_ptr<Channel> createClient(const std::string& host, uint16_t port);
    static std::unique_ptr<Channel> createServer(const std::string& host, uint16_t port);
    
    // Á¬½Ó¹ÜÀí
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual ChannelState getState() const = 0;
    
    virtual bool bind() = 0;
    virtual bool listen() = 0;
    virtual std::unique_ptr<Channel> accept() = 0;
    
    // ·¢ËÍÏûÏ¢
    virtual SendResult send(const Message& msg) = 0;
    virtual SendResult send(const BinaryMessage& msg) = 0;
    virtual SendResult send(const TextMessage& msg) = 0;
    virtual SendResult sendRaw(const void* data, size_t size) = 0;
    
    virtual void sendAsync(const Message& msg, std::function<void(SendResult)> callback) = 0;
    virtual SendResult trySend(const Message& msg) = 0;
    // ½ÓÊÕÏûÏ¢
    virtual Message receive() = 0;
    virtual bool receive(Message& msg) = 0;
    virtual bool tryReceive(Message& msg) = 0;
    virtual void receiveAsync(std::function<void(Message)> callback) = 0;
    virtual bool receive(Message& msg, uint32_t timeoutMs) = 0;
    
    // ¶©ÔÄÄ£Ê½
    virtual void subscribe(uint32_t msgType, MessageCallback callback) = 0;
    virtual void unsubscribe(uint32_t msgType) = 0;
    virtual void subscribeAll(MessageCallback callback) = 0;
    virtual void unsubscribeAll() = 0;
    
    virtual void startMessageLoop() = 0;
    virtual void stopMessageLoop() = 0;
    virtual bool isMessageLoopRunning() const = 0;
    
    // Request-Reply Ä£Ê½
    virtual std::future<Message> request(const Message& req, uint32_t timeoutMs) = 0;
    virtual void setRequestHandler(std::function<Message(const Message&)> handler) = 0;
    // ±³Ñ¹¿ØÖÆ
    virtual void setBackpressureStrategy(BackpressureStrategy strategy) = 0;
    virtual BackpressureStrategy getBackpressureStrategy() const = 0;
    virtual bool isHighWatermark() const = 0;
    virtual bool isLowWatermark() const = 0;
    virtual size_t getPendingMessages() const = 0;
    virtual size_t getPendingBytes() const = 0;
    virtual void onHighWatermark(WatermarkCallback callback) = 0;
    virtual void onLowWatermark(WatermarkCallback callback) = 0;
    
    // ×Ô¶¯ÖØÁ¬
    virtual void enableAutoReconnect(bool enable) = 0;
    virtual bool isAutoReconnectEnabled() const = 0;
    virtual bool reconnect() = 0;
    virtual int getReconnectCount() const = 0;
    virtual int getRemainingRetries() const = 0;
    virtual uint32_t getNextReconnectDelay() const = 0;
    virtual void setReconnectConfig(const ReconnectConfig& config) = 0;
    virtual ReconnectConfig getReconnectConfig() const = 0;
    // ÊôĞÔ
    virtual std::string getName() const = 0;
    virtual TransportType getTransportType() const = 0;
    virtual ChannelMode getMode() const = 0;
    virtual ChannelConfig getConfig() const = 0;
    
    // Í³¼Æ
    virtual size_t getSentBytes() const = 0;
    virtual size_t getReceivedBytes() const = 0;
    virtual size_t getSentMessages() const = 0;
    virtual size_t getReceivedMessages() const = 0;
    virtual size_t getDroppedMessages() const = 0;
    
    // ÊÂ¼ş»Øµ÷
    virtual void onStateChange(StateCallback callback) = 0;
    virtual void onError(ErrorCallback callback) = 0;
    virtual void onClose(CloseCallback callback) = 0;
    
    // »ñÈ¡´«Êä²ã
    virtual ITransport* getTransport() const = 0;
};
//==============================================================================
// ChannelBuilder - Á÷Ê½¹¹½¨Æ÷
//==============================================================================

class ChannelBuilder {
public:
    ChannelBuilder();
    ~ChannelBuilder() = default;
    
    // »ù±¾ĞÅÏ¢
    ChannelBuilder& name(const std::string& name);
    ChannelBuilder& transport(TransportType type);
    ChannelBuilder& mode(ChannelMode mode);
    
    // ÍøÂçÅäÖÃ
    ChannelBuilder& host(const std::string& host);
    ChannelBuilder& port(uint16_t port);
    ChannelBuilder& path(const std::string& path);
    
    // »º³åÇøÅäÖÃ
    ChannelBuilder& bufferSize(size_t size);
    ChannelBuilder& maxMessageSize(size_t size);
    ChannelBuilder& sharedMemorySize(size_t size);
    
    // ±³Ñ¹ÅäÖÃ
    ChannelBuilder& backpressure(BackpressureStrategy strategy);
    ChannelBuilder& watermarks(size_t low, size_t high);
    ChannelBuilder& bufferSize(size_t low, size_t high, size_t max);
    ChannelBuilder& blockTimeout(uint32_t ms);
    // ³¬Ê±ÅäÖÃ
    ChannelBuilder& connectTimeout(uint32_t ms);
    ChannelBuilder& sendTimeout(uint32_t ms);
    ChannelBuilder& recvTimeout(uint32_t ms);
    ChannelBuilder& blocking(bool enabled);
    ChannelBuilder& nagle(bool enabled);
    
    // »Øµ÷
    ChannelBuilder& onHighWatermark(std::function<void()> callback);
    ChannelBuilder& onLowWatermark(std::function<void()> callback);
    
    // ÖØÁ¬ÅäÖÃ
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
    // ¹¹½¨
    std::unique_ptr<Channel> build();
    std::unique_ptr<Channel> connect();
    
private:
    std::string name_;
    ChannelConfig config_;
};
//==============================================================================
// ±ã½İ¹¤³§º¯Êı
//==============================================================================

inline std::unique_ptr<Channel> createTcpClient(const std::string& host, uint16_t port) {
    return Channel::createClient(host, port);
}

inline std::unique_ptr<Channel> createTcpServer(const std::string& host, uint16_t port) {
    return Channel::createServer(host, port);
}
//==============================================================================
// ¸¨Öúº¯Êı - ·¢ËÍ½á¹û×ª×Ö·û´®
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
// ¸¨Öúº¯Êı - ±³Ñ¹²ßÂÔ×ª×Ö·û´®
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
// æœåŠ¡å‘ç°é›†æˆ
//==============================================================================

#include "apollo/ipc/service_discovery.h"

// ChannelConfig æ‰©å±•ï¼šæœåŠ¡å‘ç°é…ç½®
// åœ¨ ChannelConfig ä¸­æ·»åŠ ä»¥ä¸‹å­—æ®µ:
//
// std::string serviceName;                           // æœåŠ¡åç§°ï¼ˆç”¨äºæœåŠ¡å‘ç°ï¼‰
// std::shared_ptr<IServiceDiscovery> discovery;      // æœåŠ¡å‘ç°å®¢æˆ·ç«¯
// LoadBalanceStrategy loadBalance;                  // è´Ÿè½½å‡è¡¡ç­–ç•¥
// bool enableServiceDiscovery = false;              // æ˜¯å¦å¯ç”¨æœåŠ¡å‘ç°

// ChannelBuilder æ‰©å±•æ–¹æ³•
/*
ChannelBuilder& serviceName(const std::string& name);
ChannelBuilder& discovery(std::shared_ptr<IServiceDiscovery> discovery);
ChannelBuilder& loadBalance(LoadBalanceStrategy strategy);
*/

// ä½¿ç”¨ç¤ºä¾‹:
/*
// 1. ä½¿ç”¨æœåŠ¡å‘ç°è¿æ¥
auto discovery = ServiceDiscoveryFactory::create(
    ServiceDiscoveryFactory::Type::SQLite,
    {.dbPath = "./services.db"}
);

auto channel = ChannelBuilder()
    .name("user-service-client")
    .serviceName("user-service")           // é€šè¿‡æœåŠ¡åè¿æ¥è€Œé IP:Port
    .discovery(discovery)
    .loadBalance(LoadBalanceStrategy::RoundRobin)
    .connect();

// 2. æœåŠ¡ç«¯æ³¨å†ŒæœåŠ¡
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

// 3. ä½¿ç”¨ Redis æœåŠ¡å‘ç°
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

