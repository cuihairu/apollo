#pragma once

#include "apollo/actor/actor_ref.h"
#include "apollo/actor/actor.h"
#include "apollo/actor/message.h"
#include "apollo/ipc/channel.h"
#include "apollo/ipc/service_endpoint_ex.h"
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <mutex>
#include <future>

namespace apollo {
namespace actor {

//==============================================================================
// 消息传输地址
//==============================================================================

struct TransportAddress {
    ipc::TransportType type;
    std::string path;           // 具体路径（如 /dev/shm/xxx, /tmp/xxx.sock）
    std::string host;           // 主机地址（用于 TCP）
    uint16_t port = 0;          // 端口（用于 TCP）

    // 生成连接字符串
    std::string toString() const {
        switch (type) {
            case ipc::TransportType::SharedMemory:
                return "shm://" + path;
            case ipc::TransportType::UnixSocket:
                return "unix://" + path;
            case ipc::TransportType::Tcp:
                return "tcp://" + host + ":" + std::to_string(port);
            default:
                return path;
        }
    }

    // 从连接字符串解析
    static TransportAddress fromString(const std::string& str) {
        TransportAddress addr;
        size_t pos = str.find("://");
        if (pos == std::string::npos) {
            addr.type = ipc::TransportType::Tcp;
            addr.path = str;
            return addr;
        }

        std::string scheme = str.substr(0, pos);
        std::string rest = str.substr(pos + 3);

        if (scheme == "shm") {
            addr.type = ipc::TransportType::SharedMemory;
            addr.path = rest;
        } else if (scheme == "unix") {
            addr.type = ipc::TransportType::UnixSocket;
            addr.path = rest;
        } else if (scheme == "tcp") {
            addr.type = ipc::TransportType::Tcp;
            // 解析 host:port
            size_t colonPos = rest.find(':');
            if (colonPos != std::string::npos) {
                addr.host = rest.substr(0, colonPos);
                addr.port = static_cast<uint16_t>(std::stoi(rest.substr(colonPos + 1)));
            } else {
                addr.host = rest;
            }
        }

        return addr;
    }
};

//==============================================================================
// 传输通道（封装 Channel）
//==============================================================================

class TransportChannel {
public:
    TransportChannel(const TransportAddress& addr,
                    const ipc::ChannelConfig& config = ipc::ChannelConfig{});

    ~TransportChannel();

    // 是否已连接
    bool isConnected() const { return connected_; }

    // 获取地址
    const TransportAddress& address() const { return address_; }

    // 发送消息
    bool send(const std::vector<uint8_t>& data);

    // 接收消息（异步）
    void receive(std::function<void(const std::vector<uint8_t>&)> callback);

    // 关闭连接
    void close();

private:
    TransportAddress address_;
    std::unique_ptr<ipc::Channel> channel_;
    std::atomic<bool> connected_{false};
};

//==============================================================================
// 消息总线配置
//==============================================================================

struct MessageBusConfig {
    // 本地传输配置
    bool enableSharedMemory = true;
    std::string shmBasePath = "/dev/shm/actor";

    bool enableUnixSocket = true;
    std::string unixBasePath = "/tmp/actor";

    // 远程传输配置
    bool enableTcp = true;
    std::string tcpHost = "0.0.0.0";
    uint16_t tcpPortBase = 10000;

    // Channel 配置
    ipc::ChannelConfig channelConfig;

    // 超时配置
    int64_t connectTimeoutMs = 5000;
    int64_t sendTimeoutMs = 1000;
};

//==============================================================================
// 消息总线（封装 Channel，本地优先）
//==============================================================================

class MessageBus {
public:
    explicit MessageBus(const MessageBusConfig& config = MessageBusConfig{});
    ~MessageBus();

    //==========================================================================
    // 生命周期
    //==========================================================================

    bool start(const ActorPath& localPath);
    void stop();

    //==========================================================================
    // 消息发送
    //==========================================================================

    // 发送到本地 Actor（直接内存拷贝，最快）
    bool sendLocal(const ActorPath& target, const Message& msg);

    // 发送到远程 Actor（通过 Channel）
    bool sendRemote(const ActorPath& target, const Message& msg);

    // 发送到指定地址
    bool sendTo(const TransportAddress& addr, const std::vector<uint8_t>& data);

    // 通用发送（自动判断本地/远程）
    bool send(const ActorPath& target, const Message& msg);

    //==========================================================================
    // 消息接收
    //==========================================================================

    // 注册本地消息处理器
    using MessageHandler = std::function<void(const Message&, const ActorRef&)>;
    void registerHandler(const std::string& actorName, MessageHandler handler);

    // 注销处理器
    void unregisterHandler(const std::string& actorName);

    //==========================================================================
    // 通道管理
    //==========================================================================

    // 创建传输地址
    TransportAddress createAddress(const ActorPath& path,
                                   ipc::TransportType type) const;

    // 创建到目标 Actor 的通道
    std::shared_ptr<TransportChannel> createChannel(
        const ActorPath& target,
        const ipc::ServiceEndpointEx& endpoint);

    // 获取或创建通道（带缓存）
    std::shared_ptr<TransportChannel> getOrCreateChannel(
        const ActorPath& target,
        const ipc::ServiceEndpointEx& endpoint);

    // 关闭指定通道
    void closeChannel(const ActorPath& target);

    // 关闭所有通道
    void closeAllChannels();

    //==========================================================================
    // 服务端点集成
    //==========================================================================

    // 从 ServiceEndpointEx 选择最佳传输地址
    TransportAddress selectBestAddress(const ipc::ServiceEndpointEx& endpoint) const;

    // 获取本地 Actor 的服务端点
    ipc::ServiceEndpointEx createEndpoint(const ActorPath& path,
                                         uint32_t transportMask) const;

    //==========================================================================
    // 配置
    //==========================================================================

    const MessageBusConfig& getConfig() const { return config_; }
    const ActorPath& localPath() const { return localPath_; }

private:
    //==========================================================================
    // 内部方法
    //==========================================================================

    // 监听本地连接（共享内存 + Unix Socket）
    bool startLocalListener();

    // 监听 TCP 连接
    bool startTcpListener();

    // 处理接收到的数据
    void handleReceivedData(const std::vector<uint8_t>& data,
                           const TransportAddress& source);

    // 解析消息
    Message parseMessage(const std::vector<uint8_t>& data) const;

    // 序列化消息
    std::vector<uint8_t> serializeMessage(const Message& msg) const;

    //==========================================================================
    // 成员变量
    //==========================================================================

    MessageBusConfig config_;
    ActorPath localPath_;

    // 本地 Actor 处理器
    std::unordered_map<std::string, MessageHandler> localHandlers_;
    std::shared_mutex handlersMutex_;

    // 远程通道缓存
    std::unordered_map<std::string, std::shared_ptr<TransportChannel>> channels_;
    mutable std::shared_mutex channelsMutex_;

    // 监听器
    std::unique_ptr<ipc::Channel> shmListener_;
    std::unique_ptr<ipc::Channel> unixListener_;
    std::unique_ptr<ipc::Channel> tcpListener_;

    // 接收线程
    std::vector<std::thread> receiverThreads_;
    std::atomic<bool> running_{false};
};

//==============================================================================
// 消息包装格式（网络传输）
//==============================================================================

#pragma pack(push, 1)
struct MessageHeader {
    uint32_t magic;           // 魔数 0x41435452 ("ACTR")
    uint32_t version;         // 版本号
    uint32_t length;          // 消息体长度
    uint32_t typeHash;        // 消息类型哈希
    uint8_t  format;          // 序列化格式
    uint8_t  reserved[7];     // 保留字段

    static constexpr uint32_t MAGIC = 0x41435452;
    static constexpr uint32_t CURRENT_VERSION = 1;
};
#pragma pack(pop)

struct NetworkMessage {
    MessageHeader header;
    std::vector<uint8_t> body;  // 消息体

    // 序列化
    std::vector<uint8_t> toBytes() const {
        std::vector<uint8_t> result(sizeof(MessageHeader) + body.size());
        std::memcpy(result.data(), &header, sizeof(MessageHeader));
        if (!body.empty()) {
            std::memcpy(result.data() + sizeof(MessageHeader), body.data(), body.size());
        }
        return result;
    }

    // 反序列化
    static NetworkMessage fromBytes(const std::vector<uint8_t>& data) {
        NetworkMessage msg;
        if (data.size() < sizeof(MessageHeader)) {
            return msg;  // 无效消息
        }
        std::memcpy(&msg.header, data.data(), sizeof(MessageHeader));
        if (data.size() > sizeof(MessageHeader)) {
            msg.body.assign(data.begin() + sizeof(MessageHeader), data.end());
        }
        return msg;
    }

    // 验证
    bool isValid() const {
        return header.magic == MessageHeader::MAGIC &&
               header.version == MessageHeader::CURRENT_VERSION &&
               header.length == body.size();
    }
};

} // namespace actor
} // namespace apollo
