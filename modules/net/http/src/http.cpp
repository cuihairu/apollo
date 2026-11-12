/**
 * @file http.cpp
 * @brief HTTP 服务器和客户端实现
 */

#include "apollo/net/http.h"
#include <algorithm>
#include <thread>
#include <queue>
#include <mutex>
#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <sstream>
#include <cstring>

#ifdef _WIN32
    #pragma comment(lib, "ws2_32.lib")
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET socket_t;
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define SOCKET_ERROR_VALUE SOCKET_ERROR
    #define closesocket closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <errno.h>
    #include <string.h>
    typedef int socket_t;
    #define INVALID_SOCKET_VALUE -1
    #define SOCKET_ERROR_VALUE -1
    #define closesocket close
#endif

namespace apollo {
namespace net {
namespace http {

//==============================================================================
// 网络初始化
//==============================================================================

namespace detail {
    struct NetworkInitializer {
        NetworkInitializer() {
#ifdef _WIN32
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
        }
        ~NetworkInitializer() {
#ifdef _WIN32
            WSACleanup();
#endif
        }
    };

    static NetworkInitializer g_networkInit;
}

//==============================================================================
// 简单 Socket 连接 (用于 HTTP)
//==============================================================================

class SimpleSocket {
public:
    SimpleSocket() : socket_(INVALID_SOCKET_VALUE) {}
    explicit SimpleSocket(socket_t sock) : socket_(sock) {}

    ~SimpleSocket() {
        close();
    }

    // 禁止拷贝
    SimpleSocket(const SimpleSocket&) = delete;
    SimpleSocket& operator=(const SimpleSocket&) = delete;

    // 允许移动
    SimpleSocket(SimpleSocket&& other) noexcept : socket_(other.socket_) {
        other.socket_ = INVALID_SOCKET_VALUE;
    }

    SimpleSocket& operator=(SimpleSocket&& other) noexcept {
        if (this != &other) {
            close();
            socket_ = other.socket_;
            other.socket_ = INVALID_SOCKET_VALUE;
        }
        return *this;
    }

    bool isValid() const { return socket_ != INVALID_SOCKET_VALUE; }

    int send(const char* data, size_t size) {
        if (!isValid()) return -1;
#ifdef _WIN32
        return ::send(socket_, data, static_cast<int>(size), 0);
#else
#ifdef MSG_NOSIGNAL
        // Linux：对端关闭后的 send 不发 SIGPIPE（默认杀进程），返回 EPIPE
        return ::send(socket_, data, size, MSG_NOSIGNAL);
#else
#if defined(SO_NOSIGPIPE)
        // macOS/BSD 无 MSG_NOSIGNAL：对 socket 置 SO_NOSIGPIPE，send 改返
        // EPIPE（一次置位即可）
        if (!nosigpipeSet_) {
            int on = 1;
            ::setsockopt(socket_, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
            nosigpipeSet_ = true;
        }
#endif
        return ::send(socket_, data, size, 0);
#endif
#endif
    }

    int receive(char* buffer, size_t size) {
        if (!isValid()) return -1;
#ifdef _WIN32
        return ::recv(socket_, buffer, static_cast<int>(size), 0);
#else
        return ::recv(socket_, buffer, size, 0);
#endif
    }

    void close() {
        if (isValid()) {
            ::closesocket(socket_);
            socket_ = INVALID_SOCKET_VALUE;
        }
    }

    socket_t get() const { return socket_; }

    void setSocket(socket_t sock) { socket_ = sock; }

protected:
    socket_t socket_;
#if !defined(_WIN32) && !defined(MSG_NOSIGNAL) && defined(SO_NOSIGPIPE)
    bool nosigpipeSet_ = false;
#endif
};

//==============================================================================
// HTTP 连接接口 (简单实现)
//==============================================================================

class IConnection {
public:
    virtual ~IConnection() = default;
    virtual int send(const char* data, size_t size) = 0;
    virtual int receive(char* buffer, size_t size) = 0;
    virtual void close() = 0;
    virtual bool isConnected() const = 0;
    // 底层套接字描述符（连接线程 select 等待可读/收线用）
    virtual socket_t fd() const = 0;
};

//==============================================================================
// Socket 连接实现
//==============================================================================

class SocketConnection : public IConnection {
public:
    explicit SocketConnection(socket_t sock) : socket_(sock) {}
    ~SocketConnection() override { socket_.close(); }

    int send(const char* data, size_t size) override {
        return socket_.send(data, size);
    }

    int receive(char* buffer, size_t size) override {
        return socket_.receive(buffer, size);
    }

    void close() override {
        socket_.close();
    }

    bool isConnected() const override {
        return socket_.isValid();
    }

    socket_t fd() const override {
        return socket_.get();
    }

private:
    SimpleSocket socket_;
};

//==============================================================================
// HTTP 监听器
//==============================================================================

class Listener {
public:
    static std::unique_ptr<Listener> create() {
        return std::unique_ptr<Listener>(new Listener());
    }

    ~Listener() {
        close();
    }

    bool bind(const std::string& address, uint16_t port) {
        socket_.setSocket(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        if (!socket_.isValid()) {
            return false;
        }

        // 设置 SO_REUSEADDR
        int opt = 1;
        ::setsockopt(socket_.get(), SOL_SOCKET, SO_REUSEADDR,
            reinterpret_cast<const char*>(&opt), sizeof(opt));

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);

        if (!address.empty() && address != "0.0.0.0" && address != "*") {
#ifdef _WIN32
            addr.sin_addr.s_addr = ::inet_addr(address.c_str());
#else
            ::inet_pton(AF_INET, address.c_str(), &addr.sin_addr);
#endif
        }

        return ::bind(socket_.get(), reinterpret_cast<struct sockaddr*>(&addr),
                      sizeof(addr)) == 0;
    }

    bool listen(int backlog = 128) {
        if (::listen(socket_.get(), backlog) != 0) {
            return false;
        }
        // 监听套接字非阻塞：acceptLoop 无连接时以 10ms 周期检查 running_，
        // stop() 置 running_=false 即退出（阻塞 accept 不因标志/关闭解除）
#ifdef _WIN32
        u_long nonBlocking = 1;
        ioctlsocket(socket_.get(), FIONBIO, &nonBlocking);
#else
        int flags = fcntl(socket_.get(), F_GETFL, 0);
        if (flags >= 0) {
            fcntl(socket_.get(), F_SETFL, flags | O_NONBLOCK);
        }
#endif
        return true;
    }

    std::shared_ptr<IConnection> accept() {
        struct sockaddr_in clientAddr{};
        socklen_t addrLen = sizeof(clientAddr);

        socket_t clientSock = ::accept(socket_.get(),
            reinterpret_cast<struct sockaddr*>(&clientAddr), &addrLen);

        if (clientSock == INVALID_SOCKET_VALUE) {
            return nullptr;
        }

        // macOS/BSD 的 accept() 从监听套接字继承 O_NONBLOCK（Linux 不继承）：
        // 清回阻塞模式，连接线程 select/recv 按原语义等待
#ifdef _WIN32
        u_long blocking = 0;
        ioctlsocket(clientSock, FIONBIO, &blocking);
#else
        int clientFlags = fcntl(clientSock, F_GETFL, 0);
        if (clientFlags >= 0) {
            fcntl(clientSock, F_SETFL, clientFlags & ~O_NONBLOCK);
        }
#endif

        return std::make_shared<SocketConnection>(clientSock);
    }

    void close() {
        socket_.close();
    }

private:
    Listener() = default;

    SimpleSocket socket_;
};

//==============================================================================
// HTTP 连接器
//==============================================================================

class Connector {
public:
    static std::unique_ptr<Connector> create() {
        return std::unique_ptr<Connector>(new Connector());
    }

    ~Connector() {
        close();
    }

    bool connect(const std::string& host, uint16_t port, uint32_t timeoutMs) {
        // 创建 socket
        socket_.setSocket(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        if (!socket_.isValid()) {
            return false;
        }

        // 设置非阻塞模式用于超时
#ifdef _WIN32
        u_long mode = 1;
        ioctlsocket(socket_.get(), FIONBIO, &mode);
#else
        int flags = fcntl(socket_.get(), F_GETFL, 0);
        fcntl(socket_.get(), F_SETFL, flags | O_NONBLOCK);
#endif

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        // 解析主机名
        if (!resolveHost(host, addr)) {
            close();
            return false;
        }

        // 尝试连接
        int result = ::connect(socket_.get(),
            reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));

#ifdef _WIN32
        if (result == SOCKET_ERROR_VALUE) {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                // 等待连接完成
                if (waitForWrite(timeoutMs)) {
                    // 检查连接是否成功
                    int opt = 0;
                    socklen_t optLen = sizeof(opt);
                    getsockopt(socket_.get(), SOL_SOCKET, SO_ERROR,
                        reinterpret_cast<char*>(&opt), &optLen);
                    if (opt == 0) {
                        // 恢复阻塞模式
                        u_long mode = 0;
                        ioctlsocket(socket_.get(), FIONBIO, &mode);
                        return true;
                    }
                }
            }
        }
#else
        if (result == SOCKET_ERROR_VALUE && errno == EINPROGRESS) {
            if (waitForWrite(timeoutMs)) {
                int opt = 0;
                socklen_t optLen = sizeof(opt);
                getsockopt(socket_.get(), SOL_SOCKET, SO_ERROR, &opt, &optLen);
                if (opt == 0) {
                    // 恢复阻塞模式
                    int flags = fcntl(socket_.get(), F_GETFL, 0);
                    fcntl(socket_.get(), F_SETFL, flags & ~O_NONBLOCK);
                    return true;
                }
            }
        }
#endif

        close();
        return false;
    }

    int send(const char* data, size_t size) {
        return socket_.send(data, size);
    }

    int receive(char* buffer, size_t size) {
        return socket_.receive(buffer, size);
    }

    void close() {
        socket_.close();
    }

    // 等待对端可读（带超时）。返回：>0 可读，0 超时/无事件，<0 错误
    int waitReadable(int timeoutMs) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(socket_.get(), &readSet);

        struct timeval tv{};
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;

        return select(static_cast<int>(socket_.get()) + 1, &readSet, nullptr, nullptr, &tv);
    }

private:
    Connector() = default;

    bool resolveHost(const std::string& host, struct sockaddr_in& addr) {
        // 尝试直接转换 IP 地址
#ifdef _WIN32
        addr.sin_addr.s_addr = ::inet_addr(host.c_str());
        if (addr.sin_addr.s_addr != INADDR_NONE) {
            return true;
        }
#else
        if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) == 1) {
            return true;
        }
#endif

        // TODO: 实现 DNS 查询
        // 简化版：仅支持 IP 地址
        return false;
    }

    bool waitForWrite(uint32_t timeoutMs) {
        fd_set writeSet;
        FD_ZERO(&writeSet);
        FD_SET(socket_.get(), &writeSet);

        struct timeval tv{};
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;

        return select(static_cast<int>(socket_.get()) + 1, nullptr, &writeSet, nullptr, &tv) > 0;
    }

    SimpleSocket socket_;
};

//==============================================================================
// HTTP 连接
//==============================================================================

class HttpConnection : public std::enable_shared_from_this<HttpConnection> {
public:
    using Ptr = std::shared_ptr<HttpConnection>;

    static Ptr create(std::shared_ptr<IConnection> conn) {
        return std::shared_ptr<HttpConnection>(new HttpConnection(std::move(conn)));
    }

    void setHandler(RequestHandler handler) {
        handler_ = std::move(handler);
    }

    void onDataReceived(const char* data, size_t size) {
        receiveBuffer_.append(data, size);

        while (true) {
            size_t bytesConsumed = 0;
            Request request;

            if (!RequestParser::parse(receiveBuffer_, request, bytesConsumed)) {
                break; // 不完整，等待更多数据
            }

            receiveBuffer_ = receiveBuffer_.substr(bytesConsumed);

            // 处理请求
            Response response = handleRequest(request);

            // 发送响应
            std::string responseStr = response.toString();
            if (conn_) {
                conn_->send(responseStr.data(), responseStr.size());
            }

            // 如果不需要保持连接，关闭连接
            if (!request.shouldKeepAlive()) {
                close();
            }
        }
    }

    // 接收线程：select 等待可读（周期 ≤timeoutMs，其间检查 shutdown 标志）。
    // 返回：1 可读 / 0 超时 / -1 shutdown 或错误。收线时 stop() 置 shutdown，
    // 线程在下一 select 周期退出（原阻塞 recv 无法收线）
    int waitReadable(const std::atomic<bool>& shutdown, int timeoutMs) {
        if (!conn_) return -1;
        fd_set readSet;
        FD_ZERO(&readSet);
        socket_t fd = conn_->fd();
        FD_SET(fd, &readSet);
        struct timeval tv{};
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;
        int r = select(static_cast<int>(fd) + 1, &readSet, nullptr, nullptr, &tv);
        if (r > 0) return 1;
        if (r == 0) return shutdown.load() ? -1 : 0;
        return -1;
    }

    int receive(char* buffer, size_t size) {
        if (!conn_) return -1;
        return conn_->receive(buffer, size);
    }

    bool isConnected() const {
        return conn_ && conn_->isConnected();
    }

    void close() {
        if (conn_) {
            conn_->close();
        }
    }

private:
    HttpConnection(std::shared_ptr<IConnection> conn) : conn_(std::move(conn)) {}

    Response handleRequest(const Request& request) {
        if (handler_) {
            return handler_(request);
        }
        return Response::ok("OK");
    }

    std::shared_ptr<IConnection> conn_;
    RequestHandler handler_;
    std::string receiveBuffer_;
};

//==============================================================================
// HTTP 服务器
//==============================================================================

// 连接注册表：connections_ + 连接线程表 + shutdown 标志，以 shared_ptr 持有，
// 连接线程仅经此共享状态访问，server 析构后不悬空（audit P1#7）
class ConnectionRegistry {
public:
    using HttpConnPtr = HttpConnection::Ptr;

    void add(HttpConnPtr conn) {
        std::lock_guard<std::mutex> lock(mutex_);
        connections_.push_back(conn);
    }

    // 连接线程退出时统一移除（原 removeConnection 无人调用，每连接泄漏——
    // audit P1#7）
    void remove(HttpConnPtr conn) {
        std::lock_guard<std::mutex> lock(mutex_);
        connections_.erase(
            std::remove(connections_.begin(), connections_.end(), conn),
            connections_.end());
    }

    // 启动连接线程并跟踪（joinable，stop() 收线 join）。每线程带 done 标志，
    // acceptLoop 周期回收已结束线程防向量无限增长
    void spawn(std::function<void()> fn) {
        auto done = std::make_shared<std::atomic<bool>>(false);
        std::thread t([fn, done]() {
            fn();
            done->store(true);
        });
        std::lock_guard<std::mutex> lock(mutex_);
        threads_.push_back(TrackedThread(std::move(t), std::move(done)));
    }

    void reapFinished() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = threads_.begin(); it != threads_.end();) {
            if (it->done->load()) {
                if (it->thread.joinable()) {
                    it->thread.join();
                }
                it = threads_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void shutdown() {
        shuttingDown_.store(true);
    }

    const std::atomic<bool>& shuttingDown() const {
        return shuttingDown_;
    }

    // 收线：置 shutdown 后 join 全部连接线程（每线程 select 周期 ≤100ms 内
    // 退出）
    void joinAll() {
        std::vector<std::thread> toJoin;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& t : threads_) {
                if (t.thread.joinable()) {
                    toJoin.push_back(std::move(t.thread));
                }
            }
            threads_.clear();
        }
        for (auto& t : toJoin) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    size_t count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return connections_.size();
    }

private:
    struct TrackedThread {
        std::thread thread;
        std::shared_ptr<std::atomic<bool>> done;
        TrackedThread(std::thread t, std::shared_ptr<std::atomic<bool>> d)
            : thread(std::move(t)), done(std::move(d)) {}
    };

    mutable std::mutex mutex_;
    std::vector<HttpConnPtr> connections_;
    std::vector<TrackedThread> threads_;
    std::atomic<bool> shuttingDown_{false};
};

// 内置 HTTP 服务器（接口见 http.h）
class BuiltinHttpServer : public HttpServer {
public:
    BuiltinHttpServer() : running_(false) {}

    ~BuiltinHttpServer() override {
        stop();
    }

    void setRouter(std::shared_ptr<Router> router) override {
        router_ = std::move(router);
    }

    Router* router() const override {
        return router_.get();
    }

    bool start(const std::string& address, uint16_t port) override {
        if (running_) {
            return false;
        }

        listener_ = Listener::create();
        if (!listener_ || !listener_->bind(address, port)) {
            return false;
        }

        if (!listener_->listen(128)) {
            return false;
        }

        registry_ = std::make_shared<ConnectionRegistry>();
        running_ = true;
        acceptThread_ = std::thread(&BuiltinHttpServer::acceptLoop, this);

        return true;
    }

    void stop() override {
        if (!running_.exchange(false)) {
            return;
        }

        if (listener_) {
            listener_->close();
        }
        if (acceptThread_.joinable()) {
            acceptThread_.join();
        }
        if (registry_) {
            // 收线：置 shutdown → 连接线程 select 周期退出 → join
            registry_->shutdown();
            registry_->joinAll();
        }
    }

    bool isRunning() const override {
        return running_;
    }

    size_t getConnectionCount() const override {
        return registry_ ? registry_->count() : 0;
    }

private:
    void acceptLoop() {
        while (running_) {
            auto conn = listener_->accept();
            if (!conn) {
                if (running_) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                continue;
            }
            if (!running_) {
                conn->close();
                break;
            }

            registry_->reapFinished();

            // 捕获 router/registry shared_ptr：连接线程仅经共享状态访问，
            // server 析构后不悬空（原 handler 捕获裸 this——audit P1#7）
            auto router = router_;
            auto registry = registry_;
            auto httpConn = HttpConnection::create(conn);
            httpConn->setHandler([router](const Request& req) {
                if (router) {
                    return router->route(req);
                }
                return Response::ok();
            });

            registry->add(httpConn);

            registry->spawn([httpConn, conn, registry]() {
                char buffer[4096];
                while (conn && conn->isConnected()) {
                    // select 周期等待可读（其间检查 shutdown），替代阻塞 recv：
                    // 连接仅本线程收发，stop() 置 shutdown 后线程在周期内退出
                    int wr = httpConn->waitReadable(registry->shuttingDown(), 100);
                    if (wr <= 0) break;
                    int bytesRead = conn->receive(buffer, sizeof(buffer));
                    if (bytesRead <= 0) break;
                    httpConn->onDataReceived(buffer, bytesRead);
                }
                conn->close();
                registry->remove(httpConn);  // 统一移除（原无人调用，泄漏）
            });
        }
    }

    std::unique_ptr<Listener> listener_;
    std::thread acceptThread_;
    std::atomic<bool> running_;

    std::shared_ptr<ConnectionRegistry> registry_;
    std::shared_ptr<Router> router_;
};

std::unique_ptr<HttpServer> createHttpServer() {
    return std::make_unique<BuiltinHttpServer>();
}

//==============================================================================
// HTTP 客户端
//==============================================================================

class BuiltinHttpClient : public HttpClient {
public:
    BuiltinHttpClient() = default;

    ~BuiltinHttpClient() override {
        disconnect();
    }

    bool connect(const std::string& host, uint16_t port, uint32_t timeoutMs = 5000) override {
        connector_ = Connector::create();
        if (!connector_) {
            return false;
        }

        if (!connector_->connect(host, port, timeoutMs)) {
            return false;
        }

        host_ = host;
        port_ = port;
        connected_ = true;
        return true;
    }

    void disconnect() override {
        if (connector_) {
            connector_->close();
            connector_.reset();
        }
        connected_ = false;
    }

    Response get(const std::string& uri, const Headers& headers = {}) override {
        return request(Method::Get, uri, headers, "");
    }

    Response post(const std::string& uri, const std::string& body,
                  const std::string& contentType = "text/plain") override {
        Headers headers;
        headers.setContentType(contentType);
        headers.setContentLength(body.size());
        return request(Method::Post, uri, headers, body);
    }

    Response request(Method method, const std::string& uri,
                     const Headers& headers = {}, const std::string& body = "") override {
        if (!connected_ && !reconnect()) {
            return Response::serverError("Not connected");
        }

        // 构建请求
        std::ostringstream oss;
        oss << toString(method) << " " << uri << " HTTP/1.1\r\n";

        // 头部
        if (!headers.headers.empty()) {
            for (const auto& [key, value] : headers.headers) {
                oss << key << ": " << value << "\r\n";
            }
        }

        // Host 头
        if (!headers.has("Host")) {
            oss << "Host: " << host_ << "\r\n";
        }

        // Connection 头
        oss << "Connection: keep-alive\r\n";

        // 空行
        oss << "\r\n";

        // 消息体
        oss << body;

        std::string requestStr = oss.str();

        // 发送
        if (connector_->send(requestStr.data(), requestStr.size()) < 0) {
            return Response::serverError("Send failed");
        }

        // 接收响应
        return receiveResponse();
    }

    void setResponseTimeout(uint32_t timeoutMs) override {
        responseTimeoutMs_ = timeoutMs;
    }

    void setMaxResponseSize(size_t maxSize) override {
        maxResponseSize_ = maxSize;
    }

private:
    bool reconnect() {
        if (!host_.empty() && port_ > 0) {
            return connect(host_, port_);
        }
        return false;
    }

    Response receiveResponse() {
        std::vector<char> buffer(4096);
        std::string responseData;
        Response response;
        size_t contentLength = 0;
        size_t headerBytesReceived = 0;
        bool headersComplete = false;
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::milliseconds(responseTimeoutMs_);

        while (true) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                break;  // 超时（原无超时，对端不发即永久阻塞——audit P1#7）
            }
            if (responseData.size() >= maxResponseSize_) {
                break;  // 响应大小上限（原无上限，可 OOM）
            }

            int waitMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());
            if (waitMs <= 0) break;
            if (connector_->waitReadable(waitMs) <= 0) {
                break;  // 超时或错误
            }

            int bytesRead = connector_->receive(buffer.data(), buffer.size());
            if (bytesRead <= 0) {
                break;
            }

            responseData.append(buffer.data(), bytesRead);

            if (!headersComplete) {
                // 查找头部结束标记
                size_t headerEnd = responseData.find("\r\n\r\n");
                if (headerEnd != std::string::npos) {
                    headersComplete = true;
                    headerBytesReceived = headerEnd + 4;

                    // 解析状态行和头部
                    parseResponse(responseData.substr(0, headerEnd + 4), response);

                    // 获取 Content-Length
                    std::string lenStr = response.headers.get("Content-Length");
                    if (!lenStr.empty()) {
                        try {
                            contentLength = std::stoull(lenStr);
                        } catch (const std::exception&) {
                            contentLength = 0;  // 垃圾值：按无体处理（audit P1#7）
                        }
                    }

                    // 如果没有 Content-Length 或已接收完整响应
                    if (contentLength == 0) {
                        break;
                    }
                }
            }

            // 检查是否接收完整
            if (headersComplete && contentLength > 0) {
                if (responseData.size() >= headerBytesReceived + contentLength) {
                    response.body = responseData.substr(headerBytesReceived, contentLength);
                    break;
                }
            }
        }

        return response;
    }

    void parseResponse(const std::string& headerData, Response& response) {
        std::istringstream iss(headerData);
        std::string line;

        // 状态行
        if (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            // 解析 "HTTP/1.1 200 OK"
            size_t pos1 = line.find(' ');
            if (pos1 != std::string::npos) {
                size_t pos2 = line.find(' ', pos1 + 1);
                if (pos2 != std::string::npos) {
                    // 状态码
                    try {
                        int statusCode = std::stoi(line.substr(pos1 + 1, pos2 - pos1 - 1));
                        response.status = static_cast<StatusCode>(statusCode);
                    } catch (const std::exception&) {
                        // 垃圾状态码：保持默认
                    }
                }
            }
        }

        // 头部
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (line.empty()) {
                break;
            }

            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                std::string key = line.substr(0, colonPos);
                std::string value = line.substr(colonPos + 1);

                // 去除首尾空白
                size_t start = value.find_first_not_of(" \t");
                if (start != std::string::npos) {
                    value = value.substr(start);
                }

                response.headers.set(key, value);
            }
        }
    }

    std::unique_ptr<Connector> connector_;
    bool connected_ = false;
    std::string host_;
    uint16_t port_ = 0;
    uint32_t responseTimeoutMs_ = 5000;
    // 响应大小上限：对端声明超大 Content-Length 不无限累积（audit P1#7）
    size_t maxResponseSize_ = 16 * 1024 * 1024;
};

std::unique_ptr<HttpClient> createHttpClient() {
    return std::make_unique<BuiltinHttpClient>();
}

//==============================================================================
// 便捷函数
//==============================================================================

// 解析 http://host[:port]/path。内置客户端仅 TCP 不支持 TLS（https 拒绝）。
// 原 httpGet/httpPost 忽略 url 恒连 localhost:80——audit P1#7
bool parseUrl(const std::string& url, std::string& host, uint16_t& port, std::string& path) {
    std::string rest = url;
    if (rest.rfind("http://", 0) == 0) {
        rest = rest.substr(7);
    } else if (rest.rfind("https://", 0) == 0) {
        return false;  // 不支持 TLS
    }

    size_t slash = rest.find('/');
    std::string authority = (slash == std::string::npos) ? rest : rest.substr(0, slash);
    path = (slash == std::string::npos) ? "/" : rest.substr(slash);
    if (authority.empty()) {
        return false;
    }

    size_t colon = authority.rfind(':');
    if (colon != std::string::npos) {
        host = authority.substr(0, colon);
        try {
            port = static_cast<uint16_t>(std::stoi(authority.substr(colon + 1)));
        } catch (const std::exception&) {
            return false;
        }
    } else {
        host = authority;
    }
    return !host.empty();
}

// 快速创建 HTTP GET 请求
Response httpGet(const std::string& url, uint32_t timeoutMs) {
    std::string host, path;
    uint16_t port = 80;
    if (!parseUrl(url, host, port, path)) {
        return Response::serverError("Invalid URL");
    }

    auto client = createHttpClient();
    if (!client->connect(host, port, timeoutMs)) {
        return Response::serverError("Connection failed");
    }

    auto response = client->get(path);
    client->disconnect();
    return response;
}

// 快速创建 HTTP POST 请求
Response httpPost(const std::string& url, const std::string& body,
                  const std::string& contentType,
                  uint32_t timeoutMs) {
    std::string host, path;
    uint16_t port = 80;
    if (!parseUrl(url, host, port, path)) {
        return Response::serverError("Invalid URL");
    }

    auto client = createHttpClient();
    if (!client->connect(host, port, timeoutMs)) {
        return Response::serverError("Connection failed");
    }

    auto response = client->post(path, body, contentType);
    client->disconnect();
    return response;
}

} // namespace http
} // namespace net
} // namespace apollo
