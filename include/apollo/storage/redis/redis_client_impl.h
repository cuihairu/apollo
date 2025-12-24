#pragma once

#include "apollo/storage/redis/redis.h"

// 根据 RedisPlusPlus 是否可用选择实现
#ifdef APOLLO_USE_REDIS_PLUS_PLUS

#include <sw/redis++/redis++.h>
#include <memory>

namespace apollo {
namespace storage {
namespace redis {

/**
 * @brief 基于 redis-plus-plus 的 Redis 客户端实现
 *
 * 特性：
 * - 内置连接池
 * - 支持 Pipeline
 * - 支持 Redis Sentinel/Cluster
 * - 线程安全
 */
class RedisPlusPlusClient : public IRedisClient {
public:
    RedisPlusPlusClient() = default;
    ~RedisPlusPlusClient() override;

    bool connect(const RedisConfig& config) override;
    void disconnect() override;
    bool isConnected() const override;

    RedisReply ping() override;

    // 字符串操作
    RedisReply set(const std::string& key, const std::string& value) override;
    RedisReply set(const std::string& key, const std::string& value, int64_t ttl) override;
    RedisReply get(const std::string& key) override;
    RedisReply del(const std::string& key) override;
    RedisReply del(const std::vector<std::string>& keys) override;
    RedisReply exists(const std::string& key) override;
    RedisReply incr(const std::string& key) override;
    RedisReply incrBy(const std::string& key, int64_t increment) override;
    RedisReply decr(const std::string& key) override;
    RedisReply decrBy(const std::string& key, int64_t decrement) override;
    RedisReply expire(const std::string& key, int64_t seconds) override;
    RedisReply pexpire(const std::string& key, int64_t milliseconds) override;
    RedisReply ttl(const std::string& key) override;
    RedisReply pttl(const std::string& key) override;

    // 哈希操作
    RedisReply hSet(const std::string& key, const std::string& field, const std::string& value) override;
    RedisReply hGet(const std::string& key, const std::string& field) override;
    RedisReply hGetAll(const std::string& key) override;
    RedisReply hDel(const std::string& key, const std::string& field) override;
    RedisReply hExists(const std::string& key, const std::string& field) override;
    RedisReply hKeys(const std::string& key) override;
    RedisReply hVals(const std::string& key) override;
    RedisReply hLen(const std::string& key) override;
    RedisReply hMSet(const std::string& key, const std::map<std::string, std::string>& values) override;
    RedisReply hMGet(const std::string& key, const std::vector<std::string>& fields) override;
    RedisReply hIncrBy(const std::string& key, const std::string& field, int64_t increment) override;

    // 列表操作
    RedisReply lPush(const std::string& key, const std::string& value) override;
    RedisReply rPush(const std::string& key, const std::string& value) override;
    RedisReply lPop(const std::string& key) override;
    RedisReply rPop(const std::string& key) override;
    RedisReply lRange(const std::string& key, int64_t start, int64_t stop) override;
    RedisReply lLen(const std::string& key) override;
    RedisReply lSet(const std::string& key, int64_t index, const std::string& value) override;
    RedisReply lIndex(const std::string& key, int64_t index) override;
    RedisReply lRem(const std::string& key, int64_t count, const std::string& value) override;

    // 集合操作
    RedisReply sAdd(const std::string& key, const std::string& member) override;
    RedisReply sRem(const std::string& key, const std::string& member) override;
    RedisReply sMembers(const std::string& key) override;
    RedisReply sIsMember(const std::string& key, const std::string& member) override;
    RedisReply sCard(const std::string& key) override;
    RedisReply sPop(const std::string& key) override;

    // 有序集合操作
    RedisReply zAdd(const std::string& key, double score, const std::string& member) override;
    RedisReply zRem(const std::string& key, const std::string& member) override;
    RedisReply zRange(const std::string& key, int64_t start, int64_t stop) override;
    RedisReply zRevRange(const std::string& key, int64_t start, int64_t stop) override;
    RedisReply zScore(const std::string& key, const std::string& member) override;
    RedisReply zRangeByScore(const std::string& key, double min, double max) override;
    RedisReply zRank(const std::string& key, const std::string& member) override;
    RedisReply zRevRank(const std::string& key, const std::string& member) override;
    RedisReply zIncrBy(const std::string& key, double increment, const std::string& member) override;

    // 发布订阅
    RedisReply publish(const std::string& channel, const std::string& message) override;

    // 事务
    bool multi() override;
    bool discard() override;
    std::vector<RedisReply> exec() override;

    // 通用操作
    RedisReply select(int database) override;
    RedisReply keys(const std::string& pattern) override;
    std::vector<std::string> scan(const std::string& pattern, uint64_t cursor, uint64_t* newCursor) override;
    RedisReply flushDb() override;
    RedisReply dbSize() override;
    std::string info(const std::string& section) override;

private:
    std::shared_ptr<sw::redis::Redis> redis_;
    std::unique_ptr<sw::redis::Transaction> transaction_;
    RedisConfig config_;

    // 辅助函数：转换异常为 RedisReply
    template<typename Func>
    RedisReply tryExecute(Func&& func);
};

} // namespace redis
} // namespace storage
} // namespace apollo

#endif // APOLLO_USE_REDIS_PLUS_PLUS
