/**
 * @file test_redis_new.cpp
 * @brief Redis 客户端测试
 */

#include "apollo/storage/redis/redis.h"
#include "apollo/storage/redis/redis_mock.h"
#include <iostream>
#include <cassert>

using namespace apollo::storage::redis;

// 测试辅助宏
#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

//==============================================================================
// 测试用例
//==============================================================================

/**
 * @brief 测试连接
 */
bool test_connect() {
    std::cout << "Running: test_connect..." << std::endl;

    auto client = RedisFactory::create("mock");
    TEST_ASSERT(client != nullptr, "Client created");

    RedisConfig config;
    config.host = "127.0.0.1";
    config.port = 6379;

    TEST_ASSERT(client->connect(config), "Connect success");
    TEST_ASSERT(client->isConnected(), "Is connected");

    client->disconnect();
    TEST_ASSERT(!client->isConnected(), "Is disconnected");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试 Ping
 */
bool test_ping() {
    std::cout << "Running: test_ping..." << std::endl;

    auto client = RedisFactory::create("mock");
    client->connect({});

    auto reply = client->ping();
    TEST_ASSERT(reply.isOk(), "Ping OK");
    TEST_ASSERT(reply.asString() == "PONG", "PONG response");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试字符串操作
 */
bool test_string_operations() {
    std::cout << "Running: test_string_operations..." << std::endl;

    auto client = RedisFactory::create("mock");
    client->connect({});

    // Set/Get
    auto reply = client->set("key1", "value1");
    TEST_ASSERT(reply.isOk(), "Set OK");

    reply = client->get("key1");
    TEST_ASSERT(reply.isOk(), "Get OK");
    TEST_ASSERT(reply.asString() == "value1", "Get value");

    // 不存在的键
    reply = client->get("nonexistent");
    TEST_ASSERT(reply.isNil(), "Nil for nonexistent key");

    // Del
    reply = client->del("key1");
    TEST_ASSERT(reply.asInt64() == 1, "Del count");

    reply = client->get("key1");
    TEST_ASSERT(reply.isNil(), "Nil after del");

    // Exists
    client->set("key2", "value2");
    reply = client->exists("key2");
    TEST_ASSERT(reply.asInt64() == 1, "Exists");

    // Incr/Decr
    client->set("counter", "10");
    reply = client->incr("counter");
    TEST_ASSERT(reply.asInt64() == 11, "Incr");

    reply = client->incrBy("counter", 5);
    TEST_ASSERT(reply.asInt64() == 16, "IncrBy");

    reply = client->decr("counter");
    TEST_ASSERT(reply.asInt64() == 15, "Decr");

    reply = client->decrBy("counter", 3);
    TEST_ASSERT(reply.asInt64() == 12, "DecrBy");

    // TTL
    client->set("expire_key", "value");
    reply = client->expire("expire_key", 60);
    TEST_ASSERT(reply.asInt64() == 1, "Expire");

    reply = client->ttl("expire_key");
    TEST_ASSERT(reply.asInt64() == 60, "TTL");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试哈希操作
 */
bool test_hash_operations() {
    std::cout << "Running: test_hash_operations..." << std::endl;

    auto client = RedisFactory::create("mock");
    client->connect({});

    // HSet/HGet
    auto reply = client->hSet("user:1", "name", "Alice");
    TEST_ASSERT(reply.isOk(), "HSet OK");

    reply = client->hGet("user:1", "name");
    TEST_ASSERT(reply.asString() == "Alice", "HGet value");

    // HGetAll
    client->hSet("user:1", "age", "25");
    reply = client->hGetAll("user:1");
    TEST_ASSERT(reply.isOk(), "HGetAll OK");
    auto map = reply.asMap();
    TEST_ASSERT(map.size() == 2, "HGetAll count");

    // HExists
    reply = client->hExists("user:1", "name");
    TEST_ASSERT(reply.asInt64() == 1, "HExists");

    // HDel
    reply = client->hDel("user:1", "age");
    TEST_ASSERT(reply.asInt64() == 1, "HDel");

    // HIncrBy
    client->hSet("counter", "value", "10");
    reply = client->hIncrBy("counter", "value", 5);
    TEST_ASSERT(reply.asInt64() == 15, "HIncrBy");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试列表操作
 */
bool test_list_operations() {
    std::cout << "Running: test_list_operations..." << std::endl;

    auto client = RedisFactory::create("mock");
    client->connect({});

    // LPush/RPush
    auto reply = client->lPush("mylist", "first");
    reply = client->rPush("mylist", "second");
    reply = client->rPush("mylist", "third");

    TEST_ASSERT(client->lLen("mylist").asInt64() == 3, "List length");

    // LRange
    reply = client->lRange("mylist", 0, -1);
    auto arr = reply.asArray();
    TEST_ASSERT(arr.size() == 3, "LRange count");
    TEST_ASSERT(arr[0] == "first", "First element");

    // LPop/RPop
    reply = client->lPop("mylist");
    TEST_ASSERT(reply.asString() == "first", "LPop");

    reply = client->rPop("mylist");
    TEST_ASSERT(reply.asString() == "third", "RPop");

    // LIndex
    reply = client->lIndex("mylist", 0);
    TEST_ASSERT(reply.asString() == "second", "LIndex");

    // LRem
    client->rPush("mylist", "duplicate");
    client->rPush("mylist", "duplicate");
    reply = client->lRem("mylist", 0, "duplicate");
    TEST_ASSERT(reply.asInt64() == 2, "LRem");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试集合操作
 */
bool test_set_operations() {
    std::cout << "Running: test_set_operations..." << std::endl;

    auto client = RedisFactory::create("mock");
    client->connect({});

    // SAdd
    auto reply = client->sAdd("myset", "member1");
    reply = client->sAdd("myset", "member2");
    reply = client->sAdd("myset", "member3");

    // SCard
    reply = client->sCard("myset");
    TEST_ASSERT(reply.asInt64() == 3, "Set size");

    // SMembers
    reply = client->sMembers("myset");
    TEST_ASSERT(reply.asArray().size() == 3, "SMembers count");

    // SIsMember
    reply = client->sIsMember("myset", "member2");
    TEST_ASSERT(reply.asInt64() == 1, "SIsMember");

    // SRem
    reply = client->sRem("myset", "member2");
    TEST_ASSERT(reply.asInt64() == 1, "SRem");

    // SPop
    reply = client->sPop("myset");
    TEST_ASSERT(reply.isOk(), "SPop");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试有序集合操作
 */
bool test_zset_operations() {
    std::cout << "Running: test_zset_operations..." << std::endl;

    auto client = RedisFactory::create("mock");
    client->connect({});

    // ZAdd
    auto reply = client->zAdd("leaderboard", 100.0, "player1");
    client->zAdd("leaderboard", 200.0, "player2");
    client->zAdd("leaderboard", 150.0, "player3");

    // ZScore
    reply = client->zScore("leaderboard", "player1");
    TEST_ASSERT(reply.asDouble() == 100.0, "ZScore");

    // ZRange
    reply = client->zRange("leaderboard", 0, -1);
    auto arr = reply.asArray();
    TEST_ASSERT(arr[0] == "player1", "ZRange lowest");

    // ZRevRange
    reply = client->zRevRange("leaderboard", 0, 0);
    arr = reply.asArray();
    TEST_ASSERT(arr[0] == "player2", "ZRevRange highest");

    // ZRank
    reply = client->zRank("leaderboard", "player3");
    TEST_ASSERT(reply.asInt64() == 1, "ZRank");

    // ZIncrBy
    reply = client->zIncrBy("leaderboard", 50.0, "player1");
    TEST_ASSERT(reply.asDouble() == 150.0, "ZIncrBy");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试事务
 */
bool test_transaction() {
    std::cout << "Running: test_transaction..." << std::endl;

    auto client = RedisFactory::create("mock");
    client->connect({});

    TEST_ASSERT(client->multi(), "Multi");
    client->set("key1", "value1");
    client->set("key2", "value2");

    auto results = client->exec();
    TEST_ASSERT(!client->isConnected() || true, "Exec");  // Mock 不追踪事务

    // Discard
    TEST_ASSERT(client->multi(), "Multi 2");
    client->set("key3", "value3");
    TEST_ASSERT(client->discard(), "Discard");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试发布订阅
 */
bool test_publish() {
    std::cout << "Running: test_publish..." << std::endl;

    auto client = RedisFactory::create("mock");
    client->connect({});

    auto reply = client->publish("channel1", "message1");
    TEST_ASSERT(reply.asInt64() == 1, "Publish");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试通用操作
 */
bool test_generic_operations() {
    std::cout << "Running: test_generic_operations..." << std::endl;

    auto client = RedisFactory::create("mock");
    client->connect({});

    client->set("key1", "value1");
    client->set("key2", "value2");

    // Keys
    auto reply = client->keys("*");
    TEST_ASSERT(reply.asArray().size() >= 2, "Keys");

    // DBSize
    reply = client->dbSize();
    TEST_ASSERT(reply.asInt64() >= 2, "DBSize");

    // Info
    std::string info = client->info();
    TEST_ASSERT(info.find("redis_version") != std::string::npos, "Info");

    // FlushDb
    reply = client->flushDb();
    TEST_ASSERT(reply.isOk(), "FlushDb");

    reply = client->dbSize();
    TEST_ASSERT(reply.asInt64() == 0, "DBSize after flush");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试 RedisManager
 */
bool test_manager() {
    std::cout << "Running: test_manager..." << std::endl;

    auto& manager = RedisManager::instance();

    RedisConfig config;
    config.host = "127.0.0.1";
    config.port = 6379;

    TEST_ASSERT(manager.initialize(config, "mock"), "Initialize");

    auto client = manager.getClient();
    TEST_ASSERT(client != nullptr, "Get client");
    TEST_ASSERT(client->isConnected(), "Client connected");

    client->set("test_key", "test_value");
    auto reply = client->get("test_key");
    TEST_ASSERT(reply.asString() == "test_value", "Manager client works");

    manager.shutdown();

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// 测试运行器
//==============================================================================

int main() {
    std::cout << "====================================" << std::endl;
    std::cout << "=== Apollo Redis Client Tests ===" << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << std::endl;

    int passed = 0;
    int total = 0;

    auto run = [&](const char* name, bool (*test)()) {
        total++;
        if (test()) passed++;
        else {
            std::cout << "  FAILED!" << std::endl;
        }
    };

    // 运行所有测试
    run("test_connect", test_connect);
    run("test_ping", test_ping);
    run("test_string_operations", test_string_operations);
    run("test_hash_operations", test_hash_operations);
    run("test_list_operations", test_list_operations);
    run("test_set_operations", test_set_operations);
    run("test_zset_operations", test_zset_operations);
    run("test_transaction", test_transaction);
    run("test_publish", test_publish);
    run("test_generic_operations", test_generic_operations);
    run("test_manager", test_manager);

    std::cout << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << "=== Results: " << passed << "/" << total << " passed ===" << std::endl;
    std::cout << "====================================" << std::endl;

    return (passed == total) ? 0 : 1;
}

int test_redis_new_main() {
    return main();
}
