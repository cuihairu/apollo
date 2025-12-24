#pragma once

#include "apollo/net/net.h"
#include <atomic>
#include <cstdio>

// 定义使用 SDNet 后端
#define APOLLO_USE_SDNET

#ifdef APOLLO_USE_SDNET

// SSEngine 命名空间和类型定义
// 注意: 实际使用时需要包含 SSEngine 头文件并配置正确的 include 路径
namespace SSCP {

// 基本类型定义 (与 SSEngine 兼容)
using INT8 = signed char;
using UINT8 = unsigned char;
using INT16 = signed short;
using UINT16 = unsigned short;
using INT32 = signed int;
using UINT32 = unsigned int;
using INT64 = long long;
using UINT64 = unsigned long long;

// 版本结构
struct SSSVersion {
    UINT16 wMajorVersion;
    UINT16 wMinorVersion;
    UINT16 wCompatibleVersion;
    UINT16 wBuildNumber;
};

// 基础接口
class ISSBase {
public:
    virtual ~ISSBase() {}
    virtual void AddRef() = 0;
    virtual UINT32 QueryRef() = 0;
    virtual void Release() = 0;
    virtual SSSVersion GetVersion() = 0;
    virtual const char* GetModuleName() = 0;
};

// 前向声明
class ISSConnection;
class ISSPacketParser;
class ISSSessionFactory;

// 网络错误码
enum ESDNetErrCode {
    NET_BIND_FAIL = -7,
    NET_CONNECT_FAIL = -6,
    NET_SYSTEM_ERROR = -5,
    NET_RECV_ERROR = -4,
    NET_SEND_ERROR = -3,
    NET_SEND_OVERFLOW = -2,
    NET_PACKET_ERROR = -1,
    NET_SUCCESS = 0
};

// I/O 类型
constexpr UINT32 NETIO_COMPLETIONPORT = 1;
constexpr UINT32 NETIO_COMPLETIONPORT_GATE = 11;
constexpr UINT32 NETIO_ASYNCSELECT = 2;
constexpr UINT32 NETIO_EPOLL = 10;
constexpr UINT32 NETIO_EPOLL_GATE = 101;

/**
 * @brief SSEngine 连接接口
 */
class ISSConnection {
public:
    virtual ~ISSConnection() {}
    virtual bool IsConnected() = 0;
    virtual void Send(const char* pBuf, UINT32 dwLen) = 0;
    virtual void DelaySend(const char* pBuf, UINT32 dwLen) = 0;
    virtual void SetOpt(UINT32 dwType, void* pOpt) = 0;
    virtual void Disconnect() = 0;
    virtual const UINT32 GetRemoteIP() = 0;
    virtual const char* GetRemoteIPStr() = 0;
    virtual UINT16 GetRemotePort() = 0;
    virtual const UINT32 GetLocalIP() = 0;
    virtual const char* GetLocalIPStr() = 0;
    virtual UINT16 GetLocalPort() = 0;
    virtual UINT32 GetSendBufFree() = 0;
};

/**
 * @brief SSEngine 会话接口
 */
class ISSSession {
public:
    virtual ~ISSSession() {}
    virtual void SetConnection(ISSConnection* poConnection) = 0;
    virtual void OnEstablish() = 0;
    virtual void OnTerminate() = 0;
    virtual bool OnError(INT32 nModuleErr, INT32 nSysErr) = 0;
    virtual void OnRecv(const char* pBuf, UINT32 dwLen) = 0;
    virtual void Release() = 0;
    virtual void ActiveClose() {}
    virtual bool IsActiveClose() { return false; }
    virtual void SetActiveClose(bool bFlag) {}
};

/**
 * @brief SSEngine 数据包解析器接口
 */
class ISSPacketParser {
public:
    virtual ~ISSPacketParser() {}
    virtual INT32 ParsePacket(const char* pBuf, UINT32 dwLen) = 0;
    virtual INT32 ParseFirstPacket(const char* pBuf, UINT32 dwLen) {
        return ParsePacket(pBuf, dwLen);
    }
};

/**
 * @brief SSEngine 会话工厂接口
 */
class ISSSessionFactory {
public:
    virtual ~ISSSessionFactory() {}
    virtual ISSSession* CreateSession(ISSConnection* poConnection) = 0;
};

/**
 * @brief SSEngine 监听器接口
 */
class ISSListener {
public:
    virtual ~ISSListener() {}
    virtual void SetPacketParser(ISSPacketParser* poPacketParser) = 0;
    virtual void SetSessionFactory(ISSSessionFactory* poSessionFactory) = 0;
    virtual void SetBufferSize(UINT32 dwRecvBufSize, UINT32 dwSendBufSize) = 0;
    virtual void SetOpt(UINT32 dwType, void* pOpt) = 0;
    virtual bool Start(const char* pszIP, UINT16 wPort, bool bReUseAddr = true) = 0;
    virtual bool Stop() = 0;
    virtual void Release() = 0;
};

/**
 * @brief SSEngine 连接器接口
 */
class ISSConnector {
public:
    virtual ~ISSConnector() {}
    virtual void SetPacketParser(ISSPacketParser* poPakcetParser) = 0;
    virtual void SetSession(ISSSession* poSession) = 0;
    virtual ISSSession* GetSession() = 0;
    virtual void SetBufferSize(UINT32 dwRecvBufSize, UINT32 dwSendBufSize) = 0;
    virtual int Connect(const char* pszIP, UINT16 wPort) = 0;
    virtual int ReConnect() = 0;
    virtual void Release() = 0;
    virtual void SetOpt(UINT32 dwType, void* pOpt) = 0;
};

/**
 * @brief SSEngine 网络模块接口
 */
class ISSNet : public ISSBase {
public:
    virtual ~ISSNet() {}
    virtual ISSConnector* CreateConnector(UINT32 dwNetIOType) = 0;
    virtual ISSListener* CreateListener(UINT32 dwNetIOType) = 0;
    virtual bool Run(INT32 nCount = -1) = 0;
};

// SDNet 模块获取函数 (需要在实际链接时提供)
// ISSNet* SSNetGetModule(const SSSVersion* pstVersion);

} // namespace SSCP

namespace apollo {
namespace net {
namespace adapters {

/**
 * @brief SDNet 网络后端标识
 */
constexpr const char* SDNET_BACKEND = "sdnet";

/**
 * @brief 获取当前时间戳(毫秒)
 */
inline uint64_t getCurrentTimeMs() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

/**
 * @brief IP 地址转换工具
 */
class IpAddressConverter {
public:
    /**
     * @brief UINT32 IP 转字符串
     */
    static std::string ipToString(uint32_t ip) {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u",
                 ip & 0xFF,
                 (ip >> 8) & 0xFF,
                 (ip >> 16) & 0xFF,
                 (ip >> 24) & 0xFF);
        return std::string(buffer);
    }

    /**
     * @brief 字符串转 UINT32 IP
     */
    static uint32_t ipFromString(const std::string& ip) {
        unsigned int a = 0, b = 0, c = 0, d = 0;
        if (sscanf(ip.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
            return (d << 24) | (c << 16) | (b << 8) | a;
        }
        return 0;
    }
};

// ========== SDNet 具体实现类 ==========

/**
 * @brief SDNet 连接实现
 *
 * 包装 ISSConnection 接口，实现 Apollo 的 Connection 抽象
 */
class SDNetConnection : public Connection {
public:
    explicit SDNetConnection(SSCP::ISSConnection* nativeConn = nullptr)
        : nativeConnection_(nativeConn)
        , userData_(nullptr)
        , connectionId_(0)
        , connectTime_(getCurrentTimeMs())
        , lastActiveTime_(connectTime_)
        , sentBytes_(0)
        , receivedBytes_(0) {
        if (nativeConnection_) {
            remoteAddress_ = nativeConnection_->GetRemoteIPStr();
            localAddress_ = nativeConnection_->GetLocalIPStr();
        }
    }

    ~SDNetConnection() override {
        // 由 SDNet 管理生命周期，不在这里释放
    }

    // Connection 接口实现
    bool isConnected() const override {
        return nativeConnection_ && nativeConnection_->IsConnected();
    }

    ConnectionState getState() const override {
        if (isConnected()) return ConnectionState::Connected;
        return ConnectionState::Disconnected;
    }

    int32_t send(const char* data, uint32_t length) override {
        if (!nativeConnection_ || !isConnected()) return -1;
        nativeConnection_->Send(data, length);
        sentBytes_ += length;
        lastActiveTime_ = getCurrentTimeMs();
        return static_cast<int32_t>(length);
    }

    bool sendAsync(const char* data, uint32_t length) override {
        if (!nativeConnection_ || !isConnected()) return false;
        nativeConnection_->DelaySend(data, length);
        sentBytes_ += length;
        lastActiveTime_ = getCurrentTimeMs();
        return true;
    }

    void disconnect(const char* reason) override {
        (void)reason; // SDNet 不支持断开原因
        if (nativeConnection_) {
            nativeConnection_->Disconnect();
        }
    }

    void close() override {
        disconnect(nullptr);
    }

    const std::string& getRemoteAddress() const override {
        static std::string empty;
        return nativeConnection_ ? remoteAddress_ : empty;
    }

    uint16_t getRemotePort() const override {
        return nativeConnection_ ? nativeConnection_->GetRemotePort() : 0;
    }

    const std::string& getLocalAddress() const override {
        static std::string empty;
        return nativeConnection_ ? localAddress_ : empty;
    }

    uint16_t getLocalPort() const override {
        return nativeConnection_ ? nativeConnection_->GetLocalPort() : 0;
    }

    uint64_t getConnectionId() const override {
        return connectionId_;
    }

    void* getUserData() const override {
        return userData_;
    }

    void setUserData(void* data) override {
        userData_ = data;
    }

    uint64_t getLastActiveTime() const override {
        return lastActiveTime_;
    }

    uint64_t getConnectTime() const override {
        return connectTime_;
    }

    uint64_t getSentBytes() const override {
        return sentBytes_;
    }

    uint64_t getReceivedBytes() const override {
        return receivedBytes_;
    }

    // SDNet 特定方法
    SSCP::ISSConnection* getNativeConnection() const { return nativeConnection_; }
    void setNativeConnection(SSCP::ISSConnection* conn) {
        nativeConnection_ = conn;
        if (conn) {
            remoteAddress_ = conn->GetRemoteIPStr();
            localAddress_ = conn->GetLocalIPStr();
        }
    }

    void setConnectionId(uint64_t id) { connectionId_ = id; }
    void addReceivedBytes(uint64_t bytes) { receivedBytes_ += bytes; }

private:
    SSCP::ISSConnection* nativeConnection_;
    void* userData_;
    uint64_t connectionId_;
    uint64_t connectTime_;
    uint64_t lastActiveTime_;
    uint64_t sentBytes_;
    uint64_t receivedBytes_;
    std::string remoteAddress_;
    std::string localAddress_;
};

/**
 * @brief SDNet 数据包解析器适配器
 *
 * 将抽象接口适配到 ISSPacketParser
 */
class SDNetPacketParser : public SSCP::ISSPacketParser {
public:
    using ParseCallback = std::function<INT32(const char*, UINT32)>;

    SDNetPacketParser() : parseCallback_(nullptr) {}
    ~SDNetPacketParser() override = default;

    // ISSPacketParser 接口实现
    INT32 ParsePacket(const char* pBuf, UINT32 dwLen) override {
        if (parseCallback_) {
            return parseCallback_(pBuf, dwLen);
        }
        // 默认实现: 简单的长度前缀协议
        if (dwLen < 4) return 0;
        UINT32 packetLen = *reinterpret_cast<const UINT32*>(pBuf);
        if (packetLen > 64 * 1024) return -1; // 最大包大小限制
        if (dwLen < 4 + packetLen) return 0;
        return static_cast<INT32>(4 + packetLen);
    }

    INT32 ParseFirstPacket(const char* pBuf, UINT32 dwLen) override {
        return ParsePacket(pBuf, dwLen);
    }

    void setParseCallback(ParseCallback callback) {
        parseCallback_ = std::move(callback);
    }

private:
    ParseCallback parseCallback_;
};

/**
 * @brief SDNet 会话基类
 *
 * 适配 ISSSession 接口，同时提供 Apollo 抽象接口
 */
class SDNetSessionBase : public SSCP::ISSSession {
public:
    SDNetSessionBase()
        : apolloSession_(nullptr)
        , nativeConnection_(nullptr)
        , sessionId_(0)
        , lastHeartbeat_(0)
        , active_(false)
        , activeClose_(false) {}

    virtual ~SDNetSessionBase() = default;

    // ISSSession 接口实现 (由 SDNet 调用)
    void SetConnection(SSCP::ISSConnection* poConnection) override {
        nativeConnection_ = poConnection;
        if (apolloSession_) {
            apolloSession_->setConnection(
                poConnection ? &sdnetConnection_ : nullptr
            );
        }
    }

    void OnEstablish() override {
        active_ = true;
        lastHeartbeat_ = getCurrentTimeMs();
        sdnetConnection_.setNativeConnection(nativeConnection_);
        if (apolloSession_) {
            apolloSession_->onEstablish(&sdnetConnection_);
        }
        handleEstablish();
    }

    void OnTerminate() override {
        active_ = false;
        if (apolloSession_) {
            apolloSession_->onTerminate(
                activeClose_ ? TerminateReason::UserClose : TerminateReason::RemoteClose
            );
        }
        handleTerminate(activeClose_ ? TerminateReason::UserClose : TerminateReason::RemoteClose);
        nativeConnection_ = nullptr;
    }

    bool OnError(INT32 nModuleErr, INT32 nSysErr) override {
        (void)nSysErr;
        if (apolloSession_) {
            apolloSession_->onError(nModuleErr, "");
        }
        handleError(nModuleErr, "");
        return true;
    }

    void OnRecv(const char* pBuf, UINT32 dwLen) override {
        lastHeartbeat_ = getCurrentTimeMs();
        if (apolloSession_) {
            apolloSession_->onRecv(pBuf, dwLen);
        }
        handlePacket(pBuf, dwLen);
    }

    void Release() override {
        delete this;
    }

    void ActiveClose() override { activeClose_ = true; }
    bool IsActiveClose() override { return activeClose_; }
    void SetActiveClose(bool bFlag) override { activeClose_ = bFlag; }

    // Apollo 会话相关
    void setApolloSession(Session* session) {
        apolloSession_ = session;
    }

    Session* getApolloSession() const { return apolloSession_; }

    // 状态查询
    bool isActive() const { return active_; }
    uint64_t getLastHeartbeat() const { return lastHeartbeat_; }
    bool isTimeout(uint64_t currentTime, uint64_t timeoutMs) const {
        if (lastHeartbeat_ == 0) return false;
        return (currentTime - lastHeartbeat_) >= timeoutMs;
    }

protected:
    /**
     * @brief 子类重写以处理具体的业务逻辑
     */
    virtual void handleEstablish() {}
    virtual void handleTerminate(TerminateReason reason) {}
    virtual void handlePacket(const char* data, uint32_t length) {}
    virtual void handleError(int32_t errorCode, const char* errorMsg) {}

    Session* apolloSession_;
    SSCP::ISSConnection* nativeConnection_;
    uint64_t sessionId_;
    uint64_t lastHeartbeat_;
    bool active_;
    bool activeClose_;
    SDNetConnection sdnetConnection_;
};

/**
 * @brief SDNet 会话适配器
 *
 * 实现 Apollo 的 Session 接口，内部委托给 SDNetSessionBase
 */
class SDNetSession : public Session {
public:
    explicit SDNetSession(SDNetSessionBase* nativeSession)
        : nativeSession_(nativeSession)
        , connection_(nullptr)
        , sessionId_(generateSessionId())
        , lastHeartbeat_(0)
        , active_(false) {
        if (nativeSession_) {
            nativeSession_->setApolloSession(this);
            nativeSession_->sessionId_ = sessionId_;
        }
    }

    ~SDNetSession() override {
        // 由 SDNet 管理生命周期
    }

    // Session 接口实现
    void onEstablish(Connection* connection) override {
        connection_ = connection;
        active_ = true;
        updateHeartbeat();
    }

    void onTerminate(TerminateReason reason) override {
        (void)reason;
        active_ = false;
        connection_ = nullptr;
    }

    uint32_t onRecv(const char* data, uint32_t length) override {
        updateHeartbeat();
        return length; // 由 SDNet 调用 native session
    }

    void onError(int32_t errorCode, const char* errorMsg) override {
        (void)errorCode;
        (void)errorMsg;
    }

    uint64_t getSessionId() const override { return sessionId_; }

    Connection* getConnection() const override { return connection_; }

    void setConnection(Connection* connection) override {
        connection_ = connection;
    }

    bool isActive() const override {
        return active_ || (nativeSession_ && nativeSession_->isActive());
    }

    void updateHeartbeat() override {
        lastHeartbeat_ = getCurrentTimeMs();
    }

    uint64_t getLastHeartbeat() const override {
        if (nativeSession_) {
            return nativeSession_->getLastHeartbeat();
        }
        return lastHeartbeat_;
    }

    bool isTimeout(uint64_t currentTime, uint64_t timeoutMs) const override {
        if (nativeSession_) {
            return nativeSession_->isTimeout(currentTime, timeoutMs);
        }
        if (lastHeartbeat_ == 0) return false;
        return (currentTime - lastHeartbeat_) >= timeoutMs;
    }

    // SDNet 特定方法
    SDNetSessionBase* getNativeSession() const { return nativeSession_; }

private:
    static uint64_t generateSessionId() {
        static std::atomic<uint64_t> counter{1};
        return counter.fetch_add(1);
    }

    SDNetSessionBase* nativeSession_;
    Connection* connection_;
    uint64_t sessionId_;
    uint64_t lastHeartbeat_;
    bool active_;
};

/**
 * @brief SDNet 会话工厂适配器
 *
 * 将 Apollo 的 SessionFactory 适配到 SDNet 的 ISSSessionFactory
 */
class SDNetSessionFactory : public SSCP::ISSSessionFactory {
public:
    explicit SDNetSessionFactory(SessionFactoryPtr apolloFactory)
        : apolloFactory_(std::move(apolloFactory)) {}

    ~SDNetSessionFactory() override = default;

    // ISSSessionFactory 接口实现
    SSCP::ISSSession* CreateSession(SSCP::ISSConnection* poConnection) override {
        if (!apolloFactory_) return nullptr;

        // 创建 Apollo 会话
        SessionPtr apolloSession = apolloFactory_->createSession();
        if (!apolloSession) return nullptr;

        // 创建 SDNet 原生会话
        auto* nativeSession = createNativeSession(apolloSession);
        if (nativeSession) {
            nativeSession->SetConnection(poConnection);
            // 保存会话映射
            sessions_[poConnection] = apolloSession;
        }

        return nativeSession;
    }

    SessionPtr getApolloSession(SSCP::ISSConnection* connection) const {
        auto it = sessions_.find(connection);
        return it != sessions_.end() ? it->second : nullptr;
    }

    void removeSession(SSCP::ISSConnection* connection) {
        sessions_.erase(connection);
    }

private:
    virtual SDNetSessionBase* createNativeSession(const SessionPtr& apolloSession) = 0;

    SessionFactoryPtr apolloFactory_;
    std::unordered_map<SSCP::ISSConnection*, SessionPtr> sessions_;
};

/**
 * @brief 通用 SDNet 会话工厂模板
 *
 * @tparam NativeSessionType SDNet 原生会话类型，必须继承自 SDNetSessionBase
 */
template <typename NativeSessionType>
class TemplateSDNetSessionFactory : public SDNetSessionFactory {
public:
    explicit TemplateSDNetSessionFactory(SessionFactoryPtr apolloFactory)
        : SDNetSessionFactory(std::move(apolloFactory)) {}

private:
    SDNetSessionBase* createNativeSession(const SessionPtr& apolloSession) override {
        auto* nativeSession = new NativeSessionType();
        nativeSession->setApolloSession(apolloSession.get());
        return nativeSession;
    }
};

/**
 * @brief SDNet 监听器实现
 */
class SDNetListener : public Listener {
public:
    SDNetListener() : SDNetListener(nullptr) {}

    explicit SDNetListener(SSCP::ISSListener* nativeListener)
        : nativeListener_(nativeListener)
        , maxConnections_(1000)
        , noDelay_(true)
        , recvBufferSize_(8192)
        , sendBufferSize_(8192)
        , running_(false) {}

    ~SDNetListener() override {
        stop();
    }

    // Listener 接口实现
    bool start(const std::string& ip, uint16_t port) override {
        if (!nativeListener_) return false;

        if (recvBufferSize_ > 0 || sendBufferSize_ > 0) {
            nativeListener_->SetBufferSize(recvBufferSize_, sendBufferSize_);
        }

        running_ = nativeListener_->Start(ip.c_str(), port);
        return running_;
    }

    bool stop() override {
        if (!nativeListener_ || !running_) return true;
        running_ = nativeListener_->Stop();
        return running_;
    }

    bool isRunning() const override { return running_; }

    const std::string& getListenAddress() const override { return listenAddress_; }
    uint16_t getListenPort() const override { return listenPort_; }

    uint32_t getConnectionCount() const override {
        // 需要维护连接计数
        return 0;
    }

    uint32_t getMaxConnections() const override { return maxConnections_; }

    void setMaxConnections(uint32_t maxConn) override {
        maxConnections_ = maxConn;
    }

    void setSessionFactory(SessionFactoryPtr factory) override {
        sessionFactory_ = std::move(factory);
        if (nativeListener_ && sessionFactory_) {
            // 创建适配的 SDNet 工厂
            auto* sdnetFactory = createSDNetFactory(sessionFactory_);
            nativeListener_->SetSessionFactory(sdnetFactory);
        }
    }

    void setPacketParser(PacketParserPtr parser) override {
        packetParser_ = std::move(parser);
        if (nativeListener_ && packetParser_) {
            // 创建适配的 SDNet 解析器
            auto* sdnetParser = createSDNetParser(packetParser_);
            nativeListener_->SetPacketParser(sdnetParser);
        }
    }

    void setCallbacks(const ListenerCallbacks& callbacks) override {
        callbacks_ = callbacks;
    }

    bool disconnectConnection(uint64_t connectionId, const char* reason) override {
        (void)connectionId;
        (void)reason;
        // SDNet 通过 ISSConnection::Disconnect() 断开连接
        return false;
    }

    uint32_t disconnectAll(const char* reason) override {
        (void)reason;
        return 0;
    }

    ConnectionPtr getConnection(uint64_t connectionId) const override {
        (void)connectionId;
        return nullptr;
    }

    void setNoDelay(bool enable) override { noDelay_ = enable; }

    void setRecvBufferSize(int32_t size) override {
        recvBufferSize_ = size;
    }

    void setSendBufferSize(int32_t size) override {
        sendBufferSize_ = size;
    }

    // SDNet 特定方法
    SSCP::ISSListener* getNativeListener() const { return nativeListener_; }
    void setNativeListener(SSCP::ISSListener* listener) {
        nativeListener_ = listener;
    }

private:
    virtual SSCP::ISSSessionFactory* createSDNetFactory(const SessionFactoryPtr&) {
        // 子类需要重写此方法创建具体的 SDNet 工厂
        return nullptr;
    }

    virtual SSCP::ISSPacketParser* createSDNetParser(const PacketParserPtr&) {
        // 子类需要重写此方法创建具体的 SDNet 解析器
        return nullptr;
    }

    SSCP::ISSListener* nativeListener_;
    SessionFactoryPtr sessionFactory_;
    PacketParserPtr packetParser_;
    ListenerCallbacks callbacks_;
    uint32_t maxConnections_;
    bool noDelay_;
    int32_t recvBufferSize_;
    int32_t sendBufferSize_;
    std::string listenAddress_;
    uint16_t listenPort_ = 0;
    bool running_;
};

/**
 * @brief SDNet 连接器实现
 */
class SDNetConnector : public Connector {
public:
    SDNetConnector() : SDNetConnector(nullptr) {}

    explicit SDNetConnector(SSCP::ISSConnector* nativeConnector)
        : nativeConnector_(nativeConnector)
        , state_(ConnectorState::Stopped) {}

    ~SDNetConnector() override {
        stop();
    }

    // Connector 接口实现
    int32_t connect(const ConnectConfig& config) override {
        config_ = config;
        return connect(config.host, config.port);
    }

    int32_t connect(const std::string& host, uint16_t port) override {
        if (!nativeConnector_) return -1;

        state_ = ConnectorState::Connecting;

        int result = nativeConnector_->Connect(host.c_str(), port);

        if (result >= 0) {
            state_ = ConnectorState::Connected;
        } else {
            state_ = ConnectorState::Failed;
            if (callbacks_.onConnectFailed) {
                callbacks_.onConnectFailed(result, "Connection failed");
            }
        }

        return result;
    }

    int32_t reconnect() override {
        if (!config_.host.empty()) {
            state_ = ConnectorState::Reconnecting;
            return nativeConnector_ ? nativeConnector_->ReConnect() : -1;
        }
        return -1;
    }

    void disconnect() override {
        if (nativeConnector_) {
            SSCP::ISSSession* session = nativeConnector_->GetSession();
            if (session) {
                session->ActiveClose();
            }
        }
        state_ = ConnectorState::Stopped;
        if (connection_) {
            connection_.reset();
        }
    }

    void stop() override {
        disconnect();
    }

    ConnectorState getState() const override { return state_; }

    ConnectionPtr getConnection() const override { return connection_; }

    const ConnectConfig& getConfig() const override { return config_; }

    void updateConfig(const ConnectConfig& config) override {
        config_ = config;
    }

    void setCallbacks(const ConnectorCallbacks& callbacks) override {
        callbacks_ = callbacks;
    }

    void setPacketParser(PacketParserPtr parser) override {
        packetParser_ = std::move(parser);
        if (nativeConnector_ && packetParser_) {
            // 创建适配的 SDNet 解析器
            auto* sdnetParser = createSDNetParser(packetParser_);
            nativeConnector_->SetPacketParser(sdnetParser);
        }
    }

    bool isConnected() const {
        return state_ == ConnectorState::Connected;
    }

    bool isConnecting() const {
        return state_ == ConnectorState::Connecting ||
               state_ == ConnectorState::Reconnecting;
    }

    // SDNet 特定方法
    SSCP::ISSConnector* getNativeConnector() const { return nativeConnector_; }
    void setNativeConnector(SSCP::ISSConnector* connector) {
        nativeConnector_ = connector;
    }

    void setConnection(ConnectionPtr conn) {
        connection_ = std::move(conn);
    }

    void onConnected() {
        state_ = ConnectorState::Connected;
        if (callbacks_.onConnected && connection_) {
            callbacks_.onConnected(connection_);
        }
    }

    void onDisconnected(const char* reason) {
        state_ = ConnectorState::Stopped;
        if (callbacks_.onDisconnected) {
            callbacks_.onDisconnected(reason);
        }
    }

private:
    virtual SSCP::ISSPacketParser* createSDNetParser(const PacketParserPtr&) {
        return nullptr;
    }

    SSCP::ISSConnector* nativeConnector_;
    ConnectionPtr connection_;
    PacketParserPtr packetParser_;
    ConnectorCallbacks callbacks_;
    ConnectConfig config_;
    ConnectorState state_;
};

/**
 * @brief SDNet 网络管理器实现
 */
class SDNetNetworkManager : public NetworkManager {
public:
    SDNetNetworkManager()
        : nativeNetwork_(nullptr)
        , running_(false)
        , ioType_(0) {

        // 根据平台选择默认 I/O 类型
#ifdef _WIN32
        ioType_ = SSCP::NETIO_COMPLETIONPORT;
#else
        ioType_ = SSCP::NETIO_EPOLL;
#endif
    }

    ~SDNetNetworkManager() override {
        stop();
    }

    // NetworkManager 接口实现
    bool initialize(const NetworkConfig& config) override {
        config_ = config;

        // 获取 SDNet 模块
        // nativeNetwork_ = SSCP::SSNetGetModule(&SSCP::SDNET_VERSION);
        // if (!nativeNetwork_) {
        //     return false;
        // }

        return true;
    }

    bool start() override {
        if (running_) return true;
        running_ = true;
        return true;
    }

    void stop() override {
        if (!running_) return;

        // 停止所有监听器
        for (auto& kv : listeners_) {
            kv.second->stop();
        }
        listeners_.clear();

        // 停止所有连接器
        for (auto& kv : connectors_) {
            kv.second->stop();
        }
        connectors_.clear();

        // 释放 SDNet 模块
        // if (nativeNetwork_) {
        //     nativeNetwork_->Release();
        //     nativeNetwork_ = nullptr;
        // }

        running_ = false;
    }

    bool isRunning() const override { return running_; }

    void update() override {
        if (!running_ || !nativeNetwork_) return;

        // 处理网络事件 (非阻塞)
        // nativeNetwork_->Run(0);
    }

    ListenerPtr createListener(const std::string& name) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (listeners_.find(name) != listeners_.end()) {
            return nullptr;
        }

        // 创建 SDNet 监听器
        // SSCP::ISSListener* nativeListener = nativeNetwork_->CreateListener(ioType_);
        // if (!nativeListener) {
        //     return nullptr;
        // }

        auto listener = std::make_shared<SDNetListener>(nullptr);
        listeners_[name] = listener;
        return listener;
    }

    ListenerPtr getListener(const std::string& name) const override {
        auto it = listeners_.find(name);
        return it != listeners_.end() ? it->second : nullptr;
    }

    bool removeListener(const std::string& name) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = listeners_.find(name);
        if (it != listeners_.end()) {
            it->second->stop();
            listeners_.erase(it);
            return true;
        }
        return false;
    }

    void removeAllListeners() override {
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto& kv : listeners_) {
            kv.second->stop();
        }
        listeners_.clear();
    }

    ConnectorPtr createConnector(const std::string& name) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (connectors_.find(name) != connectors_.end()) {
            return nullptr;
        }

        // 创建 SDNet 连接器
        // SSCP::ISSConnector* nativeConnector = nativeNetwork_->CreateConnector(ioType_);
        // if (!nativeConnector) {
        //     return nullptr;
        // }

        auto connector = std::make_shared<SDNetConnector>(nullptr);
        connectors_[name] = connector;
        return connector;
    }

    ConnectorPtr getConnector(const std::string& name) const override {
        auto it = connectors_.find(name);
        return it != connectors_.end() ? it->second : nullptr;
    }

    bool removeConnector(const std::string& name) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = connectors_.find(name);
        if (it != connectors_.end()) {
            it->second->stop();
            connectors_.erase(it);
            return true;
        }
        return false;
    }

    void removeAllConnectors() override {
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto& kv : connectors_) {
            kv.second->stop();
        }
        connectors_.clear();
    }

    ConnectionPtr getConnection(uint64_t connectionId) const override {
        (void)connectionId;
        return nullptr;
    }

    bool disconnectConnection(uint64_t connectionId, const char* reason) override {
        (void)connectionId;
        (void)reason;
        return false;
    }

    uint32_t broadcast(const char* data, uint32_t length,
                       const std::vector<uint64_t>& excludeIds) override {
        (void)excludeIds;
        uint32_t count = 0;
        for (const auto& kv : listeners_) {
            (void)kv;
            // TODO: 向监听器的所有连接广播
        }
        return count;
    }

    NetworkStats getStats() const override { return stats_; }

    void resetStats() const override {
        stats_ = NetworkStats{};
    }

    const NetworkConfig& getConfig() const override { return config_; }

    // SDNet 特定方法
    SSCP::ISSNet* getNativeNetwork() const { return nativeNetwork_; }
    void setNativeNetwork(SSCP::ISSNet* network) { nativeNetwork_ = network; }

    void setIOType(uint32_t ioType) { ioType_ = ioType; }

private:
    SSCP::ISSNet* nativeNetwork_;
    NetworkConfig config_;
    mutable NetworkStats stats_;
    std::unordered_map<std::string, ListenerPtr> listeners_;
    std::unordered_map<std::string, ConnectorPtr> connectors_;
    mutable std::mutex mutex_;
    bool running_;
    uint32_t ioType_;
};

} // namespace adapters
} // namespace net
} // namespace apollo

#endif // APOLLO_USE_SDNET
