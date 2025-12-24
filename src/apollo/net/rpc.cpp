/**
 * @file rpc.cpp
 * @brief RPC 系统实现
 */

#include "apollo/net/rpc.h"
#include "apollo/net/session_manager.h"
#include "apollo/utils/time.h"
#include <cstring>

namespace apollo {
namespace net {

//==============================================================================
// RpcResponseWaiter 实现
//==============================================================================

RpcResponseWaiter::RpcResponseWaiter()
    : ready_(false), errorCode_(0) {
}

void RpcResponseWaiter::setResponse(const std::vector<uint8_t>& data, int32_t errorCode) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = data;
    errorCode_ = errorCode;
    ready_ = true;
    cv_.notify_all();
}

bool RpcResponseWaiter::wait(std::vector<uint8_t>& data, int32_t& errorCode, int timeoutMs) {
    std::unique_lock<std::mutex> lock(mutex_);

    if (!cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                     [this] { return ready_; })) {
        return false;  // 超时
    }

    data = data_;
    errorCode = errorCode_;
    return true;
}

void RpcResponseWaiter::cancel() {
    std::lock_guard<std::mutex> lock(mutex_);
    ready_ = true;
    errorCode_ = static_cast<int32_t>(RpcErrorCode::Timeout);
    cv_.notify_all();
}

//==============================================================================
// RpcServer 实现
//==============================================================================

RpcServer::RpcServer() {
}

void RpcServer::registerMethod(uint16_t serviceId, uint16_t methodId, RpcMethodHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);

    MethodKey key{serviceId, methodId};
    handlers_[key] = std::move(handler);
}

void RpcServer::unregisterMethod(uint16_t serviceId, uint16_t methodId) {
    std::lock_guard<std::mutex> lock(mutex_);

    MethodKey key{serviceId, methodId};
    handlers_.erase(key);
}

bool RpcServer::handleRequest(const RpcHeader& header, const uint8_t* data, size_t length,
                               std::vector<uint8_t>& responseData, int32_t& errorCode) {
    MethodKey key{header.serviceId, header.methodId};
    RpcMethodHandler handler;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = handlers_.find(key);
        if (it == handlers_.end()) {
            errorCode = static_cast<int32_t>(RpcErrorCode::NotFound);
            return false;
        }
        handler = it->second;
    }

    try {
        handler(data, length, responseData);
        errorCode = static_cast<int32_t>(RpcErrorCode::Success);
        return true;
    } catch (...) {
        errorCode = static_cast<int32_t>(RpcErrorCode::InternalError);
        return false;
    }
}

bool RpcServer::hasMethod(uint16_t serviceId, uint16_t methodId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    MethodKey key{serviceId, methodId};
    return handlers_.find(key) != handlers_.end();
}

//==============================================================================
// RpcClient 实现
//==============================================================================

RpcClient::RpcClient()
    : nextRequestId_(1)
    , defaultTimeoutMs_(5000) {
}

RpcClient::~RpcClient() {
    // 取消所有待处理请求
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& pair : pendingRequests_) {
        pair.second->cancel();
    }
    pendingRequests_.clear();
}

bool RpcClient::call(ISession* session, uint16_t serviceId, uint16_t methodId,
                      const uint8_t* requestData, size_t requestLength,
                      std::vector<uint8_t>& responseData, int32_t& errorCode,
                      int timeoutMs) {
    if (!session) {
        errorCode = static_cast<int32_t>(RpcErrorCode::InvalidRequest);
        return false;
    }

    // 生成请求 ID
    uint32_t requestId = nextRequestId_++;

    // 创建等待器
    auto waiter = std::make_unique<RpcResponseWaiter>();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingRequests_[requestId] = std::move(waiter);
    }

    // 构建请求头
    RpcHeader header;
    header.requestId = requestId;
    header.serviceId = serviceId;
    header.methodId = methodId;
    header.type = RpcMsgType::Request;
    header.errorCode = 0;

    // 编码并发送请求
    auto requestPacket = RpcHelper::encodeRequest(header, requestData, requestLength);
    if (!session->send(requestPacket)) {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingRequests_.erase(requestId);
        errorCode = static_cast<int32_t>(RpcErrorCode::InternalError);
        return false;
    }

    // 等待响应
    RpcResponseWaiter* waiterPtr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        waiterPtr = pendingRequests_[requestId].get();
    }

    bool success = waiterPtr->wait(responseData, errorCode,
                                   timeoutMs > 0 ? timeoutMs : defaultTimeoutMs_);

    // 清理
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingRequests_.erase(requestId);
    }

    return success && errorCode == 0;
}

void RpcClient::asyncCall(ISession* session, uint16_t serviceId, uint16_t methodId,
                          const uint8_t* requestData, size_t requestLength,
                          AsyncCallback callback) {
    if (!session) {
        std::vector<uint8_t> emptyData;
        callback(emptyData, static_cast<int32_t>(RpcErrorCode::InvalidRequest));
        return;
    }

    uint32_t requestId = nextRequestId_++;

    // 注册回调
    {
        std::lock_guard<std::mutex> lock(mutex_);
        asyncCallbacks_[requestId] = std::move(callback);
    }

    // 构建请求头
    RpcHeader header;
    header.requestId = requestId;
    header.serviceId = serviceId;
    header.methodId = methodId;
    header.type = RpcMsgType::Request;

    // 发送请求
    auto requestPacket = RpcHelper::encodeRequest(header, requestData, requestLength);
    session->send(requestPacket);
}

bool RpcClient::oneWayCall(ISession* session, uint16_t serviceId, uint16_t methodId,
                           const uint8_t* requestData, size_t requestLength) {
    if (!session) {
        return false;
    }

    RpcHeader header;
    header.requestId = 0;  // 单向调用不需要请求 ID
    header.serviceId = serviceId;
    header.methodId = methodId;
    header.type = RpcMsgType::OneWay;

    auto packet = RpcHelper::encodeRequest(header, requestData, requestLength);
    return session->send(packet);
}

void RpcClient::handleResponse(const RpcHeader& header, const uint8_t* data, size_t length) {
    // 查找同步请求的等待器
    std::unique_ptr<RpcResponseWaiter> waiter;
    AsyncCallback asyncCallback;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = pendingRequests_.find(header.requestId);
        if (it != pendingRequests_.end()) {
            waiter = std::move(it->second);
            pendingRequests_.erase(it);
        }

        auto asyncIt = asyncCallbacks_.find(header.requestId);
        if (asyncIt != asyncCallbacks_.end()) {
            asyncCallback = std::move(asyncIt->second);
            asyncCallbacks_.erase(asyncIt);
        }
    }

    // 设置响应或调用回调
    std::vector<uint8_t> dataVec;
    if (data && length > 0) {
        dataVec.assign(data, data + length);
    }

    if (waiter) {
        waiter->setResponse(dataVec, header.errorCode);
    }

    if (asyncCallback) {
        asyncCallback(dataVec, header.errorCode);
    }
}

void RpcClient::cleanupTimeouts() {
    std::lock_guard<std::mutex> lock(mutex_);

    // 简化处理：实际应该跟踪每个请求的时间
    // 这里暂不实现
}

//==============================================================================
// RpcManager 实现
//==============================================================================

RpcManager& RpcManager::instance() {
    static RpcManager instance;
    return instance;
}

void RpcManager::initialize() {
    // 初始化 RPC 系统
}

void RpcManager::handleMessage(const uint8_t* data, size_t length, ISession* session) {
    if (!data || length < RpcHeader::SIZE) {
        return;
    }

    // 解析消息头
    RpcHeader header;
    if (!RpcHelper::parseHeader(data, length, header)) {
        return;
    }

    const uint8_t* bodyData = data + RpcHeader::SIZE;
    size_t bodyLength = length - RpcHeader::SIZE;

    switch (header.type) {
        case RpcMsgType::Request:
        case RpcMsgType::OneWay: {
            // 处理服务端请求
            std::vector<uint8_t> responseData;
            int32_t errorCode = 0;

            server_.handleRequest(header, bodyData, bodyLength, responseData, errorCode);

            // 发送响应（单向调用除外）
            if (header.type == RpcMsgType::Request && session) {
                header.type = RpcMsgType::Response;
                header.errorCode = errorCode;

                auto responsePacket = RpcHelper::encodeResponse(header,
                    responseData.data(), responseData.size());
                session->send(responsePacket);
            }
            break;
        }

        case RpcMsgType::Response:
        case RpcMsgType::Error: {
            // 处理客户端响应
            client_.handleResponse(header, bodyData, bodyLength);
            break;
        }

        default:
            break;
    }
}

//==============================================================================
// RpcHelper 实现
//==============================================================================

namespace RpcHelper {

std::vector<uint8_t> encodeRequest(const RpcHeader& header, const uint8_t* data, size_t length) {
    std::vector<uint8_t> result(RpcHeader::SIZE + length);

    // 编码消息头
    header.encode(result.data());

    // 复制数据
    if (data && length > 0) {
        std::memcpy(result.data() + RpcHeader::SIZE, data, length);
    }

    return result;
}

std::vector<uint8_t> encodeResponse(const RpcHeader& header, const uint8_t* data, size_t length) {
    std::vector<uint8_t> result(RpcHeader::SIZE + length);

    // 编码消息头
    header.encode(result.data());

    // 复制数据
    if (data && length > 0) {
        std::memcpy(result.data() + RpcHeader::SIZE, data, length);
    }

    return result;
}

bool parseHeader(const uint8_t* data, size_t length, RpcHeader& header) {
    if (!data || length < RpcHeader::SIZE) {
        return false;
    }

    return header.decode(data);
}

} // namespace RpcHelper

} // namespace net
} // namespace apollo
