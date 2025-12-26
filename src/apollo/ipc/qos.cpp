/**
 * @file qos.cpp
 * @brief QoS 管理器和流量控制实现
 */

#include "apollo/ipc/qos.h"
#include <algorithm>
#include <unordered_set>

namespace apollo {
namespace ipc {

//==============================================================================
// QosManager 实现
//==============================================================================

QosManager::QosManager() = default;

QosManager::QosManager(const QosPolicy& defaultPolicy)
    : defaultPolicy_(defaultPolicy) {
}

bool QosManager::send(std::vector<uint8_t> data, const QosPolicy& qos) {
    std::lock_guard<std::mutex> lock(mutex_);

    uint64_t seq = nextSequence_++;

    PrioritizedMessage msg{
        .qos = qos,
        .sequence = seq,
        .timestamp = std::chrono::steady_clock::now().time_since_epoch().count(),
        .retryCount = 0,
        .data = std::move(data)
    };

    // 检查队列大小
    size_t idx = static_cast<size_t>(qos.priority);
    if (idx >= 5) idx = 2;  // Normal

    if (queues_[idx].queue.size() >= queues_[idx].maxSize) {
        // 根据可靠性策略决定是否丢弃
        if (qos.reliability == Reliability::AtMostOnce) {
            stats_.droppedMessages++;
            return false;
        }

        // 尝试丢弃低优先级消息
        for (size_t i = 4; i > idx; --i) {
            if (!queues_[i].queue.empty()) {
                queues_[i].queue.pop();
                stats_.droppedMessages++;
                break;
            }
        }
    }

    queues_[idx].queue.push(std::move(msg));
    stats_.sentMessages++;
    stats_.sentBytes += msg.data.size();
    stats_.pendingMessages++;

    // 对于需要确认的消息，加入待确认队列
    if (qos.reliability == Reliability::AtLeastOnce ||
        qos.reliability == Reliability::ExactlyOnce) {
        pendingAck_[seq] = queues_[idx].queue.top();
    }

    return true;
}

bool QosManager::retry(uint64_t sequence) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = pendingAck_.find(sequence);
    if (it == pendingAck_.end()) {
        return false;
    }

    auto& msg = it->second;
    if (msg.retryCount >= msg.qos.maxRetries) {
        pendingAck_.erase(it);
        stats_.droppedMessages++;
        return false;
    }

    msg.retryCount++;
    msg.timestamp = std::chrono::steady_clock::now().time_since_epoch().count();

    // 重新加入发送队列
    size_t idx = static_cast<size_t>(msg.qos.priority);
    if (idx >= 5) idx = 2;
    queues_[idx].queue.push(std::move(msg));

    pendingAck_.erase(it);
    stats_.retriedMessages++;

    return true;
}

bool QosManager::cancel(uint64_t sequence) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = pendingAck_.find(sequence);
    if (it != pendingAck_.end()) {
        pendingAck_.erase(it);
        return true;
    }

    return false;
}

void QosManager::acknowledge(uint64_t sequence) {
    std::lock_guard<std::mutex> lock(mutex_);
    pendingAck_.erase(sequence);
}

void QosManager::reject(uint64_t sequence) {
    retry(sequence);
}

bool QosManager::getNext(PrioritizedMessage& msg) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 按优先级顺序查找
    for (size_t i = 0; i < 5; ++i) {
        if (!queues_[i].queue.empty()) {
            msg = queues_[i].queue.top();
            queues_[i].queue.pop();
            stats_.pendingMessages--;
            return true;
        }
    }

    return false;
}

bool QosManager::hasPending() const {
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& q : queues_) {
        if (!q.queue.empty()) {
            return true;
        }
    }

    return false;
}

void QosManager::setDefaultPolicy(const QosPolicy& policy) {
    std::lock_guard<std::mutex> lock(mutex_);
    defaultPolicy_ = policy;
}

QosPolicy QosManager::getDefaultPolicy() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return defaultPolicy_;
}

void QosManager::setMaxPending(size_t max) {
    std::lock_guard<std::mutex> lock(mutex_);
    maxPending_ = max;
}

size_t QosManager::getMaxPending() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return maxPending_;
}

QosManager::Stats QosManager::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void QosManager::resetStats() {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_ = Stats{};
}

size_t QosManager::cleanupExpired() {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::steady_clock::now();
    size_t cleaned = 0;

    // 清理待确认队列中过期的消息
    for (auto it = pendingAck_.begin(); it != pendingAck_.end();) {
        const auto& msg = it->second;

        if (msg.qos.ttlMs > 0) {
            auto sendTime = std::chrono::steady_clock::time_point(
                std::chrono::milliseconds(msg.timestamp));
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - sendTime).count();

            if (elapsed > static_cast<int64_t>(msg.qos.ttlMs)) {
                it = pendingAck_.erase(it);
                stats_.timeoutMessages++;
                cleaned++;
                continue;
            }
        }

        ++it;
    }

    return cleaned;
}

size_t QosManager::getQueueSize(MessagePriority priority) const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t idx = static_cast<size_t>(priority);
    if (idx >= 5) idx = 2;
    return queues_[idx].queue.size();
}

//==============================================================================
// TokenBucket 实现
//==============================================================================

TokenBucket::TokenBucket(size_t rate, size_t burst)
    : rate_(rate)
    , burst_(burst)
    , tokens_(burst)
    , lastRefill_(std::chrono::steady_clock::now()) {
}

bool TokenBucket::tryConsume(size_t tokens) {
    refill();

    size_t current = tokens_.load(std::memory_order_relaxed);
    if (current < tokens) {
        return false;
    }

    size_t newval = current - tokens;
    while (!tokens_.compare_exchange_weak(current, newval,
        std::memory_order_release, std::memory_order_relaxed)) {
        if (current < tokens) {
            return false;
        }
    }

    return true;
}

void TokenBucket::consume(size_t tokens, uint32_t timeoutMs) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    while (!tryConsume(tokens)) {
        if (timeoutMs > 0) {
            auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                return;  // 超时
            }

            // 计算等待时间
            size_t current = tokens_.load(std::memory_order_relaxed);
            if (current < tokens) {
                size_t needed = tokens - current;
                auto waitMs = static_cast<int>((needed * 1000 + rate_ - 1) / rate_);

                auto waitUntil = std::min(
                    now + std::chrono::milliseconds(waitMs),
                    deadline
                );

                std::this_thread::sleep_until(waitUntil);
            }
        } else {
            refill();
            std::this_thread::yield();
        }
    }
}

size_t TokenBucket::available() const {
    return tokens_.load(std::memory_order_relaxed);
}

void TokenBucket::setRate(size_t rate) {
    std::lock_guard<std::mutex> lock(mutex_);
    rate_ = rate;
}

void TokenBucket::reset() {
    tokens_.store(burst_, std::memory_order_release);
    lastRefill_ = std::chrono::steady_clock::now();
}

void TokenBucket::refill() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - lastRefill_).count();

    if (elapsed <= 0) {
        return;
    }

    // 计算应该补充的 tokens
    size_t tokensToAdd = static_cast<size_t>(elapsed * rate_ / 1000);

    if (tokensToAdd > 0) {
        size_t oldval = tokens_.load(std::memory_order_relaxed);
        size_t newval = std::min(oldval + tokensToAdd, burst_);

        while (!tokens_.compare_exchange_weak(oldval, newval,
            std::memory_order_release, std::memory_order_relaxed)) {
            newval = std::min(oldval + tokensToAdd, burst_);
        }

        lastRefill_ = now;
    }
}

//==============================================================================
// RateLimiter 实现
//==============================================================================

class TokenBucketRateLimiter : public RateLimiter {
public:
    TokenBucketRateLimiter(size_t rate, size_t burst)
        : bucket_(rate, burst) {
    }

    bool tryAcquire(size_t permits = 1) override {
        return bucket_.tryConsume(permits);
    }

    bool acquire(size_t permits = 1, uint32_t timeoutMs = 0) override {
        bucket_.consume(permits, timeoutMs);
        return bucket_.available() >= permits;
    }

    size_t available() const override {
        return bucket_.available();
    }

    void reset() override {
        bucket_.reset();
    }

private:
    TokenBucket bucket_;
};

std::unique_ptr<RateLimiter> RateLimiter::create(
    Algorithm algo,
    size_t rate,
    size_t burst) {

    if (burst == 0) {
        burst = rate;
    }

    switch (algo) {
        case Algorithm::TokenBucket:
            return std::make_unique<TokenBucketRateLimiter>(rate, burst);

        // TODO: 实现其他算法
        case Algorithm::LeakyBucket:
        case Algorithm::SlidingWindow:
        case Algorithm::FixedWindow:
        default:
            return std::make_unique<TokenBucketRateLimiter>(rate, burst);
    }
}

//==============================================================================
// PriorityRateLimiter 实现
//==============================================================================

PriorityRateLimiter::PriorityRateLimiter() {
    // 为每个优先级创建不同的限流器
    // 高优先级限流更宽松
    limiters_[0] = RateLimiter::create(RateLimiter::Algorithm::TokenBucket, 10000, 10000);  // Critical
    limiters_[1] = RateLimiter::create(RateLimiter::Algorithm::TokenBucket, 5000, 5000);    // High
    limiters_[2] = RateLimiter::create(RateLimiter::Algorithm::TokenBucket, 1000, 2000);    // Normal
    limiters_[3] = RateLimiter::create(RateLimiter::Algorithm::TokenBucket, 500, 1000);    // Low
    limiters_[4] = RateLimiter::create(RateLimiter::Algorithm::TokenBucket, 100, 500);     // Background
}

void PriorityRateLimiter::setRate(MessagePriority priority, size_t rate) {
    size_t idx = static_cast<size_t>(priority);
    if (idx >= 5) idx = 2;
    limiters_[idx] = RateLimiter::create(RateLimiter::Algorithm::TokenBucket, rate, rate);
}

void PriorityRateLimiter::setBurst(MessagePriority priority, size_t burst) {
    size_t idx = static_cast<size_t>(priority);
    if (idx >= 5) idx = 2;
    // TODO: 支持独立设置 burst
}

bool PriorityRateLimiter::tryAcquire(MessagePriority priority, size_t permits) {
    size_t idx = static_cast<size_t>(priority);
    if (idx >= 5) idx = 2;
    return limiters_[idx]->tryAcquire(permits);
}

size_t PriorityRateLimiter::available(MessagePriority priority) const {
    size_t idx = static_cast<size_t>(priority);
    if (idx >= 5) idx = 2;
    return limiters_[idx]->available();
}

//==============================================================================
// ChannelBuilderEx 实现
//==============================================================================

ChannelBuilderEx::ChannelBuilderEx() = default;

ChannelBuilderEx& ChannelBuilderEx::name(const std::string& name) {
    config_.name = name;
    return *this;
}

ChannelBuilderEx& ChannelBuilderEx::transport(TransportType type) {
    config_.transport = type;
    return *this;
}

ChannelBuilderEx& ChannelBuilderEx::mode(ChannelMode mode) {
    config_.mode = mode;
    return *this;
}

ChannelBuilderEx& ChannelBuilderEx::address(const std::string& host, uint16_t port) {
    config_.host = host;
    config_.port = port;
    return *this;
}

ChannelBuilderEx& ChannelBuilderEx::path(const std::string& path) {
    config_.path = path;
    return *this;
}

ChannelBuilderEx& ChannelBuilderEx::qos(const QosPolicy& policy) {
    config_.defaultQos = policy;
    return *this;
}

ChannelBuilderEx& ChannelBuilderEx::priority(MessagePriority priority) {
    config_.defaultQos.priority = priority;
    return *this;
}

ChannelBuilderEx& ChannelBuilderEx::reliability(Reliability reliability) {
    config_.defaultQos.reliability = reliability;
    return *this;
}

ChannelBuilderEx& ChannelBuilderEx::delivery(DeliveryGuarantee delivery) {
    config_.defaultQos.delivery = delivery;
    return *this;
}

ChannelBuilderEx& ChannelBuilderEx::ttl(uint32_t ttlMs) {
    config_.defaultQos.ttlMs = ttlMs;
    return *this;
}

ChannelBuilderEx& ChannelBuilderEx::persist(bool enable) {
    config_.defaultQos.persist = enable;
    return *this;
}

ChannelBuilderEx& ChannelBuilderEx::maxRetries(uint32_t count) {
    config_.defaultQos.maxRetries = count;
    return *this;
}

ChannelBuilderEx& ChannelBuilderEx::retryDelay(uint32_t delayMs) {
    config_.defaultQos.retryDelayMs = delayMs;
    return *this;
}

ChannelBuilderEx& ChannelBuilderEx::enableQos(bool enable) {
    config_.enableQos = enable;
    return *this;
}

ChannelBuilderEx& ChannelBuilderEx::enablePriority(bool enable) {
    config_.enablePriority = enable;
    return *this;
}

ChannelBuilderEx& ChannelBuilderEx::rateLimit(size_t ratePerSecond) {
    config_.enableRateLimit = (ratePerSecond > 0);
    config_.rateLimit = ratePerSecond;
    return *this;
}

ChannelBuilderEx& ChannelBuilderEx::backpressure(BackpressureStrategy strategy) {
    config_.backpressure = strategy;
    return *this;
}

ChannelBuilderEx& ChannelBuilderEx::watermarks(size_t low, size_t high) {
    config_.lowWatermark = low;
    config_.highWatermark = high;
    return *this;
}

std::unique_ptr<Channel> ChannelBuilderEx::build() {
    // TODO: 实际创建 Channel
    return nullptr;
}

std::unique_ptr<Channel> ChannelBuilderEx::connect() {
    auto channel = build();
    if (channel) {
        channel->connect();
    }
    return channel;
}

} // namespace ipc
} // namespace apollo
