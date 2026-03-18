#pragma once

#include "apollo/storage/redis/redis.h"

// Re-export Redis types to the data::redis namespace
namespace apollo::data::redis {
    using RedisConfig = apollo::storage::redis::RedisConfig;
    using RedisReply = apollo::storage::redis::RedisReply;
    using IRedisClient = apollo::storage::redis::IRedisClient;
    using IRedisClientPtr = apollo::storage::redis::IRedisClientPtr;
    using RedisFactory = apollo::storage::redis::RedisFactory;
    using RedisManager = apollo::storage::redis::RedisManager;
}
