#include "gtest/gtest.h"
#include "apollo/db/redis.hpp"
#include <chrono>
#include <thread>

using namespace apollo::db;

class RedisTest : public ::testing::Test {
protected:
    void SetUp() override {
        RedisPoolConfig config;
        config.host = "localhost";
        config.port = 6379;
        config.minConnections = 2;
        config.maxConnections = 5;

        client = std::make_unique<RedisClient>(config);

        // 跳过测试如果Redis服务器不可用
        if (!client->Initialize()) {
            GTEST_SKIP() << "Redis server not available";
        }

        // 清理测试数据
        CleanupTestData();
    }

    void TearDown() override {
        CleanupTestData();
    }

    void CleanupTestData() {
        if (client) {
            client->Del("test_key");
            client->Del("test_hash");
            client->Del("test_list");
            client->Del("test_set");
            client->Del("test_zset");
            client->Del("test_counter");
        }
    }

    std::unique_ptr<RedisClient> client;
};

TEST_F(RedisTest, StringOperations) {
    // SET/GET
    EXPECT_TRUE(client->Set("test_key", "test_value"));
    EXPECT_EQ(client->Get("test_key"), "test_value");

    // SETEX
    EXPECT_TRUE(client->Set("test_key_expire", "expire_value", 2));
    EXPECT_EQ(client->Get("test_key_expire"), "expire_value");
    std::this_thread::sleep_for(std::chrono::seconds(3));
    EXPECT_EQ(client->Get("test_key_expire"), "");

    // EXISTS/DEL
    EXPECT_TRUE(client->Exists("test_key"));
    EXPECT_TRUE(client->Del("test_key"));
    EXPECT_FALSE(client->Exists("test_key"));
}

TEST_F(RedisTest, IntegerOperations) {
    // INCR
    EXPECT_EQ(client->Incr("test_counter"), 1);
    EXPECT_EQ(client->Incr("test_counter"), 2);
    EXPECT_EQ(client->Get("test_counter"), "2");

    // INCRBY
    EXPECT_EQ(client->IncrBy("test_counter", 5), 7);
    EXPECT_EQ(client->Get("test_counter"), "7");
}

TEST_F(RedisTest, HashOperations) {
    // HSET/HGET
    EXPECT_TRUE(client->HSet("test_hash", "field1", "value1"));
    EXPECT_EQ(client->HGet("test_hash", "field1"), "value1");

    // HGETALL
    EXPECT_TRUE(client->HSet("test_hash", "field2", "value2"));
    auto all = client->HGetAll("test_hash");
    EXPECT_EQ(all.size(), 2);
    EXPECT_EQ(all["field1"], "value1");
    EXPECT_EQ(all["field2"], "value2");

    // HEXISTS
    EXPECT_TRUE(client->HExists("test_hash", "field1"));
    EXPECT_FALSE(client->HExists("test_hash", "field3"));

    // HDEL
    EXPECT_TRUE(client->HDel("test_hash", "field1"));
    EXPECT_FALSE(client->HExists("test_hash", "field1"));
}

TEST_F(RedisTest, ListOperations) {
    // LPUSH/RPUSH
    EXPECT_TRUE(client->LPush("test_list", "left1"));
    EXPECT_TRUE(client->RPush("test_list", "right1"));
    EXPECT_TRUE(client->LPush("test_list", "left2"));

    // LRANGE
    auto range = client->LRange("test_list", 0, -1);
    EXPECT_EQ(range.size(), 3);
    EXPECT_EQ(range[0], "left2");
    EXPECT_EQ(range[1], "left1");
    EXPECT_EQ(range[2], "right1");

    // LPOP/RPOP
    EXPECT_EQ(client->LPop("test_list"), "left2");
    EXPECT_EQ(client->RPop("test_list"), "right1");
    EXPECT_EQ(client->LPop("test_list"), "left1");
}

TEST_F(RedisTest, SetOperations) {
    // SADD
    EXPECT_TRUE(client->SAdd("test_set", "member1"));
    EXPECT_TRUE(client->SAdd("test_set", "member2"));
    EXPECT_TRUE(client->SAdd("test_set", "member1"));  // 重复添加

    // SMEMBERS
    auto members = client->SMembers("test_set");
    EXPECT_EQ(members.size(), 2);

    // SISMEMBER
    EXPECT_TRUE(client->SIsMember("test_set", "member1"));
    EXPECT_FALSE(client->SIsMember("test_set", "member3"));

    // SREM
    EXPECT_TRUE(client->SRem("test_set", "member1"));
    EXPECT_FALSE(client->SIsMember("test_set", "member1"));
}

TEST_F(RedisTest, SortedSetOperations) {
    // ZADD
    EXPECT_TRUE(client->ZAdd("test_zset", 10.0, "member1"));
    EXPECT_TRUE(client->ZAdd("test_zset", 20.0, "member2"));
    EXPECT_TRUE(client->ZAdd("test_zset", 15.0, "member3"));

    // ZRANGE
    auto range = client->ZRange("test_zset", 0, -1);
    EXPECT_EQ(range.size(), 3);
    EXPECT_EQ(range[0], "member1");
    EXPECT_EQ(range[1], "member3");
    EXPECT_EQ(range[2], "member2");

    // ZSCORE
    EXPECT_DOUBLE_EQ(client->ZScore("test_zset", "member1"), 10.0);
    EXPECT_DOUBLE_EQ(client->ZScore("test_zset", "member3"), 15.0);

    // ZREM
    EXPECT_TRUE(client->ZRem("test_zset", "member2"));
    range = client->ZRange("test_zset", 0, -1);
    EXPECT_EQ(range.size(), 2);
}

TEST_F(RedisTest, ExpireOperations) {
    // SET
    EXPECT_TRUE(client->Set("expire_key", "value"));

    // EXPIRE
    EXPECT_TRUE(client->Expire("expire_key", 2));
    EXPECT_GE(client->TTL("expire_key"), 1);
    EXPECT_LE(client->TTL("expire_key"), 2);

    // PERSIST
    EXPECT_TRUE(client->Persist("expire_key"));
    EXPECT_EQ(client->TTL("expire_key"), -1);

    // Check still exists
    EXPECT_TRUE(client->Exists("expire_key"));
}

TEST_F(RedisTest, TransactionOperations) {
    // MULTI/EXEC
    EXPECT_TRUE(client->Multi());
    client->Set("tx_key1", "value1");
    client->Set("tx_key2", "value2");
    auto results = client->Exec();
    EXPECT_EQ(results.size(), 2);
    EXPECT_EQ(client->Get("tx_key1"), "value1");
    EXPECT_EQ(client->Get("tx_key2"), "value2");

    // MULTI/DISCARD
    EXPECT_TRUE(client->Multi());
    client->Set("tx_key3", "value3");
    EXPECT_TRUE(client->Discard());
    EXPECT_EQ(client->Get("tx_key3"), "");
}

TEST_F(RedisTest, AsyncOperations) {
    std::promise<bool> promise;
    std::future<bool> future = promise.get_future();

    client->ExecuteAsync("SET async_key async_value", [&promise](const RedisReply& reply) {
        promise.set_value(!reply.IsError() && reply.AsString() == "OK");
    });

    EXPECT_TRUE(future.get());
    EXPECT_EQ(client->Get("async_key"), "async_value");
}

TEST_F(RedisTest, ConnectionPool) {
    // 获取统计信息
    auto stats = client->GetStats();
    EXPECT_GE(stats.totalConnections, 2);
    EXPECT_GE(stats.idleConnections, 0);

    // 并发测试
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, i, &success_count]() {
            std::string key = "pool_test_" + std::to_string(i);
            std::string value = "value_" + std::to_string(i);
            if (client->Set(key, value) && client->Get(key) == value) {
                success_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count, 10);
}

// RedisCommand类测试
TEST(RedisCommandTest, BuildCommand) {
    RedisCommand cmd("SET");
    cmd.Append("key").Append("value");

    auto args = cmd.GetArgs();
    EXPECT_EQ(args.size(), 3);
    EXPECT_EQ(args[0], "SET");
    EXPECT_EQ(args[1], "key");
    EXPECT_EQ(args[2], "value");

    // 测试不同类型的参数
    RedisCommand cmd2("INCRBY");
    cmd2.Append("counter").Append(10);
    auto args2 = cmd2.GetArgs();
    EXPECT_EQ(args2[0], "INCRBY");
    EXPECT_EQ(args2[1], "counter");
    EXPECT_EQ(args2[2], "10");
}

// RedisReply类测试
TEST(RedisReplyTest, CreateReplies) {
    // 字符串回复
    RedisReply strReply(RedisReplyType::STRING);
    strReply.SetString("OK");
    EXPECT_TRUE(strReply.IsString());
    EXPECT_EQ(strReply.AsString(), "OK");

    // 整数回复
    RedisReply intReply(RedisReplyType::INTEGER);
    intReply.SetInteger(42);
    EXPECT_TRUE(intReply.IsInteger());
    EXPECT_EQ(intReply.AsInteger(), 42);

    // 数组回复
    std::vector<RedisReply> array;
    RedisReply item1(RedisReplyType::STRING);
    item1.SetString("item1");
    RedisReply item2(RedisReplyType::STRING);
    item2.SetString("item2");
    array.push_back(item1);
    array.push_back(item2);

    RedisReply arrayReply(RedisReplyType::ARRAY);
    arrayReply.SetArray(array);
    EXPECT_TRUE(arrayReply.IsArray());
    EXPECT_EQ(arrayReply.AsArray().size(), 2);

    // 错误回复
    RedisReply errorReply(RedisReplyType::ERROR);
    errorReply.SetError("Test error");
    EXPECT_TRUE(errorReply.IsError());
    EXPECT_EQ(errorReply.GetError(), "Test error");

    // NIL回复
    RedisReply nilReply(RedisReplyType::NIL);
    nilReply.SetNil();
    EXPECT_TRUE(nilReply.IsNil());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}