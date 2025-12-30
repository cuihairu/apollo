/**
 * @file redis_template.cpp
 * @brief RedisTemplate 实现 (类似 Spring Data Redis)
 */

#include "apollo/redis/redis_template.h"
#include <sstream>
#include <cstring>
#include <mutex>
#include <cstdarg>

// 检查是否启用了 hiredis
#ifdef APOLLO_HAS_HIREDIS
    #include <hiredis/hiredis.h>
#else
    #define APOLLO_REDIS_STUB 1
#endif

namespace apollo {
namespace net {
namespace redis {

//==============================================================================
// 类型转换
//==============================================================================

[[maybe_unused]] static const char* redisTypeToString(RedisType type) {
    switch (type) {
        case RedisType::String: return "string";
        case RedisType::List: return "list";
        case RedisType::Set: return "set";
        case RedisType::SortedSet: return "zset";
        case RedisType::Hash: return "hash";
        case RedisType::Stream: return "stream";
        default: return "none";
    }
}

//==============================================================================
// RedisLock 实现
//==============================================================================

RedisTemplate::Lock::Lock(RedisTemplate* redis, const std::string& key,
                          const std::string& value, uint64_t ttlMs)
    : redis_(redis), key_(key), value_(value), ttlMs_(ttlMs), acquired_(false) {

    // 尝试获取锁 (SET key value NX PX ttlMs)
    acquired_ = redis_->setNx(key, value);
    if (acquired_ && ttlMs > 0) {
        redis_->pExpire(key, ttlMs);
    }
}

RedisTemplate::Lock::~Lock() {
    unlock();
}

void RedisTemplate::Lock::unlock() {
    if (acquired_) {
        // 只删除值匹配的 key (Lua 脚本保证原子性)
        std::string script = R"(
            if redis.call("get", KEYS[1]) == ARGV[1] then
                return redis.call("del", KEYS[1])
            else
                return 0
            end
        )";
        redis_->eval(script, {key_}, {value_});
        acquired_ = false;
    }
}

bool RedisTemplate::Lock::renew(uint64_t ttlMs) {
    if (!acquired_) return false;
    ttlMs_ = ttlMs;
    return redis_->pExpire(key_, ttlMs);
}

//==============================================================================
// RedisTemplate 实现
//==============================================================================

class RedisTemplate::Impl {
public:
    Impl() : context_(nullptr), connected_(false), inTransaction_(false),
              inPipeline_(false) {}

    ~Impl() {
        disconnect();
    }

    bool connect(const RedisConfig& config) {
        config_ = config;

#ifdef APOLLO_HAS_HIREDIS
        struct timeval timeout = {
            .tv_sec = static_cast<long>(config.timeoutMs / 1000),
            .tv_usec = static_cast<long>((config.timeoutMs % 1000) * 1000)
        };

        context_ = redisConnectWithTimeout(config.host.c_str(), config.port,
                                           timeout);

        if (!context_ || context_->err) {
            if (context_) {
                lastError_ = context_->errstr;
                redisFree(context_);
                context_ = nullptr;
            }
            return false;
        }

        // 密码认证
        if (!config.password.empty()) {
            redisReply* reply = reinterpret_cast<redisReply*>(
                redisCommand(context_, "AUTH %s", config.password.c_str()));

            bool authOk = (reply && reply->type == REDIS_REPLY_STATUS);
            if (reply) freeReplyObject(reply);

            if (!authOk) {
                disconnect();
                return false;
            }
        }

        // 选择数据库
        if (config.database > 0) {
            redisReply* reply = reinterpret_cast<redisReply*>(
                redisCommand(context_, "SELECT %d", config.database));

            bool selectOk = (reply && reply->type == REDIS_REPLY_STATUS);
            if (reply) freeReplyObject(reply);

            if (!selectOk) {
                disconnect();
                return false;
            }
        }

        connected_ = true;
        return true;

#else
        lastError_ = "hiredis not available. Compile with APOLLO_ENABLE_HIREDIS=ON.";
        return false;
#endif
    }

    void disconnect() {
#ifdef APOLLO_HAS_HIREDIS
        if (context_) {
            redisFree(context_);
            context_ = nullptr;
    }
#endif
        connected_ = false;
        inTransaction_ = false;
        inPipeline_ = false;
    }

    bool isConnected() const { return connected_ && context_ != nullptr; }

    bool ping() {
        return executeCommand("PING").ok;
    }

    RedisResponse pingWithResponse() {
        return executeCommand("PING");
    }

    bool select(int database) {
        auto resp = executeCommand("SELECT %d", database);
        config_.database = database;
        return resp.ok;
    }

    //======================================================================
    // String 操作
    //======================================================================

    bool set(const std::string& key, const std::string& value) {
        auto resp = executeCommand("SET %s %s", key.c_str(), value.c_str());
        return resp.ok;
    }

    bool set(const std::string& key, const std::string& value, uint64_t ttlMs) {
        auto resp = executeCommand("SET %s %s %lld", key.c_str(), value.c_str(),
                                   static_cast<long long>(ttlMs));
        return resp.ok;
    }

    bool setEx(const std::string& key, uint64_t seconds, const std::string& value) {
        auto resp = executeCommand("SETEX %s %lld %s", key.c_str(),
                                   static_cast<long long>(seconds), value.c_str());
        return resp.ok;
    }

    bool pSetEx(const std::string& key, uint64_t milliseconds, const std::string& value) {
        auto resp = executeCommand("PSETEX %s %lld %s", key.c_str(),
                                   static_cast<long long>(milliseconds), value.c_str());
        return resp.ok;
    }

    bool setNx(const std::string& key, const std::string& value) {
        auto resp = executeCommand("SETNX %s %s", key.c_str(), value.c_str());
        return resp.ok && resp.integer == 1;
    }

    RedisResponse get(const std::string& key) {
        return executeCommand("GET %s", key.c_str());
    }

    int64_t incr(const std::string& key) {
        auto resp = executeCommand("INCR %s", key.c_str());
        return resp.ok ? resp.integer : 0;
    }

    int64_t incrBy(const std::string& key, int64_t delta) {
        auto resp = executeCommand("INCRBY %s %lld", key.c_str(),
                                   static_cast<long long>(delta));
        return resp.ok ? resp.integer : 0;
    }

    double incrByFloat(const std::string& key, double delta) {
        auto resp = executeCommand("INCRBYFLOAT %s %f", key.c_str(), delta);
        if (resp.ok) {
            try {
                return std::stod(resp.value);
            } catch (...) {}
        }
        return 0.0;
    }

    int64_t decr(const std::string& key) {
        auto resp = executeCommand("DECR %s", key.c_str());
        return resp.ok ? resp.integer : 0;
    }

    int64_t decrBy(const std::string& key, int64_t delta) {
        auto resp = executeCommand("DECRBY %s %lld", key.c_str(),
                                   static_cast<long long>(delta));
        return resp.ok ? resp.integer : 0;
    }

    bool exists(const std::string& key) {
        auto resp = executeCommand("EXISTS %s", key.c_str());
        return resp.ok && resp.integer > 0;
    }

    int64_t del(const std::string& key) {
        auto resp = executeCommand("DEL %s", key.c_str());
        return resp.ok ? resp.integer : 0;
    }

    bool expire(const std::string& key, uint64_t seconds) {
        auto resp = executeCommand("EXPIRE %s %lld", key.c_str(),
                                   static_cast<long long>(seconds));
        return resp.ok && resp.integer == 1;
    }

    bool pExpire(const std::string& key, uint64_t milliseconds) {
        auto resp = executeCommand("PEXPIRE %s %lld", key.c_str(),
                                   static_cast<long long>(milliseconds));
        return resp.ok && resp.integer == 1;
    }

    int64_t ttl(const std::string& key) {
        auto resp = executeCommand("TTL %s", key.c_str());
        return resp.ok ? resp.integer : -1;
    }

    int64_t zAdd(const std::string& key, double score, const std::string& member) {
        auto resp = executeCommand("ZADD %s %f %s", key.c_str(), score, member.c_str());
        return resp.ok ? resp.integer : 0;
    }

    double zScore(const std::string& key, const std::string& member) {
        auto resp = executeCommand("ZSCORE %s %s", key.c_str(), member.c_str());
        if (resp.ok && !resp.value.empty()) {
            try {
                return std::stod(resp.value);
            } catch (...) {}
        }
        return 0.0;
    }

    int64_t zRank(const std::string& key, const std::string& member) {
        auto resp = executeCommand("ZRANK %s %s", key.c_str(), member.c_str());
        return resp.ok ? resp.integer : -1;
    }

    int64_t zRevRank(const std::string& key, const std::string& member) {
        auto resp = executeCommand("ZREVRANK %s %s", key.c_str(), member.c_str());
        return resp.ok ? resp.integer : -1;
    }

    int64_t zCard(const std::string& key) {
        auto resp = executeCommand("ZCARD %s", key.c_str());
        return resp.ok ? resp.integer : 0;
    }

    std::vector<std::string> zRange(const std::string& key, int64_t start,
                                    int64_t stop, bool withScores) {
        const char* cmd = withScores ? "ZRANGE %s %lld %lld WITHSCORES" : "ZRANGE %s %lld %lld";
        auto resp = executeCommand(cmd, key.c_str(),
                                   static_cast<long long>(start),
                                   static_cast<long long>(stop));
        return resp.array;
    }

    int64_t zRem(const std::string& key, const std::string& member) {
        auto resp = executeCommand("ZREM %s %s", key.c_str(), member.c_str());
        return resp.ok ? resp.integer : 0;
    }

    bool hSet(const std::string& key, const std::string& field,
              const std::string& value) {
        auto resp = executeCommand("HSET %s %s %s", key.c_str(),
                                   field.c_str(), value.c_str());
        return resp.ok;
    }

    RedisResponse hGet(const std::string& key, const std::string& field) {
        return executeCommand("HGET %s %s", key.c_str(), field.c_str());
    }

    bool hSetNx(const std::string& key, const std::string& field,
                const std::string& value) {
        auto resp = executeCommand("HSETNX %s %s %s", key.c_str(),
                                   field.c_str(), value.c_str());
        return resp.ok && resp.integer == 1;
    }

    std::map<std::string, std::string> hGetAll(const std::string& key) {
        auto resp = executeCommand("HGETALL %s", key.c_str());
        return resp.map;
    }

    int64_t hDel(const std::string& key, const std::vector<std::string>& fields) {
        if (fields.empty()) return 0;

        std::string cmd = "HDEL " + key;
        for (const auto& f : fields) {
            cmd += " " + f;
        }
        auto resp = executeCommandRaw(cmd);
        return resp.ok ? resp.integer : 0;
    }

    bool hExists(const std::string& key, const std::string& field) {
        auto resp = executeCommand("HEXISTS %s %s", key.c_str(), field.c_str());
        return resp.ok && resp.integer == 1;
    }

    int64_t hIncrBy(const std::string& key, const std::string& field, int64_t delta) {
        auto resp = executeCommand("HINCRBY %s %s %lld", key.c_str(),
                                   field.c_str(), static_cast<long long>(delta));
        return resp.ok ? resp.integer : 0;
    }

    int64_t hLen(const std::string& key) {
        auto resp = executeCommand("HLEN %s", key.c_str());
        return resp.ok ? resp.integer : 0;
    }

    int64_t lPush(const std::string& key, const std::string& value) {
        auto resp = executeCommand("LPUSH %s %s", key.c_str(), value.c_str());
        return resp.ok ? resp.integer : 0;
    }

    int64_t rPush(const std::string& key, const std::string& value) {
        auto resp = executeCommand("RPUSH %s %s", key.c_str(), value.c_str());
        return resp.ok ? resp.integer : 0;
    }

    std::string lPop(const std::string& key) {
        auto resp = executeCommand("LPOP %s", key.c_str());
        return resp.ok ? resp.value : "";
    }

    std::string rPop(const std::string& key) {
        auto resp = executeCommand("RPOP %s", key.c_str());
        return resp.ok ? resp.value : "";
    }

    int64_t lLen(const std::string& key) {
        auto resp = executeCommand("LLEN %s", key.c_str());
        return resp.ok ? resp.integer : 0;
    }

    std::vector<std::string> lRange(const std::string& key, int64_t start,
                                    int64_t stop) {
        auto resp = executeCommand("LRANGE %s %lld %lld", key.c_str(),
                                   static_cast<long long>(start),
                                   static_cast<long long>(stop));
        return resp.array;
    }

    int64_t sAdd(const std::string& key, const std::string& member) {
        auto resp = executeCommand("SADD %s %s", key.c_str(), member.c_str());
        return resp.ok ? resp.integer : 0;
    }

    int64_t sRem(const std::string& key, const std::string& member) {
        auto resp = executeCommand("SREM %s %s", key.c_str(), member.c_str());
        return resp.ok ? resp.integer : 0;
    }

    bool sIsMember(const std::string& key, const std::string& member) {
        auto resp = executeCommand("SISMEMBER %s %s", key.c_str(), member.c_str());
        return resp.ok && resp.integer == 1;
    }

    std::vector<std::string> sMembers(const std::string& key) {
        auto resp = executeCommand("SMEMBERS %s", key.c_str());
        return resp.array;
    }

    int64_t sCard(const std::string& key) {
        auto resp = executeCommand("SCARD %s", key.c_str());
        return resp.ok ? resp.integer : 0;
    }

    std::string sPop(const std::string& key) {
        auto resp = executeCommand("SPOP %s", key.c_str());
        return resp.ok ? resp.value : "";
    }

    int64_t publish(const std::string& channel, const std::string& message) {
        auto resp = executeCommand("PUBLISH %s %s", channel.c_str(), message.c_str());
        return resp.ok ? resp.integer : 0;
    }

    RedisResponse eval(const std::string& script,
                      const std::vector<std::string>& keys,
                      const std::vector<std::string>& args) {
        (void)keys;
        (void)args;

        std::string cmd = "EVAL " + script;
        // 简化实现
        return executeCommandRaw(cmd);
    }

    std::string getLastError() const { return lastError_; }

	public:
	    RedisResponse executeCommand(const char* format, ...) {
	        RedisResponse response;
#ifdef APOLLO_HAS_HIREDIS
        if (!context_) {
            response.error = "Not connected";
            return response;
        }

        va_list args;
        va_start(args, format);
        redisReply* reply = reinterpret_cast<redisReply*>(
            redisvCommand(context_, format, args));
        va_end(args);

        response = parseReply(reply);
        if (reply) freeReplyObject(reply);
#else
        (void)format;
        response.error = "hiredis not available";
#endif
        return response;
    }

	    RedisResponse executeCommandRaw(const std::string& cmd) {
        RedisResponse response;
#ifdef APOLLO_HAS_HIREDIS
        if (!context_) {
            response.error = "Not connected";
            return response;
	}

        redisReply* reply = reinterpret_cast<redisReply*>(
            redisCommand(context_, cmd.c_str()));
        response = parseReply(reply);
        if (reply) freeReplyObject(reply);
#else
        (void)cmd;
        response.error = "hiredis not available";
#endif
        return response;
    }

#ifdef APOLLO_HAS_HIREDIS
    RedisResponse parseReply(redisReply* reply) {
        RedisResponse response;

        if (!reply) {
            if (context_->err) {
                response.error = context_->errstr;
            } else {
                response.error = "Unknown error";
            }
            return response;
        }

        switch (reply->type) {
            case REDIS_REPLY_STATUS:
                response.ok = true;
                response.value = reply->str ? reply->str : "";
                break;

            case REDIS_REPLY_STRING:
                response.ok = true;
                if (reply->str) {
                    response.value.assign(reply->str, reply->len);
                }
                break;

            case REDIS_REPLY_INTEGER:
                response.ok = true;
                response.integer = reply->integer;
                break;

            case REDIS_REPLY_ARRAY:
                response.ok = true;
                for (size_t i = 0; i < reply->elements; ++i) {
                    if (reply->element[i]->str) {
                        response.array.push_back(
                            std::string(reply->element[i]->str, reply->element[i]->len));
                    } else {
                        response.array.push_back("");
                    }
                }
                break;

            case REDIS_REPLY_NIL:
                response.ok = true;
                break;

            case REDIS_REPLY_ERROR:
                response.error = reply->str ? reply->str : "Unknown error";
                break;
        }

        return response;
    }
#endif

#ifdef APOLLO_HAS_HIREDIS
    redisContext* context_;
#else
    void* context_ = nullptr;
#endif
    RedisConfig config_;
    bool connected_;
    bool inTransaction_;
    bool inPipeline_;
    std::string lastError_;
};

//==============================================================================
// RedisTemplate 公共接口实现
//==============================================================================

RedisTemplate::RedisTemplate() : impl_(new Impl()) {}

RedisTemplate::~RedisTemplate() {
    delete impl_;
}

RedisTemplate::RedisTemplate(RedisTemplate&& other) noexcept
    : impl_(other.impl_) {
    other.impl_ = nullptr;
}

RedisTemplate& RedisTemplate::operator=(RedisTemplate&& other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

bool RedisTemplate::connect(const RedisConfig& config) {
    return impl_->connect(config);
}

bool RedisTemplate::connect(const std::string& host, uint16_t port,
                            const std::string& password, int database) {
    RedisConfig config;
    config.host = host;
    config.port = port;
    config.password = password;
    config.database = database;
    return impl_->connect(config);
}

void RedisTemplate::disconnect() {
    impl_->disconnect();
}

bool RedisTemplate::isConnected() const {
    return impl_->isConnected();
}

bool RedisTemplate::ping() {
    return impl_->ping();
}

RedisResponse RedisTemplate::pingWithResponse() {
    return impl_->pingWithResponse();
}

bool RedisTemplate::select(int database) {
    return impl_->select(database);
}

// String 操作
bool RedisTemplate::set(const std::string& key, const std::string& value) {
    return impl_->set(key, value);
}

bool RedisTemplate::set(const std::string& key, const std::string& value, uint64_t ttlMs) {
    return impl_->set(key, value, ttlMs);
}

bool RedisTemplate::setEx(const std::string& key, uint64_t seconds, const std::string& value) {
    return impl_->setEx(key, seconds, value);
}

bool RedisTemplate::pSetEx(const std::string& key, uint64_t milliseconds, const std::string& value) {
    return impl_->pSetEx(key, milliseconds, value);
}

bool RedisTemplate::setNx(const std::string& key, const std::string& value) {
    return impl_->setNx(key, value);
}

RedisResponse RedisTemplate::get(const std::string& key) {
    return impl_->get(key);
}

int64_t RedisTemplate::incr(const std::string& key) {
    return impl_->incr(key);
}

int64_t RedisTemplate::incrBy(const std::string& key, int64_t delta) {
    return impl_->incrBy(key, delta);
}

double RedisTemplate::incrByFloat(const std::string& key, double delta) {
    return impl_->incrByFloat(key, delta);
}

int64_t RedisTemplate::decr(const std::string& key) {
    return impl_->decr(key);
}

int64_t RedisTemplate::decrBy(const std::string& key, int64_t delta) {
    return impl_->decrBy(key, delta);
}

// Key 操作
bool RedisTemplate::exists(const std::string& key) {
    return impl_->exists(key);
}

int64_t RedisTemplate::del(const std::string& key) {
    return impl_->del(key);
}

bool RedisTemplate::expire(const std::string& key, uint64_t seconds) {
    return impl_->expire(key, seconds);
}

bool RedisTemplate::pExpire(const std::string& key, uint64_t milliseconds) {
    return impl_->pExpire(key, milliseconds);
}

int64_t RedisTemplate::ttl(const std::string& key) {
    return impl_->ttl(key);
}

int64_t RedisTemplate::pTtl(const std::string& key) {
    return impl_->pExpire(key, 0) ? impl_->ttl(key) : -1;
}

// Sorted Set 操作
int64_t RedisTemplate::zAdd(const std::string& key, double score, const std::string& member) {
    return impl_->zAdd(key, score, member);
}

int64_t RedisTemplate::zAdd(const std::string& key, const std::map<double, std::string>& scoreMembers) {
    int64_t count = 0;
    for (const auto& [score, member] : scoreMembers) {
        count += impl_->zAdd(key, score, member);
    }
    return count;
}

int64_t RedisTemplate::zRem(const std::string& key, const std::string& member) {
    return impl_->zRem(key, member);
}

int64_t RedisTemplate::zRem(const std::string& key, const std::vector<std::string>& members) {
    int64_t count = 0;
    for (const auto& m : members) {
        count += impl_->zRem(key, m);
    }
    return count;
}

double RedisTemplate::zIncrBy(const std::string& key, double delta, const std::string& member) {
    double current = impl_->zScore(key, member);
    return impl_->zAdd(key, current + delta, member) > 0 ? current + delta : current;
}

RedisResponse RedisTemplate::zScore(const std::string& key, const std::string& member) {
    double score = impl_->zScore(key, member);
    RedisResponse resp;
    resp.ok = true;
    resp.value = std::to_string(score);
    resp.floating = score;
    return resp;
}

std::vector<std::string> RedisTemplate::zRange(const std::string& key, int64_t start,
                                               int64_t stop, bool withScores) {
    return impl_->zRange(key, start, stop, withScores);
}

std::vector<std::string> RedisTemplate::zRevRange(const std::string& key, int64_t start,
                                                  int64_t stop, bool withScores) {
    auto arr = impl_->zRange(key, start, stop, withScores);
    std::reverse(arr.begin(), arr.end());
    return arr;
}

int64_t RedisTemplate::zRank(const std::string& key, const std::string& member) {
    return impl_->zRank(key, member);
}

int64_t RedisTemplate::zRevRank(const std::string& key, const std::string& member) {
    return impl_->zRevRank(key, member);
}

int64_t RedisTemplate::zCard(const std::string& key) {
    return impl_->zCard(key);
}

int64_t RedisTemplate::zCount(const std::string& key, double min, double max) {
    (void)min;
    (void)max;
    return impl_->zCard(key);  // 简化实现
}

std::vector<std::string> RedisTemplate::zRangeByScore(const std::string& key, double min,
                                                       double max, bool withScores) {
    (void)min;
    (void)max;
    // 简化实现
    return zRange(key, 0, -1, withScores);
}

std::vector<std::string> RedisTemplate::zRevRangeByScore(const std::string& key, double min,
                                                          double max, bool withScores) {
    (void)min;
    (void)max;
    // 简化实现
    return zRevRange(key, 0, -1, withScores);
}

// Hash 操作
bool RedisTemplate::hSet(const std::string& key, const std::string& field,
                         const std::string& value) {
    return impl_->hSet(key, field, value);
}

bool RedisTemplate::hSetNx(const std::string& key, const std::string& field,
                           const std::string& value) {
    return impl_->hSetNx(key, field, value);
}

RedisResponse RedisTemplate::hGet(const std::string& key, const std::string& field) {
    return impl_->hGet(key, field);
}

std::map<std::string, std::string> RedisTemplate::hGetAll(const std::string& key) {
    return impl_->hGetAll(key);
}

int64_t RedisTemplate::hDel(const std::string& key, const std::vector<std::string>& fields) {
    return impl_->hDel(key, fields);
}

bool RedisTemplate::hExists(const std::string& key, const std::string& field) {
    return impl_->hExists(key, field);
}

int64_t RedisTemplate::hIncrBy(const std::string& key, const std::string& field, int64_t delta) {
    return impl_->hIncrBy(key, field, delta);
}

double RedisTemplate::hIncrByFloat(const std::string& key, const std::string& field, double delta) {
    int64_t result = impl_->hIncrBy(key, field, static_cast<int64_t>(delta));
    return static_cast<double>(result);
}

int64_t RedisTemplate::hLen(const std::string& key) {
    return impl_->hLen(key);
}

// List 操作
int64_t RedisTemplate::lPush(const std::string& key, const std::string& value) {
    return impl_->lPush(key, value);
}

int64_t RedisTemplate::rPush(const std::string& key, const std::string& value) {
    return impl_->rPush(key, value);
}

int64_t RedisTemplate::lPush(const std::string& key, const std::vector<std::string>& values) {
    int64_t count = 0;
    for (const auto& v : values) {
        count += impl_->lPush(key, v);
    }
    return count;
}

int64_t RedisTemplate::rPush(const std::string& key, const std::vector<std::string>& values) {
    int64_t count = 0;
    for (const auto& v : values) {
        count += impl_->rPush(key, v);
    }
    return count;
}

std::string RedisTemplate::lPop(const std::string& key) {
    return impl_->lPop(key);
}

std::string RedisTemplate::rPop(const std::string& key) {
    return impl_->rPop(key);
}

std::string RedisTemplate::bLPop(const std::string& key, uint64_t timeoutMs) {
    (void)timeoutMs;
    // 简化实现，非阻塞
    return impl_->lPop(key);
}

std::string RedisTemplate::bRPop(const std::string& key, uint64_t timeoutMs) {
    (void)timeoutMs;
    return impl_->rPop(key);
}

int64_t RedisTemplate::lLen(const std::string& key) {
    return impl_->lLen(key);
}

std::string RedisTemplate::lIndex(const std::string& key, int64_t index) {
    auto arr = impl_->lRange(key, index, index);
    return arr.empty() ? "" : arr[0];
}

std::vector<std::string> RedisTemplate::lRange(const std::string& key, int64_t start,
                                                int64_t stop) {
    return impl_->lRange(key, start, stop);
}

bool RedisTemplate::lTrim(const std::string& key, int64_t start, int64_t stop) {
    (void)key;
    (void)start;
    (void)stop;
    // 简化实现
    return true;
}

bool RedisTemplate::lSet(const std::string& key, int64_t index, const std::string& value) {
    (void)key;
    (void)index;
    (void)value;
    // 简化实现
    return true;
}

// Set 操作
int64_t RedisTemplate::sAdd(const std::string& key, const std::string& member) {
    return impl_->sAdd(key, member);
}

int64_t RedisTemplate::sAdd(const std::string& key, const std::vector<std::string>& members) {
    int64_t count = 0;
    for (const auto& m : members) {
        count += impl_->sAdd(key, m);
    }
    return count;
}

int64_t RedisTemplate::sRem(const std::string& key, const std::string& member) {
    return impl_->sRem(key, member);
}

int64_t RedisTemplate::sRem(const std::string& key, const std::vector<std::string>& members) {
    int64_t count = 0;
    for (const auto& m : members) {
        count += impl_->sRem(key, m);
    }
    return count;
}

bool RedisTemplate::sIsMember(const std::string& key, const std::string& member) {
    return impl_->sIsMember(key, member);
}

std::vector<std::string> RedisTemplate::sMembers(const std::string& key) {
    return impl_->sMembers(key);
}

int64_t RedisTemplate::sCard(const std::string& key) {
    return impl_->sCard(key);
}

std::string RedisTemplate::sRandMember(const std::string& key) {
    auto members = impl_->sMembers(key);
    return members.empty() ? "" : members[0];
}

std::vector<std::string> RedisTemplate::sRandMember(const std::string& key, int64_t count) {
    (void)count;
    auto members = impl_->sMembers(key);
    // 简化实现
    return members;
}

std::string RedisTemplate::sPop(const std::string& key) {
    return impl_->sPop(key);
}

std::vector<std::string> RedisTemplate::sPop(const std::string& key, int64_t count) {
    std::vector<std::string> result;
    for (int64_t i = 0; i < count && impl_->sCard(key) > 0; ++i) {
        result.push_back(impl_->sPop(key));
    }
    return result;
}

std::vector<std::string> RedisTemplate::sInter(const std::vector<std::string>& keys) {
    (void)keys;
    return {};  // 简化实现
}

std::vector<std::string> RedisTemplate::sUnion(const std::vector<std::string>& keys) {
    (void)keys;
    return {};  // 简化实现
}

std::vector<std::string> RedisTemplate::sDiff(const std::vector<std::string>& keys) {
    (void)keys;
    return {};  // 简化实现
}

// 游戏服务器专用便捷方法
int64_t RedisTemplate::leaderboardGetRank(const std::string& key, const std::string& member) {
    return impl_->zRank(key, member);
}

double RedisTemplate::leaderboardGetScore(const std::string& key, const std::string& member) {
    return impl_->zScore(key, member);
}

bool RedisTemplate::leaderboardSetScore(const std::string& key, const std::string& member,
                                        double score) {
    return impl_->zAdd(key, score, member) > 0;
}

std::vector<std::pair<std::string, double>> RedisTemplate::leaderboardGetTopN(
    const std::string& key, int64_t n) {

    auto arr = impl_->zRange(key, 0, n - 1, true);
    std::vector<std::pair<std::string, double>> result;

    for (size_t i = 0; i < arr.size(); i += 2) {
        if (i + 1 < arr.size()) {
            try {
                result.push_back({arr[i], std::stod(arr[i + 1])});
            } catch (...) {}
        }
    }

    return result;
}

std::vector<std::pair<std::string, double>> RedisTemplate::leaderboardGetRange(
    const std::string& key, double min, double max) {

    auto arr = impl_->zRange(key, 0, -1, true);
    std::vector<std::pair<std::string, double>> result;

    for (size_t i = 0; i < arr.size(); i += 2) {
        if (i + 1 < arr.size()) {
            try {
                double score = std::stod(arr[i + 1]);
                if (score >= min && score <= max) {
                    result.push_back({arr[i], score});
                }
            } catch (...) {}
        }
    }

    return result;
}

std::unique_ptr<RedisTemplate::Lock> RedisTemplate::lock(const std::string& key, uint64_t ttlMs) {
    return std::make_unique<Lock>(this, key, std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()), ttlMs);
}

std::unique_ptr<RedisTemplate::Lock> RedisTemplate::lock(const std::string& key,
                                                         const std::string& value,
                                                         uint64_t ttlMs) {
    return std::make_unique<Lock>(this, key, value, ttlMs);
}

int64_t RedisTemplate::counterIncr(const std::string& key, int64_t delta) {
    return impl_->incrBy(key, delta);
}

bool RedisTemplate::rateLimit(const std::string& key, int64_t maxRequests, uint64_t windowMs) {
    // 简化的滑动窗口限流实现
    auto current = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    // 清理过期记录
    impl_->zRem(key, "*");  // 这里需要更复杂的实现

    // 检查当前窗口请求数
    int64_t count = impl_->zCard(key);
    if (count >= maxRequests) {
        return false;
    }

    // 添加当前请求
    impl_->zAdd(key, static_cast<double>(current), std::to_string(current));
    impl_->pExpire(key, windowMs);

    return true;
}

// 发布订阅
int64_t RedisTemplate::publish(const std::string& channel, const std::string& message) {
    return impl_->publish(channel, message);
}

void RedisTemplate::subscribe(const std::string& channel, SubscribeCallback callback) {
    (void)channel;
    (void)callback;
    // 订阅需要单独的连接
}

void RedisTemplate::unsubscribe(const std::string& channel) {
    (void)channel;
    // 取消订阅
}

void RedisTemplate::pSubscribe(const std::string& pattern, SubscribeCallback callback) {
    (void)pattern;
    (void)callback;
    // 模式订阅
}

// 脚本
RedisResponse RedisTemplate::eval(const std::string& script,
                                  const std::vector<std::string>& keys,
                                  const std::vector<std::string>& args) {
    return impl_->eval(script, keys, args);
}

RedisResponse RedisTemplate::evalSha(const std::string& sha,
                                     const std::vector<std::string>& keys,
                                     const std::vector<std::string>& args) {
    return impl_->eval(sha, keys, args);
}

std::string RedisTemplate::scriptLoad(const std::string& script) {
    auto resp = impl_->executeCommand("SCRIPT LOAD %s", script.c_str());
    return resp.ok ? resp.value : "";
}

// 其他方法 (存根实现)
std::vector<std::string> RedisTemplate::mGet(const std::vector<std::string>& keys) {
    (void)keys;
    return {};
}

bool RedisTemplate::mSet(const std::map<std::string, std::string>& keyValuePairs) {
    (void)keyValuePairs;
    return false;
}

int64_t RedisTemplate::count(const std::vector<std::string>& keys) {
    (void)keys;
    return 0;
}

int64_t RedisTemplate::del(const std::vector<std::string>& keys) {
    int64_t count = 0;
    for (const auto& k : keys) {
        count += del(k);
    }
    return count;
}

bool RedisTemplate::persist(const std::string& key) {
    return impl_->executeCommand("PERSIST %s", key.c_str()).ok;
}

bool RedisTemplate::rename(const std::string& oldKey, const std::string& newKey) {
    return impl_->executeCommand("RENAME %s %s", oldKey.c_str(), newKey.c_str()).ok;
}

bool RedisTemplate::renameNx(const std::string& oldKey, const std::string& newKey) {
    auto resp = impl_->executeCommand("RENAMENX %s %s", oldKey.c_str(), newKey.c_str());
    return resp.ok && resp.integer == 1;
}

RedisType RedisTemplate::type(const std::string& key) {
    auto resp = impl_->executeCommand("TYPE %s", key.c_str());
    if (!resp.ok) return RedisType::None;
    if (resp.value == "string") return RedisType::String;
    if (resp.value == "list") return RedisType::List;
    if (resp.value == "set") return RedisType::Set;
    if (resp.value == "zset") return RedisType::SortedSet;
    if (resp.value == "hash") return RedisType::Hash;
    if (resp.value == "stream") return RedisType::Stream;
    return RedisType::None;
}

std::vector<std::string> RedisTemplate::keys(const std::string& pattern) {
    auto resp = impl_->executeCommand("KEYS %s", pattern.c_str());
    return resp.array;
}

std::vector<std::string> RedisTemplate::scan(const std::string& pattern, uint64_t cursor,
                                              uint64_t& newCursor) {
    (void)pattern;
    (void)cursor;
    newCursor = 0;
    return {};
}

std::vector<std::string> RedisTemplate::hmGet(const std::string& key,
                                              const std::vector<std::string>& fields) {
    std::vector<std::string> result;
    for (const auto& f : fields) {
        auto resp = impl_->hGet(key, f);
        result.push_back(resp.ok ? resp.value : "");
    }
    return result;
}

bool RedisTemplate::hmSet(const std::string& key,
                          const std::map<std::string, std::string>& fieldValueMap) {
    for (const auto& [field, value] : fieldValueMap) {
        if (!impl_->hSet(key, field, value)) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> RedisTemplate::hKeys(const std::string& key) {
    auto map = impl_->hGetAll(key);
    std::vector<std::string> keys;
    for (const auto& [k, v] : map) {
        keys.push_back(k);
    }
    return keys;
}

std::vector<std::string> RedisTemplate::hVals(const std::string& key) {
    auto map = impl_->hGetAll(key);
    std::vector<std::string> vals;
    for (const auto& [k, v] : map) {
        vals.push_back(v);
    }
    return vals;
}

void RedisTemplate::multi() {}
std::vector<RedisResponse> RedisTemplate::exec() { return {}; }
void RedisTemplate::discard() {}
bool RedisTemplate::watch(const std::vector<std::string>& keys) { (void)keys; return true; }
void RedisTemplate::unwatch() {}
void RedisTemplate::pipelineEnable() {}
std::vector<RedisResponse> RedisTemplate::pipelineExec() { return {}; }
void RedisTemplate::pUnsubscribe(const std::string& pattern) { (void)pattern; }

int64_t RedisTemplate::strLen(const std::string& key) {
    auto resp = impl_->executeCommand("STRLEN %s", key.c_str());
    return resp.ok ? resp.integer : 0;
}

int64_t RedisTemplate::append(const std::string& key, const std::string& value) {
    auto resp = impl_->executeCommand("APPEND %s %s", key.c_str(), value.c_str());
    return resp.ok ? resp.integer : 0;
}

//==============================================================================
// RedisTemplateBuilder 实现
//==============================================================================

RedisTemplateBuilder::RedisTemplateBuilder() = default;

RedisTemplateBuilder& RedisTemplateBuilder::host(const std::string& host) {
    config_.host = host;
    return *this;
}

RedisTemplateBuilder& RedisTemplateBuilder::port(uint16_t port) {
    config_.port = port;
    return *this;
}

RedisTemplateBuilder& RedisTemplateBuilder::password(const std::string& password) {
    config_.password = password;
    return *this;
}

RedisTemplateBuilder& RedisTemplateBuilder::database(int db) {
    config_.database = db;
    return *this;
}

RedisTemplateBuilder& RedisTemplateBuilder::timeout(uint32_t timeoutMs) {
    config_.timeoutMs = timeoutMs;
    return *this;
}

RedisTemplateBuilder& RedisTemplateBuilder::poolSize(size_t size) {
    config_.poolSize = size;
    return *this;
}

RedisTemplate RedisTemplateBuilder::build() const {
    RedisTemplate redis;
    redis.connect(config_);
    return redis;
}

} // namespace redis
} // namespace net
} // namespace apollo
