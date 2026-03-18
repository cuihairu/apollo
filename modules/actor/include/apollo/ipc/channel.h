#pragma once

/**
 * @file channel.h
 * @brief IPC channel stub - to be replaced with NNG-based implementation
 */

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>

namespace apollo {
namespace ipc {

// Forward declarations
class Message;

/**
 * @brief Backpressure strategy for message handling
 */
enum class BackpressureStrategy {
    Drop,       // Drop messages when buffer is full
    Buffer,     // Buffer messages (may cause memory issues)
    Error       // Return error when buffer is full
};

/**
 * @brief Message buffer configuration
 */
struct Buffer {
    size_t capacity = 1024;
    size_t batchSize = 32;
    BackpressureStrategy strategy = BackpressureStrategy::Drop;
};

enum class ChannelType {
    SharedMemory,
    TCP,
    NNG  // Using NNG instead
};

struct ChannelConfig {
    ChannelType type = ChannelType::NNG;
    std::string address;
    int port = 0;
    size_t bufferSize = 1024 * 1024;  // 1MB default

    // NNG specific
    std::string url = "ipc:///tmp/apollo.ipc";
};

/**
 * @brief IPC Channel interface - stub implementation
 * TODO: Replace with NNG-based implementation from protocol module
 */
class Channel {
public:
    using MessageHandler = std::function<void(const Message&)>;

    Channel() = default;
    virtual ~Channel() = default;

    // Stub methods
    virtual bool connect(const std::string& address) { return false; }
    virtual bool disconnect() { return false; }
    virtual bool send(const Message& msg) { return false; }
    virtual void setHandler(MessageHandler handler) {}

protected:
    ChannelConfig config_;
};

/**
 * @brief Message wrapper for IPC communication
 */
class Message {
public:
    Message() = default;
    explicit Message(const std::string& data) : data_(data) {}
    explicit Message(const std::vector<uint8_t>& data) : data_(data.begin(), data.end()) {}

    const std::string& data() const { return data_; }
    void setData(const std::string& data) { data_ = data; }

private:
    std::string data_;
};

} // namespace ipc
} // namespace apollo
