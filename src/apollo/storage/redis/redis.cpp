/**
 * @file redis.cpp
 * @brief Redis 工厂和管理器实现
 */

#include "apollo/storage/redis/redis.h"
#include "apollo/storage/redis/redis_client_impl.h"
#include "apollo/storage/redis/redis_mock.h"
#include <map>
#include <mutex>

namespace apollo {
namespace storage {
namespace redis {

//==============================================================================
// RedisFactory 实现
//==============================================================================

IRedisClientPtr RedisFactory::create(const std::string& type) {
#ifdef APOLLO_USE_REDIS_PLUS_PLUS
    if (type == "redis-plus-plus" || type == "default" || type.empty()) {
        return std::make_shared<RedisPlusPlusClient>();
    }
#endif

    if (type == "mock") {
        return std::make_shared<MockRedisClient>();
    }

    // 默认返回 Mock 实现
    return std::make_shared<MockRedisClient>();
}

//==============================================================================
// RedisManager 实现
//==============================================================================

RedisManager& RedisManager::instance() {
    static RedisManager instance;
    return instance;
}

bool RedisManager::initialize(const RedisConfig& config, const std::string& implType) {
    // 如果已初始化，先关闭
    if (client_) {
        shutdown();
    }

    config_ = config;
    client_ = RedisFactory::create(implType);

    if (!client_) {
        return false;
    }

    return client_->connect(config_);
}

IRedisClientPtr RedisManager::getClient() {
    return client_;
}

void RedisManager::shutdown() {
    if (client_) {
        client_->disconnect();
        client_.reset();
    }
}

} // namespace redis
} // namespace storage
} // namespace apollo
