#include "apollo/network/rpc.hpp"
#include <chrono>

namespace apollo::net {

// RpcClient 实现
RpcClient::RpcClient(const std::string& serverAddr, int serverPort)
    : serverAddr_(serverAddr), serverPort_(serverPort) {
}

RpcClient::~RpcClient() {
    Disconnect();
}

bool RpcClient::Connect() {
    socket_ = std::make_unique<Socket>();

    if (!socket_->Create()) {
        return false;
    }

    if (!socket_->Connect(serverAddr_, serverPort_)) {
        return false;
    }

    running_ = true;
    receiveThread_ = std::thread([this]() {
        while (running_) {
            // 接收响应消息
            Buffer buffer;
            // TODO: 实现完整的消息接收逻辑
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    return true;
}

void RpcClient::Disconnect() {
    running_ = false;
    if (socket_) {
        socket_->Close();
    }
    if (receiveThread_.joinable()) {
        receiveThread_.join();
    }
}

bool RpcClient::SendMessage(const Message& msg) {
    if (!socket_ || !socket_->IsConnected()) {
        return false;
    }

    Buffer buffer;
    if (!msg.Serialize(buffer)) {
        return false;
    }

    return socket_->Send(buffer.Data(), buffer.Size()) > 0;
}

void RpcClient::RemoveCallback(uint64_t requestId) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    callbacks_.erase(requestId);
}

// RpcServer 实现
RpcServer::RpcServer(int port) : port_(port) {
}

RpcServer::~RpcServer() {
    Stop();
}

void RpcServer::RegisterService(std::shared_ptr<google::protobuf::Service> service) {
    if (!service) {
        return;
    }

    uint32_t serviceId = GetServiceId(service->GetDescriptor()->full_name());
    services_[serviceId] = service;
    serviceNames_[serviceId] = service->GetDescriptor()->full_name();
}

bool RpcServer::Start() {
    serverSocket_ = std::make_unique<Socket>();

    if (!serverSocket_->Create()) {
        return false;
    }

    if (!serverSocket_->Bind("", port_)) {
        return false;
    }

    if (!serverSocket_->Listen(128)) {
        return false;
    }

    running_ = true;
    acceptThread_ = std::thread([this]() {
        AcceptLoop();
    });

    return true;
}

void RpcServer::Stop() {
    running_ = false;
    if (serverSocket_) {
        serverSocket_->Close();
    }
    if (acceptThread_.joinable()) {
        acceptThread_.join();
    }
    for (auto& thread : handlerThreads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void RpcServer::AcceptLoop() {
    while (running_) {
        auto client = serverSocket_->Accept();
        if (client) {
            // 为每个连接创建处理线程
            handlerThreads_.emplace_back([this, client = std::move(client)]() {
                HandleConnection(std::move(client));
            });
        }
    }
}

void RpcServer::HandleConnection(std::unique_ptr<Socket> client) {
    if (!client) {
        return;
    }

    while (running_) {
        // 接收请求
        Buffer buffer;
        // TODO: 实现完整的请求接收和处理逻辑

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// RpcServiceManager 实现
void RpcServiceManager::RegisterService(uint32_t serviceId,
                                       std::shared_ptr<google::protobuf::Service> service) {
    if (!service) {
        return;
    }

    services_[serviceId] = service;
    serviceIds_[service->GetDescriptor()->full_name()] = serviceId;

    // 注册方法ID
    const auto* descriptor = service->GetDescriptor();
    for (int i = 0; i < descriptor->method_count(); ++i) {
        const auto* method = descriptor->method(i);
        methodIds_[{descriptor->full_name(), method->name()]] = i;
    }
}

std::shared_ptr<google::protobuf::Service> RpcServiceManager::GetService(uint32_t serviceId) {
    auto it = services_.find(serviceId);
    return (it != services_.end()) ? it->second : nullptr;
}

uint32_t RpcServiceManager::GetServiceId(const std::string& serviceName) const {
    auto it = serviceIds_.find(serviceName);
    return (it != serviceIds_.end()) ? it->second : 0;
}

uint32_t RpcServiceManager::GetMethodId(const std::string& serviceName,
                                        const std::string& methodName) const {
    auto it = methodIds_.find({serviceName, methodName});
    return (it != methodIds_.end()) ? it->second : 0;
}

}  // namespace apollo::net