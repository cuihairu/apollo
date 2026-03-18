/**
 * @file message_bus.cpp
 * @brief 消息总线实现（封装 Channel，本地优先）
 */

#include "apollo/actor/message_bus.h"
#include "apollo/actor/actor_system.h"
#include <cstring>

namespace apollo {
namespace actor {

//==============================================================================
// TransportChannel 实现
//==============================================================================

TransportChannel::TransportChannel(const TransportAddress& addr,
                                   const ipc::ChannelConfig& config)
    : address_(addr) {

    ipc::ChannelConfig channelConfig = config;

    // 根据地址类型配置 Channel
    switch (addr.type) {
        case ipc::TransportType::SharedMemory:
            channelConfig.type = ipc::ChannelType::SharedMemory;
            channelConfig.shmPath = addr.path;
            break;

        case ipc::TransportType::UnixSocket:
            channelConfig.type = ipc::ChannelType::UnixSocket;
            channelConfig.unixPath = addr.path;
            break;

        case ipc::TransportType::Tcp:
            channelConfig.type = ipc::ChannelType::Tcp;
            channelConfig.host = addr.host;
            channelConfig.port = addr.port;
            break;

        default:
            break;
    }

    channel_ = ipc::ChannelManager::instance().create(channelConfig);
    connected_ = (channel_ != nullptr);
}

TransportChannel::~TransportChannel() {
    close();
}

bool TransportChannel::send(const std::vector<uint8_t>& data) {
    if (!connected_ || !channel_) {
        return false;
    }

    return channel_->send(data.data(), data.size()) >= 0;
}

void TransportChannel::receive(std::function<void(const std::vector<uint8_t>&)> callback) {
    if (!connected_ || !channel_) {
        return;
    }

    // 设置接收回调
    channel_->setReceiveCallback([callback](const void* data, size_t size) {
        std::vector<uint8_t> bytes(static_cast<const uint8_t*>(data),
                                   static_cast<const uint8_t*>(data) + size);
        callback(bytes);
    });
}

void TransportChannel::close() {
    if (channel_) {
        channel_->close();
        connected_ = false;
    }
}

//==============================================================================
// MessageBus 实现
//==============================================================================

MessageBus::MessageBus(const MessageBusConfig& config)
    : config_(config) {
}

MessageBus::~MessageBus() {
    stop();
}

bool MessageBus::start(const ActorPath& localPath) {
    if (running_.load()) {
        return true;
    }

    localPath_ = localPath;
    running_.store(true);

    // 启动本地监听器
    if (!startLocalListener()) {
        stop();
        return false;
    }

    // 启动 TCP 监听器
    if (config_.enableTcp && !startTcpListener()) {
        // TCP 监听失败不是致命错误
    }

    return true;
}

void MessageBus::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    // 关闭所有监听器
    if (shmListener_) shmListener_->close();
    if (unixListener_) unixListener_->close();
    if (tcpListener_) tcpListener_->close();

    // 关闭所有通道
    closeAllChannels();

    // 等待接收线程结束
    for (auto& thread : receiverThreads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    receiverThreads_.clear();
}

bool MessageBus::send(const ActorPath& target, const Message& msg) {
    // 判断是否为本地
    if (target.system == localPath_.system &&
        (target.address.empty() || target.address == localPath_.address)) {
        return sendLocal(target, msg);
    }

    return sendRemote(target, msg);
}

bool MessageBus::sendLocal(const ActorPath& target, const Message& msg) {
    // 查找本地处理器
    std::shared_lock lock(handlersMutex_);
    auto it = localHandlers_.find(target.name);
    if (it != localHandlers_.end()) {
        // 直接调用处理器（零拷贝）
        it->second(msg, ActorRef{});  // TODO: 正确的发送者
        return true;
    }
    return false;
}

bool MessageBus::sendRemote(const ActorPath& target, const Message& msg) {
    // 从服务发现查找目标端点
    auto& discovery = localPath_.system.empty() ?
        system::instance().discovery() :
        // TODO: 需要访问系统实例
        *(static_cast<ipc::IServiceDiscovery*>(nullptr));

    auto endpoints = discovery.discover(target.name);
    if (endpoints.empty()) {
        return false;
    }

    // 选择最佳端点（本地优先）
    ipc::LocalFirstSelector selector;
    ipc::ServiceEndpointEx bestEndpoint;

    // 转换为 ServiceEndpointEx
    for (const auto& ep : endpoints) {
        auto ex = ipc::ServiceEndpointEx::fromBase(ep);
        if (ex.tcpAddress.serverHost == "127.0.0.1" ||
            ex.tcpAddress.serverHost == "localhost") {
            bestEndpoint = ex;
            break;
        }
    }

    if (bestEndpoint.id.empty() && !endpoints.empty()) {
        bestEndpoint = ipc::ServiceEndpointEx::fromBase(endpoints[0]);
    }

    // 选择最佳传输地址
    auto addr = selectBestAddress(bestEndpoint);

    // 序列化消息
    auto data = serializeMessage(msg);

    // 发送
    return sendTo(addr, data);
}

bool MessageBus::sendTo(const TransportAddress& addr,
                       const std::vector<uint8_t>& data) {
    auto channel = getOrCreateChannel(ActorPath{}, ipc::ServiceEndpointEx{});
    // TODO: 实现基于地址的通道创建
    return channel && channel->send(data);
}

void MessageBus::registerHandler(const std::string& actorName,
                                 MessageHandler handler) {
    std::unique_lock lock(handlersMutex_);
    localHandlers_[actorName] = std::move(handler);
}

void MessageBus::unregisterHandler(const std::string& actorName) {
    std::unique_lock lock(handlersMutex_);
    localHandlers_.erase(actorName);
}

TransportAddress MessageBus::createAddress(const ActorPath& path,
                                          ipc::TransportType type) const {
    TransportAddress addr;
    addr.type = type;

    switch (type) {
        case ipc::TransportType::SharedMemory:
            addr.path = config_.shmBasePath + "/" +
                       localPath_.system + "/" + path.name;
            break;

        case ipc::TransportType::UnixSocket:
            addr.path = config_.unixBasePath + "/" +
                       localPath_.system + "_" + path.name + ".sock";
            break;

        case ipc::TransportType::Tcp:
            addr.host = config_.tcpHost;
            addr.port = config_.tcpPortBase;  // TODO: 动态分配
            break;

        default:
            break;
    }

    return addr;
}

std::shared_ptr<TransportChannel> MessageBus::createChannel(
    const ActorPath& target,
    const ipc::ServiceEndpointEx& endpoint) {

    // 选择最佳传输类型
    auto addr = selectBestAddress(endpoint);

    auto channel = std::make_shared<TransportChannel>(addr, config_.channelConfig);

    // 设置接收回调
    channel->receive([this, addr](const std::vector<uint8_t>& data) {
        handleReceivedData(data, addr);
    });

    return channel;
}

std::shared_ptr<TransportChannel> MessageBus::getOrCreateChannel(
    const ActorPath& target,
    const ipc::ServiceEndpointEx& endpoint) {

    std::string key = target.toString();

    {
        std::shared_lock lock(channelsMutex_);
        auto it = channels_.find(key);
        if (it != channels_.end() && it->second->isConnected()) {
            return it->second;
        }
    }

    // 创建新通道
    auto channel = createChannel(target, endpoint);

    {
        std::unique_lock lock(channelsMutex_);
        channels_[key] = channel;
    }

    return channel;
}

void MessageBus::closeChannel(const ActorPath& target) {
    std::unique_lock lock(channelsMutex_);
    channels_.erase(target.toString());
}

void MessageBus::closeAllChannels() {
    std::unique_lock lock(channelsMutex_);
    channels_.clear();
}

TransportAddress MessageBus::selectBestAddress(
    const ipc::ServiceEndpointEx& endpoint) const {

    // 优先级：共享内存 > Unix Socket > TCP

    if (endpoint.supportsTransport(ipc::TransportType::SharedMemory) &&
        !endpoint.shmAddress.shmName.empty()) {
        TransportAddress addr;
        addr.type = ipc::TransportType::SharedMemory;
        addr.path = endpoint.shmAddress.shmName;
        return addr;
    }

    if (endpoint.supportsTransport(ipc::TransportType::UnixSocket) &&
        !endpoint.unixAddress.unixPath.empty()) {
        TransportAddress addr;
        addr.type = ipc::TransportType::UnixSocket;
        addr.path = endpoint.unixAddress.unixPath;
        return addr;
    }

    // 默认使用 TCP
    TransportAddress addr;
    addr.type = ipc::TransportType::Tcp;
    addr.host = endpoint.tcpAddress.serverHost;
    addr.port = endpoint.tcpAddress.serverPort;
    return addr;
}

ipc::ServiceEndpointEx MessageBus::createEndpoint(
    const ActorPath& path, uint32_t transportMask) const {

    ipc::ServiceEndpointEx endpoint;
    endpoint.transportMask = static_cast<ipc::TransportMask>(transportMask);

    // 共享内存地址
    endpoint.shmAddress = createAddress(path, ipc::TransportType::SharedMemory);

    // Unix Socket 地址
    endpoint.unixAddress = createAddress(path, ipc::TransportType::UnixSocket);

    // TCP 地址
    endpoint.tcpAddress = createAddress(path, ipc::TransportType::Tcp);

    return endpoint;
}

bool MessageBus::startLocalListener() {
    // TODO: 实现共享内存和 Unix Socket 监听
    return true;
}

bool MessageBus::startTcpListener() {
    // TODO: 实现 TCP 监听
    return true;
}

void MessageBus::handleReceivedData(const std::vector<uint8_t>& data,
                                   const TransportAddress& source) {
    auto msg = parseMessage(data);
    if (msg.getType().name.empty()) {
        return;  // 无效消息
    }

    // 查找处理器
    std::shared_lock lock(handlersMutex_);
    auto it = localHandlers_.find(msg.getType().name);
    if (it != localHandlers_.end()) {
        ActorRef sender;  // TODO: 根据源地址创建 ActorRef
        it->second(msg, sender);
    }
}

Message MessageBus::parseMessage(const std::vector<uint8_t>& data) const {
    // 解析网络消息格式
    if (data.size() < sizeof(MessageHeader)) {
        return Message{};
    }

    NetworkMessage netMsg = NetworkMessage::fromBytes(data);
    if (!netMsg.isValid()) {
        return Message{};
    }

    // TODO: 根据格式和类型哈希反序列化
    return Message{};
}

std::vector<uint8_t> MessageBus::serializeMessage(const Message& msg) const {
    NetworkMessage netMsg;
    netMsg.header.magic = MessageHeader::MAGIC;
    netMsg.header.version = MessageHeader::CURRENT_VERSION;
    netMsg.header.format = static_cast<uint8_t>(msg.getFormat());
    netMsg.header.typeHash = std::hash<std::string>{}(msg.getType().name);

    // 消息体
    netMsg.body = msg.getData();
    netMsg.header.length = netMsg.body.size();

    return netMsg.toBytes();
}

} // namespace actor
} // namespace apollo
