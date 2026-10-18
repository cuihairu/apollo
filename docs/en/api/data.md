---
title: Data API
icon: database
prev: /en/api/index.md
---

# Data API

> 2026-10-04 audit: this page was rewritten against the actual state of HEAD.
> The old draft's `orm::Session`/`Query`/`Entity`/`ConnectionPool`/
> `redis::RedisClient`/`cache::Cache` do not exist in the repository (external
> DB/Redis clients are not wired; P1-4 removed five legacy implementations).

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

    // row mapping: QueryRow -> T
    template<typename T>
    std::vector<T> query(const std::string& sql,
                         const std::function<T(const QueryRow&)>& mapper) const;

    template<typename T>
    std::optional<T> query_for_one(/* same as above */);
};
}
```

---

## apollo::data::orm::MemoryConnection

An in-memory connection for tests (an `IConnection` implementation);
`seed_query` pre-seeds query results.

```cpp
namespace apollo::data::orm {
class MemoryConnection final : public apollo::data::core::IConnection {
public:
    bool connect() override;
    void disconnect() override;
    bool is_connected() const override;
    apollo::data::core::QueryResult execute_query(const std::string& sql) override;
    apollo::data::core::QueryResult execute_update(const std::string& sql) override;

    // pre-seed query results (test fixture)
    void seed_query(std::string sql, std::vector<QueryRow> rows);
};
}
```

---

## apollo::data::journal::PersistJournal

A write-behind journal (P1-4): append (write-ahead flush) → drain (quota
dequeue, sink persists to archive) → compact (snapshot thinning, tmp+rename
atomic rewrite) → replay (crash playback + seq continuation).
Line format `seq|timestamp_ms|key|payload`.

```cpp
namespace apollo::data::journal {
struct JournalEntry {
    std::uint64_t sequence = 0;
    std::int64_t timestamp_ms = 0;
    std::string key;      // business key (e.g. decimal string of a player id)
    std::string payload;  // business payload (JSON line)
};

class PersistJournal {
public:
    explicit PersistJournal(std::string journal_path);

    bool open();
    bool append(std::string key, std::string payload, std::int64_t timestamp_ms);
    std::size_t pending() const;                      // entries awaiting drain
    std::size_t drain(const ApplySink& sink, std::size_t max_entries);
    std::size_t replay(const ApplySink& sink);        // returns replayed entry count
    std::size_t compact();
    std::uint64_t next_sequence() const;
};
}
```

**Thread safety**: unsafe (requires serialized storage-owner calls, targeting database:
autoSaveLoop drain + shutdown close-out)

---

## apollo::data::cache::CacheManager / PrimitiveCache

In-process cache: the `ICacheProvider` abstraction (string key-value + TTL),
`PrimitiveCache` as the map + TTL implementation, and `CacheManager` as the
singleton facade (provider injection).

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

**Not implemented**: real MySQL/Redis connectors and connection pools, a
cross-process shared hot-data layer (Redis), entity-property ORM mapping (the
old draft's Session/Query/Entity API never existed).
