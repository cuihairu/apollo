#pragma once

#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <cstdint>

namespace apollo {
namespace net {
namespace websocket {

//==============================================================================
// WebSocket 状态码
//==============================================================================

enum class OpCode : uint8_t {
    Continuation = 0x0,   // 连续帧
    Text = 0x1,           // 文本帧
    Binary = 0x2,         // 二进制帧
    Close = 0x8,          // 关闭连接
    Ping = 0x9,           // Ping
    Pong = 0xA            // Pong
};

//==============================================================================
// WebSocket 关闭状态
//==============================================================================

enum class CloseStatus : uint16_t {
    Normal = 1000,                    // 正常关闭
    GoingAway = 1001,                 // 端点离开
    ProtocolError = 1002,             // 协议错误
    UnsupportedData = 1003,           // 不支持的数据类型
    NoStatus = 1005,                  // 无状态码 (保留)
    AbnormalClose = 1006,             // 异常关闭 (保留)
    InvalidFramePayload = 1007,       // 无效帧数据
    PolicyViolation = 1008,           // 违反策略
    MessageTooBig = 1009,             // 消息过大
    MandatoryExtension = 1010,        // 需要扩展
    InternalError = 1011,             // 内部错误
    TLSHandshakeFailed = 1015         // TLS 握手失败
};

//==============================================================================
// WebSocket 消息
//==============================================================================

struct Message {
    OpCode opcode;           // 操作码
    std::vector<uint8_t> data;  // 数据
    bool isFinal;            // 是否是最后一帧

    Message() : opcode(OpCode::Text), isFinal(true) {}

    Message(OpCode op, std::vector<uint8_t> d, bool final = true)
        : opcode(op), data(std::move(d)), isFinal(final) {}

    // 便捷方法
    bool isText() const { return opcode == OpCode::Text; }
    bool isBinary() const { return opcode == OpCode::Binary; }
    bool isControl() const {
        return opcode == OpCode::Close || opcode == OpCode::Ping || opcode == OpCode::Pong;
    }

    // 获取文本内容
    std::string getText() const {
        if (isText() && !data.empty()) {
            return std::string(data.begin(), data.end());
        }
        return "";
    }

    // 创建文本消息
    static Message text(const std::string& text) {
        std::vector<uint8_t> data(text.begin(), text.end());
        return Message(OpCode::Text, std::move(data));
    }

    // 创建二进制消息
    static Message binary(const std::vector<uint8_t>& data) {
        return Message(OpCode::Binary, data);
    }

    // 创建二进制消息 (从指针)
    static Message binary(const uint8_t* data, size_t size) {
        return Message(OpCode::Binary, std::vector<uint8_t>(data, data + size));
    }

    // 创建关闭消息
    static Message close(CloseStatus status = CloseStatus::Normal) {
        uint16_t code = static_cast<uint16_t>(status);
        std::vector<uint8_t> data;
        data.push_back((code >> 8) & 0xFF);
        data.push_back(code & 0xFF);
        return Message(OpCode::Close, std::move(data));
    }

    // 创建 Ping 消息
    static Message ping() {
        return Message(OpCode::Ping, {});
    }

    // 创建 Pong 消息
    static Message pong() {
        return Message(OpCode::Pong, {});
    }
};

//==============================================================================
// WebSocket 配置
//==============================================================================

struct Config {
    uint32_t maxMessageSize = 16 * 1024 * 1024;  // 最大消息大小 (16MB)
    uint32_t maxFrameSize = 64 * 1024;           // 最大帧大小 (64KB)
    uint32_t handshakeTimeoutMs = 5000;          // 握手超时
    bool enableCompression = false;              // 是否启用压缩
    bool autoPing = false;                       // 是否自动 Ping
    uint32_t pingIntervalMs = 30000;             // Ping 间隔 (30秒)
    uint32_t pongTimeoutMs = 10000;              // Pong 超时
};

//==============================================================================
// WebSocket 事件回调
//==============================================================================

struct Callbacks {
    std::function<void()> onOpen;                           // 连接建立
    std::function<void(const Message&)> onMessage;          // 收到消息
    std::function<void(CloseStatus, const std::string&)> onClose;  // 连接关闭
    std::function<void(const std::string&)> onError;        // 发生错误

    Callbacks() = default;
};

//==============================================================================
// WebSocket 握手
//==============================================================================

class Handshake {
public:
    // 生成服务器握手响应
    static std::string generateServerResponse(
        const std::string& acceptKey,
        const std::string& protocol = "");

    // 生成客户端握手请求
    static std::string generateClientRequest(
        const std::string& uri,
        const std::string& host,
        const std::string& origin = "",
        const std::vector<std::string>& protocols = {});

    // 验证 Sec-WebSocket-Accept
    static bool validateAccept(
        const std::string& acceptKey,
        const std::string& expectedKey);

    // 计算 Sec-WebSocket-Accept
    static std::string computeAcceptKey(const std::string& clientKey);

    // 解析握手请求
    static bool parseClientRequest(
        const std::string& request,
        std::string& uri,
        std::string& host,
        std::string& clientKey,
        std::string& protocol,
        std::string& version);

    // 从原始数据中提取 WebSocket Key
    static std::string extractWebSocketKey(const std::string& handshakeData);
};

//==============================================================================
// WebSocket 帧解析器
//==============================================================================

class FrameParser {
public:
    struct Frame {
        bool final;              // 是否是最后一帧
        bool masked;             // 是否掩码
        OpCode opcode;           // 操作码
        std::vector<uint8_t> payload;  // 载荷数据
    };

    FrameParser();
    ~FrameParser();

    // 解析帧
    // 返回: >0 表示解析成功，返回帧大小
    //       0 表示需要更多数据
    //       -1 表示解析错误
    int parse(const uint8_t* data, size_t size, Frame& frame);

    // 编码帧
    static std::vector<uint8_t> encode(
        OpCode opcode,
        const uint8_t* payload,
        size_t payloadSize,
        bool mask = true,         // 客户端发送需要掩码，服务器不需要
        bool final = true);

    // 编码文本帧
    static std::vector<uint8_t> encodeText(const std::string& text, bool mask = false);

    // 编码二进制帧
    static std::vector<uint8_t> encodeBinary(const std::vector<uint8_t>& data, bool mask = false);

    // 编码关闭帧
    static std::vector<uint8_t> encodeClose(CloseStatus status, const std::string& reason = "");

    // 编码 Ping 帧
    static std::vector<uint8_t> encodePing(const std::vector<uint8_t>& data = {});

    // 编码 Pong 帧
    static std::vector<uint8_t> encodePong(const std::vector<uint8_t>& data = {});

private:
    class Impl;
    Impl* impl_;
};

//==============================================================================
// WebSocket 服务器连接
//==============================================================================

class ServerConnection {
public:
    virtual ~ServerConnection() = default;

    // 发送消息
    virtual bool send(const Message& message) = 0;

    // 发送文本
    virtual bool sendText(const std::string& text) = 0;

    // 发送二进制
    virtual bool sendBinary(const std::vector<uint8_t>& data) = 0;

    // 发送关闭
    virtual bool sendClose(CloseStatus status = CloseStatus::Normal,
                          const std::string& reason = "") = 0;

    // 发送 Ping
    virtual bool sendPing(const std::vector<uint8_t>& data = {}) = 0;

    // 获取远程地址
    virtual std::string getRemoteAddress() const = 0;

    // 获取连接ID
    virtual uint64_t getConnectionId() const = 0;

    // 是否已连接
    virtual bool isConnected() const = 0;

    // 关闭连接
    virtual void close() = 0;
};

//==============================================================================
// WebSocket 客户端连接
//==============================================================================

class ClientConnection {
public:
    virtual ~ClientConnection() = default;

    // 连接到服务器
    virtual bool connect(const std::string& url,
                        const Callbacks& callbacks,
                        const Config& config = {}) = 0;

    // 断开连接
    virtual void disconnect() = 0;

    // 发送消息
    virtual bool send(const Message& message) = 0;

    // 发送文本
    virtual bool sendText(const std::string& text) = 0;

    // 发送二进制
    virtual bool sendBinary(const std::vector<uint8_t>& data) = 0;

    // 关闭连接
    virtual bool close(CloseStatus status = CloseStatus::Normal,
                      const std::string& reason = "") = 0;

    // 获取连接状态
    virtual bool isConnected() const = 0;

    // 获取连接URL
    virtual std::string getUrl() const = 0;

    // 更新 (处理接收数据)
    virtual void update() = 0;
};

//==============================================================================
// WebSocket 服务器
//==============================================================================

class Server {
public:
    virtual ~Server() = default;

    // 启动服务器
    virtual bool start(const std::string& host, uint16_t port,
                      const Callbacks& callbacks,
                      const Config& config = {}) = 0;

    // 停止服务器
    virtual void stop() = 0;

    // 是否正在运行
    virtual bool isRunning() const = 0;

    // 广播消息到所有连接
    virtual void broadcast(const Message& message) = 0;

    // 广播文本到所有连接
    virtual void broadcastText(const std::string& text) = 0;

    // 获取连接数
    virtual size_t getConnectionCount() const = 0;

    // 获取连接
    virtual ServerConnection* getConnection(uint64_t connId) const = 0;

    // 设置路由 (基于路径)
    using RouteHandler = std::function<bool(ServerConnection*, const std::string& path)>;
    virtual void setRoute(const std::string& path, RouteHandler handler) = 0;
};

//==============================================================================
// 工厂函数
//==============================================================================

// 创建 WebSocket 服务器
std::unique_ptr<Server> createServer();

// 创建 WebSocket 客户端
std::unique_ptr<ClientConnection> createClient();

//==============================================================================
// 便捷函数
//==============================================================================

// 解析 WebSocket URL
struct UrlInfo {
    bool ssl;
    std::string host;
    uint16_t port;
    std::string path;
    std::string query;
};

UrlInfo parseUrl(const std::string& url);

// 生成 Sec-WebSocket-Key
std::string generateWebSocketKey();

// Base64 编码
std::string base64Encode(const std::vector<uint8_t>& data);

// Base64 解码
std::vector<uint8_t> base64Decode(const std::string& data);

// SHA-1 哈希
std::vector<uint8_t> sha1(const std::string& data);

} // namespace websocket
} // namespace net
} // namespace apollo
