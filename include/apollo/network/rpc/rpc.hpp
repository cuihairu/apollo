#pragma once

#include "apollo/network/messaging/protobuf_message.hpp"
#include "apollo/network/transport/socket.hpp"
#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>
#include <unordered_map>
#include <memory>
#include <functional>
#include <future>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace apollo::net {

/// RPC消息类型
enum class RpcMessageType {
    REQUEST = 1,
    RESPONSE = 2,
    ERROR = 3
};

/// RPC请求头
struct RpcRequestHeader {
    uint64_t id;              // 请求ID
    uint32_t serviceId;       // 服务ID
    uint32_t methodId;        // 方法ID

    // 序列化到Buffer
    bool Serialize(Buffer& buffer) const {
        buffer.Write(&id, sizeof(id));
        buffer.Write(&serviceId, sizeof(serviceId));
        buffer.Write(&methodId, sizeof(methodId));
        return true;
    }

    // 从Buffer反序列化
    bool Deserialize(Buffer& buffer) {
        if (buffer.GetReadableSize() < sizeof(id) + sizeof(serviceId) + sizeof(methodId)) {
            return false;
        }
        buffer.Read(&id, sizeof(id));
        buffer.Read(&serviceId, sizeof(serviceId));
        buffer.Read(&methodId, sizeof(methodId));
        return true;
    }
};

/// RPC响应头
struct RpcResponseHeader {
    uint64_t id;              // 请求ID
    int32_t errorCode;        // 错误码（0表示成功）

    // 序列化到Buffer
    bool Serialize(Buffer& buffer) const {
        buffer.Write(&id, sizeof(id));
        buffer.Write(&errorCode, sizeof(errorCode));
        return true;
    }

    // 从Buffer反序列化
    bool Deserialize(Buffer& buffer) {
        if (buffer.GetReadableSize() < sizeof(id) + sizeof(errorCode)) {
            return false;
        }
        buffer.Read(&id, sizeof(id));
        buffer.Read(&errorCode, sizeof(errorCode));
        return true;
    }
};

/// RPC控制器
class RpcController : public google::protobuf::RpcController {
public:
    RpcController() : failed_(false), errorCode_(0) {}

    // RpcController implementation
    void Reset() override {
        failed_ = false;
        errorCode_ = 0;
        errorText_.clear();
    }

    bool Failed() const override {
        return failed_;
    }

    std::string ErrorText() const override {
        return errorText_;
    }

    void StartCancel() override {
        // TODO: 实现取消功能
    }

    void SetFailed(const std::string& reason) override {
        failed_ = true;
        errorText_ = reason;
    }

    bool IsCanceled() const override {
        // TODO: 实现取消功能
        return false;
    }

    void NotifyOnCancel([[maybe_unused]] google::protobuf::Closure* callback) override {
        // TODO: 实现取消功能
    }

    // 自定义方法
    void SetErrorCode(int32_t code) { errorCode_ = code; }
    int32_t GetErrorCode() const { return errorCode_; }

private:
    bool failed_;
    int32_t errorCode_;
    std::string errorText_;
};

/// RPC请求消息
template<typename Request>
class RpcRequestMessage : public ProtobufMessageWrapper<Request> {
public:
    using Ptr = std::shared_ptr<RpcRequestMessage<Request>>;

    RpcRequestMessage() : requestId_(0), serviceId_(0), methodId_(0) {}

    RpcRequestMessage(uint64_t requestId, uint32_t serviceId, uint32_t methodId,
                      const Request& request)
        : requestId_(requestId), serviceId_(serviceId), methodId_(methodId),
          ProtobufMessageWrapper<Request>(request) {}

    void SetRpcHeader(uint64_t requestId, uint32_t serviceId, uint32_t methodId) {
        requestId_ = requestId;
        serviceId_ = serviceId;
        methodId_ = methodId;
    }

    const RpcRequestHeader& GetRpcHeader() const {
        header_.id = requestId_;
        header_.serviceId = serviceId_;
        header_.methodId = methodId_;
        return header_;
    }

private:
    uint64_t requestId_;
    uint32_t serviceId_;
    uint32_t methodId_;
    mutable RpcRequestHeader header_;
};

/// RPC响应消息
template<typename Response>
class RpcResponseMessage : public ProtobufMessageWrapper<Response> {
public:
    using Ptr = std::shared_ptr<RpcResponseMessage<Response>>;

    RpcResponseMessage() : requestId_(0), errorCode_(0) {}

    RpcResponseMessage(uint64_t requestId, int32_t errorCode,
                       const Response& response)
        : requestId_(requestId), errorCode_(errorCode),
          ProtobufMessageWrapper<Response>(response) {}

    void SetRpcHeader(uint64_t requestId, int32_t errorCode) {
        requestId_ = requestId;
        errorCode_ = errorCode;
    }

    const RpcResponseHeader& GetRpcHeader() const {
        header_.id = requestId_;
        header_.errorCode = errorCode_;
        return header_;
    }

private:
    uint64_t requestId_;
    int32_t errorCode_;
    mutable RpcResponseHeader header_;
};

/// RPC客户端
class RpcClient {
public:
    explicit RpcClient(const std::string& serverAddr, int serverPort);
    ~RpcClient();

    /// 连接到服务器
    bool Connect();

    /// 断开连接
    void Disconnect();

    /// 调用RPC方法（同步）
    template<typename Request, typename Response>
    bool Call(uint32_t serviceId, uint32_t methodId,
              const Request& request, Response& response,
              int timeoutMs = 5000) {

        uint64_t requestId = GenerateRequestId();

        // 创建请求
        auto reqMsg = std::make_shared<RpcRequestMessage<Request>>(
            requestId, serviceId, methodId, request);

        // 创建promise/future用于等待响应
        std::promise<RpcResponseMessage<Response>> promise;
        auto future = promise.get_future();

        // 注册回调
        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            callbacks_[requestId] = [&promise](RpcResponseMessage<Response>&& resp) {
                promise.set_value(std::move(resp));
            };
        }

        // 发送请求
        if (!SendMessage(*reqMsg)) {
            RemoveCallback(requestId);
            return false;
        }

        // 等待响应
        if (future.wait_for(std::chrono::milliseconds(timeoutMs)) == std::future_status::timeout) {
            RemoveCallback(requestId);
            return false;
        }

        auto respMsg = future.get();
        if (respMsg.GetRpcHeader().errorCode != 0) {
            return false;
        }

        response = *respMsg.GetTypedMessage();
        return true;
    }

    /// 调用RPC方法（异步）
    template<typename Request, typename Response>
    void AsyncCall(uint32_t serviceId, uint32_t methodId,
                   const Request& request,
                   std::function<void(const Response&, bool)> callback) {

        uint64_t requestId = GenerateRequestId();

        // 创建请求
        auto reqMsg = std::make_shared<RpcRequestMessage<Request>>(
            requestId, serviceId, methodId, request);

        // 注册回调
        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            callbacks_[requestId] = [callback](RpcResponseMessage<Response>&& resp) {
                bool success = resp.GetRpcHeader().errorCode == 0;
                if (success) {
                    callback(*resp.GetTypedMessage(), true);
                } else {
                    Response emptyResponse;
                    callback(emptyResponse, false);
                }
            };
        }

        // 发送请求
        SendMessage(*reqMsg);
    }

private:
    uint64_t GenerateRequestId() {
        return ++nextRequestId_;
    }

    bool SendMessage(const Message& msg);
    void RemoveCallback(uint64_t requestId);

    std::string serverAddr_;
    int serverPort_;
    std::unique_ptr<Socket> socket_;
    std::atomic<uint64_t> nextRequestId_{0};
    std::unordered_map<uint64_t, std::function<void(void)>> callbacks_;
    std::mutex callbackMutex_;
    std::thread receiveThread_;
    std::atomic<bool> running_{false};
};

/// RPC服务端
class RpcServer {
public:
    explicit RpcServer(int port);
    ~RpcServer();

    /// 注册服务
    void RegisterService(std::shared_ptr<google::protobuf::Service> service);

    /// 启动服务器
    bool Start();

    /// 停止服务器
    void Stop();

    /// 处理单个连接
    void HandleConnection(std::unique_ptr<Socket> client);

private:
    int port_;
    std::unique_ptr<Socket> serverSocket_;
    std::unordered_map<uint32_t, std::shared_ptr<google::protobuf::Service>> services_;
    std::unordered_map<uint32_t, std::string> serviceNames_;
    std::atomic<bool> running_{false};
    std::thread acceptThread_;
    std::vector<std::thread> handlerThreads_;

    void AcceptLoop();
};

/// RPC服务管理器
class RpcServiceManager {
public:
    static RpcServiceManager& Instance() {
        static RpcServiceManager instance;
        return instance;
    }

    /// 注册服务到管理器
    void RegisterService(uint32_t serviceId,
                        std::shared_ptr<google::protobuf::Service> service);

    /// 获取服务
    std::shared_ptr<google::protobuf::Service> GetService(uint32_t serviceId);

    /// 获取服务ID
    uint32_t GetServiceId(const std::string& serviceName) const;

    /// 获取方法ID
    uint32_t GetMethodId(const std::string& serviceName,
                         const std::string& methodName) const;

private:
    struct PairHash {
        std::size_t operator()(const std::pair<std::string, std::string>& p) const {
            auto h1 = std::hash<std::string>{}(p.first);
            auto h2 = std::hash<std::string>{}(p.second);
            return h1 ^ (h2 << 1);
        }
    };

    std::unordered_map<uint32_t, std::shared_ptr<google::protobuf::Service>> services_;
    std::unordered_map<std::string, uint32_t> serviceIds_;
    std::unordered_map<std::pair<std::string, std::string>, uint32_t,
                       PairHash> methodIds_;
};

/// 服务代理生成器（用于客户端）
template<typename ServiceType>
class ServiceProxy {
public:
    ServiceProxy(RpcClient* client, uint32_t serviceId)
        : client_(client), serviceId_(serviceId) {}

    /// 获取服务代理
    std::shared_ptr<typename ServiceType::Stub> GetStub() {
        return std::make_shared<typename ServiceType::Stub>(
            client_, serviceId_, this
        );
    }

private:
    RpcClient* client_;
    uint32_t serviceId_;
};

/// 服务存根基类（用于服务端）
class RpcServiceStub {
public:
    virtual ~RpcServiceStub() = default;
    virtual void CallMethod(const RpcRequestHeader& header,
                           const google::protobuf::Message& request,
                           RpcResponseHeader& responseHeader,
                           google::protobuf::Message& response) = 0;
};

/// 便捷宏定义
#define REGISTER_RPC_SERVICE(ServiceClass, ServiceId) \
    RpcServiceManager::Instance().RegisterService(ServiceId, \
        std::make_shared<ServiceClass>())

#define CREATE_RPC_CLIENT(ServerAddr, Port) \
    std::make_unique<RpcClient>(ServerAddr, Port)

#define CREATE_RPC_SERVER(Port) \
    std::make_unique<RpcServer>(Port)

}  // namespace apollo::net
