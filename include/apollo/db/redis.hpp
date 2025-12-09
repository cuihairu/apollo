#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <atomic>

namespace apollo {
namespace db {

/// Redis回复类型
enum class RedisReplyType {
    NIL = 0,
    STRING = 1,
    INTEGER = 2,
    ARRAY = 3,
    ERROR = 4
};

/// Redis回复
class RedisReply {
public:
    RedisReply() : type_(RedisReplyType::NIL) {}
    RedisReply(RedisReplyType type) : type_(type) {}

    RedisReplyType GetType() const { return type_; }
    bool IsError() const { return type_ == RedisReplyType::ERROR; }
    bool IsNil() const { return type_ == RedisReplyType::NIL; }
    bool IsString() const { return type_ == RedisReplyType::STRING; }
    bool IsInteger() const { return type_ == RedisReplyType::INTEGER; }
    bool IsArray() const { return type_ == RedisReplyType::ARRAY; }

    const std::string& AsString() const { return strValue_; }
    int64_t AsInteger() const { return intValue_; }
    const std::vector<RedisReply>& AsArray() const { return arrayValue_; }
    const std::string& GetError() const { return error_; }

    void SetString(const std::string& value) {
        type_ = RedisReplyType::STRING;
        strValue_ = value;
    }

    void SetInteger(int64_t value) {
        type_ = RedisReplyType::INTEGER;
        intValue_ = value;
    }

    void SetArray(const std::vector<RedisReply>& value) {
        type_ = RedisReplyType::ARRAY;
        arrayValue_ = value;
    }

    void SetError(const std::string& error) {
        type_ = RedisReplyType::ERROR;
        error_ = error;
    }

    void SetNil() {
        type_ = RedisReplyType::NIL;
    }

private:
    RedisReplyType type_;
    std::string strValue_;
    int64_t intValue_ = 0;
    std::vector<RedisReply> arrayValue_;
    std::string error_;
};

/// Redis命令
class RedisCommand {
public:
    RedisCommand(const std::string& cmd);

    RedisCommand& Append(const std::string& arg);
    RedisCommand& Append(int64_t arg);
    RedisCommand& Append(uint32_t arg);
    RedisCommand& Append(int32_t arg);
    RedisCommand& Append(double arg);

    const std::vector<std::string>& GetArgs() const { return args_; }

private:
    std::vector<std::string> args_;
};

/// Redis连接状态
enum class RedisConnectionState {
    DISCONNECTED = 0,
    CONNECTING = 1,
    CONNECTED = 2,
    ERROR = 3
};

/// Redis连接
class RedisConnection {
public:
    RedisConnection();
    ~RedisConnection();

    /// 连接到Redis服务器
    bool Connect(const std::string& host, int port, const std::string& password = "");

    /// 断开连接
    bool Disconnect();

    /// 执行命令
    RedisReply Execute(const RedisCommand& command);

    /// 异步执行命令
    void ExecuteAsync(const RedisCommand& command,
                      std::function<void(const RedisReply&)> callback);

    /// 获取连接状态
    RedisConnectionState GetState() const { return state_; }

    /// 是否已连接
    bool IsConnected() const { return state_ == RedisConnectionState::CONNECTED; }

    /// Ping测试
    bool Ping();

    /// 选择数据库
    bool Select(int db);

    /// 获取错误信息
    const std::string& GetLastError() const { return lastError_; }

private:
    int sockfd_;
    RedisConnectionState state_;
    std::string host_;
    int port_;
    std::string password_;
    std::string lastError_;
    std::string buffer_;

    bool ConnectSocket();
    bool Authenticate();
    bool SendCommand(const RedisCommand& command);
    RedisReply ReadReply();
    RedisReply ParseReply();
    std::string ReadLine();
    size_t ReadBytes(void* data, size_t size);
};

/// Redis连接池配置
struct RedisPoolConfig {
    std::string host = "localhost";
    int port = 6379;
    std::string password;
    int db = 0;
    size_t minConnections = 2;
    size_t maxConnections = 10;
    int connectionTimeout = 5;  // 秒
    int commandTimeout = 30;    // 秒
    bool autoReconnect = true;
    int keepAlive = 60;         // 秒
};

/// Redis连接池
class RedisConnectionPool {
public:
    explicit RedisConnectionPool(const RedisPoolConfig& config);
    ~RedisConnectionPool();

    /// 初始化连接池
    bool Initialize();

    /// 获取连接
    std::shared_ptr<RedisConnection> GetConnection();

    /// 归还连接
    void ReturnConnection(std::shared_ptr<RedisConnection> conn);

    /// 关闭所有连接
    void CloseAll();

    /// 获取配置
    const RedisPoolConfig& GetConfig() const { return config_; }

    /// 获取统计信息
    struct Stats {
        size_t totalConnections;
        size_t activeConnections;
        size_t idleConnections;
        size_t createdConnections;
        size_t destroyedConnections;
    };

    Stats GetStats() const;

private:
    void CreateConnection();
    void CheckConnections();
    bool ValidateConnection(std::shared_ptr<RedisConnection> conn);

    RedisPoolConfig config_;
    std::queue<std::shared_ptr<RedisConnection>> idleConnections_;
    std::vector<std::shared_ptr<RedisConnection>> allConnections_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::atomic<bool> shutdown_{false};

    Stats stats_;
    std::thread checkThread_;
};

/// Redis客户端
class RedisClient {
public:
    explicit RedisClient(const RedisPoolConfig& config);
    ~RedisClient();

    /// 初始化客户端
    bool Initialize();

    /// 执行Redis命令
    RedisReply Execute(const std::string& command);
    RedisReply Execute(const RedisCommand& command);

    /// 异步执行命令
    void ExecuteAsync(const std::string& command,
                      std::function<void(const RedisReply&)> callback);
    void ExecuteAsync(const RedisCommand& command,
                      std::function<void(const RedisReply&)> callback);

    /// 字符串操作
    bool Set(const std::string& key, const std::string& value);
    bool Set(const std::string& key, const std::string& value, int ttl);
    std::string Get(const std::string& key);
    bool Del(const std::string& key);
    bool Exists(const std::string& key);
    int64_t Incr(const std::string& key);
    int64_t IncrBy(const std::string& key, int64_t increment);

    /// 哈希操作
    bool HSet(const std::string& key, const std::string& field, const std::string& value);
    std::string HGet(const std::string& key, const std::string& field);
    bool HDel(const std::string& key, const std::string& field);
    std::map<std::string, std::string> HGetAll(const std::string& key);
    bool HExists(const std::string& key, const std::string& field);

    /// 列表操作
    bool LPush(const std::string& key, const std::string& value);
    bool RPush(const std::string& key, const std::string& value);
    std::string LPop(const std::string& key);
    std::string RPop(const std::string& key);
    std::vector<std::string> LRange(const std::string& key, int start, int stop);

    /// 集合操作
    bool SAdd(const std::string& key, const std::string& member);
    bool SRem(const std::string& key, const std::string& member);
    std::vector<std::string> SMembers(const std::string& key);
    bool SIsMember(const std::string& key, const std::string& member);

    /// 有序集合操作
    bool ZAdd(const std::string& key, double score, const std::string& member);
    bool ZRem(const std::string& key, const std::string& member);
    std::vector<std::string> ZRange(const std::string& key, int start, int stop);
    double ZScore(const std::string& key, const std::string& member);

    /// 过期操作
    bool Expire(const std::string& key, int seconds);
    bool Persist(const std::string& key);
    int64_t TTL(const std::string& key);

    /// 发布订阅
    bool Publish(const std::string& channel, const std::string& message);

    /// 事务
    bool Multi();
    bool Discard();
    std::vector<RedisReply> Exec();

    /// 获取统计信息
    RedisConnectionPool::Stats GetStats() const;

private:
    RedisConnectionPool pool_;
    std::string lastError_;
};

}  // namespace db
}  // namespace apollo