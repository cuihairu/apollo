#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <future>
#include <mutex>
#include <condition_variable>

namespace apollo {
namespace net {

class ISession;
class MessageCodec;

/// RPC 消息类型
enum class RpcMsgType : uint8_t {
    Request = 1,
    Response = 2,
    Error = 3,
    OneWay = 4       // 单向调用，不需要响应
};

/// RPC 错误码
enum class RpcErrorCode : int32_t {
    Success = 0,
    Timeout = -1,
    NotFound = -2,
    InvalidRequest = -3,
    InternalError = -4,
    ServiceUnavailable = -5,
    DeserializeFailed = -6,
    SerializeFailed = -7
};

/// RPC 请求头（网络传输格式）
struct RpcHeader {
    uint32_t requestId;      // 请求 ID
    uint16_t serviceId;      // 服务 ID
    uint16_t methodId;       // 方法 ID
    RpcMsgType type;         // 消息类型
    int32_t errorCode;       // 错误码（仅响应）

    static constexpr size_t SIZE = 4 + 2 + 2 + 1 + 4;

    RpcHeader()
        : requestId(0), serviceId(0), methodId(0), type(RpcMsgType::Request), errorCode(0) {}

    bool encode(uint8_t* buffer) const {
        uint8_t* p = buffer;

        // requestId (big-endian)
        *p++ = (requestId >> 24) & 0xFF;
        *p++ = (requestId >> 16) & 0xFF;
        *p++ = (requestId >> 8) & 0xFF;
        *p++ = requestId & 0xFF;

        // serviceId
        *p++ = (serviceId >> 8) & 0xFF;
        *p++ = serviceId & 0xFF;

        // methodId
        *p++ = (methodId >> 8) & 0xFF;
        *p++ = methodId & 0xFF;

        // type
        *p++ = static_cast<uint8_t>(type);

        // errorCode
        *p++ = (errorCode >> 24) & 0xFF;
        *p++ = (errorCode >> 16) & 0xFF;
        *p++ = (errorCode >> 8) & 0xFF;
        *p++ = errorCode & 0xFF;

        return true;
    }

    bool decode(const uint8_t* buffer) {
        const uint8_t* p = buffer;

        requestId = (static_cast<uint32_t>(p[0]) << 24) |
                     (static_cast<uint32_t>(p[1]) << 16) |
                     (static_cast<uint32_t>(p[2]) << 8) |
                     static_cast<uint32_t>(p[3]);
        p += 4;

        serviceId = (static_cast<uint16_t>(p[0]) << 8) |
                    static_cast<uint16_t>(p[1]);
        p += 2;

        methodId = (static_cast<uint16_t>(p[0]) << 8) |
                   static_cast<uint16_t>(p[1]);
        p += 2;

        type = static_cast<RpcMsgType>(p[0]);
        p += 1;

        errorCode = (static_cast<int32_t>(p[0]) << 24) |
                    (static_cast<int32_t>(p[1]) << 16) |
                    (static_cast<int32_t>(p[2]) << 8) |
                    static_cast<int32_t>(p[3]);

        return true;
    }
};

/// RPC 方法回调（原始数据版本）
using RpcRawCallback = std::function<void(const uint8_t* data, size_t length)>;

/// RPC 方法处理函数
using RpcMethodHandler = std::function<void(const uint8_t* requestData, size_t requestLength,
                                           std::vector<uint8_t>& responseData)>;

/// RPC 响应等待器
class RpcResponseWaiter {
public:
    RpcResponseWaiter();

    /// 设置响应
    void setResponse(const std::vector<uint8_t>& data, int32_t errorCode);

    /// 等待响应
    bool wait(std::vector<uint8_t>& data, int32_t& errorCode, int timeoutMs);

    /// 取消等待
    void cancel();

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    bool ready_;
    std::vector<uint8_t> data_;
    int32_t errorCode_;
};

/// RPC 服务端
class RpcServer {
public:
    RpcServer();
    ~RpcServer() = default;

    /// 注册服务方法
    void registerMethod(uint16_t serviceId, uint16_t methodId, RpcMethodHandler handler);

    /// 取消注册
    void unregisterMethod(uint16_t serviceId, uint16_t methodId);

    /// 处理 RPC 请求
    bool handleRequest(const RpcHeader& header, const uint8_t* data, size_t length,
                       std::vector<uint8_t>& responseData, int32_t& errorCode);

    /// 检查方法是否存在
    bool hasMethod(uint16_t serviceId, uint16_t methodId) const;

private:
    // 服务方法键
    struct MethodKey {
        uint16_t serviceId;
        uint16_t methodId;

        bool operator==(const MethodKey& other) const {
            return serviceId == other.serviceId && methodId == other.methodId;
        }
    };

    struct MethodKeyHash {
        size_t operator()(const MethodKey& key) const {
            return (static_cast<size_t>(key.serviceId) << 16) | key.methodId;
        }
    };

    std::unordered_map<MethodKey, RpcMethodHandler, MethodKeyHash> handlers_;
    mutable std::mutex mutex_;
};

/// RPC 客户端
class RpcClient {
public:
    RpcClient();
    ~RpcClient();

    /// 调用 RPC 方法（同步）
    bool call(ISession* session, uint16_t serviceId, uint16_t methodId,
              const uint8_t* requestData, size_t requestLength,
              std::vector<uint8_t>& responseData, int32_t& errorCode,
              int timeoutMs = 5000);

    /// 调用 RPC 方法（异步）
    using AsyncCallback = std::function<void(const std::vector<uint8_t>& data, int32_t errorCode)>;

    void asyncCall(ISession* session, uint16_t serviceId, uint16_t methodId,
                   const uint8_t* requestData, size_t requestLength,
                   AsyncCallback callback);

    /// 单向调用（不等待响应）
    bool oneWayCall(ISession* session, uint16_t serviceId, uint16_t methodId,
                    const uint8_t* requestData, size_t requestLength);

    /// 处理 RPC 响应
    void handleResponse(const RpcHeader& header, const uint8_t* data, size_t length);

    /// 清理超时的请求
    void cleanupTimeouts();

    /// 设置默认超时时间
    void setDefaultTimeout(int timeoutMs) { defaultTimeoutMs_ = timeoutMs; }

private:
    uint32_t nextRequestId_;
    int defaultTimeoutMs_;
    std::unordered_map<uint32_t, std::unique_ptr<RpcResponseWaiter>> pendingRequests_;
    std::unordered_map<uint32_t, AsyncCallback> asyncCallbacks_;
    std::mutex mutex_;
};

/// RPC 管理器（服务端+客户端集成）
class RpcManager {
public:
    static RpcManager& instance();

    /// 初始化
    void initialize();

    /// 获取服务端
    RpcServer& getServer() { return server_; }

    /// 获取客户端
    RpcClient& getClient() { return client_; }

    /// 注册服务方法（便捷方法）
    void registerMethod(uint16_t serviceId, uint16_t methodId, RpcMethodHandler handler) {
        server_.registerMethod(serviceId, methodId, handler);
    }

    /// 同步调用（便捷方法）
    bool call(ISession* session, uint16_t serviceId, uint16_t methodId,
              const uint8_t* requestData, size_t requestLength,
              std::vector<uint8_t>& responseData, int32_t& errorCode,
              int timeoutMs = 5000) {
        return client_.call(session, serviceId, methodId, requestData, requestLength,
                           responseData, errorCode, timeoutMs);
    }

    /// 处理接收到的 RPC 消息
    void handleMessage(const uint8_t* data, size_t length, ISession* session);

private:
    RpcManager() = default;
    ~RpcManager() = default;

    RpcServer server_;
    RpcClient client_;
};

/// RPC 辅助函数
namespace RpcHelper {

/// 编码 RPC 请求
std::vector<uint8_t> encodeRequest(const RpcHeader& header, const uint8_t* data, size_t length);

/// 编码 RPC 响应
std::vector<uint8_t> encodeResponse(const RpcHeader& header, const uint8_t* data, size_t length);

/// 解析 RPC 消息头
bool parseHeader(const uint8_t* data, size_t length, RpcHeader& header);

} // namespace RpcHelper

} // namespace net
} // namespace apollo
