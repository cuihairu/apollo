# Q20: 长连接如何保持心跳？断线重连如何设计？

## 问题分析

本题考察对长连接维护的理解：
- 心跳机制的设计与实现
- 连接状态检测与超时处理
- 断线重连策略
- KBEngine 的心跳和超时机制

---

## 一、心跳机制基础

### 1.1 为什么需要心跳

```
┌─────────────────────────────────────────────────────────────┐
│                    心跳机制的必要性                           │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  问题: TCP 连接的"假死"状态                                  │
│  ┌─────────────────────────────────────────────────┐       │
│  │  正常情况:                                         │       │
│  │  客户端 ────ping───→ 服务器 ────pong───→ 客户端    │       │
│  │                                                   │       │
│  │  假死情况:                                         │       │
│  │  客户端 ────ping───× 服务器 (网络断开)             │       │
│  │  但 TCP 连接状态仍显示 ESTABLISHED                 │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  TCP 假死的原因:                                            │
│  ├── 网络中断 (网线拔掉、WiFi 断开)                         │
│  ├── 路由器重启                                            │
│  ├── NAT 超时清理                                          │
│  ├── 防火墙中断                                            │
│  └── 长时间无数据传输                                       │
│                                                             │
│  心跳的作用:                                                │
│  ├── 检测连接是否存活                                       │
│  ├── 保持 NAT 映射                                         │
│  ├── 快速发现断线                                           │
│  └── 同步服务器时间                                         │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 心跳类型

```
┌─────────────────────────────────────────────────────────────┐
│                    心跳类型对比                               │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 双向心跳 (推荐)                                         │
│  ┌─────────────────────────────────────────────────┐       │
│  │  客户端 ────ping───→ 服务器                       │       │
│  │  客户端 ←────pong─── 服务器                       │       │
│  │                                                   │       │
│  │  优势:                                            │       │
│  │  ├── 双方都能检测断线                               │       │
│  │  ├── 负载均衡                                       │       │
│  │  └── 确认双向连通                                   │       │
│  │                                                   │       │
│  │  劣势:                                            │       │
│  │  ├── 带宽开销翻倍                                   │       │
│  │  └── 实现稍复杂                                     │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  2. 单向心跳 (客户端主动)                                    │
│  ┌─────────────────────────────────────────────────┐       │
│  │  客户端 ────ping───→ 服务器                       │       │
│  │  服务器不回复                                       │       │
│  │                                                   │       │
│  │  优势:                                            │       │
│  │  ├── 实现简单                                       │       │
│  │  ├── 带宽开销小                                     │       │
│  │  └── 服务器压力小                                   │       │
│  │                                                   │       │
│  │  劣势:                                            │       │
│  │  ├── 服务器无法主动检测断线                          │       │
│  │  └── 客户端不知道服务器状态                          │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  3. TCP Keepalive (系统级)                                  │
│  ┌─────────────────────────────────────────────────┐       │
│  │  由操作系统 TCP 协议栈处理                         │       │
│  │                                                   │       │
│  │  优势:                                            │       │
│  │  ├── 无需应用层处理                                 │       │
│  │  └── 标准化                                        │       │
│  │                                                   │       │
│  │  劣势:                                            │       │
│  │  ├── 时间太长 (默认 2 小时)                         │       │
│  │  ├── 配置复杂                                       │       │
│  │  └── 粒度太粗                                       │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、心跳实现

### 2.1 客户端心跳

```cpp
// 客户端心跳实现

class HeartbeatClient {
public:
    HeartbeatClient(const std::string& serverAddr, uint16_t port)
        : serverAddr_(serverAddr), port_(port),
          running_(true), lastPongTime_(0) {

        // 连接服务器
        connect();

        // 启动心跳线程
        heartbeatThread_ = std::thread(&HeartbeatClient::heartbeatLoop, this);

        // 启动接收线程
        receiveThread_ = std::thread(&HeartbeatClient::receiveLoop, this);
    }

    ~HeartbeatClient() {
        running_ = false;
        if (heartbeatThread_.joinable()) {
            heartbeatThread_.join();
        }
        if (receiveThread_.joinable()) {
            receiveThread_.join();
        }
    }

private:
    void heartbeatLoop() {
        while (running_) {
            // 发送心跳
            sendPing();

            // 检查是否超时
            if (checkTimeout()) {
                onTimeout();
                reconnect();
            }

            // 等待下一个心跳间隔
            std::this_thread::sleep_for(
                std::chrono::milliseconds(HEARTBEAT_INTERVAL)
            );
        }
    }

    void sendPing() {
        HeartbeatMessage msg;
        msg.type = MSG_PING;
        msg.timestamp = getCurrentTime();
        msg.sequence = nextSequence_++;

        // 发送
        socket_.send(&msg, sizeof(msg));

        LOG_DEBUG("Sent ping, seq=" + std::to_string(msg.sequence));
    }

    void receiveLoop() {
        uint8_t buffer[4096];

        while (running_) {
            int len = socket_.recv(buffer, sizeof(buffer));

            if (len > 0) {
                handleMessage(buffer, len);
            } else if (len < 0) {
                // 连接错误
                if (isConnectionError()) {
                    onDisconnect();
                    reconnect();
                }
            }
        }
    }

    void handleMessage(const uint8_t* data, size_t len) {
        Message* msg = (Message*)data;

        switch (msg->type) {
            case MSG_PONG:
                handlePong((HeartbeatMessage*)msg);
                break;

            case MSG_KICK:
                handleKick((KickMessage*)msg);
                break;

            default:
                // 其他业务消息
                break;
        }
    }

    void handlePong(HeartbeatMessage* msg) {
        lastPongTime_ = getCurrentTime();
        latency_ = lastPongTime_ - msg->timestamp;

        LOG_DEBUG("Received pong, seq=" + std::to_string(msg->sequence) +
                  ", latency=" + std::to_string(latency_) + "ms");
    }

    bool checkTimeout() {
        uint32_t currentTime = getCurrentTime();

        // 首次连接
        if (lastPongTime_ == 0) {
            return false;
        }

        // 检查超时
        return (currentTime - lastPongTime_) > TIMEOUT_THRESHOLD;
    }

    void onTimeout() {
        LOG_WARNING("Connection timeout detected");
        onDisconnect();
    }

    void onDisconnect() {
        socket_.close();
        lastPongTime_ = 0;
    }

    void reconnect() {
        LOG_INFO("Attempting to reconnect...");

        // 指数退避
        static int retryCount = 0;
        int delay = std::min(1000 * (1 << retryCount), 30000);

        std::this_thread::sleep_for(std::chrono::milliseconds(delay));

        if (connect()) {
            LOG_INFO("Reconnected successfully");
            retryCount = 0;
        } else {
            retryCount++;
            LOG_ERROR("Reconnect failed, retrying...");
        }
    }

    bool connect() {
        return socket_.connect(serverAddr_, port_);
    }

    struct HeartbeatMessage {
        uint8_t type;
        uint32_t timestamp;
        uint32_t sequence;
    };

    static constexpr uint32_t HEARTBEAT_INTERVAL = 5000;  // 5秒
    static constexpr uint32_t TIMEOUT_THRESHOLD = 15000;  // 15秒

    std::string serverAddr_;
    uint16_t port_;
    TCPSocket socket_;

    std::thread heartbeatThread_;
    std::thread receiveThread_;

    std::atomic<bool> running_;
    uint32_t lastPongTime_;
    uint32_t nextSequence_ = 0;
    uint32_t latency_ = 0;
};
```

### 2.2 服务器心跳处理

```cpp
// 服务器心跳处理

class HeartbeatServer {
public:
    struct ClientSession {
        uint32_t clientId;
        uint32_t lastPingTime;
        uint32_t lastPongTime;
        uint32_t latency;

        bool isTimeout() const {
            uint32_t now = getCurrentTime();
            return (now - lastPingTime) > CLIENT_TIMEOUT;
        }
    };

    HeartbeatServer(uint16_t port) : port_(port) {
        // 启动服务器
        serverSocket_.bind(port_);
        serverSocket_.listen();

        // 启动心跳检查线程
        checkThread_ = std::thread(&HeartbeatServer::checkLoop, this);
    }

    // 处理客户端 ping
    void onPing(const TCPConnection& conn, const HeartbeatMessage& msg) {
        uint32_t clientId = getClientId(conn);

        // 更新会话
        auto& session = sessions_[clientId];
        session.clientId = clientId;
        session.lastPingTime = getCurrentTime();

        // 发送 pong
        HeartbeatMessage pong;
        pong.type = MSG_PONG;
        pong.timestamp = msg.timestamp;
        pong.sequence = msg.sequence;

        conn.send(&pong, sizeof(pong));

        LOG_DEBUG("Client " + std::to_string(clientId) + " ping, seq=" +
                  std::to_string(msg.sequence));
    }

    // 处理客户端其他消息（更新心跳时间）
    void onClientMessage(uint32_t clientId) {
        auto it = sessions_.find(clientId);
        if (it != sessions_.end()) {
            it->second.lastPingTime = getCurrentTime();
        }
    }

private:
    void checkLoop() {
        while (running_) {
            checkTimeouts();

            std::this_thread::sleep_for(
                std::chrono::milliseconds(CHECK_INTERVAL)
            );
        }
    }

    void checkTimeouts() {
        std::vector<uint32_t> timeoutClients;

        for (auto& [clientId, session] : sessions_) {
            if (session.isTimeout()) {
                timeoutClients.push_back(clientId);
            }
        }

        // 断开超时客户端
        for (uint32_t clientId : timeoutClients) {
            LOG_WARNING("Client " + std::to_string(clientId) + " timeout");

            // 保存玩家数据
            savePlayerData(clientId);

            // 断开连接
            disconnectClient(clientId);

            // 通知相关系统
            onClientDisconnect(clientId);
        }
    }

    void disconnectClient(uint32_t clientId) {
        auto it = sessions_.find(clientId);
        if (it != sessions_.end()) {
            connections_[clientId].close();
            connections_.erase(clientId);
            sessions_.erase(it);
        }
    }

    void savePlayerData(uint32_t clientId) {
        // 自动保存玩家数据
        Player* player = getPlayer(clientId);
        if (player) {
            player->saveToDatabase();
        }
    }

    void onClientDisconnect(uint32_t clientId) {
        // 通知其他客户端
        broadcastPlayerLeave(clientId);

        // 通知游戏逻辑
        onPlayerLogout(clientId);
    }

    std::unordered_map<uint32_t, ClientSession> sessions_;
    std::unordered_map<uint32_t, TCPConnection> connections_;

    static constexpr uint32_t CLIENT_TIMEOUT = 30000;  // 30秒
    static constexpr uint32_t CHECK_INTERVAL = 5000;   // 5秒
};

// KBEngine 风格的心跳处理
// src/server/baseapp/baseapp_interface.cpp
namespace KBEngine {

class BaseApp {
public:
    // 处理客户端消息（更新心跳）
    void onClientMessage(Network::Channel* pChannel, MemoryStream& stream) {
        // 更新客户端心跳时间
        pChannel->updateLastRecvTime();

        // 处理消息
        MessageHandler::handle(pChannel, stream);
    }

    // 检查超时客户端
    void checkIdleClients() {
        std::vector<Network::Channel*> timeoutChannels;

        for (auto& pChannel : channels_) {
            if (pChannel->isIdle()) {
                timeoutChannels.push_back(pChannel);
            }
        }

        // 断开超时连接
        for (auto* pChannel : timeoutChannels) {
            LOG_WARNING("Client timeout: " +
                       pChannel->addrAsString());

            // 触发断线事件
            onClientDisconnect(pChannel);

            // 关闭连接
            pChannel->close();
        }
    }

private:
    std::vector<Network::Channel*> channels_;
};

} // namespace KBEngine
```

---

## 三、断线重连设计

### 3.1 重连策略

```
┌─────────────────────────────────────────────────────────────┐
│                    断线重连策略                               │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  策略 1: 立即重连                                            │
│  ┌─────────────────────────────────────────────────┐       │
│  │  断线后立即尝试重连                                 │       │
│  │                                                   │       │
│  │  优点: 响应快                                       │       │
│  │  缺点: 可能频繁重连，浪费资源                       │       │
│  │                                                   │       │
│  │  适用: 客户端，网络稳定环境                          │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  策略 2: 固定延迟重连                                        │
│  ┌─────────────────────────────────────────────────┐       │
│  │  断线后等待固定时间再重连                           │       │
│  │  例如: 5秒后重连                                    │       │
│  │                                                   │       │
│  │  优点: 简单                                         │       │
│  │  缺点: 可能还是太频繁                               │       │
│  │                                                   │       │
│  │  适用: 一般网络环境                                  │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  策略 3: 指数退避重连 (推荐)                                 │
│  ┌─────────────────────────────────────────────────┐       │
│  │  每次失败后延迟时间翻倍                             │       │
│  │  1s → 2s → 4s → 8s → 16s → 32s (最大)             │       │
│  │                                                   │       │
│  │  优点: 平衡响应速度和服务器负载                      │       │
│  │  缺点: 实现稍复杂                                   │       │
│  │                                                   │       │
│  │  适用: 大部分场景                                    │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  策略 4: 随机抖动重连                                        │
│  ┌─────────────────────────────────────────────────┐       │
│  │  在指数退避基础上加入随机抖动                       │       │
│  │  delay = baseDelay + random(0, baseDelay * 0.5)   │       │
│  │                                                   │       │
│  │  优点: 避免大量客户端同时重连                       │       │
│  │  缺点: 更复杂                                       │       │
│  │                                                   │       │
│  │  适用: 服务器重启后大量客户端重连                    │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 重连实现

```cpp
// 完整的断线重连实现

class ReconnectClient {
public:
    ReconnectClient(const std::string& serverAddr, uint16_t port)
        : serverAddr_(serverAddr), port_(port),
          state_(State::Disconnected), retryCount_(0) {

        // 连接
        connect();

        // 启动接收线程
        receiveThread_ = std::thread(&ReconnectClient::receiveLoop, this);
    }

    enum class State {
        Disconnected,
        Connecting,
        Connected,
        Reconnecting
    };

private:
    void connect() {
        state_ = State::Connecting;

        LOG_INFO("Connecting to " + serverAddr_ + ":" +
                 std::to_string(port_));

        if (socket_.connect(serverAddr_, port_)) {
            onConnected();
        } else {
            onConnectFailed();
        }
    }

    void onConnected() {
        state_ = State::Connected;
        retryCount_ = 0;

        LOG_INFO("Connected successfully");

        // 发送登录请求
        sendLogin();

        // 开始心跳
        startHeartbeat();
    }

    void onConnectFailed() {
        LOG_ERROR("Connection failed");

        if (state_ == State::Connecting) {
            scheduleReconnect();
        }
    }

    void onDisconnected() {
        LOG_WARNING("Disconnected");

        state_ = State::Disconnected;
        stopHeartbeat();

        // 安排重连
        scheduleReconnect();
    }

    void scheduleReconnect() {
        state_ = State::Reconnecting;

        // 计算重连延迟
        uint32_t delay = calculateReconnectDelay();

        LOG_INFO("Reconnecting in " + std::to_string(delay) + "ms...");

        // 延迟后重连
        std::thread([this, delay]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            if (state_ == State::Reconnecting) {
                connect();
            }
        }).detach();
    }

    uint32_t calculateReconnectDelay() {
        // 指数退避 + 随机抖动
        uint32_t baseDelay = std::min(
            uint32_t(1000 * (1 << retryCount_)),
            MAX_RETRY_DELAY
        );

        // 添加随机抖动 (±25%)
        uint32_t jitter = baseDelay / 4;
        uint32_t randomJitter = rand() % (2 * jitter + 1) - jitter;

        return baseDelay + randomJitter;
    }

    void receiveLoop() {
        uint8_t buffer[4096];

        while (true) {
            if (state_ != State::Connected) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(100)
                );
                continue;
            }

            int len = socket_.recv(buffer, sizeof(buffer));

            if (len > 0) {
                handleMessage(buffer, len);
            } else if (len < 0) {
                // 连接断开
                if (state_ == State::Connected) {
                    onDisconnected();
                }
                break;
            }
        }
    }

    void sendLogin() {
        LoginRequest msg;
        msg.token = savedToken_;
        msg.lastSequence = lastSequence_;

        socket_.send(&msg, sizeof(msg));
    }

    void onLoginResponse(const LoginResponse& resp) {
        if (resp.success) {
            LOG_INFO("Re-login successful");

            // 恢复状态
            lastSequence_ = resp.serverSequence;

            // 处理断线期间的消息
            handleMissedMessages(resp.missedMessages);
        } else {
            LOG_ERROR("Re-login failed: " + resp.reason);

            // 清空 token，需要重新登录
            savedToken_.clear();

            // 通知用户重新登录
            notifyReloginRequired();
        }
    }

    void handleMissedMessages(const std::vector<Message>& missed) {
        // 处理断线期间服务器缓存的消息
        for (const auto& msg : missed) {
            handleMessage(msg);
        }
    }

    State state_;
    std::string serverAddr_;
    uint16_t port_;
    TCPSocket socket_;

    std::thread receiveThread_;

    uint32_t retryCount_;
    std::string savedToken_;      // 保存的登录 token
    uint32_t lastSequence_;       // 最后收到的消息序列号

    static constexpr uint32_t MAX_RETRY_DELAY = 30000; // 30秒
};
```

---

## 四、KBEngine 心跳机制

### 4.1 KBEngine 超时配置

```python
# KBEngine 心跳配置
# kbengine_defaults.xml

<root>
    <!-- 客户端超时时间 (秒) -->
    <timeout>
        <!-- 默认超时 -->
        <default>60</default>

        <!-- 登录超时 -->
        <login>30</login>

        <!-- 其他特定超时 -->
    </timeout>

    <!-- 心跳间隔 -->
    <tcp>
        <!-- 客户端发送心跳间隔 -->
        <heartbeat>5</heartbeat>

        <!-- 服务器检查间隔 -->
        <heartbeatCheck>1</heartbeatCheck>
    </tcp>
</root>
```

### 4.2 KBEngine 源码分析

```cpp
// KBEngine Channel 超时检测
// src/lib/network/channel.h

namespace KBEngine {

class Channel {
public:
    enum ChannelType {
        CHANNEL_NORMAL = 0,
        CHANNEL_INTERNAL = 1,
        CHANNEL_CLIENT = 2,
    };

    Channel(Network::EndPoint& endpoint,
            ChannelType type,
            Network::Address& addr):
        endPoint_(endpoint),
        type_(type),
        addr_(addr),
        lastRecvTime_(timestamp()),
        inactiveTime_(0)
    {}

    // 更新最后接收时间
    void updateLastRecvTime() {
        lastRecvTime_ = timestamp();
        inactiveTime_ = 0;
    }

    // 检查是否空闲
    bool isIdle() const {
        uint32_t now = timestamp();
        uint32_t inactive = now - lastRecvTime_;

        return inactive > getTimeout();
    }

    // 获取超时时间
    uint32_t getTimeout() const {
        switch (type_) {
            case CHANNEL_CLIENT:
                return g_kbeSrvConfig.timeout();  // 客户端超时
            case CHANNEL_INTERNAL:
                return 300;  // 内部连接 5 分钟
            default:
                return 60;   // 默认 1 分钟
        }
    }

    // 更新空闲时间
    void updateInactivity() {
        uint32_t now = timestamp();
        inactiveTime_ = now - lastRecvTime_;
    }

    uint32_t inactiveTime() const {
        return inactiveTime_;
    }

private:
    Network::EndPoint& endPoint_;
    ChannelType type_;
    Network::Address addr_;

    uint32_t lastRecvTime_;
    uint32_t inactiveTime_;
};

} // namespace KBEngine

// KBEngine 超时检查
// src/server/baseapp/baseapp.cpp

namespace KBEngine {

void BaseApp::processChannels() {
    // 检查所有通道的超时状态
    for (auto& pChannel : channels_) {
        pChannel->updateInactivity();

        if (pChannel->isIdle()) {
            LOG_WARNING(fmt::format(
                "Channel idle timeout: {}, inactive for {} seconds",
                pChannel->addrAsString(),
                pChannel->inactiveTime()
            ));

            // 触发断线事件
            onChannelTimeout(pChannel);

            // 关闭通道
            pChannel->close();
        }
    }
}

void BaseApp::onChannelTimeout(Channel* pChannel) {
    if (pChannel->isClient()) {
        // 客户端超时
        Entity* pEntity = pChannel->entity();
        if (pEntity) {
            // 保存玩家数据
            pEntity->saveToDatabase();

            // 触发断线事件
            pEntity->onLogout();

            // 销毁实体
            pEntity->destroy();
        }
    }
}

} // namespace KBEngine
```

### 4.3 KBEngine 客户端重连

```cpp
// KBEngine 客户端重连机制
// 客户端代码 (Python/C++)

class KBEngineClient {
public:
    // 服务器断线回调
    void onDisconnect() {
        LOG_INFO("Disconnected from server");

        // 检查是否需要重连
        if (shouldReconnect()) {
            startReconnect();
        } else {
            showReconnectUI();
        }
    }

    bool shouldReconnect() {
        // 自动重连条件：
        // 1. 不是主动断开
        // 2. 在游戏中
        // 3. 未被踢出
        return !isLogout_ && isInGame_ && !isKicked_;
    }

    void startReconnect() {
        showReconnectingUI();

        // 重连线程
        reconnectThread_ = std::thread([this]() {
            int retry = 0;
            while (retry < MAX_RETRIES) {
                std::this_thread::sleep_for(
                    std::chrono::seconds(calculateRetryDelay(retry))
                );

                if (tryReconnect()) {
                    onReconnectSuccess();
                    return;
                }

                retry++;
                updateRetryCount(retry);
            }

            onReconnectFailed();
        });
    }

    bool tryReconnect() {
        // 连接登录服务器
        if (!connectToLoginApp()) {
            return false;
        }

        // 使用保存的 token 重连
        if (!relogin()) {
            return false;
        }

        // 恢复连接
        if (!restoreConnection()) {
            return false;
        }

        return true;
    }

    bool relogin() {
        ReloginRequest req;
        req.accountName = accountName_;
        req.token = savedToken_;
        req.lastRecvTime = lastRecvTime_;

        send(req);

        // 等待响应
        ReloginResponse resp;
        if (!waitForResponse(resp, 5000)) {
            return false;
        }

        return resp.success;
    }

    void onReconnectSuccess() {
        LOG_INFO("Reconnect successful");

        hideReconnectingUI();

        // 请求断线期间的消息
        requestMissedMessages();

        // 恢复游戏状态
        restoreGameState();
    }

private:
    int calculateRetryDelay(int retry) {
        // 指数退避: 1s, 2s, 4s, 8s, 16s
        return std::min(1 << retry, 16);
    }

    static constexpr int MAX_RETRIES = 5;

    std::string accountName_;
    std::string savedToken_;
    uint32_t lastRecvTime_;

    bool isLogout_ = false;
    bool isInGame_ = false;
    bool isKicked_ = false;

    std::thread reconnectThread_;
};
```

---

## 五、最佳实践

### 5.1 心跳参数配置

```
┌─────────────────────────────────────────────────────────────┐
│                  心跳参数配置建议                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  心跳间隔:                                                  │
│  ┌─────────────────────────────────────────────────┐       │
│  │  5 秒   ← 推荐 (平衡流量和响应速度)               │       │
│  │  3 秒   ← 快速响应 (高频游戏)                     │       │
│  │  10 秒  ← 低流量要求                              │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  超时阈值:                                                  │
│  ┌─────────────────────────────────────────────────┐       │
│  │  = 心跳间隔 × 3                                   │       │
│  │                                                   │       │
│  │  心跳间隔 5 秒 → 超时 15 秒                        │       │
│  │  心跳间隔 3 秒 → 超时 9 秒                         │       │
│  │  心跳间隔 10 秒 → 超时 30 秒                       │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  原则: 超时阈值应该至少是心跳间隔的 2-3 倍                  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 5.2 断线处理流程

```
┌─────────────────────────────────────────────────────────────┐
│              完整的断线处理流程                               │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  客户端断线处理:                                            │
│  ┌─────────────────────────────────────────────────┐       │
│  │  1. 检测到断线                                     │       │
│  │  2. 保存本地状态                                   │       │
│  │  3. 显示重连中 UI                                  │       │
│  │  4. 尝试重连                                       │       │
│  │  5. 如果重连成功:                                   │       │
│  │     - 请求断线期间的消息                            │       │
│  │     - 恢复游戏状态                                  │       │
│  │     - 隐藏重连 UI                                   │       │
│  │  6. 如果重连失败:                                   │       │
│  │     - 显示重新登录按钮                              │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  服务器断线处理:                                            │
│  ┌─────────────────────────────────────────────────┐       │
│  │  1. 检测到客户端超时                               │       │
│  │  2. 保存玩家数据到数据库                           │       │
│  │  3. 通知其他玩家玩家离开                           │       │
│  │  4. 缓存玩家状态 (短暂保留，等待重连)               │       │
│  │  5. 销毁/暂停实体                                  │       │
│  │  6. 如果玩家重连成功:                              │       │
│  │     - 恢复实体                                     │       │
│  │     - 发送断线期间的消息                            │       │
│  │  7. 如果超时未重连:                                │       │
│  │     - 完全清理                                     │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 5.3 状态恢复设计

```cpp
// 断线状态恢复

class GameStateRecovery {
public:
    // 服务器端保存断线玩家状态
    void saveDisconnectedPlayerState(Player* player) {
        DisconnectedPlayerState state;
        state.playerId = player->id();
        state.accountId = player->accountId();
        state.lastPosition = player->position();
        state.lastHP = player->hp();
        state.lastMP = player->mp();
        state.buffList = player->getBuffs();
        state.inventory = player->getInventory();
        state.disconnectTime = getCurrentTime();

        // 保存到内存 (短暂保留)
        disconnectedPlayers_[player->accountId()] = state;

        // 同时保存到数据库 (持久化)
        player->saveToDatabase();
    }

    // 玩家重连时恢复状态
    bool restorePlayerState(uint32_t accountId, Player* outPlayer) {
        auto it = disconnectedPlayers_.find(accountId);
        if (it == disconnectedPlayers_.end()) {
            // 超时清理，从数据库加载
            return loadPlayerFromDatabase(accountId, outPlayer);
        }

        const DisconnectedPlayerState& state = it->second;

        // 检查是否超时
        if (getCurrentTime() - state.disconnectTime > STATE_TIMEOUT) {
            // 超时，从数据库加载
            disconnectedPlayers_.erase(it);
            return loadPlayerFromDatabase(accountId, outPlayer);
        }

        // 恢复状态
        outPlayer->setPosition(state.lastPosition);
        outPlayer->setHP(state.lastHP);
        outPlayer->setMP(state.lastMP);
        outPlayer->restoreBuffs(state.buffList);
        outPlayer->restoreInventory(state.inventory);

        // 清理临时状态
        disconnectedPlayers_.erase(it);

        return true;
    }

    // 获取断线期间的消息
    std::vector<Message> getMissedMessages(uint32_t playerId) {
        std::vector<Message> messages;

        auto it = messageBuffer_.find(playerId);
        if (it != messageBuffer_.end()) {
            messages = std::move(it->second);
            messageBuffer_.erase(it);
        }

        return messages;
    }

    // 为断线玩家缓存消息
    void bufferMessage(uint32_t playerId, const Message& msg) {
        messageBuffer_[playerId].push_back(msg);

        // 限制缓冲大小
        if (messageBuffer_[playerId].size() > MAX_BUFFERED_MESSAGES) {
            messageBuffer_[playerId].erase(
                messageBuffer_[playerId].begin()
            );
        }
    }

private:
    struct DisconnectedPlayerState {
        uint32_t playerId;
        uint32_t accountId;
        Position lastPosition;
        float lastHP;
        float lastMP;
        std::vector<Buff> buffList;
        Inventory inventory;
        uint32_t disconnectTime;
    };

    std::unordered_map<uint32_t, DisconnectedPlayerState> disconnectedPlayers_;
    std::unordered_map<uint32_t, std::vector<Message>> messageBuffer_;

    static constexpr uint32_t STATE_TIMEOUT = 300; // 5分钟
    static constexpr size_t MAX_BUFFERED_MESSAGES = 100;
};
```

---

## 六、总结

### 心跳重连方案对比

| 方案 | 心跳间隔 | 超时阈值 | 重连策略 | 适用场景 |
|------|----------|----------|----------|----------|
| **标准** | 5秒 | 15秒 | 指数退避 | 大部分 MMO |
| **快速响应** | 3秒 | 9秒 | 立即重连 | 竞技游戏 |
| **低流量** | 10秒 | 30秒 | 固定延迟 | 休闲游戏 |
| **高可靠** | 5秒 | 25秒 | 指数+抖动 | 关键业务 |

### KBEngine 心跳机制

```
KBEngine 默认配置:
- 客户端超时: 60 秒
- 心跳间隔: 5 秒 (可配置)
- 内部连接超时: 300 秒

特点:
1. 基于 Channel 的超时检测
2. 自动保存玩家数据
3. 支持断线重连
```

---

## 参考资料

- [KBEngine GitHub - Channel 超时](https://github.com/kbengine/kbengine/blob/master/kbe/src/lib/network/channel.h)
- [TCP Keepalive 详解](https://tldp.org/HOWTO/TCP-Keepalive-HOWTO/)
- [WebSocket 心跳最佳实践](https://tools.ietf.org/html/rfc6455)
