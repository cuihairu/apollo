#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <future>
#include <chrono>

namespace apollo {
namespace net {
namespace redis {

//==============================================================================
// Redis 值类型
//==============================================================================

enum class RedisType {
    None,
    String,
    List,
    Set,
    SortedSet,
    Hash,
    Stream
};

//==============================================================================
// Redis 响应
//==============================================================================

struct RedisResponse {
    bool ok = false;              // 是否成功
    std::string value;            // 字符串值
    int64_t integer = 0;          // 整数值
    double floating = 0.0;        // 浮点数值
    std::vector<std::string> array;  // 数组值
    std::map<std::string, std::string> map;  // Hash 值
    std::string error;            // 错误信息

    // 便捷方法
    bool isOk() const { return ok && error.empty(); }
    bool isError() const { return !error.empty(); }
    explicit operator bool() const { return isOk(); }

    // 获取单个值
    const std::string& asString() const { return value; }
    int64_t asInt() const { return integer; }
    double asDouble() const { return floating; }
    const std::vector<std::string>& asArray() const { return array; }
    const std::map<std::string, std::string>& asMap() const { return map; }
};

//==============================================================================
// 连接配置
//==============================================================================

struct RedisConfig {
    std::string host = "localhost";
    uint16_t port = 6379;
    std::string password;
    int database = 0;
    uint32_t timeoutMs = 3000;
    uint32_t poolSize = 10;

    // Sentinel 配置
    std::vector<std::string> sentinelHosts;
    std::string sentinelMasterName;

    // Cluster 配置
    std::vector<std::pair<std::string, uint16_t>> clusterNodes;

    bool isCluster() const { return !clusterNodes.empty(); }
    bool isSentinel() const { return !sentinelHosts.empty(); }
};

//==============================================================================
// RedisTemplate - 类似 Spring Data Redis 的 Redis 客户端
//==============================================================================

class RedisTemplate {
public:
    RedisTemplate();
    ~RedisTemplate();

    // 禁止拷贝，允许移动
    RedisTemplate(const RedisTemplate&) = delete;
    RedisTemplate& operator=(const RedisTemplate&) = delete;
    RedisTemplate(RedisTemplate&&) noexcept;
    RedisTemplate& operator=(RedisTemplate&&) noexcept;

    //==========================================================================
    // 连接管理
    //==========================================================================

    // 连接到 Redis
    bool connect(const RedisConfig& config);
    bool connect(const std::string& host, uint16_t port = 6379,
                const std::string& password = "", int database = 0);

    // 断开连接
    void disconnect();

    // 是否已连接
    bool isConnected() const;

    // Ping
    bool ping();
    RedisResponse pingWithResponse();

    // 选择数据库
    bool select(int database);

    //==========================================================================
    // String 操作
    //==========================================================================

    // SET
    bool set(const std::string& key, const std::string& value);
    bool set(const std::string& key, const std::string& value, uint64_t ttlMs);
    bool setEx(const std::string& key, uint64_t seconds, const std::string& value);
    bool pSetEx(const std::string& key, uint64_t milliseconds, const std::string& value);

    // SETNX (只在 key 不存在时设置)
    bool setNx(const std::string& key, const std::string& value);

    // GET
    RedisResponse get(const std::string& key);

    // MGET
    std::vector<std::string> mGet(const std::vector<std::string>& keys);

    // MSET
    bool mSet(const std::map<std::string, std::string>& keyValuePairs);

    // INCR / DECR
    int64_t incr(const std::string& key);
    int64_t incrBy(const std::string& key, int64_t delta);
    double incrByFloat(const std::string& key, double delta);
    int64_t decr(const std::string& key);
    int64_t decrBy(const std::string& key, int64_t delta);

    // STRLEN
    int64_t strLen(const std::string& key);

    // APPEND
    int64_t append(const std::string& key, const std::string& value);

    //==========================================================================
    // Key 操作
    //==========================================================================

    // EXISTS
    bool exists(const std::string& key);
    int64_t count(const std::vector<std::string>& keys);

    // DEL
    int64_t del(const std::string& key);
    int64_t del(const std::vector<std::string>& keys);

    // EXPIRE / TTL
    bool expire(const std::string& key, uint64_t seconds);
    bool pExpire(const std::string& key, uint64_t milliseconds);
    bool expireAt(const std::string& key, int64_t timestamp);
    int64_t ttl(const std::string& key);
    int64_t pTtl(const std::string& key);

    // PERSIST
    bool persist(const std::string& key);

    // RENAME
    bool rename(const std::string& oldKey, const std::string& newKey);
    bool renameNx(const std::string& oldKey, const std::string& newKey);

    // TYPE
    RedisType type(const std::string& key);

    // KEYS / SCAN
    std::vector<std::string> keys(const std::string& pattern);
    std::vector<std::string> scan(const std::string& pattern, uint64_t cursor,
                                  uint64_t& newCursor);

    //==========================================================================
    // List 操作
    //==========================================================================

    // LPUSH / RPUSH
    int64_t lPush(const std::string& key, const std::string& value);
    int64_t rPush(const std::string& key, const std::string& value);
    int64_t lPush(const std::string& key, const std::vector<std::string>& values);
    int64_t rPush(const std::string& key, const std::vector<std::string>& values);

    // LPOP / RPOP
    std::string lPop(const std::string& key);
    std::string rPop(const std::string& key);

    // BLPOP / BRPOP (阻塞弹出)
    std::string bLPop(const std::string& key, uint64_t timeoutMs);
    std::string bRPop(const std::string& key, uint64_t timeoutMs);

    // LLEN
    int64_t lLen(const std::string& key);

    // LINDEX
    std::string lIndex(const std::string& key, int64_t index);

    // LRANGE
    std::vector<std::string> lRange(const std::string& key, int64_t start,
                                    int64_t stop);

    // LTRIM
    bool lTrim(const std::string& key, int64_t start, int64_t stop);

    // LSET
    bool lSet(const std::string& key, int64_t index, const std::string& value);

    //==========================================================================
    // Set 操作
    //==========================================================================

    // SADD
    int64_t sAdd(const std::string& key, const std::string& member);
    int64_t sAdd(const std::string& key, const std::vector<std::string>& members);

    // SREM
    int64_t sRem(const std::string& key, const std::string& member);
    int64_t sRem(const std::string& key, const std::vector<std::string>& members);

    // SISMEMBER
    bool sIsMember(const std::string& key, const std::string& member);

    // SMEMBERS
    std::vector<std::string> sMembers(const std::string& key);

    // SCARD
    int64_t sCard(const std::string& key);

    // SRANDMEMBER
    std::string sRandMember(const std::string& key);
    std::vector<std::string> sRandMember(const std::string& key, int64_t count);

    // SPOP
    std::string sPop(const std::string& key);
    std::vector<std::string> sPop(const std::string& key, int64_t count);

    // SINTER / SUNION / SDIFF
    std::vector<std::string> sInter(const std::vector<std::string>& keys);
    std::vector<std::string> sUnion(const std::vector<std::string>& keys);
    std::vector<std::string> sDiff(const std::vector<std::string>& keys);

    //==========================================================================
    // Sorted Set 操作
    //==========================================================================

    // ZADD
    int64_t zAdd(const std::string& key, double score, const std::string& member);
    int64_t zAdd(const std::string& key,
                 const std::map<double, std::string>& scoreMembers);

    // ZREM
    int64_t zRem(const std::string& key, const std::string& member);
    int64_t zRem(const std::string& key, const std::vector<std::string>& members);

    // ZINCRBY
    double zIncrBy(const std::string& key, double delta, const std::string& member);

    // ZSCORE
    RedisResponse zScore(const std::string& key, const std::string& member);

    // ZRANGE / ZREVRANGE
    std::vector<std::string> zRange(const std::string& key, int64_t start,
                                    int64_t stop, bool withScores = false);
    std::vector<std::string> zRevRange(const std::string& key, int64_t start,
                                       int64_t stop, bool withScores = false);

    // ZRANGEBYSCORE / ZREVRANGEBYSCORE
    std::vector<std::string> zRangeByScore(const std::string& key, double min,
                                           double max, bool withScores = false);
    std::vector<std::string> zRevRangeByScore(const std::string& key, double min,
                                              double max, bool withScores = false);

    // ZRANK / ZREVRANK
    int64_t zRank(const std::string& key, const std::string& member);
    int64_t zRevRank(const std::string& key, const std::string& member);

    // ZCARD
    int64_t zCard(const std::string& key);

    // ZCOUNT
    int64_t zCount(const std::string& key, double min, double max);

    //==========================================================================
    // Hash 操作
    //==========================================================================

    // HSET / HSETNX
    bool hSet(const std::string& key, const std::string& field,
              const std::string& value);
    bool hSetNx(const std::string& key, const std::string& field,
                const std::string& value);

    // HGET
    RedisResponse hGet(const std::string& key, const std::string& field);

    // HMGET
    std::vector<std::string> hmGet(const std::string& key,
                                   const std::vector<std::string>& fields);

    // HMSET
    bool hmSet(const std::string& key,
               const std::map<std::string, std::string>& fieldValueMap);

    // HGETALL
    std::map<std::string, std::string> hGetAll(const std::string& key);

    // HDEL
    int64_t hDel(const std::string& key, const std::vector<std::string>& fields);

    // HEXISTS
    bool hExists(const std::string& key, const std::string& field);

    // HKEYS / HVALS
    std::vector<std::string> hKeys(const std::string& key);
    std::vector<std::string> hVals(const std::string& key);

    // HINCRBY / HINCRBYFLOAT
    int64_t hIncrBy(const std::string& key, const std::string& field, int64_t delta);
    double hIncrByFloat(const std::string& key, const std::string& field,
                        double delta);

    // HLEN
    int64_t hLen(const std::string& key);

    //==========================================================================
    // 事务
    //==========================================================================

    // MULTI
    void multi();

    // EXEC
    std::vector<RedisResponse> exec();

    // DISCARD
    void discard();

    // WATCH
    bool watch(const std::vector<std::string>& keys);

    // UNWATCH
    void unwatch();

    //==========================================================================
    // Pipeline (批量操作)
    //==========================================================================

    // 开启 pipeline
    void pipelineEnable();

    // 执行 pipeline
    std::vector<RedisResponse> pipelineExec();

    //==========================================================================
    // 发布订阅
    //==========================================================================

    // PUBLISH
    int64_t publish(const std::string& channel, const std::string& message);

    // SUBSCRIBE
    using SubscribeCallback = std::function<void(const std::string& channel,
                                                  const std::string& message)>;
    void subscribe(const std::string& channel, SubscribeCallback callback);

    // UNSUBSCRIBE
    void unsubscribe(const std::string& channel);

    // PSUBSCRIBE (模式订阅)
    void pSubscribe(const std::string& pattern, SubscribeCallback callback);

    //==========================================================================
    // 脚本 (Lua)
    //==========================================================================

    // EVAL
    RedisResponse eval(const std::string& script,
                      const std::vector<std::string>& keys,
                      const std::vector<std::string>& args);

    // EVALSHA
    RedisResponse evalSha(const std::string& sha,
                          const std::vector<std::string>& keys,
                          const std::vector<std::string>& args);

    // SCRIPT LOAD
    std::string scriptLoad(const std::string& script);

    //==========================================================================
    // 游戏服务器专用便捷方法
    //==========================================================================

    // 排行榜 - 获取排名
    int64_t leaderboardGetRank(const std::string& key, const std::string& member);

    // 排行榜 - 获取分数
    double leaderboardGetScore(const std::string& key, const std::string& member);

    // 排行榜 - 设置分数
    bool leaderboardSetScore(const std::string& key, const std::string& member,
                             double score);

    // 排行榜 - 获取 Top N
    std::vector<std::pair<std::string, double>> leaderboardGetTopN(
        const std::string& key, int64_t n);

    // 排行榜 - 按分数范围获取
    std::vector<std::pair<std::string, double>> leaderboardGetRange(
        const std::string& key, double min, double max);

    // 分布式锁
    class Lock {
    public:
        Lock(RedisTemplate* redis, const std::string& key,
             const std::string& value, uint64_t ttlMs);
        ~Lock();

        bool acquired() const { return acquired_; }
        explicit operator bool() const { return acquired_; }

        // 释放锁
        void unlock();

        // 续期锁
        bool renew(uint64_t ttlMs);

    private:
        RedisTemplate* redis_;
        std::string key_;
        std::string value_;
        uint64_t ttlMs_;
        bool acquired_;
    };

    // 获取分布式锁
    std::unique_ptr<Lock> lock(const std::string& key, uint64_t ttlMs = 10000);
    std::unique_ptr<Lock> lock(const std::string& key, const std::string& value,
                               uint64_t ttlMs = 10000);

    // 计数器 (原子递增)
    int64_t counterIncr(const std::string& key, int64_t delta = 1);

    // 限流 (令牌桶)
    bool rateLimit(const std::string& key, int64_t maxRequests, uint64_t windowMs);

private:
    class Impl;
    Impl* impl_;
};

//==============================================================================
// RedisTemplateBuilder
//==============================================================================

class RedisTemplateBuilder {
public:
    RedisTemplateBuilder();

    RedisTemplateBuilder& host(const std::string& host);
    RedisTemplateBuilder& port(uint16_t port);
    RedisTemplateBuilder& password(const std::string& password);
    RedisTemplateBuilder& database(int db);
    RedisTemplateBuilder& timeout(uint32_t timeoutMs);
    RedisTemplateBuilder& poolSize(size_t size);

    RedisTemplate build() const;

private:
    RedisConfig config_;
};

//==============================================================================
// 便捷全局函数
//==============================================================================

// 快速创建 RedisTemplate
inline RedisTemplate redisConnect(const std::string& host = "localhost",
                                   uint16_t port = 6379) {
    RedisTemplate redis;
    redis.connect(host, port);
    return redis;
}

} // namespace redis
} // namespace net
} // namespace apollo
