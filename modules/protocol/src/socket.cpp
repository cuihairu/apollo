#include "apollo/protocol/socket.hpp"
#include <thread>
#include <chrono>
#include <iostream>

namespace apollo {
namespace protocol {

//==============================================================================
// ReqSocket 实现
//==============================================================================

ReqSocket::ReqSocket(const std::string& url, const SocketConfig& config)
    : url_(url), config_(config) {
    int rv = nng_req0_open(&socket_.get());
    if (rv != 0) {
        throw NngError(rv);
    }
}

ReqSocket::~ReqSocket() {
    stop();
}

void ReqSocket::start() {
    if (running_) return;

    // 配置 socket
    socket_.set_int(NNG_OPT_RECVBUF, config_.recvBufferSize);
    socket_.set_int(NNG_OPT_SENDBUF, config_.sendBufferSize);
    socket_.set_ms(NNG_OPT_RECVTIMEO, config_.recvTimeoutMs);
    socket_.set_ms(NNG_OPT_SENDTIMEO, config_.sendTimeoutMs);
    socket_.set_bool(NNG_OPT_KEEPALIVE, config_.keepAlive);

    // 连接
    dialer_ = std::make_unique<NngDialer>(socket_.get(), url_);

    // 配置 dialer
    nng_dialer_set_ms(dialer_->get(), NNG_OPT_RECONNMINT, config_.reconnectIntervalMs);

    running_ = true;
}

void ReqSocket::stop() {
    running_ = false;
    dialer_.reset();
}

std::vector<uint8_t> ReqSocket::sendRequest(const std::vector<uint8_t>& data) {
    if (!running_) {
        throw NngError(NNG_ECLOSED);
    }

    // 发送
    socket_.send(data);

    // 接收响应
    size_t sz = 0;
    int rv = nng_recv(socket_.get(), nullptr, 0, nng_flag::NNG_FLAG_ALLOC);
    if (rv != 0) {
        throw NngError(rv);
    }

    // 获取消息大小
    nng_msg* msg;
    rv = nng_recvmsg(socket_.get(), &msg, 0);
    if (rv != 0) {
        throw NngError(rv);
    }

    sz = nng_msg_len(msg);
    std::vector<uint8_t> result(sz);
    std::memcpy(result.data(), nng_msg_body(msg), sz);
    nng_msg_free(msg);

    return result;
}

void ReqSocket::sendRequestAsync(const std::vector<uint8_t>& data, ResponseCallback cb) {
    // 异步发送需要 AIO，这里简化为同步 + 线程
    std::thread([this, data, cb]() {
        try {
            auto response = sendRequest(data);
            cb(response);
        } catch (const NngError& e) {
            // 错误处理
            std::vector<uint8_t> empty;
            cb(empty);
        }
    }).detach();
}

//==============================================================================
// RepSocket 实现
//==============================================================================

RepSocket::RepSocket(const std::string& url, const SocketConfig& config)
    : url_(url), config_(config) {
    int rv = nng_rep0_open(&socket_.get());
    if (rv != 0) {
        throw NngError(rv);
    }
}

RepSocket::~RepSocket() {
    stop();
}

void RepSocket::start() {
    if (running_) return;

    // 配置 socket
    socket_.set_int(NNG_OPT_RECVBUF, config_.recvBufferSize);
    socket_.set_int(NNG_OPT_SENDBUF, config_.sendBufferSize);

    // 监听
    listener_ = std::make_unique<NngListener>(socket_.get(), url_);

    running_ = true;

    // 启动工作线程
    workerThread_ = std::thread(&RepSocket::workerLoop, this);
}

void RepSocket::stop() {
    running_ = false;
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    listener_.reset();
}

void RepSocket::setRequestHandler(RequestHandler handler) {
    handler_ = std::move(handler);
}

void RepSocket::workerLoop() {
    while (running_) {
        try {
            // 接收请求
            nng_msg* msg;
            int rv = nng_recvmsg(socket_.get(), &msg, 0);
            if (rv != 0) {
                if (running_) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                continue;
            }

            // 获取数据
            size_t sz = nng_msg_len(msg);
            std::vector<uint8_t> data(sz);
            std::memcpy(data.data(), nng_msg_body(msg), sz);
            nng_msg_free(msg);

            // 处理请求
            std::vector<uint8_t> response;
            if (handler_) {
                response = handler_(data);
            }

            // 发送响应
            if (!response.empty()) {
                socket_.send(response);
            }
        } catch (const std::exception& e) {
            // 错误处理，继续运行
        }
    }
}

//==============================================================================
// PubSocket 实现
//==============================================================================

PubSocket::PubSocket(const std::string& url, const SocketConfig& config)
    : url_(url), config_(config) {
    int rv = nng_pub0_open(&socket_.get());
    if (rv != 0) {
        throw NngError(rv);
    }
}

PubSocket::~PubSocket() {
    stop();
}

void PubSocket::start() {
    if (running_) return;

    socket_.set_int(NNG_OPT_SENDBUF, config_.sendBufferSize);

    listener_ = std::make_unique<NngListener>(socket_.get(), url_);
    running_ = true;
}

void PubSocket::stop() {
    running_ = false;
    listener_.reset();
}

void PubSocket::publish(const std::vector<uint8_t>& data) {
    if (!running_) {
        throw NngError(NNG_ECLOSED);
    }
    socket_.send(data);
}

//==============================================================================
// SubSocket 实现
//==============================================================================

SubSocket::SubSocket(const std::string& url, const SocketConfig& config)
    : url_(url), config_(config) {
    int rv = nng_sub0_open(&socket_.get());
    if (rv != 0) {
        throw NngError(rv);
    }
}

SubSocket::~SubSocket() {
    stop();
}

void SubSocket::start() {
    if (running_) return;

    socket_.set_int(NNG_OPT_RECVBUF, config_.recvBufferSize);
    socket_.set_ms(NNG_OPT_RECVTIMEO, 100); // 短超时，用于循环检查

    dialer_ = std::make_unique<NngDialer>(socket_.get(), url_);
    running_ = true;

    workerThread_ = std::thread(&SubSocket::workerLoop, this);
}

void SubSocket::stop() {
    running_ = false;
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    dialer_.reset();
}

void SubSocket::setMessageCallback(MessageCallback callback) {
    callback_ = std::move(callback);
}

void SubSocket::subscribe(const std::string& topic) {
    // 订阅主题（空字符串表示订阅所有）
    int rv = nng_socket_set(socket_.get(), NNG_OPT_SUBSCRIBE,
                            topic.empty() ? nullptr : topic.c_str(),
                            topic.size());
    if (rv != 0) {
        throw NngError(rv);
    }
}

void SubSocket::workerLoop() {
    while (running_) {
        try {
            nng_msg* msg;
            int rv = nng_recvmsg(socket_.get(), &msg, 0);
            if (rv != 0) {
                if (running_) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                continue;
            }

            size_t sz = nng_msg_len(msg);
            std::vector<uint8_t> data(sz);
            std::memcpy(data.data(), nng_msg_body(msg), sz);
            nng_msg_free(msg);

            if (callback_) {
                callback_(data);
            }
        } catch (const std::exception& e) {
            // 继续
        }
    }
}

//==============================================================================
// PairSocket 实现
//==============================================================================

PairSocket::PairSocket(const std::string& url, bool server, const SocketConfig& config)
    : url_(url), server_(server), config_(config) {
    int rv = nng_pair0_open(&socket_.get());
    if (rv != 0) {
        throw NngError(rv);
    }
}

PairSocket::~PairSocket() {
    stop();
}

void PairSocket::start() {
    if (running_) return;

    socket_.set_int(NNG_OPT_RECVBUF, config_.recvBufferSize);
    socket_.set_int(NNG_OPT_SENDBUF, config_.sendBufferSize);

    if (server_) {
        listener_ = std::make_unique<NngListener>(socket_.get(), url_);
    } else {
        dialer_ = std::make_unique<NngDialer>(socket_.get(), url_);
    }

    running_ = true;

    if (callback_) {
        workerThread_ = std::thread(&PairSocket::workerLoop, this);
    }
}

void PairSocket::stop() {
    running_ = false;
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    listener_.reset();
    dialer_.reset();
}

void PairSocket::send(const std::vector<uint8_t>& data) {
    if (!running_) {
        throw NngError(NNG_ECLOSED);
    }
    socket_.send(data);
}

void PairSocket::setMessageCallback(MessageCallback callback) {
    callback_ = std::move(callback);
}

void PairSocket::workerLoop() {
    while (running_) {
        try {
            nng_msg* msg;
            int rv = nng_recvmsg(socket_.get(), &msg, 0);
            if (rv != 0) {
                if (running_) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                continue;
            }

            size_t sz = nng_msg_len(msg);
            std::vector<uint8_t> data(sz);
            std::memcpy(data.data(), nng_msg_body(msg), sz);
            nng_msg_free(msg);

            if (callback_) {
                callback_(data);
            }
        } catch (const std::exception& e) {
            // 继续
        }
    }
}

//==============================================================================
// RpcClient 实现
//==============================================================================

RpcClient::RpcClient(const std::string& serverUrl, const SocketConfig& config)
    : serverUrl_(serverUrl), client_(serverUrl, config) {}

RpcClient::~RpcClient() {
    disconnect();
}

void RpcClient::connect() {
    client_.start();
}

void RpcClient::disconnect() {
    client_.stop();
}

//==============================================================================
// RpcServer 实现
//==============================================================================

RpcServer::RpcServer(const std::string& bindUrl, const SocketConfig& config)
    : bindUrl_(bindUrl), config_(config) {}

RpcServer::~RpcServer() {
    stop();
}

void RpcServer::start() {
    server_ = std::make_unique<RepSocket>(bindUrl_, config_);

    server_->setRequestHandler([this](const std::vector<uint8_t>& data) -> std::vector<uint8_t> {
        // 解析消息头
        auto header = MessageCodec::parseHeader(data);
        uint16_t msgType = header.type;

        // 查找处理器
        std::lock_guard<std::mutex> lock(handlersMutex_);
        auto it = handlers_.find(msgType);
        if (it == handlers_.end()) {
            // 返回错误
            ErrorMessage err{static_cast<uint32_t>(NNG_ENOTSUP), "Unknown message type"};
            return MessageCodec::encode(err, header.sessionId);
        }

        // 调用处理器
        std::vector<uint8_t> bodyData(data.begin() + sizeof(MessageHeader), data.end());
        return it->second(bodyData, header.sessionId);
    });

    server_->start();
}

void RpcServer::stop() {
    server_->stop();
    server_.reset();
}

} // namespace protocol
} // namespace apollo
