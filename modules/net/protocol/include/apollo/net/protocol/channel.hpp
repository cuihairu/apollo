#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <system_error>

namespace apollo {
namespace net {
namespace protocol {

/**
 * @brief Protocol types supported by the channel
 */
enum class Protocol {
    Ipc,       // 共享内存 (本机最优)
    Tcp,       // TCP (远程通用)
    InProc,    // 进程内 (最快)
    Ws,        // WebSocket
    Auto       // 自动选择
};

/**
 * @brief Message wrapper for channel communication
 */
class Message {
public:
    Message() = default;
    explicit Message(std::vector<uint8_t> data);
    explicit Message(const std::string& str);
    explicit Message(const void* data, size_t size);

    const uint8_t* data() const { return data_.data(); }
    size_t size() const { return data_.size(); }
    std::string toString() const;

    bool empty() const { return data_.empty(); }

private:
    std::vector<uint8_t> data_;
};

/**
 * @brief Channel callback for receiving messages
 */
using MessageCallback = std::function<void(const Message&)>;

/**
 * @brief Error callback
 */
using ErrorCallback = std::function<void(const std::error_code&)>;

/**
 * @brief Channel configuration
 */
struct ChannelConfig {
    int recvTimeoutMs = 5000;     // 接收超时
    int sendTimeoutMs = 5000;     // 发送超时
    bool autoReconnect = true;     // 自动重连
    int reconnectIntervalMs = 1000;
    int maxRetry = 3;              // 最大重试次数

    // Protocol preference (for Auto mode)
    Protocol protocol = Protocol::Auto;
};

/**
 * @brief Thin channel abstraction over NNG
 *
 * Features:
 * - Automatic protocol negotiation (ipc/tcp/inproc)
 * - Local/remote detection
 * - Auto-reconnect
 * - Simple send/receive API
 */
class Channel {
public:
    Channel() = default;
    ~Channel();

    // Client: connect to a service
    // name: service name (will be resolved via registry)
    // or direct URL: "tcp://localhost:8080"
    explicit Channel(const std::string& name, const ChannelConfig& config = {});

    // No copy, move only
    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;
    Channel(Channel&&) noexcept;
    Channel& operator=(Channel&&) noexcept;

    // Connect to a service endpoint
    bool connect(const std::string& endpoint, std::error_code& ec);
    bool connect(const std::string& endpoint);

    // Bind as a server (listens on multiple protocols)
    bool bind(const std::string& url, std::error_code& ec);
    bool bind(const std::string& url);

    // Send message
    bool send(const Message& msg, std::error_code& ec);
    bool send(const Message& msg);

    // Receive message (blocking)
    bool receive(Message& msg, std::error_code& ec);
    bool receive(Message& msg);

    // Async receive with callback
    void receiveAsync(MessageCallback onMsg, ErrorCallback onError = nullptr);

    // Close channel
    void close();

    // Check if connected
    bool isConnected() const { return connected_; }

    // Get current protocol
    Protocol getProtocol() const { return protocol_; }

    // Get endpoint URL
    std::string getEndpoint() const { return endpoint_; }

    // Static: create client channel
    static Channel connect(const std::string& service, const ChannelConfig& config = {});

    // Static: create server channel
    static Channel bind(const std::string& url, const ChannelConfig& config = {});

private:
    void* socket_ = nullptr;      // nng_socket
    std::string endpoint_;
    Protocol protocol_ = Protocol::Auto;
    bool connected_ = false;
    bool server_ = false;
    ChannelConfig config_;

    // Protocol negotiation
    bool negotiateProtocol(const std::string& service, std::string& outUrl);
    Protocol detectBestProtocol(const std::string& endpoint);
};

/**
 * @brief Convenience function for RPC-style request/response
 */
Message request(Channel& ch, const Message& req, int timeoutMs = 5000);

} // namespace protocol
} // namespace net
} // namespace apollo
