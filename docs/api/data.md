---
title: Data API
icon: database
prev: /api/README.md
---

# Data API

## apollo::data::orm::Session

```cpp
namespace apollo::data::orm {
class Session {
public:
    // 插入
    template<typename T>
    int64_t insert(const T& entity);

    // 更新
    template<typename T>
    int update(const T& entity);

    // 删除
    template<typename T>
    int delete_(const T& entity);

    // 查询
    template<typename T>
    Query<T> query();

    // 执行原生SQL
    Result execute(const std::string& sql);

    // 事务
    Transaction beginTransaction();
};
}
```

**线程安全**: ⚠️ 建议使用连接池

---

## apollo::data::orm::Query

```cpp
namespace apollo::data::orm {
template<typename T>
class Query {
public:
    // WHERE 条件
    Query& where(const std::string& condition);
    template<typename... Args>
    Query& where(const std::string& condition, Args&&... args);

    // ORDER BY
    Query& orderBy(const std::string& field);
    Query& orderByDesc(const std::string& field);

    // LIMIT
    Query& limit(int count);
    Query& offset(int count);

    // 执行查询
    std::vector<T> list();
    T one();
    int count();
};
}
```

---

## apollo::data::orm::Entity

```cpp
namespace apollo::data::orm {
class Entity {
public:
    // 主键
    int64_t id() const;

    // 表名
    virtual std::string tableName() const = 0;

    // 保存
    void save();

    // 删除
    void remove();

    // 刷新
    void reload();
};
}
```

---

## apollo::data::orm::ConnectionPool

```cpp
namespace apollo::data::orm {
class ConnectionPool {
public:
    static std::shared_ptr<ConnectionPool> create(const ConnectionPoolConfig& config);

    // 获取连接
    ConnectionPtr getConnection();

    // 归还连接
    void returnConnection(ConnectionPtr conn);

    // 池信息
    size_t activeCount() const;
    size_t idleCount() const;
};
}
```

**线程安全**: ✅

---

## apollo::data::redis::RedisClient

```cpp
namespace apollo::data::redis {
class RedisClient {
public:
    static std::shared_ptr<RedisClient> create(const std::string& url);

    // 字符串操作
    void set(const std::string& key, const std::string& value);
    void set(const std::string& key, const std::string& value, int ttl);
    std::string get(const std::string& key);
    bool del(const std::string& key);

    // 哈希操作
    void hset(const std::string& key, const std::string& field, const std::string& value);
    std::string hget(const std::string& key, const std::string& field);
    std::map<std::string, std::string> hgetall(const std::string& key);

    // 列表操作
    void lpush(const std::string& key, const std::string& value);
    void rpush(const std::string& key, const std::string& value);
    std::string lpop(const std::string& key);
    std::string rpop(const std::string& key);
    std::vector<std::string> lrange(const std::string& key, int start, int stop);

    // 集合操作
    void sadd(const std::string& key, const std::string& member);
    bool sismember(const std::string& key, const std::string& member);
    std::vector<std::string> smembers(const std::string& key);

    // 过期时间
    void expire(const std::string& key, int seconds);
    int ttl(const std::string& key);

    // 连接
    void connect();
    void disconnect();
    bool isConnected() const;
};
}
```

**线程安全**: ⚠️ 建议使用连接池

---

## apollo::data::cache::Cache

```cpp
namespace apollo::data::cache {
class Cache {
public:
    static std::shared_ptr<Cache> create(const std::string& url);

    // 设置
    template<typename T>
    void put(const std::string& key, const T& value);
    template<typename T>
    void put(const std::string& key, const T& value, int ttl);

    // 获取
    template<typename T>
    std::optional<T> get(const std::string& key);

    // 删除
    bool remove(const std::string& key);

    // 清空
    void clear();

    // 检查
    bool exists(const std::string& key);

    // 过期
    void expire(const std::string& key, int ttl);
};
}
```

**线程安全**: ✅
