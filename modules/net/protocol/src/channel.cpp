/**
 * @file channel.cpp
 * @brief Thin NNG channel wrapper implementation
 */

#include "apollo/net/protocol/channel.hpp"
#include "apollo/net/protocol/endpoint.hpp"

#include <cstring>

namespace apollo {
namespace net {
namespace protocol {

//==============================================================================
// Message implementation
//==============================================================================

Message::Message(std::vector<uint8_t> data) : data_(std::move(data)) {}

Message::Message(const std::string& str) {
    data_.assign(str.begin(), str.end());
}

Message::Message(const void* data, size_t size) {
    data_.resize(size);
    std::memcpy(data_.data(), data, size);
}

std::string Message::toString() const {
    return std::string(data_.begin(), data_.end());
}

} // namespace protocol
} // namespace net
} // namespace apollo

#ifdef APOLLO_USE_NNG

#include <nng/nng.h>
#include <nng/protocol/reqrep0/req.h>
#include <nng/protocol/pair0/pair.h>
#include <nng/protocol/pubsub/pub.h>
#include <nng/supplemental/util/platform.h>
#include <nng/supplemental/util/id_generator.h>

#include <thread>
#include <chrono>
#include <unordered_map>

namespace apollo {
namespace net {
namespace protocol {

//==============================================================================
// Channel implementation
//==============================================================================

Channel::~Channel() {
    close();
}

Channel::Channel(const std::string& name, const ChannelConfig& config)
    : config_(config) {
    connect(name);
}

Channel::Channel(Channel&& other) noexcept
    : socket_(other.socket_)
    , endpoint_(std::move(other.endpoint_))
    , protocol_(other.protocol_)
    , connected_(other.connected_)
    , server_(other.server_)
    , config_(other.config_) {
    other.socket_ = nullptr;
    other.connected_ = false;
}

Channel& Channel::operator=(Channel&& other) noexcept {
    if (this != &other) {
        close();
        socket_ = other.socket_;
        endpoint_ = std::move(other.endpoint_);
        protocol_ = other.protocol_;
        connected_ = other.connected_;
        server_ = other.server_;
        config_ = other.config_;
        other.socket_ = nullptr;
        other.connected_ = false;
    }
    return *this;
}

bool Channel::connect(const std::string& endpoint, std::error_code& ec) {
    close();

    // Check if it's a direct URL or a service name
    if (endpoint.find("://") != std::string::npos) {
        // Direct URL
        endpoint_ = endpoint;
        protocol_ = detectBestProtocol(endpoint);
    } else {
        // Service name - need to resolve via registry
        if (!negotiateProtocol(endpoint, endpoint_)) {
            ec = std::make_error_code(std::errc::connection_refused);
            return false;
        }
        protocol_ = detectBestProtocol(endpoint_);
    }

    // Create socket based on protocol
    int rv;
    switch (protocol_) {
        case Protocol::Ipc:
            rv = nng_req0_open(&socket_);
            break;
        case Protocol::Tcp:
            rv = nng_req0_open(&socket_);
            break;
        case Protocol::InProc:
            rv = nng_req0_open(&socket_);
            break;
        case Protocol::Ws:
            // WebSocket would use different socket type
            rv = nng_req0_open(&socket_);  // Fallback
            break;
        default:
            rv = NNG_ENOTSUP;
            break;
    }

    if (rv != 0) {
        ec = std::make_error_code(static_cast<int>(rv), std::system_category());
        return false;
    }

    // Set timeouts
    nng_duration recvto = config_.recvTimeoutMs < 0
        ? NNG_DURATION_INFINITE
        : nng_millis(config_.recvTimeoutMs);
    nng_duration sendto = config_.sendTimeoutMs < 0
        ? NNG_DURATION_INFINITE
        : nng_millis(config_.sendTimeoutMs);

    nng_setopt_recv_timeout(socket_, recvto);
    nng_setopt_send_timeout(socket_, sendto);

    // Dial (connect)
    rv = nng_dial(socket_, endpoint_.c_str());

    if (rv != 0) {
        ec = std::make_error_code(static_cast<int>(rv), std::system_category());
        close();
        return false;
    }

    connected_ = true;
    server_ = false;
    return true;
}

bool Channel::connect(const std::string& endpoint) {
    std::error_code ec;
    return connect(endpoint, ec);
}

bool Channel::bind(const std::string& url, std::error_code& ec) {
    close();

    endpoint_ = url;
    protocol_ = detectBestProtocol(url);
    server_ = true;

    // Create socket
    int rv = nng_rep0_open(&socket_);
    if (rv != 0) {
        ec = std::make_error_code(static_cast<int>(rv), std::system_category());
        return false;
    }

    // Listen
    rv = nng_listen(socket_, endpoint_.c_str());
    if (rv != 0) {
        ec = std::make_error_code(static_cast<int>(rv), std::system_category());
        close();
        return false;
    }

    connected_ = true;
    return true;
}

bool Channel::bind(const std::string& url) {
    std::error_code ec;
    return bind(url, ec);
}

bool Channel::send(const Message& msg, std::error_code& ec) {
    if (!connected_ || !socket_) {
        ec = std::make_error_code(std::errc::not_connected);
        return false;
    }

    nng_msg* nngMsg;
    int rv = nng_msg_alloc(&nngMsg, msg.size());
    if (rv != 0) {
        ec = std::make_error_code(static_cast<int>(rv), std::system_category());
        return false;
    }

    std::memcpy(nng_msg_body(nngMsg), msg.data(), msg.size());

    rv = nng_sendmsg(socket_, nngMsg, 0);
    nng_msg_free(nngMsg);

    if (rv != 0) {
        ec = std::make_error_code(static_cast<int>(rv), std::system_category());
        return false;
    }

    return true;
}

bool Channel::send(const Message& msg) {
    std::error_code ec;
    return send(msg, ec);
}

bool Channel::receive(Message& msg, std::error_code& ec) {
    if (!connected_ || !socket_) {
        ec = std::make_error_code(std::errc::not_connected);
        return false;
    }

    nng_msg* nngMsg = nullptr;
    int rv = nng_recvmsg(socket_, &nngMsg, 0);

    if (rv != 0) {
        ec = std::make_error_code(static_cast<int>(rv), std::system_category());
        return false;
    }

    if (nngMsg) {
        size_t size = nng_msg_len(nngMsg);
        std::vector<uint8_t> data(size);
        std::memcpy(data.data(), nng_msg_body(nngMsg), size);
        msg = Message(std::move(data));
        nng_msg_free(nngMsg);
    }

    return true;
}

bool Channel::receive(Message& msg) {
    std::error_code ec;
    return receive(msg, ec);
}

void Channel::receiveAsync(MessageCallback onMsg, ErrorCallback onError) {
    // Simple async receive (in production, use thread pool)
    std::thread([this, onMsg, onError]() {
        while (connected_) {
            Message msg;
            std::error_code ec;
            if (receive(msg, ec)) {
                if (onMsg) onMsg(msg);
            } else {
                if (onError) onError(ec);
                break;
            }
        }
    }).detach();
}

void Channel::close() {
    if (socket_) {
        nng_close(socket_);
        socket_ = nullptr;
    }
    connected_ = false;
}

Channel Channel::connect(const std::string& service, const ChannelConfig& config) {
    Channel ch;
    ch.config_ = config;
    ch.connect(service);
    return ch;
}

Channel Channel::bind(const std::string& url, const ChannelConfig& config) {
    Channel ch;
    ch.config_ = config;
    ch.bind(url);
    return ch;
}

//==============================================================================
// Protocol negotiation
//==============================================================================

bool Channel::negotiateProtocol(const std::string& service, std::string& outUrl) {
    // Try to discover service from registry
    auto& registry = getRegistry();
    Endpoint endpoint;

    if (registry.getBestEndpoint(service, endpoint)) {
        // Protocol negotiation
        Protocol bestProtocol = Protocol::Tcp;  // Default

        // Priority: InProc > Ipc > Tcp > Ws
        if (endpoint.supports(Protocol::InProc)) {
            bestProtocol = Protocol::InProc;
        } else if (endpoint.supports(Protocol::Ipc)) {
            bestProtocol = Protocol::Ipc;
        } else if (endpoint.supports(Protocol::Tcp)) {
            bestProtocol = Protocol::Tcp;
        } else if (endpoint.supports(Protocol::Ws)) {
            bestProtocol = Protocol::Ws;
        }

        outUrl = endpoint.toUrl(bestProtocol);
        return true;
    }

    // Fallback: assume TCP localhost
    outUrl = "tcp://127.0.0.1:8080";
    return false;
}

Protocol Channel::detectBestProtocol(const std::string& endpoint) {
    if (endpoint.find("ipc://") == 0) return Protocol::Ipc;
    if (endpoint.find("tcp://") == 0) return Protocol::Tcp;
    if (endpoint.find("inproc://") == 0) return Protocol::InProc;
    if (endpoint.find("ws://") == 0 || endpoint.find("wss://") == 0) return Protocol::Ws;
    return Protocol::Tcp;  // Default
}

//==============================================================================
// RPC helper
//==============================================================================

Message request(Channel& ch, const Message& req, int timeoutMs) {
    // Simple request-response (synchronous)
    ch.send(req);

    Message resp;
    // In production, use async with timeout
    // For now, just try to receive once
    ch.receive(resp);

    return resp;
}

} // namespace protocol
} // namespace net
} // namespace apollo

#else // !APOLLO_USE_NNG

namespace apollo {
namespace net {
namespace protocol {

// Stub implementation when NNG is not available
Channel::~Channel() { close(); }
Channel::Channel(const std::string&, const ChannelConfig&) {}
Channel::Channel(Channel&&) noexcept = default;
Channel& Channel::operator=(Channel&&) noexcept = default;

bool Channel::connect(const std::string&, std::error_code&) { return false; }
bool Channel::connect(const std::string&) { return false; }
bool Channel::bind(const std::string&, std::error_code&) { return false; }
bool Channel::bind(const std::string&) { return false; }
bool Channel::send(const Message&, std::error_code&) { return false; }
bool Channel::send(const Message&) { return false; }
bool Channel::receive(Message&, std::error_code&) { return false; }
bool Channel::receive(Message&) { return false; }
void Channel::receiveAsync(MessageCallback, ErrorCallback) {}
void Channel::close() {}
Channel Channel::connect(const std::string&, const ChannelConfig&) { return Channel(); }
Channel Channel::bind(const std::string&, const ChannelConfig&) { return Channel(); }

Message request(Channel&, const Message&, int) { return Message(); }

} // namespace protocol
} // namespace net
} // namespace apollo

#endif // APOLLO_USE_NNG
