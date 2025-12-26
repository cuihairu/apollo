/**
 * @file http.cpp
 * @brief HTTP 服务器和客户端实现
 */

#include "apollo/net/http.h"
#include <thread>
#include <queue>
#include <mutex>
#include <atomic>
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
        return ::send(socket_, data, size, 0);
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
        return ::listen(socket_.get(), backlog) == 0;
    }

    std::unique_ptr<IConnection> accept() {
        struct sockaddr_in clientAddr{};
        socklen_t addrLen = sizeof(clientAddr);

        socket_t clientSock = ::accept(socket_.get(),
            reinterpret_cast<struct sockaddr*>(&clientAddr), &addrLen);

        if (clientSock == INVALID_SOCKET_VALUE) {
            return nullptr;
        }

        return std::unique_ptr<IConnection>(new SocketConnection(clientSock));
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

    static Ptr create(IConnection* baseConn) {
        return std::shared_ptr<HttpConnection>(new HttpConnection(baseConn));
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
            if (baseConn_) {
                baseConn_->send(responseStr.data(), responseStr.size());
            }

            // 如果不需要保持连接，关闭连接
            if (!request.shouldKeepAlive()) {
                close();
            }
        }
    }

    void close() {
        if (baseConn_) {
            baseConn_->close();
            baseConn_ = nullptr;
        }
    }

private:
    HttpConnection(IConnection* baseConn) : baseConn_(baseConn) {}

    Response handleRequest(const Request& request) {
        if (handler_) {
            return handler_(request);
        }
        return Response::ok("OK");
    }

    IConnection* baseConn_ = nullptr;
    RequestHandler handler_;
    std::string receiveBuffer_;
};

//==============================================================================
// HTTP 服务器
//==============================================================================

class HttpServer {
public:
    HttpServer() : running_(false) {}

    ~HttpServer() {
        stop();
    }

    void setRouter(std::shared_ptr<Router> router) {
        router_ = std::move(router);
    }

    Router* router() const { return router_.get(); }

    bool start(const std::string& address, uint16_t port) {
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

        running_ = true;
        acceptThread_ = std::thread(&HttpServer::acceptLoop, this);

        return true;
    }

    void stop() {
        running_ = false;

        if (acceptThread_.joinable()) {
            acceptThread_.join();
        }

        if (listener_) {
            listener_->close();
        }

        std::lock_guard<std::mutex> lock(connectionsMutex_);
        for (auto& conn : connections_) {
            conn->close();
        }
        connections_.clear();
    }

    bool isRunning() const {
        return running_;
    }

    size_t getConnectionCount() const {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        return connections_.size();
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

            auto httpConn = HttpConnection::create(conn.get());
            httpConn->setHandler([this](const Request& req) {
                if (router_) {
                    return router_->route(req);
                }
                return Response::ok();
            });

            addConnection(httpConn);

            // 简化版：在接收线程中处理
            std::thread([httpConn, conn = std::move(conn)]() mutable {
                char buffer[4096];
                while (conn && conn->isConnected()) {
                    int bytesRead = conn->receive(buffer, sizeof(buffer));
                    if (bytesRead <= 0) break;
                    httpConn->onDataReceived(buffer, bytesRead);
                }
                httpConn->close();
            }).detach();
        }
    }

    void addConnection(HttpConnection::Ptr conn) {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        connections_.push_back(conn);
    }

    void removeConnection(HttpConnection::Ptr conn) {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        connections_.erase(
            std::remove(connections_.begin(), connections_.end(), conn),
            connections_.end());
    }

    std::unique_ptr<Listener> listener_;
    std::thread acceptThread_;
    std::atomic<bool> running_;

    std::vector<HttpConnection::Ptr> connections_;
    mutable std::mutex connectionsMutex_;
    std::shared_ptr<Router> router_;
};

//==============================================================================
// HTTP 客户端
//==============================================================================

class HttpClient {
public:
    HttpClient() = default;
    ~HttpClient() { disconnect(); }

    bool connect(const std::string& host, uint16_t port, uint32_t timeoutMs = 5000) {
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

    void disconnect() {
        if (connector_) {
            connector_->close();
            connector_.reset();
        }
        connected_ = false;
    }

    Response get(const std::string& uri, const Headers& headers = {}) {
        return request(Method::Get, uri, headers, "");
    }

    Response post(const std::string& uri, const std::string& body,
                 const std::string& contentType = "text/plain") {
        Headers headers;
        headers.setContentType(contentType);
        headers.setContentLength(body.size());
        return request(Method::Post, uri, headers, body);
    }

    Response put(const std::string& uri, const std::string& body,
                const std::string& contentType = "text/plain") {
        Headers headers;
        headers.setContentType(contentType);
        headers.setContentLength(body.size());
        return request(Method::Put, uri, headers, body);
    }

    Response del(const std::string& uri) {
        return request(Method::Delete, uri, {}, "");
    }

    Response request(Method method, const std::string& uri,
                     const Headers& headers = {}, const std::string& body = "") {
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

        while (true) {
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
                        contentLength = std::stoull(lenStr);
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
                    int statusCode = std::stoi(line.substr(pos1 + 1, pos2 - pos1 - 1));
                    response.status = static_cast<StatusCode>(statusCode);
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
};

//==============================================================================
// 便捷函数
//==============================================================================

// 快速创建 HTTP GET 请求
inline Response httpGet(const std::string& url, uint32_t timeoutMs) {
    // 简化 URL 解析: http://host:port/path
    std::string host = "localhost";
    uint16_t port = 80;
    std::string path = "/";

    // TODO: 解析完整 URL
    HttpClient client;
    if (!client.connect(host, port, timeoutMs)) {
        return Response::serverError("Connection failed");
    }

    auto response = client.get(path);
    client.disconnect();
    return response;
}

// 快速创建 HTTP POST 请求
inline Response httpPost(const std::string& url, const std::string& body,
                         const std::string& contentType = "application/json",
                         uint32_t timeoutMs = 5000) {
    std::string host = "localhost";
    uint16_t port = 80;
    std::string path = "/";

    HttpClient client;
    if (!client.connect(host, port, timeoutMs)) {
        return Response::serverError("Connection failed");
    }

    auto response = client.post(path, body, contentType);
    client.disconnect();
    return response;
}

} // namespace http
} // namespace net
} // namespace apollo
