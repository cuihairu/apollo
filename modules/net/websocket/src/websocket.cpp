/**
 * @file websocket.cpp
 * @brief WebSocket 实现
 */

#include "apollo/net/websocket.h"
#include "apollo/net/http.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <cstring>
#include <random>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
    #pragma comment(lib, "ws2_32.lib")
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET socket_t;
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define SOCKET_ERROR_VALUE SOCKET_ERROR
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <errno.h>
    typedef int socket_t;
    #define INVALID_SOCKET_VALUE -1
    #define SOCKET_ERROR_VALUE -1
    #define closesocket close
#endif

// SHA-1 实现 (简化版)
#include <openssl/sha.h>
#if defined(_MSC_VER)
#pragma comment(lib, "libcrypto.lib")
#endif

namespace apollo {
namespace net {
namespace websocket {

//==============================================================================
// Base64 编解码
//==============================================================================

static const char BASE64_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(const std::vector<uint8_t>& data) {
    std::string result;
    result.reserve(((data.size() + 2) / 3) * 4);

    for (size_t i = 0; i < data.size(); i += 3) {
        uint32_t value = 0;
        value |= static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < data.size()) {
            value |= static_cast<uint32_t>(data[i + 1]) << 8;
        }
        if (i + 2 < data.size()) {
            value |= static_cast<uint32_t>(data[i + 2]);
        }

        result.push_back(BASE64_CHARS[(value >> 18) & 0x3F]);
        result.push_back(BASE64_CHARS[(value >> 12) & 0x3F]);
        result.push_back((i + 1 < data.size()) ? BASE64_CHARS[(value >> 6) & 0x3F] : '=');
        result.push_back((i + 2 < data.size()) ? BASE64_CHARS[value & 0x3F] : '=');
    }

    return result;
}

std::vector<uint8_t> base64Decode(const std::string& encoded) {
    std::vector<uint8_t> result;

    // 构建反向查找表
    int8_t lookup[256] = {};
    for (int8_t i = 0; i < 64; ++i) {
        lookup[static_cast<uint8_t>(BASE64_CHARS[i])] = i;
    }

    for (size_t i = 0; i < encoded.size(); i += 4) {
        uint32_t value = 0;
        size_t count = 0;

        for (size_t j = 0; j < 4; ++j) {
            if (i + j < encoded.size() && encoded[i + j] != '=') {
                value |= static_cast<uint32_t>(
                    lookup[static_cast<uint8_t>(encoded[i + j])]) << (18 - j * 6);
                ++count;
            }
        }

        if (count >= 2) {
            result.push_back((value >> 16) & 0xFF);
        }
        if (count >= 3) {
            result.push_back((value >> 8) & 0xFF);
        }
        if (count >= 4) {
            result.push_back(value & 0xFF);
        }
    }

    return result;
}

//==============================================================================
// SHA-1 哈希
//==============================================================================

std::vector<uint8_t> sha1(const std::string& data) {
    std::vector<uint8_t> result(SHA_DIGEST_LENGTH);
    ::SHA1(reinterpret_cast<const uint8_t*>(data.c_str()), data.size(), result.data());
    return result;
}

//==============================================================================
// WebSocket Key 生成
//==============================================================================

std::string generateWebSocketKey() {
    // 生成 16 字节随机数
    std::vector<uint8_t> raw(16);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<unsigned int> dis(0, 255);

    for (auto& byte : raw) {
        byte = static_cast<uint8_t>(dis(gen));
    }

    return base64Encode(raw);
}

//==============================================================================
// URL 解析
//==============================================================================

UrlInfo parseUrl(const std::string& url) {
    UrlInfo info{};
    info.port = 80;
    info.path = "/";

    // 解析协议
    size_t pos = url.find("://");
    if (pos != std::string::npos) {
        std::string protocol = url.substr(0, pos);
        info.ssl = (protocol == "wss");
        pos += 3;
    } else {
        pos = 0;
    }

    // 解析主机和路径
    size_t slashPos = url.find('/', pos);
    size_t colonPos = url.find(':', pos);
    size_t hostEnd = (slashPos != std::string::npos) ? slashPos : url.length();

    if (colonPos != std::string::npos && colonPos < hostEnd) {
        // 有端口
        info.host = url.substr(pos, colonPos - pos);
        info.port = static_cast<uint16_t>(std::stoi(url.substr(colonPos + 1, hostEnd - colonPos - 1)));
    } else {
        info.host = url.substr(pos, hostEnd - pos);
        if (info.ssl) {
            info.port = 443;
        }
    }

    // 解析路径
    if (slashPos != std::string::npos) {
        size_t queryPos = url.find('?', slashPos);
        if (queryPos != std::string::npos) {
            info.path = url.substr(slashPos, queryPos - slashPos);
            info.query = url.substr(queryPos + 1);
        } else {
            info.path = url.substr(slashPos);
        }
    }

    return info;
}

//==============================================================================
// WebSocket 握手实现
//==============================================================================

std::string Handshake::computeAcceptKey(const std::string& clientKey) {
    // GUID: 258EAFA5-E914-47DA-95CA-C5AB0DC85B11
    const std::string GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = clientKey + GUID;
    auto hash = sha1(combined);
    return base64Encode(hash);
}

std::string Handshake::generateServerResponse(
    const std::string& acceptKey,
    const std::string& protocol) {

    std::ostringstream oss;
    oss << "HTTP/1.1 101 Switching Protocols\r\n";
    oss << "Upgrade: websocket\r\n";
    oss << "Connection: Upgrade\r\n";
    oss << "Sec-WebSocket-Accept: " << acceptKey << "\r\n";

    if (!protocol.empty()) {
        oss << "Sec-WebSocket-Protocol: " << protocol << "\r\n";
    }

    oss << "\r\n";
    return oss.str();
}

std::string Handshake::generateClientRequest(
    const std::string& uri,
    const std::string& host,
    const std::string& origin,
    const std::vector<std::string>& protocols) {

    std::string key = generateWebSocketKey();

    std::ostringstream oss;
    oss << "GET " << uri << " HTTP/1.1\r\n";
    oss << "Host: " << host << "\r\n";
    oss << "Upgrade: websocket\r\n";
    oss << "Connection: Upgrade\r\n";
    oss << "Sec-WebSocket-Key: " << key << "\r\n";
    oss << "Sec-WebSocket-Version: 13\r\n";

    if (!origin.empty()) {
        oss << "Origin: " << origin << "\r\n";
    }

    if (!protocols.empty()) {
        oss << "Sec-WebSocket-Protocol: ";
        for (size_t i = 0; i < protocols.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << protocols[i];
        }
        oss << "\r\n";
    }

    oss << "\r\n";
    return oss.str();
}

bool Handshake::validateAccept(
    const std::string& acceptKey,
    const std::string& expectedKey) {

    return acceptKey == expectedKey;
}

std::string Handshake::extractWebSocketKey(const std::string& handshakeData) {
    // 查找 Sec-WebSocket-Key
    size_t keyPos = handshakeData.find("Sec-WebSocket-Key:");
    if (keyPos == std::string::npos) {
        return "";
    }

    keyPos += 19; // "Sec-WebSocket-Key:" 长度
    while (keyPos < handshakeData.size() &&
           (handshakeData[keyPos] == ' ' || handshakeData[keyPos] == '\t')) {
        ++keyPos;
    }

    size_t keyEnd = handshakeData.find("\r\n", keyPos);
    if (keyEnd == std::string::npos) {
        return "";
    }

    return handshakeData.substr(keyPos, keyEnd - keyPos);
}

bool Handshake::parseClientRequest(
    const std::string& request,
    std::string& uri,
    std::string& host,
    std::string& clientKey,
    std::string& protocol,
    std::string& version) {

    uri.clear();
    host.clear();
    clientKey.clear();
    protocol.clear();
    version.clear();

    // 简化解析 - 逐行处理
    std::istringstream iss(request);
    std::string line;

    // 请求行
    if (!std::getline(iss, line)) return false;
    if (line.size() > 0 && line.back() == '\r') line.pop_back();

    // 解析 "GET /path HTTP/1.1"
    size_t pos1 = line.find(' ');
    if (pos1 == std::string::npos) return false;
    size_t pos2 = line.find(' ', pos1 + 1);
    if (pos2 == std::string::npos) return false;

    uri = line.substr(pos1 + 1, pos2 - pos1 - 1);

    // 头部
    while (std::getline(iss, line)) {
        if (line.size() > 0 && line.back() == '\r') line.pop_back();

        if (line.empty()) break;

        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;

        std::string key = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);

        // 去除首尾空白
        size_t start = value.find_first_not_of(" \t");
        size_t end = value.find_last_not_of(" \t\r\n");
        if (start != std::string::npos && end != std::string::npos) {
            value = value.substr(start, end - start + 1);
        }

        if (key == "Host") {
            host = value;
        } else if (key == "Sec-WebSocket-Key") {
            clientKey = value;
        } else if (key == "Sec-WebSocket-Protocol") {
            protocol = value;
        } else if (key == "Sec-WebSocket-Version") {
            version = value;
        }
    }

    return !clientKey.empty();
}

//==============================================================================
// 帧解析器实现
//==============================================================================

class FrameParser::Impl {
public:
    Impl() = default;

    int parse(const uint8_t* data, size_t size, Frame& frame) {
        if (size < 2) {
            return 0; // 需要更多数据
        }

        // 第一个字节
        uint8_t byte0 = data[0];
        frame.final = (byte0 & 0x80) != 0;
        uint8_t opcode = byte0 & 0x0F;
        frame.opcode = static_cast<OpCode>(opcode);

        // 第二个字节
        uint8_t byte1 = data[1];
        frame.masked = (byte1 & 0x80) != 0;
        uint64_t payloadLen = byte1 & 0x7F;

        size_t headerSize = 2;

        // 扩展载荷长度
        if (payloadLen == 126) {
            if (size < headerSize + 2) {
                return 0;
            }
            payloadLen = (static_cast<uint16_t>(data[2]) << 8) | data[3];
            headerSize += 2;
        } else if (payloadLen == 127) {
            if (size < headerSize + 8) {
                return 0;
            }
            payloadLen = (static_cast<uint64_t>(data[2]) << 56) |
                        (static_cast<uint64_t>(data[3]) << 48) |
                        (static_cast<uint64_t>(data[4]) << 40) |
                        (static_cast<uint64_t>(data[5]) << 32) |
                        (static_cast<uint64_t>(data[6]) << 24) |
                        (static_cast<uint64_t>(data[7]) << 16) |
                        (static_cast<uint64_t>(data[8]) << 8) |
                        static_cast<uint64_t>(data[9]);
            headerSize += 8;
        }

        // 掩码
        uint8_t maskingKey[4] = {};
        if (frame.masked) {
            if (size < headerSize + 4) {
                return 0;
            }
            std::memcpy(maskingKey, data + headerSize, 4);
            headerSize += 4;
        }

        // 载荷
        if (size < headerSize + payloadLen) {
            return 0; // 数据不完整
        }

        frame.payload.resize(payloadLen);
        std::memcpy(frame.payload.data(), data + headerSize, payloadLen);

        // 解掩码
        if (frame.masked) {
            for (size_t i = 0; i < payloadLen; ++i) {
                frame.payload[i] ^= maskingKey[i % 4];
            }
        }

        return static_cast<int>(headerSize + payloadLen);
    }
};

FrameParser::FrameParser() : impl_(new Impl()) {}
FrameParser::~FrameParser() { delete impl_; }

int FrameParser::parse(const uint8_t* data, size_t size, Frame& frame) {
    return impl_->parse(data, size, frame);
}

std::vector<uint8_t> FrameParser::encode(
    OpCode opcode,
    const uint8_t* payload,
    size_t payloadSize,
    bool mask,
    bool final) {

    std::vector<uint8_t> frame;

    // 第一个字节
    uint8_t byte0 = (final ? 0x80 : 0x00) | static_cast<uint8_t>(opcode);
    frame.push_back(byte0);

    // 第二个字节
    uint8_t maskBit = mask ? 0x80 : 0x00;

    if (payloadSize < 126) {
        frame.push_back(maskBit | static_cast<uint8_t>(payloadSize));
    } else if (payloadSize < 65536) {
        frame.push_back(maskBit | 126);
        frame.push_back((payloadSize >> 8) & 0xFF);
        frame.push_back(payloadSize & 0xFF);
    } else {
        frame.push_back(maskBit | 127);
        frame.push_back((payloadSize >> 56) & 0xFF);
        frame.push_back((payloadSize >> 48) & 0xFF);
        frame.push_back((payloadSize >> 40) & 0xFF);
        frame.push_back((payloadSize >> 32) & 0xFF);
        frame.push_back((payloadSize >> 24) & 0xFF);
        frame.push_back((payloadSize >> 16) & 0xFF);
        frame.push_back((payloadSize >> 8) & 0xFF);
        frame.push_back(payloadSize & 0xFF);
    }

    // 掩码密钥 (客户端需要掩码)
    uint8_t maskingKey[4] = {};
    if (mask) {
        // 生成随机掩码
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<unsigned int> dis(0, 255);
        for (auto& byte : maskingKey) {
            byte = static_cast<uint8_t>(dis(gen));
        }

        frame.push_back(maskingKey[0]);
        frame.push_back(maskingKey[1]);
        frame.push_back(maskingKey[2]);
        frame.push_back(maskingKey[3]);
    }

    // 载荷
    size_t payloadStart = frame.size();
    frame.insert(frame.end(), payload, payload + payloadSize);

    // 应用掩码
    if (mask) {
        for (size_t i = 0; i < payloadSize; ++i) {
            frame[payloadStart + i] ^= maskingKey[i % 4];
        }
    }

    return frame;
}

std::vector<uint8_t> FrameParser::encodeText(const std::string& text, bool mask) {
    return encode(OpCode::Text,
        reinterpret_cast<const uint8_t*>(text.c_str()),
        text.size(), mask);
}

std::vector<uint8_t> FrameParser::encodeBinary(const std::vector<uint8_t>& data, bool mask) {
    return encode(OpCode::Binary, data.data(), data.size(), mask);
}

std::vector<uint8_t> FrameParser::encodeClose(CloseStatus status, const std::string& reason) {
    std::vector<uint8_t> payload;
    uint16_t code = static_cast<uint16_t>(status);
    payload.push_back((code >> 8) & 0xFF);
    payload.push_back(code & 0xFF);
    payload.insert(payload.end(), reason.begin(), reason.end());
    return encode(OpCode::Close, payload.data(), payload.size(), false);
}

std::vector<uint8_t> FrameParser::encodePing(const std::vector<uint8_t>& data) {
    return encode(OpCode::Ping, data.data(), data.size(), false);
}

std::vector<uint8_t> FrameParser::encodePong(const std::vector<uint8_t>& data) {
    return encode(OpCode::Pong, data.data(), data.size(), false);
}

//==============================================================================
// 简单 Socket (复用)
//==============================================================================

class SimpleSocket {
public:
    SimpleSocket() : socket_(INVALID_SOCKET_VALUE) {}
    explicit SimpleSocket(socket_t sock) : socket_(sock) {}

    ~SimpleSocket() { close(); }

    SimpleSocket(const SimpleSocket&) = delete;
    SimpleSocket& operator=(const SimpleSocket&) = delete;

    SimpleSocket(SimpleSocket&& other) noexcept : socket_(other.socket_) {
        other.socket_ = INVALID_SOCKET_VALUE;
    }

    SimpleSocket& operator=(SimpleSocket&& other) noexcept {
        if (this != &other) {
            close();
            socket_ = other.socket_;
            other.socket_ = INVALID_SOCKET_VALUE;
        }
        return *this;
    }

    bool isValid() const { return socket_ != INVALID_SOCKET_VALUE; }

    int send(const char* data, size_t size) {
        if (!isValid()) return -1;
#ifdef _WIN32
        return ::send(socket_, data, static_cast<int>(size), 0);
#else
        return ::send(socket_, data, size, 0);
#endif
    }

    int send(const uint8_t* data, size_t size) {
        return send(reinterpret_cast<const char*>(data), size);
    }

    int receive(char* buffer, size_t size) {
        if (!isValid()) return -1;
#ifdef _WIN32
        return ::recv(socket_, buffer, static_cast<int>(size), 0);
#else
        return ::recv(socket_, buffer, size, 0);
#endif
    }

    void close() {
        if (isValid()) {
            ::closesocket(socket_);
            socket_ = INVALID_SOCKET_VALUE;
        }
    }

    socket_t get() const { return socket_; }
    void setSocket(socket_t sock) { socket_ = sock; }

private:
    socket_t socket_;
};

//==============================================================================
// WebSocket 服务器连接实现
//==============================================================================

class WebSocketServerConnection : public ServerConnection {
public:
    WebSocketServerConnection(socket_t sock, uint64_t connId,
                             const std::string& remoteAddr,
                             const Callbacks& callbacks,
                             const Config& config)
        : socket_(sock)
        , connectionId_(connId)
        , remoteAddr_(remoteAddr)
        , callbacks_(callbacks)
        , config_(config)
        , connected_(true) {
    }

    ~WebSocketServerConnection() override {
        close();
    }

    bool send(const Message& message) override {
        if (!connected_) return false;

        auto frame = FrameParser::encode(
            message.opcode,
            message.data.data(),
            message.data.size(),
            false,  // 服务器发送不掩码
            message.isFinal);

        int sent = socket_.send(frame.data(), frame.size());
        return sent == static_cast<int>(frame.size());
    }

    bool sendText(const std::string& text) override {
        return send(Message::text(text));
    }

    bool sendBinary(const std::vector<uint8_t>& data) override {
        return send(Message::binary(data));
    }

    bool sendClose(CloseStatus status, const std::string& reason) override {
        auto frame = FrameParser::encodeClose(status, reason);
        int sent = socket_.send(frame.data(), frame.size());
        close();
        return sent == static_cast<int>(frame.size());
    }

    bool sendPing(const std::vector<uint8_t>& data) override {
        auto frame = FrameParser::encodePing(data);
        int sent = socket_.send(frame.data(), frame.size());
        return sent == static_cast<int>(frame.size());
    }

    std::string getRemoteAddress() const override {
        return remoteAddr_;
    }

    uint64_t getConnectionId() const override {
        return connectionId_;
    }

    bool isConnected() const override {
        return connected_;
    }

    void close() override {
        if (connected_) {
            connected_ = false;
            socket_.close();
        }
    }

    // 处理接收数据
    void handleReceive(const uint8_t* data, size_t size) {
        receiveBuffer_.insert(receiveBuffer_.end(), data, data + size);

        FrameParser parser;
        FrameParser::Frame frame;

        while (true) {
            int parsed = parser.parse(receiveBuffer_.data(), receiveBuffer_.size(), frame);
            if (parsed <= 0) break;

            // 移除已解析的数据
            receiveBuffer_.erase(receiveBuffer_.begin(),
                                receiveBuffer_.begin() + parsed);

            // 处理帧
            handleMessage(frame);
        }
    }

private:
    void handleMessage(const FrameParser::Frame& frame) {
        switch (frame.opcode) {
            case OpCode::Text:
            case OpCode::Binary:
            case OpCode::Continuation:
                if (callbacks_.onMessage) {
                    Message msg(frame.opcode, frame.payload, true);
                    callbacks_.onMessage(msg);
                }
                break;

            case OpCode::Close:
                // 解析关闭状态
                if (frame.payload.size() >= 2) {
                    uint16_t code = (static_cast<uint16_t>(frame.payload[0]) << 8) |
                                   static_cast<uint16_t>(frame.payload[1]);
                    std::string reason;
                    if (frame.payload.size() > 2) {
                        size_t reason_len = std::min(frame.payload.size() - 2, static_cast<size_t>(123));
                        const char* reason_data = reinterpret_cast<const char*>(frame.payload.data() + 2);
                        reason.assign(reason_data, reason_len);
                    }
                    if (callbacks_.onClose) {
                        callbacks_.onClose(static_cast<CloseStatus>(code), reason);
                    }
                }
                close();
                break;

            case OpCode::Ping:
                // 自动回复 Pong
                {
                    auto pongFrame = FrameParser::encodePong(frame.payload);
                    socket_.send(pongFrame.data(), pongFrame.size());
                }
                break;

            case OpCode::Pong:
                // Pong 响应 (可以用于更新心跳)
                break;

            default:
                break;
        }
    }

    SimpleSocket socket_;
    uint64_t connectionId_;
    std::string remoteAddr_;
    Callbacks callbacks_;
    Config config_;
    std::atomic<bool> connected_;
    std::vector<uint8_t> receiveBuffer_;
};

//==============================================================================
// WebSocket 服务器实现
//==============================================================================

class WebSocketServer : public Server {
public:
    WebSocketServer() : running_(false), nextConnId_(1) {}

    ~WebSocketServer() override {
        stop();
    }

    bool start(const std::string& host, uint16_t port,
              const Callbacks& callbacks,
              const Config& config) override {

        if (running_) return false;
        (void)host;

        // 创建监听 socket
        listenSocket_.setSocket(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        if (!listenSocket_.isValid()) {
            return false;
        }

        int opt = 1;
        ::setsockopt(listenSocket_.get(), SOL_SOCKET, SO_REUSEADDR,
            reinterpret_cast<const char*>(&opt), sizeof(opt));

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);

        if (::bind(listenSocket_.get(),
            reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
            return false;
        }

        if (::listen(listenSocket_.get(), 128) != 0) {
            return false;
        }

        callbacks_ = callbacks;
        config_ = config;
        running_ = true;
        acceptThread_ = std::thread(&WebSocketServer::acceptLoop, this);

        return true;
    }

    void stop() override {
        running_ = false;

        if (acceptThread_.joinable()) {
            acceptThread_.join();
        }

        listenSocket_.close();

        std::lock_guard<std::mutex> lock(connsMutex_);
        for (auto& conn : connections_) {
            conn.second->close();
        }
        connections_.clear();
    }

    bool isRunning() const override {
        return running_;
    }

    void broadcast(const Message& message) override {
        std::lock_guard<std::mutex> lock(connsMutex_);
        for (auto& conn : connections_) {
            conn.second->send(message);
        }
    }

    void broadcastText(const std::string& text) override {
        broadcast(Message::text(text));
    }

    size_t getConnectionCount() const override {
        std::lock_guard<std::mutex> lock(connsMutex_);
        return connections_.size();
    }

    ServerConnection* getConnection(uint64_t connId) const override {
        std::lock_guard<std::mutex> lock(connsMutex_);
        auto it = connections_.find(connId);
        return it != connections_.end() ? it->second.get() : nullptr;
    }

    void setRoute(const std::string& path, RouteHandler handler) override {
        routes_[path] = std::move(handler);
    }

private:
    void acceptLoop() {
        while (running_) {
            struct sockaddr_in clientAddr{};
            socklen_t addrLen = sizeof(clientAddr);

            socket_t clientSock = ::accept(listenSocket_.get(),
                reinterpret_cast<struct sockaddr*>(&clientAddr), &addrLen);

            if (clientSock == INVALID_SOCKET_VALUE) {
                if (running_) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                continue;
            }

            // 获取客户端地址
            std::string remoteAddr = ::inet_ntoa(clientAddr.sin_addr);

            // 启动连接处理线程
            std::thread([this, clientSock, remoteAddr]() {
                handleClient(clientSock, remoteAddr);
            }).detach();
        }
    }

    void handleClient(socket_t sock, const std::string& remoteAddr) {
        SimpleSocket socket(sock);

        // 握手阶段
        char buffer[4096];
        int received = socket.receive(buffer, sizeof(buffer) - 1);
        if (received <= 0) {
            return;
        }
        buffer[received] = '\0';

        std::string uri, host, clientKey, protocol, version;
        if (!Handshake::parseClientRequest(
            std::string(buffer, received), uri, host, clientKey, protocol, version)) {
            return; // 无效握手
        }

        // 检查路由
        if (!routes_.empty()) {
            auto it = routes_.find(uri);
            if (it == routes_.end() && !routes_.empty()) {
                // 尝试通配符匹配
                it = routes_.find("*");
            }
            if (it != routes_.end()) {
                // 路由处理
                // 这里简化处理，实际可能需要自定义握手逻辑
            }
        }

        // 生成握手响应
        std::string acceptKey = Handshake::computeAcceptKey(clientKey);
        std::string response = Handshake::generateServerResponse(acceptKey, protocol);

        if (socket.send(response.data(), response.size()) !=
            static_cast<int>(response.size())) {
            return;
        }

        // 创建连接对象
        uint64_t connId = nextConnId_++;
        auto conn = std::make_shared<WebSocketServerConnection>(
            sock, connId, remoteAddr, callbacks_, config_);

        {
            std::lock_guard<std::mutex> lock(connsMutex_);
            connections_[connId] = conn;
        }

        if (callbacks_.onOpen) {
            callbacks_.onOpen();
        }

        // 消息循环
        std::vector<uint8_t> recvBuf(4096);
        while (conn->isConnected()) {
            int received = socket.receive(
                reinterpret_cast<char*>(recvBuf.data()), recvBuf.size());

            if (received <= 0) break;

            conn->handleReceive(recvBuf.data(), received);
        }

        // 连接关闭
        if (callbacks_.onClose) {
            callbacks_.onClose(CloseStatus::Normal, "");
        }

        conn->close();

        {
            std::lock_guard<std::mutex> lock(connsMutex_);
            connections_.erase(connId);
        }
    }

    SimpleSocket listenSocket_;
    std::thread acceptThread_;
    std::atomic<bool> running_;
    std::atomic<uint64_t> nextConnId_;

    mutable std::mutex connsMutex_;
    std::unordered_map<uint64_t, std::shared_ptr<WebSocketServerConnection>> connections_;

    Callbacks callbacks_;
    Config config_;

    std::unordered_map<std::string, RouteHandler> routes_;
};

//==============================================================================
// WebSocket 客户端连接实现
//==============================================================================

class WebSocketClientConnection : public ClientConnection {
public:
    WebSocketClientConnection() : connected_(false) {}
    ~WebSocketClientConnection() override { disconnect(); }

    bool connect(const std::string& url,
                const Callbacks& callbacks,
                const Config& config) override {

        url_ = url;
        callbacks_ = callbacks;
        config_ = config;

        // 解析 URL
        UrlInfo info = parseUrl(url);

        // 创建 socket
        socket_.setSocket(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        if (!socket_.isValid()) {
            if (callbacks_.onError) {
                callbacks_.onError("Failed to create socket");
            }
            return false;
        }

        // 连接
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(info.port);

        // 简化: 仅支持 IP 地址
#ifdef _WIN32
        addr.sin_addr.s_addr = ::inet_addr(info.host.c_str());
        if (addr.sin_addr.s_addr == INADDR_NONE) {
            // TODO: DNS 解析
            if (callbacks_.onError) {
                callbacks_.onError("DNS resolution not implemented");
            }
            socket_.close();
            return false;
        }
#else
        if (::inet_pton(AF_INET, info.host.c_str(), &addr.sin_addr) != 1) {
            if (callbacks_.onError) {
                callbacks_.onError("DNS resolution not implemented");
            }
            socket_.close();
            return false;
        }
#endif

        if (::connect(socket_.get(),
            reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
            if (callbacks_.onError) {
                callbacks_.onError("Connection failed");
            }
            socket_.close();
            return false;
        }

        // 发送握手请求
        std::string handshake = Handshake::generateClientRequest(
            info.path + (info.query.empty() ? "" : "?" + info.query),
            info.host);

        if (socket_.send(handshake.data(), handshake.size()) !=
            static_cast<int>(handshake.size())) {
            if (callbacks_.onError) {
                callbacks_.onError("Failed to send handshake");
            }
            socket_.close();
            return false;
        }

        // 接收握手响应
        char responseBuffer[4096];
        int received = socket_.receive(responseBuffer, sizeof(responseBuffer) - 1);
        if (received <= 0) {
            if (callbacks_.onError) {
                callbacks_.onError("Failed to receive handshake response");
            }
            socket_.close();
            return false;
        }
        responseBuffer[received] = '\0';

        std::string response(responseBuffer, received);

        // 验证响应
        if (response.find("101") == std::string::npos) {
            if (callbacks_.onError) {
                callbacks_.onError("Handshake failed: not 101");
            }
            socket_.close();
            return false;
        }

        connected_ = true;

        if (callbacks_.onOpen) {
            callbacks_.onOpen();
        }

        return true;
    }

    void disconnect() override {
        if (connected_) {
            close(CloseStatus::Normal, "");
        }
    }

    bool send(const Message& message) override {
        if (!connected_) return false;

        auto frame = FrameParser::encode(
            message.opcode,
            message.data.data(),
            message.data.size(),
            true,  // 客户端发送需要掩码
            message.isFinal);

        int sent = socket_.send(frame.data(), frame.size());
        return sent == static_cast<int>(frame.size());
    }

    bool sendText(const std::string& text) override {
        return send(Message::text(text));
    }

    bool sendBinary(const std::vector<uint8_t>& data) override {
        return send(Message::binary(data));
    }

    bool close(CloseStatus status, const std::string& reason) override {
        auto frame = FrameParser::encodeClose(status, reason);
        int sent = socket_.send(frame.data(), frame.size());

        connected_ = false;
        socket_.close();

        return sent == static_cast<int>(frame.size());
    }

    bool isConnected() const override {
        return connected_;
    }

    std::string getUrl() const override {
        return url_;
    }

    void update() override {
        if (!connected_) return;

        char buffer[4096];
        int received = socket_.receive(buffer, sizeof(buffer));

        if (received <= 0) {
            if (callbacks_.onClose) {
                callbacks_.onClose(CloseStatus::AbnormalClose, "");
            }
            connected_ = false;
            socket_.close();
            return;
        }

        receiveBuffer_.insert(receiveBuffer_.end(),
            reinterpret_cast<uint8_t*>(buffer),
            reinterpret_cast<uint8_t*>(buffer) + received);

        FrameParser parser;
        FrameParser::Frame frame;

        while (true) {
            int parsed = parser.parse(receiveBuffer_.data(),
                                     receiveBuffer_.size(), frame);
            if (parsed <= 0) break;

            receiveBuffer_.erase(receiveBuffer_.begin(),
                                receiveBuffer_.begin() + parsed);

            handleMessage(frame);
        }
    }

private:
    void handleMessage(const FrameParser::Frame& frame) {
        switch (frame.opcode) {
            case OpCode::Text:
            case OpCode::Binary:
                if (callbacks_.onMessage) {
                    Message msg(frame.opcode, frame.payload, true);
                    callbacks_.onMessage(msg);
                }
                break;

            case OpCode::Close:
                if (callbacks_.onClose) {
                    callbacks_.onClose(CloseStatus::Normal, "");
                }
                connected_ = false;
                socket_.close();
                break;

            case OpCode::Ping:
                {
                    auto pongFrame = FrameParser::encodePong(frame.payload);
                    socket_.send(pongFrame.data(), pongFrame.size());
                }
                break;

            default:
                break;
        }
    }

    SimpleSocket socket_;
    std::atomic<bool> connected_;
    std::string url_;
    Callbacks callbacks_;
    Config config_;
    std::vector<uint8_t> receiveBuffer_;
};

//==============================================================================
// 工厂函数
//==============================================================================

std::unique_ptr<Server> createServer() {
    return std::unique_ptr<Server>(new WebSocketServer());
}

std::unique_ptr<ClientConnection> createClient() {
    return std::unique_ptr<ClientConnection>(new WebSocketClientConnection());
}

} // namespace websocket
} // namespace net
} // namespace apollo
