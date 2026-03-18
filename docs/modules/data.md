---
title: Data 模块
icon: database
order: 4
category:
  - 模块
tag:
  - data
  - 数据库
---

# Data 模块

Data 模块提供数据访问层，支持 MySQL、Redis 等多种数据存储。

## ORM

数据库 ORM 映射。

```cpp
#include <apollo/data/orm/session.hpp>
#include <apollo/data/orm/query.hpp>

using namespace apollo::data::orm;

// 定义实体
class Player : public Entity<Player> {
public:
    PROPERTY(id, int64_t).primary().autoIncrement();
    PROPERTY(name, std::string).notNull();
    PROPERTY(level, int).defaultValue(1);
    PROPERTY(exp, int64_t).defaultValue(0);
};

// 使用 ORM
auto session = SessionFactory::create("mysql://localhost:3306/apollo");

// 插入
Player player;
player.name = "Player1";
player.level = 10;
session.insert(player);

// 查询
auto players = session.query<Player>()
    .where("level > ?", 5)
    .orderBy("exp DESC")
    .limit(10)
    .list();

// 更新
session.update<Player>()
    .set("exp = exp + ?", 100)
    .where("id = ?", player.id)
    .execute();

// 删除
session.delete<Player>()
    .where("level < ?", 10)
    .execute();
```

## Redis

Redis 客户端。

```cpp
#include <apollo/data/redis/client.hpp>

using namespace apollo::data::redis;

// 连接
auto redis = RedisClient::create("tcp://127.0.0.1:6379");

// 字符串操作
redis->set("key", "value");
auto value = redis->get("key");

// 哈希操作
redis->hset("player:12345", "level", "10");
redis->hset("player:12345", "exp", "1000");
auto level = redis->hget("player:12345", "level");

// 列表操作
redis->lpush("queue", "item1");
redis->lpush("queue", "item2");
auto item = redis->rpop("queue");

// 集合操作
redis->sadd("online_players", "12345");
redis->sadd("online_players", "67890");
auto members = redis->smembers("online_players");
```

## 连接池

数据库连接池。

```cpp
#include <apollo/data/orm/connection_pool.hpp>

// 配置连接池
ConnectionPoolConfig config;
config.host = "localhost";
config.port = 3306;
config.database = "apollo";
config.username = "root";
config.password = "123456";
config.minConnections = 5;
config.maxConnections = 20;

// 创建连接池
auto pool = ConnectionPool::create(config);

// 获取连接
auto conn = pool->getConnection();
// 使用连接...
// 连接自动归还到池中
```

## 缓存

缓存抽象层。

```cpp
#include <apollo/data/cache/cache.hpp>

using namespace apollo::data::cache;

// 创建缓存
auto cache = Cache::create("redis://localhost");

// 设置缓存
cache->put("player:12345", playerData, 3600);  // 1小时过期

// 获取缓存
auto data = cache->get("player:12345");
if (!data) {
    // 缓存未命中，从数据库加载
    playerData = loadFromDatabase(12345);
    cache->put("player:12345", playerData, 3600);
}

// 删除缓存
cache->remove("player:12345");
```

## 依赖

- apollo::core
- apollo::base

## 链接

```cmake
find_package(apollo-data-core REQUIRED)
find_package(apollo-data-orm REQUIRED)
find_package(apollo-data-redis REQUIRED)

target_link_libraries(my_app
    apollo::data_core
    apollo::data_orm
    apollo::data_redis
)
```
