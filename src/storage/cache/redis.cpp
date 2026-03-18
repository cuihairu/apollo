#include "apollo/storage/cache/redis.hpp"

// Windows 平台头文件（必须最先包含）
#ifdef _WIN32
    #ifndef _WINSOCKAPI_
        #define _WINSOCKAPI_
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <sys/time.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #define closesocket close
    #define SOCKET int
    #define INVALID_SOCKET -1
#endif

#include <sstream>
#include <iostream>
#include <cstring>
#include <chrono>
#include <algorithm>

namespace apollo {
namespace db {

// RedisCommand 实现
RedisCommand::RedisCommand(const std::string& cmd) {
    args_.push_back(cmd);
}

RedisCommand& RedisCommand::Append(const std::string& arg) {
    args_.push_back(arg);
    return *this;
}

RedisCommand& RedisCommand::Append(int64_t arg) {
    args_.push_back(std::to_string(arg));
    return *this;
}

RedisCommand& RedisCommand::Append(uint32_t arg) {
    args_.push_back(std::to_string(arg));
    return *this;
}

RedisCommand& RedisCommand::Append(int32_t arg) {
    args_.push_back(std::to_string(arg));
    return *this;
}

RedisCommand& RedisCommand::Append(double arg) {
    args_.push_back(std::to_string(arg));
    return *this;
}

// RedisConnection 实现
RedisConnection::RedisConnection()
    : sockfd_(-1), state_(RedisConnectionState::DISCONNECTED), port_(0) {
}

RedisConnection::~RedisConnection() {
    Disconnect();
}

bool RedisConnection::Connect(const std::string& host,
                              int port,
                              const std::string& password,
                              int connectionTimeoutSeconds,
                              int commandTimeoutSeconds) {
    host_ = host;
    port_ = port;
    password_ = password;
    connectionTimeoutSeconds_ = std::max(1, connectionTimeoutSeconds);
    commandTimeoutSeconds_ = std::max(1, commandTimeoutSeconds);
    state_ = RedisConnectionState::CONNECTING;

    if (!ConnectSocket()) {
        return false;
    }

    if (!password_.empty()) {
        if (!Authenticate()) {
            Disconnect();
            return false;
        }
    }

    state_ = RedisConnectionState::CONNECTED;
    return true;
}

bool RedisConnection::ConnectSocket() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        lastError_ = "WSAStartup failed";
        return false;
    }
#endif

    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_ == INVALID_SOCKET) {
        lastError_ = "Failed to create socket";
        return false;
    }

    // 设置非阻塞模式用于连接超时处理
    int flags = fcntl(sockfd_, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(sockfd_, F_SETFL, flags | O_NONBLOCK);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);

    // 解析主机名
    struct hostent* he = gethostbyname(host_.c_str());
    if (!he) {
        lastError_ = "Failed to resolve host: " + host_;
        closesocket(sockfd_);
        sockfd_ = INVALID_SOCKET;
        return false;
    }
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    // 连接
    if (connect(sockfd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        if (errno != EINPROGRESS) {
            lastError_ = "Failed to connect: " + std::string(strerror(errno));
            closesocket(sockfd_);
            sockfd_ = INVALID_SOCKET;
            return false;
        }
    }

    // 等待连接完成
    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(sockfd_, &write_fds);

    struct timeval timeout;
    timeout.tv_sec = connectionTimeoutSeconds_;
    timeout.tv_usec = 0;

    if (select(sockfd_ + 1, NULL, &write_fds, NULL, &timeout) <= 0) {
        lastError_ = "Connection timeout";
        closesocket(sockfd_);
        sockfd_ = INVALID_SOCKET;
        return false;
    }

    // 检查连接是否成功
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(sockfd_, SOL_SOCKET, SO_ERROR, (char*)&error, &len) < 0 || error != 0) {
        lastError_ = "Connection failed: " + std::string(strerror(error));
        closesocket(sockfd_);
        sockfd_ = INVALID_SOCKET;
        return false;
    }

    // 恢复阻塞模式，简化后续读写逻辑
    if (flags >= 0) {
        fcntl(sockfd_, F_SETFL, flags);
    }

#ifdef _WIN32
    DWORD timeoutMs = static_cast<DWORD>(commandTimeoutSeconds_) * 1000;
    setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));
    setsockopt(sockfd_, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));
#else
    struct timeval ioTimeout;
    ioTimeout.tv_sec = commandTimeoutSeconds_;
    ioTimeout.tv_usec = 0;
    setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, &ioTimeout, sizeof(ioTimeout));
    setsockopt(sockfd_, SOL_SOCKET, SO_SNDTIMEO, &ioTimeout, sizeof(ioTimeout));
#endif

    return true;
}

bool RedisConnection::Authenticate() {
    RedisCommand auth("AUTH");
    auth.Append(password_);
    RedisReply reply = Execute(auth);

    if (reply.IsError()) {
        lastError_ = reply.GetError();
        return false;
    }

    return reply.IsString() && reply.AsString() == "OK";
}

bool RedisConnection::Disconnect() {
    if (sockfd_ != INVALID_SOCKET) {
        closesocket(sockfd_);
        sockfd_ = INVALID_SOCKET;
    }
    state_ = RedisConnectionState::DISCONNECTED;
    return true;
}

RedisReply RedisConnection::Execute(const RedisCommand& command) {
    if (!IsConnected()) {
        RedisReply reply;
        reply.SetError("Not connected to Redis server");
        return reply;
    }

    if (!SendCommand(command)) {
        RedisReply reply;
        reply.SetError(lastError_);
        return reply;
    }

    return ReadReply();
}

void RedisConnection::ExecuteAsync(const RedisCommand& command,
                                   std::function<void(const RedisReply&)> callback) {
    // 简化实现，实际项目中应该使用线程池
    std::thread([this, command, callback]() {
        RedisReply reply = Execute(command);
        callback(reply);
    }).detach();
}

bool RedisConnection::SendCommand(const RedisCommand& command) {
    const auto& args = command.GetArgs();
    std::ostringstream oss;

    // 构建Redis协议命令
    oss << "*" << args.size() << "\r\n";
    for (const auto& arg : args) {
        oss << "$" << arg.length() << "\r\n" << arg << "\r\n";
    }

    std::string cmd = oss.str();

    size_t sentTotal = 0;
    while (sentTotal < cmd.size()) {
        int sent = send(sockfd_, cmd.data() + sentTotal, cmd.size() - sentTotal, 0);
        if (sent <= 0) {
            lastError_ = "Failed to send command: " + std::string(strerror(errno));
            return false;
        }
        sentTotal += static_cast<size_t>(sent);
    }

    return true;
}

RedisReply RedisConnection::ReadReply() {
    buffer_.clear();
    return ParseReply();
}

RedisReply RedisConnection::ParseReply() {
    std::string line = ReadLine();
    if (line.empty()) {
        RedisReply reply;
        reply.SetError("Failed to read reply");
        return reply;
    }

    if (line[0] == '+') {
        // 简单字符串
        RedisReply reply(RedisReplyType::STRING);
        reply.SetString(line.substr(1));
        return reply;
    } else if (line[0] == '-') {
        // 错误
        RedisReply reply(RedisReplyType::ERROR);
        reply.SetError(line.substr(1));
        return reply;
    } else if (line[0] == ':') {
        // 整数
        RedisReply reply(RedisReplyType::INTEGER);
        reply.SetInteger(std::stoll(line.substr(1)));
        return reply;
    } else if (line[0] == '$') {
        // 批量字符串
        long long len = std::stoll(line.substr(1));
        if (len < 0) {
            RedisReply reply(RedisReplyType::NIL);
            reply.SetNil();
            return reply;
        }

        std::string data;
        data.resize(static_cast<size_t>(len));
        if (ReadBytes(&data[0], static_cast<size_t>(len)) != static_cast<size_t>(len)) {
            RedisReply reply;
            reply.SetError("Failed to read bulk string");
            return reply;
        }

        // 读取CRLF
        char crlf[2];
        ReadBytes(crlf, 2);

        RedisReply reply(RedisReplyType::STRING);
        reply.SetString(data);
        return reply;
    } else if (line[0] == '*') {
        // 数组
        long long count = std::stoll(line.substr(1));
        if (count < 0) {
            RedisReply reply(RedisReplyType::NIL);
            reply.SetNil();
            return reply;
        }

        std::vector<RedisReply> array;
        for (long long i = 0; i < count; ++i) {
            array.push_back(ParseReply());
        }

        RedisReply reply(RedisReplyType::ARRAY);
        reply.SetArray(array);
        return reply;
    }

    RedisReply reply;
    reply.SetError("Invalid reply format");
    return reply;
}

std::string RedisConnection::ReadLine() {
    while (true) {
        size_t pos = buffer_.find("\r\n");
        if (pos != std::string::npos) {
            std::string line = buffer_.substr(0, pos);
            buffer_.erase(0, pos + 2);
            return line;
        }

        char buf[1024];
        int len = recv(sockfd_, buf, sizeof(buf), 0);
        if (len <= 0) {
            return "";
        }

        buffer_.append(buf, len);
    }
}

size_t RedisConnection::ReadBytes(void* data, size_t size) {
    size_t total = 0;
    while (total < size) {
        if (buffer_.empty()) {
            char buf[1024];
            int len = recv(sockfd_, buf, sizeof(buf), 0);
            if (len <= 0) {
                return total;
            }
            buffer_.append(buf, len);
        }

        size_t toRead = std::min(size - total, buffer_.size());
        memcpy(static_cast<char*>(data) + total, buffer_.data(), toRead);
        buffer_.erase(0, toRead);
        total += toRead;
    }
    return total;
}

bool RedisConnection::Ping() {
    RedisCommand ping("PING");
    RedisReply reply = Execute(ping);
    return !reply.IsError() && (reply.AsString() == "PONG" || reply.AsString() == "pong");
}

bool RedisConnection::Select(int db) {
    RedisCommand select("SELECT");
    select.Append(db);
    RedisReply reply = Execute(select);
    return !reply.IsError() && reply.AsString() == "OK";
}

// RedisConnectionPool 实现
RedisConnectionPool::RedisConnectionPool(const RedisPoolConfig& config)
    : config_(config) {
    stats_.totalConnections = 0;
    stats_.activeConnections = 0;
    stats_.idleConnections = 0;
    stats_.createdConnections = 0;
    stats_.destroyedConnections = 0;
}

RedisConnectionPool::~RedisConnectionPool() {
    CloseAll();
}

bool RedisConnectionPool::Initialize() {
    // 创建最小数量的连接
    for (size_t i = 0; i < config_.minConnections; ++i) {
        if (!CreateConnection()) {
            CloseAll();
            return false;
        }
    }

    // 启动连接检查线程
    checkThread_ = std::thread(&RedisConnectionPool::CheckConnections, this);

    return true;
}

std::shared_ptr<RedisConnection> RedisConnectionPool::GetConnection() {
    std::unique_lock<std::mutex> lock(mutex_);

    // 如果有空闲连接，直接返回
    if (!idleConnections_.empty()) {
        auto conn = idleConnections_.front();
        idleConnections_.pop();
        if (stats_.idleConnections > 0) {
            stats_.idleConnections--;
        }
        stats_.activeConnections++;

        // 检查连接是否有效
        if (!ValidateConnection(conn)) {
            // 连接无效，重新创建
            stats_.activeConnections--;
            if (!CreateConnection() || idleConnections_.empty()) {
                return nullptr;
            }
            conn = idleConnections_.front();
            idleConnections_.pop();
            if (stats_.idleConnections > 0) {
                stats_.idleConnections--;
            }
            stats_.activeConnections++;
        }

        return conn;
    }

    // 如果还能创建新连接
    if (allConnections_.size() < config_.maxConnections) {
        if (!CreateConnection() || idleConnections_.empty()) {
            return nullptr;
        }
        auto conn = idleConnections_.front();
        idleConnections_.pop();
        if (stats_.idleConnections > 0) {
            stats_.idleConnections--;
        }
        stats_.activeConnections++;
        return conn;
    }

    // 等待空闲连接
    condition_.wait(lock, [this]() {
        return !idleConnections_.empty() || shutdown_;
    });

    if (shutdown_) {
        return nullptr;
    }

    auto conn = idleConnections_.front();
    idleConnections_.pop();
    stats_.idleConnections--;
    stats_.activeConnections++;

    if (!ValidateConnection(conn)) {
        CreateConnection();
        conn = idleConnections_.front();
        idleConnections_.pop();
        stats_.idleConnections--;
        stats_.activeConnections++;
    }

    return conn;
}

void RedisConnectionPool::ReturnConnection(std::shared_ptr<RedisConnection> conn) {
    if (!conn) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    if (shutdown_) {
        // 连接池正在关闭，直接销毁连接
        auto it = std::remove(allConnections_.begin(), allConnections_.end(), conn);
        if (it != allConnections_.end()) {
            allConnections_.erase(it, allConnections_.end());
            if (stats_.totalConnections > 0) {
                stats_.totalConnections--;
            }
        }
        if (stats_.activeConnections > 0) {
            stats_.activeConnections--;
        }
        stats_.destroyedConnections++;
        return;
    }

    // 连接有效，放回空闲队列
    idleConnections_.push(conn);
    stats_.idleConnections++;
    if (stats_.activeConnections > 0) {
        stats_.activeConnections--;
    }

    condition_.notify_one();
}

void RedisConnectionPool::CloseAll() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
    }
    condition_.notify_all();

    if (checkThread_.joinable()) {
        checkThread_.join();
    }

    std::lock_guard<std::mutex> lock(mutex_);
    // 销毁所有连接
    allConnections_.clear();
    std::queue<std::shared_ptr<RedisConnection>> empty;
    std::swap(idleConnections_, empty);

    stats_.totalConnections = 0;
    stats_.activeConnections = 0;
    stats_.idleConnections = 0;
}

bool RedisConnectionPool::CreateConnection() {
    auto conn = std::make_shared<RedisConnection>();

    if (conn->Connect(config_.host,
                      config_.port,
                      config_.password,
                      config_.connectionTimeout,
                      config_.commandTimeout) &&
        conn->Ping()) {
        if (config_.db != 0) {
            conn->Select(config_.db);
        }

        allConnections_.push_back(conn);
        idleConnections_.push(conn);
        stats_.totalConnections++;
        stats_.idleConnections++;
        stats_.createdConnections++;
        return true;
    }
    return false;
}

void RedisConnectionPool::CheckConnections() {
    while (true) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (condition_.wait_for(
                lock,
                std::chrono::seconds(config_.keepAlive),
                [this]() { return shutdown_.load(); })) {
            break;
        }

        if (shutdown_) {
            break;
        }

        const size_t idleCount = idleConnections_.size();
        for (size_t i = 0; i < idleCount; ++i) {
            auto conn = idleConnections_.front();
            idleConnections_.pop();
            if (stats_.idleConnections > 0) {
                stats_.idleConnections--;
            }

            if (!ValidateConnection(conn)) {
                auto it = std::remove(allConnections_.begin(), allConnections_.end(), conn);
                if (it != allConnections_.end()) {
                    allConnections_.erase(it, allConnections_.end());
                    if (stats_.totalConnections > 0) {
                        stats_.totalConnections--;
                    }
                }
                stats_.destroyedConnections++;

                CreateConnection();
            } else {
                idleConnections_.push(conn);
                stats_.idleConnections++;
            }
        }

        condition_.notify_all();
    }
}

bool RedisConnectionPool::ValidateConnection(std::shared_ptr<RedisConnection> conn) {
    if (!conn || !conn->IsConnected()) {
        return false;
    }

    return conn->Ping();
}

RedisConnectionPool::Stats RedisConnectionPool::GetStats() const {
    return stats_;
}

// RedisClient 实现
RedisClient::RedisClient(const RedisPoolConfig& config) : pool_(config) {
}

RedisClient::~RedisClient() {
}

bool RedisClient::Initialize() {
    return pool_.Initialize();
}

RedisReply RedisClient::Execute(const std::string& command) {
    std::istringstream iss(command);
    std::string token;
    RedisCommand cmd("");

    bool first = true;
    while (iss >> token) {
        if (first) {
            cmd = RedisCommand(token);
            first = false;
        } else {
            cmd.Append(token);
        }
    }

    return Execute(cmd);
}

RedisReply RedisClient::Execute(const RedisCommand& command) {
    std::shared_ptr<RedisConnection> conn;
    {
        std::lock_guard<std::mutex> lock(txMutex_);
        conn = txConnection_;
    }

    if (conn) {
        return conn->Execute(command);
    }

    conn = pool_.GetConnection();
    if (!conn) {
        RedisReply reply;
        reply.SetError("Failed to get connection from pool");
        return reply;
    }

    auto reply = conn->Execute(command);
    pool_.ReturnConnection(conn);
    return reply;
}

void RedisClient::ExecuteAsync(const std::string& command,
                               std::function<void(const RedisReply&)> callback) {
    std::thread([this, command, callback]() {
        RedisReply reply = Execute(command);
        callback(reply);
    }).detach();
}

void RedisClient::ExecuteAsync(const RedisCommand& command,
                               std::function<void(const RedisReply&)> callback) {
    auto conn = pool_.GetConnection();
    if (!conn) {
        RedisReply reply;
        reply.SetError("Failed to get connection from pool");
        callback(reply);
        return;
    }

    conn->ExecuteAsync(command, [this, conn, callback](const RedisReply& reply) {
        pool_.ReturnConnection(conn);
        callback(reply);
    });
}

bool RedisClient::Set(const std::string& key, const std::string& value) {
    RedisCommand cmd("SET");
    cmd.Append(key).Append(value);
    auto reply = Execute(cmd);
    return !reply.IsError() && (reply.AsString() == "OK" || reply.AsString() == "QUEUED");
}

bool RedisClient::Set(const std::string& key, const std::string& value, int ttl) {
    RedisCommand cmd("SETEX");
    cmd.Append(key).Append(ttl).Append(value);
    auto reply = Execute(cmd);
    return !reply.IsError() && (reply.AsString() == "OK" || reply.AsString() == "QUEUED");
}

std::string RedisClient::Get(const std::string& key) {
    RedisCommand cmd("GET");
    cmd.Append(key);
    auto reply = Execute(cmd);
    if (reply.IsError() || reply.IsNil()) {
        return "";
    }
    return reply.AsString();
}

bool RedisClient::Del(const std::string& key) {
    RedisCommand cmd("DEL");
    cmd.Append(key);
    auto reply = Execute(cmd);
    return !reply.IsError() && reply.AsInteger() > 0;
}

bool RedisClient::Exists(const std::string& key) {
    RedisCommand cmd("EXISTS");
    cmd.Append(key);
    auto reply = Execute(cmd);
    return !reply.IsError() && reply.AsInteger() > 0;
}

int64_t RedisClient::Incr(const std::string& key) {
    RedisCommand cmd("INCR");
    cmd.Append(key);
    auto reply = Execute(cmd);
    return reply.IsError() ? 0 : reply.AsInteger();
}

int64_t RedisClient::IncrBy(const std::string& key, int64_t increment) {
    RedisCommand cmd("INCRBY");
    cmd.Append(key).Append(increment);
    auto reply = Execute(cmd);
    return reply.IsError() ? 0 : reply.AsInteger();
}

bool RedisClient::HSet(const std::string& key, const std::string& field, const std::string& value) {
    RedisCommand cmd("HSET");
    cmd.Append(key).Append(field).Append(value);
    auto reply = Execute(cmd);
    return !reply.IsError();
}

std::string RedisClient::HGet(const std::string& key, const std::string& field) {
    RedisCommand cmd("HGET");
    cmd.Append(key).Append(field);
    auto reply = Execute(cmd);
    if (reply.IsError() || reply.IsNil()) {
        return "";
    }
    return reply.AsString();
}

bool RedisClient::HDel(const std::string& key, const std::string& field) {
    RedisCommand cmd("HDEL");
    cmd.Append(key).Append(field);
    auto reply = Execute(cmd);
    return !reply.IsError() && reply.AsInteger() > 0;
}

std::map<std::string, std::string> RedisClient::HGetAll(const std::string& key) {
    RedisCommand cmd("HGETALL");
    cmd.Append(key);
    auto reply = Execute(cmd);

    std::map<std::string, std::string> result;
    if (reply.IsError() || !reply.IsArray()) {
        return result;
    }

    const auto& array = reply.AsArray();
    for (size_t i = 0; i < array.size(); i += 2) {
        if (i + 1 < array.size()) {
            result[array[i].AsString()] = array[i + 1].AsString();
        }
    }

    return result;
}

bool RedisClient::HExists(const std::string& key, const std::string& field) {
    RedisCommand cmd("HEXISTS");
    cmd.Append(key).Append(field);
    auto reply = Execute(cmd);
    return !reply.IsError() && reply.AsInteger() > 0;
}

bool RedisClient::LPush(const std::string& key, const std::string& value) {
    RedisCommand cmd("LPUSH");
    cmd.Append(key).Append(value);
    auto reply = Execute(cmd);
    return !reply.IsError();
}

bool RedisClient::RPush(const std::string& key, const std::string& value) {
    RedisCommand cmd("RPUSH");
    cmd.Append(key).Append(value);
    auto reply = Execute(cmd);
    return !reply.IsError();
}

std::string RedisClient::LPop(const std::string& key) {
    RedisCommand cmd("LPOP");
    cmd.Append(key);
    auto reply = Execute(cmd);
    if (reply.IsError() || reply.IsNil()) {
        return "";
    }
    return reply.AsString();
}

std::string RedisClient::RPop(const std::string& key) {
    RedisCommand cmd("RPOP");
    cmd.Append(key);
    auto reply = Execute(cmd);
    if (reply.IsError() || reply.IsNil()) {
        return "";
    }
    return reply.AsString();
}

std::vector<std::string> RedisClient::LRange(const std::string& key, int start, int stop) {
    RedisCommand cmd("LRANGE");
    cmd.Append(key).Append(start).Append(stop);
    auto reply = Execute(cmd);

    std::vector<std::string> result;
    if (reply.IsError() || !reply.IsArray()) {
        return result;
    }

    const auto& array = reply.AsArray();
    for (const auto& item : array) {
        result.push_back(item.AsString());
    }

    return result;
}

bool RedisClient::SAdd(const std::string& key, const std::string& member) {
    RedisCommand cmd("SADD");
    cmd.Append(key).Append(member);
    auto reply = Execute(cmd);
    return !reply.IsError();
}

bool RedisClient::SRem(const std::string& key, const std::string& member) {
    RedisCommand cmd("SREM");
    cmd.Append(key).Append(member);
    auto reply = Execute(cmd);
    return !reply.IsError() && reply.AsInteger() > 0;
}

std::vector<std::string> RedisClient::SMembers(const std::string& key) {
    RedisCommand cmd("SMEMBERS");
    cmd.Append(key);
    auto reply = Execute(cmd);

    std::vector<std::string> result;
    if (reply.IsError() || !reply.IsArray()) {
        return result;
    }

    const auto& array = reply.AsArray();
    for (const auto& item : array) {
        result.push_back(item.AsString());
    }

    return result;
}

bool RedisClient::SIsMember(const std::string& key, const std::string& member) {
    RedisCommand cmd("SISMEMBER");
    cmd.Append(key).Append(member);
    auto reply = Execute(cmd);
    return !reply.IsError() && reply.AsInteger() > 0;
}

bool RedisClient::ZAdd(const std::string& key, double score, const std::string& member) {
    RedisCommand cmd("ZADD");
    cmd.Append(key).Append(score).Append(member);
    auto reply = Execute(cmd);
    return !reply.IsError();
}

bool RedisClient::ZRem(const std::string& key, const std::string& member) {
    RedisCommand cmd("ZREM");
    cmd.Append(key).Append(member);
    auto reply = Execute(cmd);
    return !reply.IsError() && reply.AsInteger() > 0;
}

std::vector<std::string> RedisClient::ZRange(const std::string& key, int start, int stop) {
    RedisCommand cmd("ZRANGE");
    cmd.Append(key).Append(start).Append(stop);
    auto reply = Execute(cmd);

    std::vector<std::string> result;
    if (reply.IsError() || !reply.IsArray()) {
        return result;
    }

    const auto& array = reply.AsArray();
    for (const auto& item : array) {
        result.push_back(item.AsString());
    }

    return result;
}

double RedisClient::ZScore(const std::string& key, const std::string& member) {
    RedisCommand cmd("ZSCORE");
    cmd.Append(key).Append(member);
    auto reply = Execute(cmd);
    if (reply.IsError() || reply.IsNil()) {
        return 0.0;
    }
    return std::stod(reply.AsString());
}

bool RedisClient::Expire(const std::string& key, int seconds) {
    RedisCommand cmd("EXPIRE");
    cmd.Append(key).Append(seconds);
    auto reply = Execute(cmd);
    return !reply.IsError() && reply.AsInteger() > 0;
}

bool RedisClient::Persist(const std::string& key) {
    RedisCommand cmd("PERSIST");
    cmd.Append(key);
    auto reply = Execute(cmd);
    return !reply.IsError() && reply.AsInteger() > 0;
}

int64_t RedisClient::TTL(const std::string& key) {
    RedisCommand cmd("TTL");
    cmd.Append(key);
    auto reply = Execute(cmd);
    return reply.IsError() ? -1 : reply.AsInteger();
}

bool RedisClient::Publish(const std::string& channel, const std::string& message) {
    RedisCommand cmd("PUBLISH");
    cmd.Append(channel).Append(message);
    auto reply = Execute(cmd);
    return !reply.IsError();
}

bool RedisClient::Multi() {
    std::lock_guard<std::mutex> lock(txMutex_);
    if (txConnection_) {
        return false;
    }

    auto conn = pool_.GetConnection();
    if (!conn) {
        return false;
    }

    RedisCommand cmd("MULTI");
    auto reply = conn->Execute(cmd);
    if (reply.IsError() || reply.AsString() != "OK") {
        pool_.ReturnConnection(conn);
        return false;
    }

    txConnection_ = std::move(conn);
    return true;
}

bool RedisClient::Discard() {
    std::shared_ptr<RedisConnection> conn;
    {
        std::lock_guard<std::mutex> lock(txMutex_);
        conn = std::exchange(txConnection_, nullptr);
    }

    if (!conn) {
        RedisCommand cmd("DISCARD");
        auto reply = Execute(cmd);
        return !reply.IsError() && reply.AsString() == "OK";
    }

    RedisCommand cmd("DISCARD");
    auto reply = conn->Execute(cmd);
    pool_.ReturnConnection(conn);
    return !reply.IsError() && reply.AsString() == "OK";
}

std::vector<RedisReply> RedisClient::Exec() {
    std::shared_ptr<RedisConnection> conn;
    {
        std::lock_guard<std::mutex> lock(txMutex_);
        conn = std::exchange(txConnection_, nullptr);
    }

    RedisReply reply;
    RedisCommand cmd("EXEC");
    if (conn) {
        reply = conn->Execute(cmd);
        pool_.ReturnConnection(conn);
    } else {
        reply = Execute(cmd);
    }

    std::vector<RedisReply> result;
    if (reply.IsArray()) {
        result = reply.AsArray();
    }

    return result;
}

RedisConnectionPool::Stats RedisClient::GetStats() const {
    return pool_.GetStats();
}

}  // namespace db
}  // namespace apollo
