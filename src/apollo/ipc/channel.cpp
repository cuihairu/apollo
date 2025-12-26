/**
 * @file channel.cpp
 */

#include "apollo/ipc/channel.h"
#include <thread>
#include <chrono>

namespace apollo {
namespace ipc {

BackpressureManager::BackpressureManager(const BackpressureConfig& config)
    : config_(config) {}

bool BackpressureManager::canSend(size_t pendingSize) const {
    size_t sz = pendingSize_.load(std::memory_order_relaxed);
    size_t cnt = pendingCount_.load(std::memory_order_relaxed);
    switch (config_.strategy) {
        case BackpressureStrategy::Drop:
            return (sz + pendingSize) <= config_.maxByteCount && (cnt + 1) <= config_.maxMessageCount;
        case BackpressureStrategy::Buffer:
        case BackpressureStrategy::Block:
            return true;
        case BackpressureStrategy::Fail:
        case BackpressureStrategy::Signal:
            return (sz + pendingSize) <= config_.highByteCount && (cnt + 1) <= config_.highMessageCount;
        default:
            return true;
    }
}

SendResult BackpressureManager::applyBackpressure(size_t messageSize) {
    size_t sz = pendingSize_.load(std::memory_order_relaxed);
    size_t cnt = pendingCount_.load(std::memory_order_relaxed);
    if (sz + messageSize > config_.maxByteCount || cnt + 1 > config_.maxMessageCount) {
        switch (config_.strategy) {
            case BackpressureStrategy::Drop: return SendResult::Dropped;
            case BackpressureStrategy::Block:
                if (!waitForAvailable(messageSize, config_.blockTimeoutMs)) return SendResult::TimedOut;
                break;
            case BackpressureStrategy::Fail: return SendResult::BufferFull;
            case BackpressureStrategy::Signal:
                if (!highWatermarkTriggered_.exchange(true, std::memory_order_acq_rel)) {
                    if (config_.onHighWatermark) config_.onHighWatermark();
                }
                return SendResult::BufferFull;
            default: break;
        }
    }
    addMessage(messageSize);
    updateWatermark(pendingSize_.load(std::memory_order_relaxed));
    return SendResult::Success;
}

void BackpressureManager::updateWatermark(size_t currentSize) {
    bool wasHigh = highWatermarkTriggered_.load(std::memory_order_acquire);
    size_t cnt = pendingCount_.load(std::memory_order_acquire);
    bool isHigh = cnt >= config_.highMessageCount || currentSize >= config_.highByteCount;
    bool isLow = cnt <= config_.lowMessageCount && currentSize <= config_.lowByteCount;
    if (isHigh && !wasHigh) {
        highWatermarkTriggered_.store(true, std::memory_order_release);
        if (config_.onHighWatermark) config_.onHighWatermark();
    } else if (isLow && wasHigh) {
        highWatermarkTriggered_.store(false, std::memory_order_release);
        if (config_.onLowWatermark) config_.onLowWatermark();
    }
}

bool BackpressureManager::waitForAvailable(size_t requiredSize, uint32_t timeoutMs) {
    std::unique_lock<std::mutex> lock(mutex_);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (isHighWatermark()) {
        if (cv_.wait_until(lock, deadline) == std::cv_status::timeout) return false;
    }
    return true;
}

void BackpressureManager::addMessage(size_t messageSize) {
    pendingCount_.fetch_add(1, std::memory_order_relaxed);
    pendingSize_.fetch_add(messageSize, std::memory_order_relaxed);
}

void BackpressureManager::removeMessage(size_t messageSize) {
    pendingCount_.fetch_sub(1, std::memory_order_relaxed);
    size_t oldSize = pendingSize_.fetch_sub(messageSize, std::memory_order_relaxed);
    if (oldSize >= config_.highByteCount && oldSize - messageSize <= config_.lowByteCount) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.notify_all();
    }
}

bool BackpressureManager::isHighWatermark() const {
    size_t cnt = pendingCount_.load(std::memory_order_acquire);
    size_t sz = pendingSize_.load(std::memory_order_acquire);
    return cnt >= config_.highMessageCount || sz >= config_.highByteCount;
}

bool BackpressureManager::isLowWatermark() const {
    size_t cnt = pendingCount_.load(std::memory_order_acquire);
    size_t sz = pendingSize_.load(std::memory_order_acquire);
    return cnt <= config_.lowMessageCount && sz <= config_.lowByteCount;
}

void BackpressureManager::reset() {
    pendingCount_.store(0, std::memory_order_release);
    pendingSize_.store(0, std::memory_order_release);
    highWatermarkTriggered_.store(false, std::memory_order_release);
}

} // namespace ipc
} // namespace apollo

//==============================================================================
// ChannelBuilder 实现
//==============================================================================

ChannelBuilder::ChannelBuilder() = default;

ChannelBuilder& ChannelBuilder::name(const std::string& name) {
    name_ = name;
    return *this;
}

ChannelBuilder& ChannelBuilder::transport(TransportType type) {
    config_.transport = type;
    return *this;
}

ChannelBuilder& ChannelBuilder::mode(ChannelMode mode) {
    config_.mode = mode;
    return *this;
}

ChannelBuilder& ChannelBuilder::host(const std::string& host) {
    config_.host = host;
    return *this;
}

ChannelBuilder& ChannelBuilder::port(uint16_t port) {
    config_.port = port;
    return *this;
}

ChannelBuilder& ChannelBuilder::path(const std::string& path) {
    config_.path = path;
    return *this;
}

ChannelBuilder& ChannelBuilder::bufferSize(size_t size) {
    config_.sendBufferSize = size;
    config_.recvBufferSize = size;
    config_.backpressure.maxByteCount = size * 2;
    config_.backpressure.maxMessageCount = size * 2;
    return *this;
}

ChannelBuilder& ChannelBuilder::maxMessageSize(size_t size) {
    config_.maxMessageSize = size;
    return *this;
}

ChannelBuilder& ChannelBuilder::sharedMemorySize(size_t size) {
    config_.sharedMemorySize = size;
    return *this;
}

ChannelBuilder& ChannelBuilder::backpressure(BackpressureStrategy strategy) {
    config_.backpressure.strategy = strategy;
    return *this;
}

ChannelBuilder& ChannelBuilder::watermarks(size_t low, size_t high) {
    config_.backpressure.lowByteCount = low;
    config_.backpressure.highByteCount = high;
    return *this;
}

ChannelBuilder& ChannelBuilder::bufferSize(size_t low, size_t high, size_t max) {
    config_.backpressure.lowByteCount = low;
    config_.backpressure.highByteCount = high;
    config_.backpressure.maxByteCount = max;
    return *this;
}

ChannelBuilder& ChannelBuilder::connectTimeout(uint32_t ms) {
    config_.connectTimeoutMs = ms;
    return *this;
}

ChannelBuilder& ChannelBuilder::sendTimeout(uint32_t ms) {
    config_.sendTimeoutMs = ms;
    return *this;
}

ChannelBuilder& ChannelBuilder::recvTimeout(uint32_t ms) {
    config_.recvTimeoutMs = ms;
    return *this;
}

ChannelBuilder& ChannelBuilder::blockTimeout(uint32_t ms) {
    config_.backpressure.blockTimeoutMs = ms;
    return *this;
}

ChannelBuilder& ChannelBuilder::blocking(bool enabled) {
    config_.blocking = enabled;
    return *this;
}

ChannelBuilder& ChannelBuilder::nagle(bool enabled) {
    config_.enableNagle = enabled;
    return *this;
}

ChannelBuilder& ChannelBuilder::onHighWatermark(std::function<void()> callback) {
    config_.backpressure.onHighWatermark = std::move(callback);
    return *this;
}

ChannelBuilder& ChannelBuilder::onLowWatermark(std::function<void()> callback) {
    config_.backpressure.onLowWatermark = std::move(callback);
    return *this;
}

ChannelBuilder& ChannelBuilder::autoReconnect(bool enable) {
    config_.reconnect.enable = enable;
    return *this;
}

ChannelBuilder& ChannelBuilder::reconnectStrategy(ReconnectStrategy strategy) {
    config_.reconnect.strategy = strategy;
    return *this;
}

ChannelBuilder& ChannelBuilder::reconnectDelay(uint32_t initialMs, uint32_t maxMs) {
    config_.reconnect.initialDelayMs = initialMs;
    config_.reconnect.maxDelayMs = maxMs;
    return *this;
}

ChannelBuilder& ChannelBuilder::maxRetries(int maxRetries) {
    config_.reconnect.maxRetries = maxRetries;
    return *this;
}

ChannelBuilder& ChannelBuilder::heartbeat(uint32_t intervalMs, uint32_t timeoutMs) {
    config_.reconnect.enableHeartbeat = true;
    config_.reconnect.heartbeatIntervalMs = intervalMs;
    config_.reconnect.heartbeatTimeoutMs = timeoutMs;
    return *this;
}

ChannelBuilder& ChannelBuilder::healthCheck(uint32_t intervalMs, uint32_t timeoutMs) {
    config_.reconnect.enableHealthCheck = true;
    config_.reconnect.healthCheckIntervalMs = intervalMs;
    config_.reconnect.healthCheckTimeoutMs = timeoutMs;
    return *this;
}

ChannelBuilder& ChannelBuilder::onReconnecting(std::function<void()> callback) {
    config_.reconnect.onReconnecting = std::move(callback);
    return *this;
}

ChannelBuilder& ChannelBuilder::onReconnected(std::function<void()> callback) {
    config_.reconnect.onReconnected = std::move(callback);
    return *this;
}

ChannelBuilder& ChannelBuilder::onReconnectFailed(std::function<void(int)> callback) {
    config_.reconnect.onReconnectFailed = std::move(callback);
    return *this;
}

ChannelBuilder& ChannelBuilder::onGiveUp(std::function<void()> callback) {
    config_.reconnect.onGiveUp = std::move(callback);
    return *this;
}

std::unique_ptr<Channel> ChannelBuilder::build() {
    config_.name = name_;
    return Channel::create(name_, config_);
}

std::unique_ptr<Channel> ChannelBuilder::connect() {
    auto channel = build();
    if (channel) {
        channel->connect();
    }
    return channel;
}
