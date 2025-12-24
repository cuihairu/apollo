#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <optional>

namespace apollo {
namespace storage {
namespace redis {

/**
 * @brief Redis 值类型
 */
enum class RedisType : int {
    None = 0,
    String,
    Hash,
    List,
    Set,
    ZSet,
    Stream
};

/**
 * @brief Redis 回复
 */
class RedisReply {
public:
    RedisReply() = default;

    bool isOk() const { return !isError_; }
    bool isError() const { return isError_; }
    bool isNil() const { return isNil_; }
    const std::string& error() const { return errorMsg_; }

    const std::string& asString() const { return strValue_; }
    int64_t asInt64() const { return intValue_; }
    double asDouble() const { return dblValue_; }
    const std::vector<std::string>& asArray() const { return arrValue_; }
    const std::map<std::string, std::string>& asMap() const { return mapValue_; }

    static RedisReply Ok(const std::string& value = "OK") {
        RedisReply r;
        r.isError_ = false;
        r.isNil_ = false;
        r.strValue_ = value;
        return r;
    }

    static RedisReply String(const std::string& value) {
        return Ok(value);
    }

    static RedisReply Int64(int64_t value) {
        RedisReply r;
        r.isError_ = false;
        r.isNil_ = false;
        r.intValue_ = value;
        return r;
    }

    static RedisReply Double(double value) {
        RedisReply r;
        r.isError_ = false;
        r.isNil_ = false;
        r.dblValue_ = value;
        return r;
    }

    static RedisReply Array(const std::vector<std::string>& value) {
        RedisReply r;
        r.isError_ = false;
        r.isNil_ = false;
        r.arrValue_ = value;
        return r;
    }

    static RedisReply Map(const std::map<std::string, std::string>& value) {
        RedisReply r;
        r.isError_ = false;
        r.isNil_ = false;
        r.mapValue_ = value;
        return r;
    }

    static RedisReply Nil() {
        RedisReply r;
        r.isError_ = false;
        r.isNil_ = true;
        return r;
    }

    static RedisReply Error(const std::string& msg) {
        RedisReply r;
        r.isError_ = true;
        r.isNil_ = false;
        r.errorMsg_ = msg;
        return r;
    }

private:
    bool isError_ = false;
    bool isNil_ = false;
    std::string strValue_;
    int64_t intValue_ = 0;
    double dblValue_ = 0.0;
    std::vector<std::string> arrValue_;
    std::map<std::string, std::string> mapValue_;
    std::string errorMsg_;
};

/**
 * @brief Redis 连接配置
 */
struct RedisConfig {
    std::string host = "127.0.0.1";
    uint16_t port = 6379;
    std::string password;
    int database = 0;

    // 连接池配置
    size_t poolSize = 8;
    uint32_t connectTimeoutMs = 5000;
    uint32_t socketTimeoutMs = 30000;

    // 是否自动重连
    bool autoReconnect = true;
    uint32_t reconnectIntervalMs = 1000;

    // 是否保持连接
    bool keepAlive = true;
};

/**
 * @brief Redis 客户端接口
 *
 * 定义 Redis 操作的抽象接口
 * 支持多种实现（hiredis, redis-plus-plus 等）
 */
class IRedisClient {
public:
    virtual ~IRedisClient() = default;

    /**
     * @brief 连接到 Redis 服务器
     */
    virtual bool connect(const RedisConfig& config) = 0;

    /**
     * @brief 断开连接
     */
    virtual void disconnect() = 0;

    /**
     * @brief 检查连接状态
     */
    virtual bool isConnected() const = 0;

    /**
     * @brief Ping 测试
     */
    virtual RedisReply ping() = 0;

    // ========== 字符串操作 ==========

    /**
     * @brief 设置键值
     */
    virtual RedisReply set(const std::string& key, const std::string& value) = 0;

    /**
     * @brief 设置键值（带过期时间，秒）
     */
    virtual RedisReply set(const std::string& key, const std::string& value, int64_t ttl) = 0;

    /**
     * @brief 获取键值
     */
    virtual RedisReply get(const std::string& key) = 0;

    /**
     * @brief 删除键
     */
    virtual RedisReply del(const std::string& key) = 0;

    /**
     * @brief 批量删除
     */
    virtual RedisReply del(const std::vector<std::string>& keys) = 0;

    /**
     * @brief 检查键是否存在
     */
    virtual RedisReply exists(const std::string& key) = 0;

    /**
     * @brief 递增
     */
    virtual RedisReply incr(const std::string& key) = 0;

    /**
     * @brief 按值递增
     */
    virtual RedisReply incrBy(const std::string& key, int64_t increment) = 0;

    /**
     * @brief 递减
     */
    virtual RedisReply decr(const std::string& key) = 0;

    /**
     * @brief 按值递减
     */
    virtual RedisReply decrBy(const std::string& key, int64_t decrement) = 0;

    /**
     * @brief 设置过期时间（秒）
     */
    virtual RedisReply expire(const std::string& key, int64_t seconds) = 0;

    /**
     * @brief 设置过期时间（毫秒）
     */
    virtual RedisReply pexpire(const std::string& key, int64_t milliseconds) = 0;

    /**
     * @brief 获取剩余时间（秒）
     */
    virtual RedisReply ttl(const std::string& key) = 0;

    /**
     * @brief 获取剩余时间（毫秒）
     */
    virtual RedisReply pttl(const std::string& key) = 0;

    // ========== 哈希操作 ==========

    /**
     * @brief 设置哈希字段
     */
    virtual RedisReply hSet(const std::string& key, const std::string& field, const std::string& value) = 0;

    /**
     * @brief 获取哈希字段
     */
    virtual RedisReply hGet(const std::string& key, const std::string& field) = 0;

    /**
     * @brief 获取所有哈希字段
     */
    virtual RedisReply hGetAll(const std::string& key) = 0;

    /**
     * @brief 删除哈希字段
     */
    virtual RedisReply hDel(const std::string& key, const std::string& field) = 0;

    /**
     * @brief 检查哈希字段是否存在
     */
    virtual RedisReply hExists(const std::string& key, const std::string& field) = 0;

    /**
     * @brief 获取哈希所有字段名
     */
    virtual RedisReply hKeys(const std::string& key) = 0;

    /**
     * @brief 获取哈希所有值
     */
    virtual RedisReply hVals(const std::string& key) = 0;

    /**
     * @brief 获取哈希字段数量
     */
    virtual RedisReply hLen(const std::string& key) = 0;

    /**
     * @brief 批量设置哈希字段
     */
    virtual RedisReply hMSet(const std::string& key, const std::map<std::string, std::string>& values) = 0;

    /**
     * @brief 批量获取哈希字段
     */
    virtual RedisReply hMGet(const std::string& key, const std::vector<std::string>& fields) = 0;

    /**
     * @brief 哈希字段递增
     */
    virtual RedisReply hIncrBy(const std::string& key, const std::string& field, int64_t increment) = 0;

    // ========== 列表操作 ==========

    /**
     * @brief 左侧推入
     */
    virtual RedisReply lPush(const std::string& key, const std::string& value) = 0;

    /**
     * @brief 右侧推入
     */
    virtual RedisReply rPush(const std::string& key, const std::string& value) = 0;

    /**
     * @brief 左侧弹出
     */
    virtual RedisReply lPop(const std::string& key) = 0;

    /**
     * @brief 右侧弹出
     */
    virtual RedisReply rPop(const std::string& key) = 0;

    /**
     * @brief 获取列表范围
     */
    virtual RedisReply lRange(const std::string& key, int64_t start, int64_t stop) = 0;

    /**
     * @brief 获取列表长度
     */
    virtual RedisReply lLen(const std::string& key) = 0;

    /**
     * @brief 设置列表索引位置
     */
    virtual RedisReply lSet(const std::string& key, int64_t index, const std::string& value) = 0;

    /**
     * @brief 获取列表索引位置
     */
    virtual RedisReply lIndex(const std::string& key, int64_t index) = 0;

    /**
     * @brief 删除列表元素
     */
    virtual RedisReply lRem(const std::string& key, int64_t count, const std::string& value) = 0;

    // ========== 集合操作 ==========

    /**
     * @brief 添加集合成员
     */
    virtual RedisReply sAdd(const std::string& key, const std::string& member) = 0;

    /**
     * @brief 移除集合成员
     */
    virtual RedisReply sRem(const std::string& key, const std::string& member) = 0;

    /**
     * @brief 获取所有成员
     */
    virtual RedisReply sMembers(const std::string& key) = 0;

    /**
     * @brief 检查成员是否存在
     */
    virtual RedisReply sIsMember(const std::string& key, const std::string& member) = 0;

    /**
     * @brief 获取集合大小
     */
    virtual RedisReply sCard(const std::string& key) = 0;

    /**
     * @brief 随机弹出成员
     */
    virtual RedisReply sPop(const std::string& key) = 0;

    // ========== 有序集合操作 ==========

    /**
     * @brief 添加有序集合成员
     */
    virtual RedisReply zAdd(const std::string& key, double score, const std::string& member) = 0;

    /**
     * @brief 移除有序集合成员
     */
    virtual RedisReply zRem(const std::string& key, const std::string& member) = 0;

    /**
     * @brief 获取范围（按分数）
     */
    virtual RedisReply zRange(const std::string& key, int64_t start, int64_t stop) = 0;

    /**
     * @brief 获取范围（按分数，降序）
     */
    virtual RedisReply zRevRange(const std::string& key, int64_t start, int64_t stop) = 0;

    /**
     * @brief 获取成员分数
     */
    virtual RedisReply zScore(const std::string& key, const std::string& member) = 0;

    /**
     * @brief 按分数范围获取成员
     */
    virtual RedisReply zRangeByScore(const std::string& key, double min, double max) = 0;

    /**
     * @brief 获取成员排名
     */
    virtual RedisReply zRank(const std::string& key, const std::string& member) = 0;

    /**
     * @brief 获取成员排名（降序）
     */
    virtual RedisReply zRevRank(const std::string& key, const std::string& member) = 0;

    /**
     * @brief 分数递增
     */
    virtual RedisReply zIncrBy(const std::string& key, double increment, const std::string& member) = 0;

    // ========== 发布订阅 ==========

    /**
     * @brief 发布消息
     */
    virtual RedisReply publish(const std::string& channel, const std::string& message) = 0;

    // ========== 事务 ==========

    /**
     * @brief 开始事务
     */
    virtual bool multi() = 0;

    /**
     * @brief 取消事务
     */
    virtual bool discard() = 0;

    /**
     * @brief 执行事务
     */
    virtual std::vector<RedisReply> exec() = 0;

    // ========== 通用操作 ==========

    /**
     * @brief 选择数据库
     */
    virtual RedisReply select(int database) = 0;

    /**
     * @brief 获取所有匹配的键
     */
    virtual RedisReply keys(const std::string& pattern) = 0;

    /**
     * @brief 扫描键
     */
    virtual std::vector<std::string> scan(const std::string& pattern, uint64_t cursor, uint64_t* newCursor) = 0;

    /**
     * @brief 清空当前数据库
     */
    virtual RedisReply flushDb() = 0;

    /**
     * @brief 获取数据库大小
     */
    virtual RedisReply dbSize() = 0;

    /**
     * @brief 获取信息
     */
    virtual std::string info(const std::string& section = "") = 0;
};

/**
 * @brief Redis 客户端智能指针类型
 */
using IRedisClientPtr = std::shared_ptr<IRedisClient>;

/**
 * @brief Redis 工厂
 *
 * 根据编译选项创建不同的 Redis 实现
 */
class RedisFactory {
public:
    /**
     * @brief 创建 Redis 客户端
     *
     * @param type 实现类型："hiredis", "redis-plus-plus", "mock"
     */
    static IRedisClientPtr create(const std::string& type = "hiredis");
};

/**
 * @brief Redis 管理器（单例）
 *
 * 管理默认的 Redis 连接
 */
class RedisManager {
public:
    static RedisManager& instance();

    /**
     * @brief 初始化
     */
    bool initialize(const RedisConfig& config,
                   const std::string& implType = "hiredis");

    /**
     * @brief 获取客户端
     */
    IRedisClientPtr getClient();

    /**
     * @brief 关闭连接
     */
    void shutdown();

    /**
     * @brief 检查是否已初始化
     */
    bool isInitialized() const { return client_ != nullptr; }

private:
    RedisManager() = default;
    ~RedisManager() = default;

    IRedisClientPtr client_;
    RedisConfig config_;
};

} // namespace redis
} // namespace storage
} // namespace apollo

// 便捷宏
#define APOLLO_REDIS() apollo::storage::redis::RedisManager::instance().getClient()
