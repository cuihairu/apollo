#include "apollo/net/adapters/native_adapter.h"
#include <iostream>
#include <algorithm>

namespace apollo {
namespace net {
namespace adapters {

// ========== 静态工具函数 ==========

inline uint64_t getCurrentTimeMs() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

// ========== NativeConnection ==========

NativeConnection::NativeConnection(socket_t sock, uint64_t connId,
                                   const std::string& remoteAddr, uint16_t remotePort)
    : socket_(sock)
    , connectionId_(connId)
    , remoteAddress_(remoteAddr)
    , remotePort_(remotePort)
    , localPort_(0)
    , userData_(nullptr)
    , connectTime_(getCurrentTimeMs())
    , lastActiveTime_(connectTime_)
    , sentBytes_(0)
    , receivedBytes_(0)
    , shouldClose_(false)
    , state_(ConnectionState::Connected) {

    // 获取本地地址信息
    struct sockaddr_in localAddr;
    socklen_t addrLen = sizeof(localAddr);
    getsockname(socket_, (struct sockaddr*)&localAddr, &addrLen);
    localAddress_ = inet_ntoa(localAddr.sin_addr);
    localPort_ = ntohs(localAddr.sin_port);
}

NativeConnection::~NativeConnection() {
    close();
}

bool NativeConnection::isConnected() const {
    return state_ == ConnectionState::Connected && socket_ != INVALID_SOCKET_VALUE;
}

ConnectionState NativeConnection::getState() const {
    return state_;
}

int32_t NativeConnection::send(const char* data, uint32_t length) {
    if (!isConnected()) return -1;

    ssize_t sent = ::send(socket_, data, length, 0);
    if (sent > 0) {
        sentBytes_ += sent;
        lastActiveTime_ = getCurrentTimeMs();
        return static_cast<int32_t>(sent);
    }

#ifdef _WIN32
    int err = WSAGetLastError();
    if (err == WSAEWOULDBLOCK) {
        return 0; // 需要稍后重试
    }
#else
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return 0;
    }
#endif

    state_ = ConnectionState::Disconnected;
    return -1;
}

bool NativeConnection::sendAsync(const char* data, uint32_t length) {
    std::lock_guard<std::mutex> lock(sendMutex_);

    // 尝试直接发送
    ssize_t sent = ::send(socket_, data, length, 0);
    if (sent == length) {
        sentBytes_ += sent;
        lastActiveTime_ = getCurrentTimeMs();
        return true;
    }

#ifdef _WIN32
    int err = WSAGetLastError();
    if (sent < 0 && err != WSAEWOULDBLOCK) {
        state_ = ConnectionState::Disconnected;
        return false;
    }
#else
    if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        state_ = ConnectionState::Disconnected;
        return false;
    }
#endif

    // 部分发送或需要缓冲，加入队列
    size_t offset = (sent > 0) ? static_cast<size_t>(sent) : 0;
    std::vector<char> packet(data + offset, data + length);
    sendQueue_.push(std::move(packet));
    return true;
}

void NativeConnection::disconnect(const char* reason) {
    (void)reason;
    if (socket_ != INVALID_SOCKET_VALUE) {
        shutdownSocketBoth(socket_);
        closeSocket(socket_);
        socket_ = INVALID_SOCKET_VALUE;
    }
    state_ = ConnectionState::Disconnected;
}

void NativeConnection::close() {
    disconnect(nullptr);
}

void NativeConnection::updateActiveTime() {
    lastActiveTime_ = getCurrentTimeMs();
}

const std::string& NativeConnection::getRemoteAddress() const {
    return remoteAddress_;
}

uint16_t NativeConnection::getRemotePort() const {
    return remotePort_;
}

const std::string& NativeConnection::getLocalAddress() const {
    return localAddress_;
}

uint16_t NativeConnection::getLocalPort() const {
    return localPort_;
}

uint64_t NativeConnection::getConnectionId() const {
    return connectionId_;
}

void* NativeConnection::getUserData() const {
    return userData_;
}

void NativeConnection::setUserData(void* data) {
    userData_ = data;
}

uint64_t NativeConnection::getLastActiveTime() const {
    return lastActiveTime_;
}

uint64_t NativeConnection::getConnectTime() const {
    return connectTime_;
}

uint64_t NativeConnection::getSentBytes() const {
    return sentBytes_;
}

uint64_t NativeConnection::getReceivedBytes() const {
    return receivedBytes_;
}

bool NativeConnection::handleRecv() {
    char tempBuffer[8192];
    ssize_t recvLen = ::recv(socket_, tempBuffer, sizeof(tempBuffer), 0);

    if (recvLen > 0) {
        receivedBytes_ += recvLen;
        lastActiveTime_ = getCurrentTimeMs();
        recvBuffer_.write(tempBuffer, recvLen);

        // 解析并处理数据包
        if (packetHandler_) {
            while (recvBuffer_.readableBytes() >= sizeof(uint32_t)) {
                char peekBuf[4];
                recvBuffer_.peek(peekBuf, 4);
                uint32_t packetLen = *reinterpret_cast<uint32_t*>(peekBuf);

                if (packetLen > 64 * 1024) {
                    // 包太大，错误
                    markForClose();
                    return false;
                }

                if (recvBuffer_.readableBytes() < sizeof(uint32_t) + packetLen) {
                    // 数据不完整
                    break;
                }

                // 跳过长度头
                recvBuffer_.skip(sizeof(uint32_t));

                // 读取包数据
                std::vector<char> packetData(packetLen);
                recvBuffer_.read(packetData.data(), packetLen);

                packetHandler_(this, packetData.data(), packetLen);
            }
        }
        return true;
    }

#ifdef _WIN32
    int err = WSAGetLastError();
    if (recvLen < 0 && (err == WSAEWOULDBLOCK || err == WSAEINTR)) {
        return true; // 继续等待
    }
#else
    if (recvLen < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
        return true;
    }
#endif

    // 连接关闭或错误
    markForClose();
    return false;
}

bool NativeConnection::handleSend() {
    std::lock_guard<std::mutex> lock(sendMutex_);

    while (!sendQueue_.empty() && socket_ != INVALID_SOCKET_VALUE) {
        auto& packet = sendQueue_.front();
        ssize_t sent = ::send(socket_, packet.data(), packet.size(), 0);

        if (sent > 0) {
            sentBytes_ += sent;
            lastActiveTime_ = getCurrentTimeMs();
            if (static_cast<size_t>(sent) == packet.size()) {
                sendQueue_.pop();
            } else {
                // 部分发送，移除已发送部分
                packet.erase(packet.begin(), packet.begin() + sent);
                break;
            }
        } else {
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                break; // 稍后重试
            }
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
#endif
            markForClose();
            return false;
        }
    }

    return true;
}

// ========== NativeSession ==========

uint64_t NativeSession::generateSessionId() {
    static std::atomic<uint64_t> counter{1};
    return counter.fetch_add(1);
}

NativeSession::NativeSession(NativeConnection* conn)
    : connection_(conn)
    , sessionId_(generateSessionId())
    , lastHeartbeat_(getCurrentTimeMs())
    , active_(false) {
}

NativeSession::~NativeSession() {
    connection_ = nullptr;
}

void NativeSession::onEstablish(Connection* connection) {
    (void)connection;
    active_ = true;
    lastHeartbeat_ = getCurrentTimeMs();
    handleEstablish();
}

void NativeSession::onTerminate(TerminateReason reason) {
    active_ = false;
    handleTerminate(reason);
}

uint32_t NativeSession::onRecv(const char* data, uint32_t length) {
    lastHeartbeat_ = getCurrentTimeMs();
    return handlePacket(data, length);
}

void NativeSession::onError(int32_t errorCode, const char* errorMsg) {
    handleError(errorCode, errorMsg);
}

uint64_t NativeSession::getSessionId() const {
    return sessionId_;
}

Connection* NativeSession::getConnection() const {
    return connection_;
}

void NativeSession::setConnection(Connection* connection) {
    connection_ = static_cast<NativeConnection*>(connection);
}

bool NativeSession::isActive() const {
    return active_;
}

void NativeSession::updateHeartbeat() {
    lastHeartbeat_ = getCurrentTimeMs();
}

uint64_t NativeSession::getLastHeartbeat() const {
    return lastHeartbeat_;
}

bool NativeSession::isTimeout(uint64_t currentTime, uint64_t timeoutMs) const {
    if (lastHeartbeat_ == 0) return false;
    return (currentTime - lastHeartbeat_) >= timeoutMs;
}

// ========== NativeListener ==========

NativeListener::NativeListener()
    : listenSocket_(INVALID_SOCKET_VALUE)
    , listenPort_(0)
    , maxConnections_(1000)
    , currentConnections_(0)
    , nativeSessionFactory_(nullptr)
    , noDelay_(true)
    , recvBufferSize_(8192)
    , sendBufferSize_(8192)
    , running_(false)
    , nextConnectionId_(1) {
}

NativeListener::~NativeListener() {
    stop();
}

bool NativeListener::start(const std::string& ip, uint16_t port) {
    if (running_) return true;

    listenSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket_ == INVALID_SOCKET_VALUE) {
        return false;
    }

    // 设置套接字选项
    int reuseAddr = 1;
    setsockopt(listenSocket_, SOL_SOCKET, SO_REUSEADDR,
               (char*)&reuseAddr, sizeof(reuseAddr));

    // 绑定地址
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (ip.empty() || ip == "0.0.0.0" || ip == "*") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
    }

    if (bind(listenSocket_, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR_VALUE) {
        closeSocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET_VALUE;
        return false;
    }

    // 开始监听
    if (listen(listenSocket_, SOMAXCONN) == SOCKET_ERROR_VALUE) {
        closeSocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET_VALUE;
        return false;
    }

    listenAddress_ = ip;
    listenPort_ = port;
    running_ = true;

    return true;
}

bool NativeListener::stop() {
    if (!running_) return true;

    running_ = false;

    if (listenSocket_ != INVALID_SOCKET_VALUE) {
        closeSocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET_VALUE;
    }

    // 关闭所有连接
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    for (auto& kv : connections_) {
        kv.second->close();
    }
    connections_.clear();
    currentConnections_ = 0;

    return true;
}

uint32_t NativeListener::getConnectionCount() const {
    return currentConnections_.load();
}

void NativeListener::setSessionFactory(SessionFactoryPtr factory) {
    apolloSessionFactory_ = std::move(factory);
}

void NativeListener::setPacketParser(PacketParserPtr parser) {
    packetParser_ = std::move(parser);
    nativePacketParser_ = std::make_shared<LengthPrefixPacketParser>();
}

void NativeListener::setCallbacks(const ListenerCallbacks& callbacks) {
    callbacks_ = callbacks;
}

bool NativeListener::disconnectConnection(uint64_t connectionId, const char* reason) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);

    auto it = connections_.find(connectionId);
    if (it != connections_.end()) {
        it->second->disconnect(reason);
        connections_.erase(it);
        currentConnections_--;
        return true;
    }
    return false;
}

uint32_t NativeListener::disconnectAll(const char* reason) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);

    uint32_t count = 0;
    for (auto& kv : connections_) {
        kv.second->disconnect(reason);
        count++;
    }
    connections_.clear();
    currentConnections_ = 0;
    return count;
}

ConnectionPtr NativeListener::getConnection(uint64_t connectionId) const {
    std::lock_guard<std::mutex> lock(connectionsMutex_);

    auto it = connections_.find(connectionId);
    return it != connections_.end() ? it->second : nullptr;
}

bool NativeListener::handleAccept() {
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);

    socket_t clientSocket = accept(listenSocket_,
                                   (struct sockaddr*)&clientAddr,
                                   &addrLen);

    if (clientSocket == INVALID_SOCKET_VALUE) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) return true;
#endif
        return false;
    }

    // 检查连接数限制
    if (currentConnections_.load() >= maxConnections_) {
        closeSocket(clientSocket);
        return true;
    }

    // 设置非阻塞
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(clientSocket, FIONBIO, &mode);
#else
    int flags = fcntl(clientSocket, F_GETFL, 0);
    fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK);
#endif

    // 设置套接字选项
    if (noDelay_) {
        int flag = 1;
        setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
    }

    if (recvBufferSize_ > 0) {
        setsockopt(clientSocket, SOL_SOCKET, SO_RCVBUF,
                   (char*)&recvBufferSize_, sizeof(recvBufferSize_));
    }

    if (sendBufferSize_ > 0) {
        setsockopt(clientSocket, SOL_SOCKET, SO_SNDBUF,
                   (char*)&sendBufferSize_, sizeof(sendBufferSize_));
    }

    // 获取客户端信息
    std::string clientIP = inet_ntoa(clientAddr.sin_addr);
    uint16_t clientPort = ntohs(clientAddr.sin_port);

    // 创建连接对象
    uint64_t connId = nextConnectionId_++;
    auto conn = std::make_shared<NativeConnection>(
        clientSocket, connId, clientIP, clientPort
    );

    // 设置数据包处理器
    conn->setPacketHandler([this](NativeConnection* nc, const char* data, uint32_t len) {
        (void)nc;
        (void)data;
        (void)len;
        if (callbacks_.onAccept) {
            // 这里可以触发数据回调
        }
    });

    // 添加到连接列表
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        connections_[connId] = conn;
        currentConnections_++;
    }

    // 创建会话
    if (nativeSessionFactory_) {
        auto* session = nativeSessionFactory_->createSession(conn.get());
        if (session) {
            session->onEstablish(conn.get());
        }
    }

    return true;
}

void NativeListener::onConnectionClosed(NativeConnection* conn) {
    if (!conn) return;

    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        connections_.erase(conn->getConnectionId());
        currentConnections_--;
    }

    if (callbacks_.onDisconnect) {
        callbacks_.onDisconnect(conn->getConnectionId(), "Connection closed");
    }
}

// ========== NativeConnector ==========

NativeConnector::NativeConnector()
    : socket_(INVALID_SOCKET_VALUE)
    , state_(ConnectorState::Stopped)
    , nextConnectionId_(1) {
}

NativeConnector::~NativeConnector() {
    stop();
}

int32_t NativeConnector::connect(const ConnectConfig& config) {
    config_ = config;
    return connect(config.host, config.port);
}

int32_t NativeConnector::connect(const std::string& host, uint16_t port) {
    if (state_ == ConnectorState::Connected) return 0;

    // 创建套接字
    socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_ == INVALID_SOCKET_VALUE) {
        return -1;
    }

    // 设置非阻塞
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(socket_, FIONBIO, &mode);
#else
    int flags = fcntl(socket_, F_GETFL, 0);
    fcntl(socket_, F_SETFL, flags | O_NONBLOCK);
#endif

    // 连接服务器
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        // 需要域名解析
        closeSocket(socket_);
        socket_ = INVALID_SOCKET_VALUE;
        return -1;
    }

    int result = ::connect(socket_, (struct sockaddr*)&addr, sizeof(addr));

#ifdef _WIN32
    int err = WSAGetLastError();
    if (result == SOCKET_ERROR_VALUE && err != WSAEWOULDBLOCK) {
        closeSocket(socket_);
        socket_ = INVALID_SOCKET_VALUE;
        state_ = ConnectorState::Failed;
        return -1;
    }
#else
    if (result == SOCKET_ERROR_VALUE && errno != EINPROGRESS) {
        closeSocket(socket_);
        socket_ = INVALID_SOCKET_VALUE;
        state_ = ConnectorState::Failed;
        return -1;
    }
#endif

    state_ = ConnectorState::Connecting;
    return 0; // 非阻塞连接已发起
}

int32_t NativeConnector::reconnect() {
    if (!config_.host.empty()) {
        return connect(config_);
    }
    return -1;
}

void NativeConnector::disconnect() {
    if (socket_ != INVALID_SOCKET_VALUE) {
        closeSocket(socket_);
        socket_ = INVALID_SOCKET_VALUE;
    }
    state_ = ConnectorState::Stopped;
    connection_.reset();
}

void NativeConnector::stop() {
    disconnect();
}

void NativeConnector::setCallbacks(const ConnectorCallbacks& callbacks) {
    callbacks_ = callbacks;
}

void NativeConnector::setPacketParser(PacketParserPtr parser) {
    packetParser_ = std::move(parser);
    nativePacketParser_ = std::make_shared<LengthPrefixPacketParser>();
}

bool NativeConnector::isConnected() const {
    return state_ == ConnectorState::Connected;
}

bool NativeConnector::isConnecting() const {
    return state_ == ConnectorState::Connecting ||
           state_ == ConnectorState::Reconnecting;
}

bool NativeConnector::handleConnect() {
    if (state_ != ConnectorState::Connecting) return false;

    // 检查连接状态
#ifdef _WIN32
    // Windows 使用 select 检查可写状态
    fd_set writeSet;
    FD_ZERO(&writeSet);
    FD_SET(socket_, &writeSet);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    int result = select(0, nullptr, &writeSet, nullptr, &tv);

    if (result > 0) {
        int error = 0;
        socklen_t len = sizeof(error);
        getsockopt(socket_, SOL_SOCKET, SO_ERROR, (char*)&error, &len);

        if (error == 0) {
            // 连接成功
            state_ = ConnectorState::Connected;

            // 获取服务器地址
            struct sockaddr_in addr;
            socklen_t addrLen = sizeof(addr);
            getpeername(socket_, (struct sockaddr*)&addr, &addrLen);

            std::string serverIP = inet_ntoa(addr.sin_addr);
            uint16_t serverPort = ntohs(addr.sin_port);

            // 创建连接对象
            uint64_t connId = nextConnectionId_++;
            connection_ = std::make_shared<NativeConnection>(
                socket_, connId, serverIP, serverPort
            );

            if (callbacks_.onConnected) {
                callbacks_.onConnected(connection_);
            }
            return true;
        }
    }
#else
    // Linux 版本
    int error = 0;
    socklen_t len = sizeof(error);
    getsockopt(socket_, SOL_SOCKET, SO_ERROR, &error, &len);

    if (error == 0) {
        state_ = ConnectorState::Connected;

        struct sockaddr_in addr;
        socklen_t addrLen = sizeof(addr);
        getpeername(socket_, (struct sockaddr*)&addr, &addrLen);

        std::string serverIP = inet_ntoa(addr.sin_addr);
        uint16_t serverPort = ntohs(addr.sin_port);

        uint64_t connId = nextConnectionId_++;
        connection_ = std::make_shared<NativeConnection>(
            socket_, connId, serverIP, serverPort
        );

        if (callbacks_.onConnected) {
            callbacks_.onConnected(connection_);
        }
        return true;
    }
#endif

    // 连接失败
    state_ = ConnectorState::Failed;
    if (callbacks_.onConnectFailed) {
        callbacks_.onConnectFailed(-1, "Connection failed");
    }
    return false;
}

// ========== NativeNetworkManager ==========

NativeNetworkManager::NativeNetworkManager()
    : running_(false)
    , threadCount_(0) {

#ifdef _WIN32
    ioPort_ = nullptr;
#else
    epollFd_ = -1;
#endif

    // 默认使用 CPU 核心数的线程
    threadCount_ = std::thread::hardware_concurrency();
    if (threadCount_ == 0) threadCount_ = 4;
}

NativeNetworkManager::~NativeNetworkManager() {
    stop();
}

bool NativeNetworkManager::initialize(const NetworkConfig& config) {
    config_ = config;

    // 初始化网络库
    if (!initNetwork()) {
        return false;
    }

#ifdef _WIN32
    // Windows: 创建 I/O 完成端口
    ioPort_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if (ioPort_ == nullptr) {
        cleanupNetwork();
        return false;
    }
#else
#if defined(__linux__)
    // Linux: 创建 epoll
    epollFd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epollFd_ < 0) {
        cleanupNetwork();
        return false;
    }
#endif
#endif

    return true;
}

bool NativeNetworkManager::start() {
    if (running_) return true;

    running_ = true;

    // 启动 IO 线程
    for (uint32_t i = 0; i < threadCount_; ++i) {
        ioThreads_.emplace_back([this]() { ioThreadProc(); });
    }

    return true;
}

void NativeNetworkManager::stop() {
    if (!running_) return;

    running_ = false;

    // 停止所有监听器
    for (auto& kv : listeners_) {
        kv.second->stop();
    }

    // 停止所有连接器
    for (auto& connector : connectors_) {
        connector.second->stop();
    }

    // 关闭所有连接
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& kv : connections_) {
            kv.second->close();
        }
        connections_.clear();
    }

#ifdef _WIN32
    // 通知 IO 线程退出
    if (ioPort_) {
        for (size_t i = 0; i < ioThreads_.size(); ++i) {
            PostQueuedCompletionStatus(ioPort_, 0, 0, nullptr);
        }
    }
#else
#if defined(__linux__)
    if (epollFd_ >= 0) {
        close(epollFd_);
        epollFd_ = -1;
    }
#endif
#endif

    // 等待 IO 线程结束
    for (auto& thread : ioThreads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    ioThreads_.clear();

#ifdef _WIN32
    if (ioPort_) {
        CloseHandle(ioPort_);
        ioPort_ = nullptr;
    }
#endif

    cleanupNetwork();
}

void NativeNetworkManager::update() {
    processEvents(0);
}

ListenerPtr NativeNetworkManager::createListener(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (listeners_.find(name) != listeners_.end()) {
        return nullptr;
    }

    auto listener = std::make_shared<NativeListener>();
    listeners_[name] = listener;
    return listener;
}

ListenerPtr NativeNetworkManager::getListener(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = listeners_.find(name);
    return it != listeners_.end() ? it->second : nullptr;
}

bool NativeNetworkManager::removeListener(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = listeners_.find(name);
    if (it != listeners_.end()) {
        it->second->stop();
        listeners_.erase(it);
        return true;
    }
    return false;
}

void NativeNetworkManager::removeAllListeners() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& kv : listeners_) {
        kv.second->stop();
    }
    listeners_.clear();
}

ConnectorPtr NativeNetworkManager::createConnector(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (connectors_.find(name) != connectors_.end()) {
        return nullptr;
    }

    auto connector = std::make_shared<NativeConnector>();
    connectors_[name] = connector;
    return connector;
}

ConnectorPtr NativeNetworkManager::getConnector(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = connectors_.find(name);
    return it != connectors_.end() ? it->second : nullptr;
}

bool NativeNetworkManager::removeConnector(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = connectors_.find(name);
    if (it != connectors_.end()) {
        it->second->stop();
        connectors_.erase(it);
        return true;
    }
    return false;
}

void NativeNetworkManager::removeAllConnectors() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& kv : connectors_) {
        kv.second->stop();
    }
    connectors_.clear();
}

ConnectionPtr NativeNetworkManager::getConnection(uint64_t connectionId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = connections_.find(connectionId);
    return it != connections_.end() ? it->second : nullptr;
}

bool NativeNetworkManager::disconnectConnection(uint64_t connectionId, const char* reason) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = connections_.find(connectionId);
    if (it != connections_.end()) {
        it->second->disconnect(reason);
        connections_.erase(it);
        return true;
    }
    return false;
}

uint32_t NativeNetworkManager::broadcast(const char* data, uint32_t length,
                                          const std::vector<uint64_t>& excludeIds) {
    std::lock_guard<std::mutex> lock(mutex_);

    uint32_t count = 0;
    for (auto& kv : connections_) {
        uint64_t connId = kv.first;

        // 检查是否需要排除
        bool excluded = false;
        for (uint64_t excludeId : excludeIds) {
            if (connId == excludeId) {
                excluded = true;
                break;
            }
        }

        if (!excluded) {
            if (kv.second->send(data, length) >= 0) {
                count++;
            }
        }
    }
    return count;
}

NetworkStats NativeNetworkManager::getStats() const {
    return stats_;
}

void NativeNetworkManager::resetStats() {
    stats_ = NetworkStats{};
}

void NativeNetworkManager::addConnection(std::shared_ptr<NativeConnection> conn) {
    if (!conn) return;

    std::lock_guard<std::mutex> lock(mutex_);
    connections_[conn->getConnectionId()] = conn;
    stats_.currentConnections = connections_.size();
    stats_.totalConnections++;
}

void NativeNetworkManager::removeConnection(uint64_t connectionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    connections_.erase(connectionId);
    stats_.currentConnections = connections_.size();
}

std::shared_ptr<NativeConnection> NativeNetworkManager::getNativeConnection(uint64_t connectionId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = connections_.find(connectionId);
    return it != connections_.end() ? it->second : nullptr;
}

void NativeNetworkManager::ioThreadProc() {
    while (running_) {
        processEvents(100);
    }
}

bool NativeNetworkManager::processEvents(int timeoutMs) {
    (void)timeoutMs;
    // 处理监听器的接受事件
    for (auto& kv : listeners_) {
        auto& listener = kv.second;
        if (listener->isRunning()) {
            listener->handleAccept();
        }
    }

    // 处理连接器的连接事件
    for (auto& kv : connectors_) {
        auto& connector = kv.second;
        if (connector->isConnecting()) {
            connector->handleConnect();
        }
    }

    // 处理连接的收发事件
    std::vector<std::shared_ptr<NativeConnection>> activeConnections;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        activeConnections.reserve(connections_.size());
        for (auto& kv : connections_) {
            activeConnections.push_back(kv.second);
        }
    }

    std::vector<uint64_t> toRemove;

    for (auto& conn : activeConnections) {
        if (!conn->isConnected()) {
            toRemove.push_back(conn->getConnectionId());
            continue;
        }

        // 处理接收
        if (!conn->handleRecv()) {
            toRemove.push_back(conn->getConnectionId());
            continue;
        }

        // 处理发送
        if (!conn->handleSend()) {
            toRemove.push_back(conn->getConnectionId());
        }
    }

    // 移除关闭的连接
    for (uint64_t connId : toRemove) {
        removeConnection(connId);
    }

    return true;
}

// ========== NativeAdapter ==========

NetworkManagerPtr NativeAdapter::createManager() {
    return std::make_shared<NativeNetworkManager>();
}

bool NativeAdapter::initialize() {
    return initNetwork();
}

void NativeAdapter::shutdown() {
    cleanupNetwork();
}

std::string NativeAdapter::getVersion() {
    return "Native/1.0.0";
}

} // namespace adapters
} // namespace net
} // namespace apollo
