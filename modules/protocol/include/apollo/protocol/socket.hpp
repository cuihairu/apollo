#pragma once

#include "apollo/protocol/nng_wrapper.hpp"
#include "apollo/protocol/codec.hpp"
#include <string>
#include <functional>
#include <memory>
#include <unordered_map>
#include <mutex>

namespace apollo {
namespace protocol {

//==============================================================================
// Socket 基类
//==============================================================================

class ISocket {
public:
    virtual ~ISocket() = default;

    // 启动
    virtual void start() = 0;

    // 停止
    virtual void stop() = 0;

    // 是否运行中
    virtual bool isRunning() const = 0;

    // 获取地址
    virtual std::string getAddress() const = 0;

    // 获取 socket
    virtual nng_socket getSocket() const = 0;
};

//==============================================================================
// 通用 Socket 配置
//==============================================================================

struct SocketConfig {
    // 接收缓冲区大小
    int recvBufferSize = 1024 * 64;  // 64KB

    // 发送缓冲区大小
    int sendBufferSize = 1024 * 64;  // 64KB

    // 接收超时 (毫秒)
    int recvTimeoutMs = 5000;

    // 发送超时 (毫秒)
    int sendTimeoutMs = 5000;

    // 重连间隔 (毫秒)
    int reconnectIntervalMs = 1000;

    // 是否启用 Keep-Alive
    bool keepAlive = true;

    // Keep-Alive 空闲时间 (毫秒)
    int keepAliveIdleMs = 5000;

    // Keep-Alive 间隔 (毫秒)
    int keepAliveIntervalMs = 1000;
};

//==============================================================================
// REQ/REP Socket (请求/响应模式)
//==============================================================================

// 客户端 (REQ)
class ReqSocket : public ISocket {
public:
    // 响应回调
    using ResponseCallback = std::function<void(const std::vector<uint8_t>&)>;

    ReqSocket(const std::string& url, const SocketConfig& config = SocketConfig{});
    ~ReqSocket() override;

    // ISocket 接口
    void start() override;
    void stop() override;
    bool isRunning() const override { return running_; }
    std::string getAddress() const override { return url_; }
    nng_socket getSocket() const override { return socket_.get(); }

    // 发送请求（同步）
    template<typename Request>
    std::vector<uint8_t> sendRequest(const Request& req, SessionID sessionId = 0) {
        auto data = MessageCodec::encode(req, sessionId);
        return sendRequest(data);
    }

    std::vector<uint8_t> sendRequest(const std::vector<uint8_t>& data);

    // 发送请求（异步）
    template<typename Request>
    void sendRequestAsync(const Request& req, ResponseCallback cb, SessionID sessionId = 0) {
        auto data = MessageCodec::encode(req, sessionId);
        sendRequestAsync(data, std::move(cb));
    }

    void sendRequestAsync(const std::vector<uint8_t>& data, ResponseCallback cb);

private:
    std::string url_;
    SocketConfig config_;
    NngSocket socket_;
    std::unique_ptr<NngDialer> dialer_;
    std::atomic<bool> running_{false};
};

// 服务端 (REP)
class RepSocket : public ISocket {
public:
    // 请求处理器
    using RequestHandler = std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>;

    RepSocket(const std::string& url, const SocketConfig& config = SocketConfig{});
    ~RepSocket() override;

    // ISocket 接口
    void start() override;
    void stop() override;
    bool isRunning() const override { return running_; }
    std::string getAddress() const override { return url_; }
    nng_socket getSocket() const override { return socket_.get(); }

    // 设置请求处理器
    void setRequestHandler(RequestHandler handler);

private:
    void workerLoop();

    std::string url_;
    SocketConfig config_;
    NngSocket socket_;
    std::unique_ptr<NngListener> listener_;
    RequestHandler handler_;
    std::atomic<bool> running_{false};
    std::thread workerThread_;
};

//==============================================================================
// PUB/SUB Socket (发布/订阅模式)
//==============================================================================

// 发布端 (PUB)
class PubSocket : public ISocket {
public:
    PubSocket(const std::string& url, const SocketConfig& config = SocketConfig{});
    ~PubSocket() override;

    // ISocket 接口
    void start() override;
    void stop() override;
    bool isRunning() const override { return running_; }
    std::string getAddress() const override { return url_; }
    nng_socket getSocket() const override { return socket_.get(); }

    // 发布消息
    template<typename Message>
    void publish(const Message& msg, SessionID sessionId = 0) {
        auto data = MessageCodec::encode(msg, sessionId);
        publish(data);
    }

    void publish(const std::vector<uint8_t>& data);

private:
    std::string url_;
    SocketConfig config_;
    NngSocket socket_;
    std::unique_ptr<NngListener> listener_;
    std::atomic<bool> running_{false};
};

// 订阅端 (SUB)
class SubSocket : public ISocket {
public:
    // 消息回调
    using MessageCallback = std::function<void(const std::vector<uint8_t>&)>;

    SubSocket(const std::string& url, const SocketConfig& config = SocketConfig{});
    ~SubSocket() override;

    // ISocket 接口
    void start() override;
    void stop() override;
    bool isRunning() const override { return running_; }
    std::string getAddress() const override { return url_; }
    nng_socket getSocket() const override { return socket_.get(); }

    // 设置消息回调
    void setMessageCallback(MessageCallback callback);

    // 订阅主题 (空字符串表示订阅所有)
    void subscribe(const std::string& topic = "");

private:
    void workerLoop();

    std::string url_;
    SocketConfig config_;
    NngSocket socket_;
    std::unique_ptr<NngDialer> dialer_;
    MessageCallback callback_;
    std::atomic<bool> running_{false};
    std::thread workerThread_;
};

//==============================================================================
// PAIR Socket (双向通信模式)
//==============================================================================

class PairSocket : public ISocket {
public:
    // 消息回调
    using MessageCallback = std::function<void(const std::vector<uint8_t>&)>;

    PairSocket(const std::string& url, bool server, const SocketConfig& config = SocketConfig{});
    ~PairSocket() override;

    // ISocket 接口
    void start() override;
    void stop() override;
    bool isRunning() const override { return running_; }
    std::string getAddress() const override { return url_; }
    nng_socket getSocket() const override { return socket_.get(); }

    // 发送消息
    template<typename Message>
    void send(const Message& msg, SessionID sessionId = 0) {
        auto data = MessageCodec::encode(msg, sessionId);
        send(data);
    }

    void send(const std::vector<uint8_t>& data);

    // 设置消息回调
    void setMessageCallback(MessageCallback callback);

private:
    void workerLoop();

    std::string url_;
    bool server_;
    SocketConfig config_;
    NngSocket socket_;
    std::unique_ptr<NngListener> listener_;
    std::unique_ptr<NngDialer> dialer_;
    MessageCallback callback_;
    std::atomic<bool> running_{false};
    std::thread workerThread_;
};

//==============================================================================
// 简化的 RPC 客户端
//==============================================================================

class RpcClient {
public:
    RpcClient(const std::string& serverUrl, const SocketConfig& config = SocketConfig{});
    ~RpcClient();

    // 连接服务器
    void connect();

    // 断开连接
    void disconnect();

    // 发送请求（同步）
    template<typename Request, typename Response>
    Response call(const Request& req, SessionID sessionId = 0) {
        auto requestData = MessageCodec::encode(req, sessionId);
        auto responseData = client_.sendRequest(requestData);

        auto header = MessageCodec::parseHeader(responseData);
        if (static_cast<MessageType>(header.type) != Response::TYPE) {
            throw NngError(NNG_EPROTO);
        }

        // 跳过头部
        std::vector<uint8_t> bodyData(
            responseData.begin() + sizeof(MessageHeader),
            responseData.end()
        );
        return MessageCodec::decodeBody<Response>(bodyData);
    }

    // 发送请求（异步）
    template<typename Request>
    void callAsync(const Request& req, ReqSocket::ResponseCallback cb, SessionID sessionId = 0) {
        client_.sendRequestAsync(req, std::move(cb), sessionId);
    }

private:
    std::string serverUrl_;
    ReqSocket client_;
};

//==============================================================================
// 简化的 RPC 服务器
//==============================================================================

class RpcServer {
public:
    // 通用请求处理器
    template<typename Request, typename Response>
    using Handler = std::function<Response(const Request&, SessionID)>;

    RpcServer(const std::string& bindUrl, const SocketConfig& config = SocketConfig{});
    ~RpcServer();

    // 启动服务器
    void start();

    // 停止服务器
    void stop();

    // 注册处理器
    template<typename Request, typename Response>
    void registerHandler(Handler<Request, Response> handler) {
        handlers_[static_cast<uint16_t>(Request::TYPE)] =
            [handler = std::move(handler)](const std::vector<uint8_t>& data, SessionID sessionId) -> std::vector<uint8_t> {
                Request req = MessageCodec::decodeBody<Request>(data);
                Response resp = handler(req, sessionId);
                return MessageCodec::encode(resp, sessionId);
            };
    }

private:
    std::string bindUrl_;
    SocketConfig config_;
    std::unique_ptr<RepSocket> server_;
    std::unordered_map<uint16_t, std::function<std::vector<uint8_t>(const std::vector<uint8_t>&, SessionID)>> handlers_;
    std::mutex handlersMutex_;
};

} // namespace protocol
} // namespace apollo
