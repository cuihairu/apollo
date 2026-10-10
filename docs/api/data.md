---
title: Data API
icon: database
prev: /api/README.md
---

# Data API

> 2026-10-04 对账：本页按 HEAD 实况重写。旧稿的 `orm::Session`/`Query`/
> `Entity`/`ConnectionPool`/`redis::RedisClient`/`cache::Cache` 在仓库中不存在
> （外部 DB/Redis 客户端未接线，P1-4 已删五套存量实现）。

## apollo::data::core::IConnection

```cpp
namespace apollo::data::core {
using QueryRow = std::unordered_map<std::string, std::string>;

struct QueryResult {
    bool ok = true;
    std::string error;
    uint64_t affected_rows = 0;
    std::vector<QueryRow> rows;
};

class IConnection {
public:
    virtual ~IConnection() = default;
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    virtual QueryResult execute_query(const std::string& sql) = 0;
    virtual QueryResult execute_update(const std::string& sql) = 0;
};
}
```

---

## apollo::data::core::IDataSource / SimpleDataSource

```cpp
namespace apollo::data::core {
class IDataSource {
public:
    virtual ~IDataSource() = default;
    virtual ConnectionPtr acquire() = 0;
};

using ConnectionFactory = std::function<ConnectionPtr()>;

class SimpleDataSource final : public IDataSource {
public:
    explicit SimpleDataSource(ConnectionFactory factory);
};
}
```

---

## apollo::data::orm::SqlTemplate

```cpp
namespace apollo::data::orm {
class SqlTemplate {
public:
    explicit SqlTemplate(std::shared_ptr<apollo::data::core::IDataSource> data_source);

    apollo::data::core::QueryResult query(const std::string& sql) const;
    apollo::data::core::QueryResult update(const std::string& sql) const;

    // 行映射：QueryRow → T
    template<typename T>
    std::vector<T> query(const std::string& sql,
                         const std::function<T(const QueryRow&)>& mapper) const;

    template<typename T>
    std::optional<T> query_for_one(/* 同上 */);
};
}
```

---

## apollo::data::orm::MemoryConnection

测试用内存连接（`IConnection` 实现），`seed_query` 预置查询结果。

```cpp
namespace apollo::data::orm {
class MemoryConnection final : public apollo::data::core::IConnection {
public:
    bool connect() override;
    void disconnect() override;
    bool is_connected() const override;
    apollo::data::core::QueryResult execute_query(const std::string& sql) override;
    apollo::data::core::QueryResult execute_update(const std::string& sql) override;

    // 预置查询结果（测试夹具）
    void seed_query(std::string sql, std::vector<QueryRow> rows);
};
}
```

---

## apollo::data::journal::PersistJournal

write-behind 日志（P1-4）：append（write-ahead 落盘）→ drain（定额出队，sink
落档案）→ compact（快照压薄，tmp+rename 原子重写）→ replay（崩溃回放 + seq 续接）。
行格式 `seq|timestamp_ms|key|payload`。

```cpp
namespace apollo::data::journal {
struct JournalEntry {
    std::uint64_t sequence = 0;
    std::int64_t timestamp_ms = 0;
    std::string key;      // 业务键（如玩家 id 十进制串）
    std::string payload;  // 业务载荷（JSON 行）
};

class PersistJournal {
public:
    explicit PersistJournal(std::string journal_path);

    bool open();
    bool append(std::string key, std::string payload, std::int64_t timestamp_ms);
    std::size_t pending() const;                      // 待 drain 条数
    std::size_t drain(const ApplySink& sink, std::size_t max_entries);
    std::size_t replay(const ApplySink& sink);        // 返回回放条数
    std::size_t compact();
    std::uint64_t next_sequence() const;
};
}
```

**线程安全**: 不安全（归宿主保存线程（zone-app：autoSaveLoop drain + 关停收口））

---

## apollo::data::cache::CacheManager / PrimitiveCache

进程内缓存：`ICacheProvider` 抽象（string 键值 + TTL），`PrimitiveCache` 为
map + TTL 实现，`CacheManager` 单例门面（provider 注入）。

```cpp
namespace apollo::data::cache {
class ICacheProvider {
public:
    virtual ~ICacheProvider() = default;
    virtual bool get(const std::string& key, std::string& value) = 0;
    virtual void set(const std::string& key, const std::string& value, int ttl_seconds) = 0;
    virtual void remove(const std::string& key) = 0;
    virtual bool exists(const std::string& key) = 0;
};

class CacheManager {
public:
    static CacheManager& instance();
    void set_provider(std::shared_ptr<ICacheProvider> provider);
    bool get(const std::string& key, std::string& value);
    void set(const std::string& key, const std::string& value, int ttl_seconds = 0);
    void remove(const std::string& key);
    bool exists(const std::string& key);
    template <typename T>
    T get_or_compute(const std::string& key, std::function<T()> compute, int ttl_seconds = 0);
};
}
```

**未实现**：MySQL/Redis 真实连接器与连接池、跨进程共享热数据层（Redis）、
实体属性 ORM 映射（旧稿 Session/Query/Entity API 从未存在）。
