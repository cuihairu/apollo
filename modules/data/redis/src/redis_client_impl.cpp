/**
 * @file redis_client_impl.cpp
 * @brief 基于 redis-plus-plus 的 Redis 客户端实现
 */

#ifdef APOLLO_USE_REDIS_PLUS_PLUS

#include "apollo/storage/redis/redis_client_impl.h"
#include <stdexcept>

namespace apollo {
namespace storage {
namespace redis {

//==============================================================================
// RedisPlusPlusClient 实现
//==============================================================================

RedisPlusPlusClient::~RedisPlusPlusClient() {
    disconnect();
}

bool RedisPlusPlusClient::connect(const RedisConfig& config) {
    config_ = config;

    try {
        // 创建连接选项
        sw::redis::ConnectionOptions opts;
        opts.host = config.host;
        opts.port = config.port;
        opts.password = config.password;
        opts.db = config.database;
        opts.connect_timeout = std::chrono::milliseconds(config.connectTimeoutMs);
        opts.socket_timeout = std::chrono::milliseconds(config.socketTimeoutMs);
        opts.keep_alive = config.keepAlive;
        // Note: redis-plus-plus auto-reconnects by default
        // opts.reconnect = config.autoReconnect;  // Different field name
        // opts.reconnect_interval = std::chrono::milliseconds(config.reconnectIntervalMs);

        // 创建连接池选项
        sw::redis::ConnectionPoolOptions poolOpts;
        poolOpts.size = config.poolSize;

        // 创建 Redis 实例（带连接池）
        redis_ = std::make_shared<sw::redis::Redis>(opts, poolOpts);

        // 测试连接
        redis_->ping();

        return true;
    } catch (const std::exception& e) {
        // 错误处理
        return false;
    }
}

void RedisPlusPlusClient::disconnect() {
    redis_.reset();
    transaction_.reset();
}

bool RedisPlusPlusClient::isConnected() const {
    return redis_ != nullptr;
}

template<typename Func>
RedisReply RedisPlusPlusClient::tryExecute(Func&& func) {
    if (!redis_) {
        return RedisReply::Error("Not connected to Redis");
    }

    try {
        return func();
    } catch (const sw::redis::ReplyError& e) {
        return RedisReply::Error(e.what());
    } catch (const sw::redis::TimeoutError& e) {
        return RedisReply::Error("Timeout: " + std::string(e.what()));
    } catch (const std::exception& e) {
        return RedisReply::Error(e.what());
    }
}

RedisReply RedisPlusPlusClient::ping() {
    return tryExecute([this]() {
        auto val = redis_->ping();
        return RedisReply::String(val);
    });
}

// ========== 字符串操作 ==========

RedisReply RedisPlusPlusClient::set(const std::string& key, const std::string& value) {
    return tryExecute([this, &key, &value]() {
        redis_->set(key, value);
        return RedisReply::Ok();
    });
}

RedisReply RedisPlusPlusClient::set(const std::string& key, const std::string& value, int64_t ttl) {
    return tryExecute([this, &key, &value, ttl]() {
        redis_->set(key, value, std::chrono::seconds(ttl));
        return RedisReply::Ok();
    });
}

RedisReply RedisPlusPlusClient::get(const std::string& key) {
    return tryExecute([this, &key]() {
        auto val = redis_->get(key);
        if (val) {
            return RedisReply::String(*val);
        }
        return RedisReply::Nil();
    });
}

RedisReply RedisPlusPlusClient::del(const std::string& key) {
    return tryExecute([this, &key]() {
        auto count = redis_->del(key);
        return RedisReply::Int64(count);
    });
}

RedisReply RedisPlusPlusClient::del(const std::vector<std::string>& keys) {
    return tryExecute([this, &keys]() {
        long long count = 0;
        for (const auto& key : keys) {
            count += redis_->del(key);
        }
        return RedisReply::Int64(count);
    });
}

RedisReply RedisPlusPlusClient::exists(const std::string& key) {
    return tryExecute([this, &key]() {
        auto val = redis_->exists(key);
        return RedisReply::Int64(val);
    });
}

RedisReply RedisPlusPlusClient::incr(const std::string& key) {
    return tryExecute([this, &key]() {
        auto val = redis_->incr(key);
        return RedisReply::Int64(val);
    });
}

RedisReply RedisPlusPlusClient::incrBy(const std::string& key, int64_t increment) {
    return tryExecute([this, &key, increment]() {
        auto val = redis_->incrby(key, increment);
        return RedisReply::Int64(val);
    });
}

RedisReply RedisPlusPlusClient::decr(const std::string& key) {
    return tryExecute([this, &key]() {
        auto val = redis_->decr(key);
        return RedisReply::Int64(val);
    });
}

RedisReply RedisPlusPlusClient::decrBy(const std::string& key, int64_t decrement) {
    return tryExecute([this, &key, decrement]() {
        auto val = redis_->decrby(key, decrement);
        return RedisReply::Int64(val);
    });
}

RedisReply RedisPlusPlusClient::expire(const std::string& key, int64_t seconds) {
    return tryExecute([this, &key, seconds]() {
        auto val = redis_->expire(key, std::chrono::seconds(seconds));
        return RedisReply::Int64(val ? 1 : 0);
    });
}

RedisReply RedisPlusPlusClient::pexpire(const std::string& key, int64_t milliseconds) {
    return tryExecute([this, &key, milliseconds]() {
        auto val = redis_->pexpire(key, std::chrono::milliseconds(milliseconds));
        return RedisReply::Int64(val ? 1 : 0);
    });
}

RedisReply RedisPlusPlusClient::ttl(const std::string& key) {
    return tryExecute([this, &key]() {
        auto val = redis_->ttl(key);
        // Redis returns: -2 if key doesn't exist, -1 if no expiry
        return RedisReply::Int64(val);
    });
}

RedisReply RedisPlusPlusClient::pttl(const std::string& key) {
    return tryExecute([this, &key]() {
        auto val = redis_->pttl(key);
        // Redis returns: -2 if key doesn't exist, -1 if no expiry
        return RedisReply::Int64(val);
    });
}

// ========== 哈希操作 ==========

RedisReply RedisPlusPlusClient::hSet(const std::string& key, const std::string& field, const std::string& value) {
    return tryExecute([this, &key, &field, &value]() {
        redis_->hset(key, field, value);
        return RedisReply::Ok();
    });
}

RedisReply RedisPlusPlusClient::hGet(const std::string& key, const std::string& field) {
    return tryExecute([this, &key, &field]() {
        auto val = redis_->hget(key, field);
        if (val) {
            return RedisReply::String(*val);
        }
        return RedisReply::Nil();
    });
}

RedisReply RedisPlusPlusClient::hGetAll(const std::string& key) {
    return tryExecute([this, &key]() {
        std::vector<std::pair<std::string, std::string>> result;
        redis_->hgetall(key, std::back_inserter(result));
        std::map<std::string, std::string> mapResult(result.begin(), result.end());
        return RedisReply::Map(mapResult);
    });
}

RedisReply RedisPlusPlusClient::hDel(const std::string& key, const std::string& field) {
    return tryExecute([this, &key, &field]() {
        auto val = redis_->hdel(key, field);
        return RedisReply::Int64(val);
    });
}

RedisReply RedisPlusPlusClient::hExists(const std::string& key, const std::string& field) {
    return tryExecute([this, &key, &field]() {
        auto val = redis_->hexists(key, field);
        return RedisReply::Int64(val ? 1 : 0);
    });
}

RedisReply RedisPlusPlusClient::hKeys(const std::string& key) {
    return tryExecute([this, &key]() {
        std::vector<std::string> result;
        redis_->hkeys(key, std::back_inserter(result));
        return RedisReply::Array(result);
    });
}

RedisReply RedisPlusPlusClient::hVals(const std::string& key) {
    return tryExecute([this, &key]() {
        std::vector<std::string> result;
        redis_->hvals(key, std::back_inserter(result));
        return RedisReply::Array(result);
    });
}

RedisReply RedisPlusPlusClient::hLen(const std::string& key) {
    return tryExecute([this, &key]() {
        auto val = redis_->hlen(key);
        return RedisReply::Int64(val);
    });
}

RedisReply RedisPlusPlusClient::hMSet(const std::string& key, const std::map<std::string, std::string>& values) {
    return tryExecute([this, &key, &values]() {
        // redis-plus-plus uses hset() with multiple fields
        redis_->hset(key, values.begin(), values.end());
        return RedisReply::Ok();
    });
}

RedisReply RedisPlusPlusClient::hMGet(const std::string& key, const std::vector<std::string>& fields) {
    return tryExecute([this, &key, &fields]() {
        std::vector<sw::redis::OptionalString> result;
        redis_->hmget(key, fields.begin(), fields.end(), std::back_inserter(result));
        std::vector<std::string> stringResult;
        for (const auto& val : result) {
            stringResult.push_back(val ? *val : "");
        }
        return RedisReply::Array(stringResult);
    });
}

RedisReply RedisPlusPlusClient::hIncrBy(const std::string& key, const std::string& field, int64_t increment) {
    return tryExecute([this, &key, &field, increment]() {
        auto val = redis_->hincrby(key, field, increment);
        return RedisReply::Int64(val);
    });
}

// ========== 列表操作 ==========

RedisReply RedisPlusPlusClient::lPush(const std::string& key, const std::string& value) {
    return tryExecute([this, &key, &value]() {
        auto val = redis_->lpush(key, value);
        return RedisReply::Int64(val);
    });
}

RedisReply RedisPlusPlusClient::rPush(const std::string& key, const std::string& value) {
    return tryExecute([this, &key, &value]() {
        auto val = redis_->rpush(key, value);
        return RedisReply::Int64(val);
    });
}

RedisReply RedisPlusPlusClient::lPop(const std::string& key) {
    return tryExecute([this, &key]() {
        auto val = redis_->lpop(key);
        if (val) {
            return RedisReply::String(*val);
        }
        return RedisReply::Nil();
    });
}

RedisReply RedisPlusPlusClient::rPop(const std::string& key) {
    return tryExecute([this, &key]() {
        auto val = redis_->rpop(key);
        if (val) {
            return RedisReply::String(*val);
        }
        return RedisReply::Nil();
    });
}

RedisReply RedisPlusPlusClient::lRange(const std::string& key, int64_t start, int64_t stop) {
    return tryExecute([this, &key, start, stop]() {
        std::vector<std::string> result;
        redis_->lrange(key, start, stop, std::back_inserter(result));
        return RedisReply::Array(result);
    });
}

RedisReply RedisPlusPlusClient::lLen(const std::string& key) {
    return tryExecute([this, &key]() {
        auto val = redis_->llen(key);
        return RedisReply::Int64(val);
    });
}

RedisReply RedisPlusPlusClient::lSet(const std::string& key, int64_t index, const std::string& value) {
    return tryExecute([this, &key, index, &value]() {
        redis_->lset(key, index, value);
        return RedisReply::Ok();
    });
}

RedisReply RedisPlusPlusClient::lIndex(const std::string& key, int64_t index) {
    return tryExecute([this, &key, index]() {
        auto val = redis_->lindex(key, index);
        if (val) {
            return RedisReply::String(*val);
        }
        return RedisReply::Nil();
    });
}

RedisReply RedisPlusPlusClient::lRem(const std::string& key, int64_t count, const std::string& value) {
    return tryExecute([this, &key, count, &value]() {
        auto val = redis_->lrem(key, count, value);
        return RedisReply::Int64(val);
    });
}

// ========== 集合操作 ==========

RedisReply RedisPlusPlusClient::sAdd(const std::string& key, const std::string& member) {
    return tryExecute([this, &key, &member]() {
        auto val = redis_->sadd(key, member);
        return RedisReply::Int64(val);
    });
}

RedisReply RedisPlusPlusClient::sRem(const std::string& key, const std::string& member) {
    return tryExecute([this, &key, &member]() {
        auto val = redis_->srem(key, member);
        return RedisReply::Int64(val);
    });
}

RedisReply RedisPlusPlusClient::sMembers(const std::string& key) {
    return tryExecute([this, &key]() {
        std::vector<std::string> result;
        redis_->smembers(key, std::back_inserter(result));
        return RedisReply::Array(result);
    });
}

RedisReply RedisPlusPlusClient::sIsMember(const std::string& key, const std::string& member) {
    return tryExecute([this, &key, &member]() {
        auto val = redis_->sismember(key, member);
        return RedisReply::Int64(val ? 1 : 0);
    });
}

RedisReply RedisPlusPlusClient::sCard(const std::string& key) {
    return tryExecute([this, &key]() {
        auto val = redis_->scard(key);
        return RedisReply::Int64(val);
    });
}

RedisReply RedisPlusPlusClient::sPop(const std::string& key) {
    return tryExecute([this, &key]() {
        auto val = redis_->spop(key);
        if (val) {
            return RedisReply::String(*val);
        }
        return RedisReply::Nil();
    });
}

// ========== 有序集合操作 ==========

RedisReply RedisPlusPlusClient::zAdd(const std::string& key, double score, const std::string& member) {
    return tryExecute([this, &key, score, &member]() {
        redis_->zadd(key, member, score);
        return RedisReply::Ok();
    });
}

RedisReply RedisPlusPlusClient::zRem(const std::string& key, const std::string& member) {
    return tryExecute([this, &key, &member]() {
        auto val = redis_->zrem(key, member);
        return RedisReply::Int64(val);
    });
}

RedisReply RedisPlusPlusClient::zRange(const std::string& key, int64_t start, int64_t stop) {
    return tryExecute([this, &key, start, stop]() {
        std::vector<std::string> result;
        redis_->zrange(key, start, stop, std::back_inserter(result));
        return RedisReply::Array(result);
    });
}

RedisReply RedisPlusPlusClient::zRevRange(const std::string& key, int64_t start, int64_t stop) {
    return tryExecute([this, &key, start, stop]() {
        std::vector<std::string> result;
        redis_->zrevrange(key, start, stop, std::back_inserter(result));
        return RedisReply::Array(result);
    });
}

RedisReply RedisPlusPlusClient::zScore(const std::string& key, const std::string& member) {
    return tryExecute([this, &key, &member]() {
        auto val = redis_->zscore(key, member);
        if (val) {
            return RedisReply::Double(*val);
        }
        return RedisReply::Nil();
    });
}

RedisReply RedisPlusPlusClient::zRangeByScore(const std::string& key, double min, double max) {
    return tryExecute([this, &key, min, max]() {
        std::vector<std::string> result;
        // Use BoundedInterval for zrangebyscore
        redis_->zrangebyscore(key, sw::redis::BoundedInterval<double>(min, max, sw::redis::BoundType::CLOSED),
                              std::back_inserter(result));
        return RedisReply::Array(result);
    });
}

RedisReply RedisPlusPlusClient::zRank(const std::string& key, const std::string& member) {
    return tryExecute([this, &key, &member]() {
        auto val = redis_->zrank(key, member);
        if (val) {
            return RedisReply::Int64(static_cast<int64_t>(*val));
        }
        return RedisReply::Nil();
    });
}

RedisReply RedisPlusPlusClient::zRevRank(const std::string& key, const std::string& member) {
    return tryExecute([this, &key, &member]() {
        auto val = redis_->zrevrank(key, member);
        if (val) {
            return RedisReply::Int64(static_cast<int64_t>(*val));
        }
        return RedisReply::Nil();
    });
}

RedisReply RedisPlusPlusClient::zIncrBy(const std::string& key, double increment, const std::string& member) {
    return tryExecute([this, &key, increment, &member]() {
        auto val = redis_->zincrby(key, increment, member);
        return RedisReply::Double(val);
    });
}

// ========== 发布订阅 ==========

RedisReply RedisPlusPlusClient::publish(const std::string& channel, const std::string& message) {
    return tryExecute([this, &channel, &message]() {
        auto val = redis_->publish(channel, message);
        return RedisReply::Int64(val);
    });
}

// ========== 事务 ==========

bool RedisPlusPlusClient::multi() {
    if (!redis_) return false;
    try {
        // redis-plus-plus transaction() returns Transaction by value
        // We create a new Transaction and store it
        auto tx = redis_->transaction(true);  // pipelined = false
        transaction_ = std::make_unique<sw::redis::Transaction>(std::move(tx));
        inTransaction_ = true;
        return true;
    } catch (...) {
        return false;
    }
}

bool RedisPlusPlusClient::discard() {
    if (!transaction_) return false;
    try {
        transaction_->discard();
        transaction_.reset();
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<RedisReply> RedisPlusPlusClient::exec() {
    std::vector<RedisReply> result;
    if (!transaction_) return result;

    try {
        auto replies = transaction_->exec();
        // 简化处理：redis-plus-plus 事务返回的是 void
        // 实际应用中需要记录事务过程中的命令
        transaction_.reset();
    } catch (...) {
        // 事务失败
    }

    return result;
}

// ========== 通用操作 ==========

RedisReply RedisPlusPlusClient::select(int database) {
    // redis-plus-plus 不支持在连接后切换数据库
    // 需要在连接时指定
    return RedisReply::Error("Use 'database' option in config instead");
}

RedisReply RedisPlusPlusClient::keys(const std::string& pattern) {
    return tryExecute([this, &pattern]() {
        std::vector<std::string> result;
        redis_->keys(pattern, std::back_inserter(result));
        return RedisReply::Array(result);
    });
}

std::vector<std::string> RedisPlusPlusClient::scan(const std::string& pattern, uint64_t cursor, uint64_t* newCursor) {
    std::vector<std::string> result;
    if (!redis_) {
        if (newCursor) *newCursor = 0;
        return result;
    }

    try {
        // redis-plus-plus scan uses output iterator
        sw::redis::Cursor cur(cursor);
        cur = redis_->scan(cur, pattern, std::back_inserter(result));
        if (newCursor) *newCursor = cur;
    } catch (...) {
        if (newCursor) *newCursor = 0;
    }

    return result;
}

RedisReply RedisPlusPlusClient::flushDb() {
    return tryExecute([this]() {
        redis_->flushdb();
        return RedisReply::Ok();
    });
}

RedisReply RedisPlusPlusClient::dbSize() {
    return tryExecute([this]() {
        auto val = redis_->dbsize();
        return RedisReply::Int64(val);
    });
}

std::string RedisPlusPlusClient::info(const std::string& section) {
    if (!redis_) return "";

    try {
        return redis_->info(section);
    } catch (...) {
        return "";
    }
}

} // namespace redis
} // namespace storage
} // namespace apollo

#endif // APOLLO_USE_REDIS_PLUS_PLUS
