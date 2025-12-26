#pragma once

#include <string>
#include <memory>
#include <queue>
#include <vector>
#include <chrono>
#include <functional>
#include <atomic>
#include <variant>

namespace apollo {
namespace ipc {

//==============================================================================
// QoS 策略
//==============================================================================

// 消息优先级
enum class MessagePriority : uint8_t {
    Critical = 0,      // 关键消息 (系统级)
    High = 1,          // 高优先级 (游戏事件)
    Normal = 2,        // 普通优先级 (聊天等)
    Low = 3,           // 低优先级 (日志等)
    Background = 4     // 后台任务
};

// 可靠性级别
enum class Reliability : uint8_t {
    AtMostOnce,        // 至多一次 (允许丢失，最快)
    AtLeastOnce,       // 至少一次 (可能重复)
    ExactlyOnce        // 精确一次 (最慢，需确认)
};

// 传递保证
enum class DeliveryGuarantee : uint8_t {
    Unordered,         // 无序 (最快)
    FIFO,              // 先进先出
    Priority           // 按优先级
};

// QoS 策略组合
struct QosPolicy {
    MessagePriority priority = MessagePriority::Normal;
    Reliability reliability = Reliability::AtLeastOnce;
    DeliveryGuarantee delivery = DeliveryGuarantee::FIFO;

    uint32_t ttlMs = 0;              // 消息生存时间 (0 = 无限)
    bool persist = false;             // 持久化
    bool compress = false;            // 压缩
    uint32_t maxRetries = 3;          // 最大重试次数
    uint32_t retryDelayMs = 100;      // 重试延迟
};

//==============================================================================
// 带优先级的消息
//==============================================================================

struct PrioritizedMessage {
    QosPolicy qos;
    uint64_t sequence;               // 序列号
    uint64_t timestamp;              // 发送时间戳
    uint32_t retryCount;             // 重试次数
    std::vector<uint8_t> data;       // 消息数据

    // 优先级比较 (用于优先队列)
    bool operator>(const PrioritizedMessage& other) const {
        if (static_cast<uint8_t>(qos.priority) !=
            static_cast<uint8_t>(other.qos.priority)) {
            return static_cast<uint8_t>(qos.priority) <
                   static_cast<uint8_t>(other.qos.priority);
        }
        return sequence > other.sequence;  // 相同优先级按序列号
    }
};

//==============================================================================
// QoS 管理器
//==============================================================================

class QosManager {
public:
    struct Stats {
        uint64_t sentMessages = 0;
        uint64_t sentBytes = 0;
        uint64_t droppedMessages = 0;
        uint64_t retriedMessages = 0;
        uint64_t timeoutMessages = 0;
        uint64_t pendingMessages = 0;
    };

    QosManager();
    explicit QosManager(const QosPolicy& defaultPolicy);

    //======================================================================
    // 发送路径
    //======================================================================

    // 发送消息 (应用 QoS)
    bool send(std::vector<uint8_t> data, const QosPolicy& qos);

    // 重试消息
    bool retry(uint64_t sequence);

    // 取消消息
    bool cancel(uint64_t sequence);

    //======================================================================
    // 接收路径
    //======================================================================

    // 确认消息 (用于可靠性)
    void acknowledge(uint64_t sequence);

    // 拒绝消息
    void reject(uint64_t sequence);

    // 获取下一个待发送消息
    bool getNext(PrioritizedMessage& msg);

    // 检查是否有待发送消息
    bool hasPending() const;

    //======================================================================
    // 配置
    //======================================================================

    void setDefaultPolicy(const QosPolicy& policy);
    QosPolicy getDefaultPolicy() const;

    void setMaxPending(size_t max);
    size_t getMaxPending() const;

    //======================================================================
    // 统计
    //======================================================================

    Stats getStats() const;
    void resetStats();

    //======================================================================
    // 维护
    //======================================================================

    // 清理过期消息
    size_t cleanupExpired();

    // 获取待发送队列大小 (按优先级)
    size_t getQueueSize(MessagePriority priority) const;

private:
    // 优先队列 (每个优先级一个)
    struct PriorityQueue {
        std::priority_queue<PrioritizedMessage,
                           std::vector<PrioritizedMessage>,
                           std::greater<PrioritizedMessage>> queue;
        size_t maxSize = 1024;
    };

    PriorityQueue queues_[5];  // 按 5 个优先级
    QosPolicy defaultPolicy_;
    size_t maxPending_ = 10000;
    std::atomic<uint64_t> nextSequence_{1};

    // 待确认消息 (用于 AtLeastOnce / ExactlyOnce)
    std::unordered_map<uint64_t, PrioritizedMessage> pendingAck_;
    std::mutex mutex_;

    Stats stats_;
};

//==============================================================================
// 流量控制 (Token Bucket 算法)
//==============================================================================

class TokenBucket {
public:
    TokenBucket(size_t rate, size_t burst);

    // 尝试消费 tokens
    bool tryConsume(size_t tokens);

    // 消费 tokens (阻塞直到可用)
    void consume(size_t tokens, uint32_t timeoutMs = 0);

    // 获取当前可用 tokens
    size_t available() const;

    // 设置速率
    void setRate(size_t rate);

    // 重置
    void reset();

private:
    void refill();

    size_t rate_;           // tokens per second
    size_t burst_;          // max tokens
    std::atomic<size_t> tokens_;
    std::chrono::steady_clock::time_point lastRefill_;
};

//==============================================================================
// 消息限流器
//==============================================================================

class RateLimiter {
public:
    // 限流策略
    enum class Algorithm {
        TokenBucket,       // 令牌桶 (平滑流量)
        LeakyBucket,       // 漏桶 (恒定速率)
        SlidingWindow,     // 滑动窗口
        FixedWindow        // 固定窗口
    };

    static std::unique_ptr<RateLimiter> create(
        Algorithm algo,
        size_t rate,          // 每秒请求数
        size_t burst = 0      // 突发容量
    );

    virtual ~RateLimiter() = default;

    // 尝试获取许可
    virtual bool tryAcquire(size_t permits = 1) = 0;

    // 获取许可 (阻塞)
    virtual bool acquire(size_t permits = 1, uint32_t timeoutMs = 0) = 0;

    // 获取当前可用许可
    virtual size_t available() const = 0;

    // 重置
    virtual void reset() = 0;
};

//==============================================================================
// 优先级限流器 (确保高优先级消息不被限流)
//==============================================================================

class PriorityRateLimiter {
public:
    PriorityRateLimiter();

    // 为每个优先级设置速率
    void setRate(MessagePriority priority, size_t rate);
    void setBurst(MessagePriority priority, size_t burst);

    // 尝试发送
    bool tryAcquire(MessagePriority priority, size_t permits = 1);

    // 获取各优先级可用数
    size_t available(MessagePriority priority) const;

private:
    std::array<std::unique_ptr<RateLimiter>, 5> limiters_;
};

//==============================================================================
// 完整的 Channel 配置 (包含 QoS)
//==============================================================================

struct ChannelConfigEx {
    // 基础配置
    TransportType transport = TransportType::Auto;
    ChannelMode mode = ChannelMode::Bidirectional;
    std::string name;
    std::string host = "localhost";
    uint16_t port = 0;
    std::string path;

    // 缓冲区
    size_t sendBufferSize = 64 * 1024;
    size_t recvBufferSize = 64 * 1024;
    size_t maxMessageSize = 4 * 1024 * 1024;

    // 背压
    BackpressureStrategy backpressure = BackpressureStrategy::Buffer;
    size_t lowWatermark = 32 * 1024;
    size_t highWatermark = 128 * 1024;

    // QoS
    QosPolicy defaultQos;
    bool enableQos = true;
    bool enablePriority = true;

    // 流量控制
    bool enableRateLimit = false;
    size_t rateLimit = 0;        // 0 = 不限制

    // 超时
    uint32_t connectTimeoutMs = 5000;
    uint32_t sendTimeoutMs = 3000;
    uint32_t recvTimeoutMs = 0;

    // 性能
    bool enableNagle = true;
    bool enableKeepAlive = true;
};

//==============================================================================
// ChannelBuilderEx (支持 QoS)
//==============================================================================

class ChannelBuilderEx {
public:
    ChannelBuilderEx();

    // 基础配置
    ChannelBuilderEx& name(const std::string& name);
    ChannelBuilderEx& transport(TransportType type);
    ChannelBuilderEx& mode(ChannelMode mode);
    ChannelBuilderEx& address(const std::string& host, uint16_t port);
    ChannelBuilderEx& path(const std::string& path);

    // QoS 配置
    ChannelBuilderEx& qos(const QosPolicy& policy);
    ChannelBuilderEx& priority(MessagePriority priority);
    ChannelBuilderEx& reliability(Reliability reliability);
    ChannelBuilderEx& delivery(DeliveryGuarantee delivery);
    ChannelBuilderEx& ttl(uint32_t ttlMs);
    ChannelBuilderEx& persist(bool enable);
    ChannelBuilderEx& maxRetries(uint32_t count);
    ChannelBuilderEx& retryDelay(uint32_t delayMs);

    // 启用/禁用 QoS
    ChannelBuilderEx& enableQos(bool enable);
    ChannelBuilderEx& enablePriority(bool enable);

    // 流量控制
    ChannelBuilderEx& rateLimit(size_t ratePerSecond);

    // 背压
    ChannelBuilderEx& backpressure(BackpressureStrategy strategy);
    ChannelBuilderEx& watermarks(size_t low, size_t high);

    // 构建
    std::unique_ptr<Channel> build();
    std::unique_ptr<Channel> connect();

private:
    ChannelConfigEx config_;
};

//==============================================================================
// 便捷函数 - 创建带 QoS 的 Channel
//==============================================================================

// 创建高可靠 Channel
inline std::unique_ptr<Channel> createReliableChannel(
    const std::string& name,
    Reliability reliability = Reliability::AtLeastOnce) {

    ChannelBuilderEx builder;
    builder.name(name)
           .reliability(reliability)
           .delivery(DeliveryGuarantee::FIFO)
           .maxRetries(5)
           .enableQos(true);
    return builder.build();
}

// 创建低延迟 Channel (牺牲可靠性)
inline std::unique_ptr<Channel> createLowLatencyChannel(
    const std::string& name) {

    ChannelBuilderEx builder;
    builder.name(name)
           .transport(TransportType::SharedMemory)
           .reliability(Reliability::AtMostOnce)
           .delivery(DeliveryGuarantee::Unordered)
           .enableQos(false)
           .backpressure(BackpressureStrategy::Drop);
    return builder.build();
}

// 创建游戏服务器 Channel (平衡型)
inline std::unique_ptr<Channel> createGameServerChannel(
    const std::string& name,
    TransportType transport = TransportType::SharedMemory) {

    ChannelBuilderEx builder;
    builder.name(name)
           .transport(transport)
           .mode(ChannelMode::Bidirectional)
           .priority(MessagePriority::High)
           .reliability(Reliability::AtLeastOnce)
           .delivery(DeliveryGuarantee::FIFO)
           .rateLimit(100000)        // 10万 msg/s
           .watermarks(64 * 1024, 512 * 1024)
           .maxRetries(3);
    return builder.build();
}

} // namespace ipc
} // namespace apollo
