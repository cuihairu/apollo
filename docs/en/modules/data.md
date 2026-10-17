---
title: Data Module
icon: database
order: 4
category:
  - Modules
tag:
  - data
  - database
---

# Data Module

The Data module provides the data access layer. State as of the 2026-10-04 reconciliation:
three sub-libraries — core (connection abstraction), orm (SQL templates + in-memory
connection + write-behind journal), cache (in-process cache); external database
(MySQL/Redis) clients are not wired, and P1-4 deleted the five legacy implementations
(see todo.md P1-4).

## core (`apollo::data::core`, header-only library)

Connection and data-source abstraction.

```cpp
#include <apollo/data/core/connection.hpp>
#include <apollo/data/core/data_source.hpp>

namespace apollo::data::core {
// QueryResult：ok / error / affected_rows / rows（unordered_map<string,string>）
class IConnection {
public:
    virtual ~IConnection() = default;
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    virtual QueryResult execute_query(const std::string& sql) = 0;
    virtual QueryResult execute_update(const std::string& sql) = 0;
};

class IDataSource {
public:
    virtual ConnectionPtr acquire() = 0;   // 连接工厂注入
};
}
```

## ORM（apollo::data_orm）

SQL template execution + an in-memory test connection + a write-behind journal.

```cpp
#include <apollo/data/orm/sql_template.hpp>

// SqlTemplate：IDataSource 注入，query/update/带 mapper 的行映射
apollo::data::orm::SqlTemplate sql(data_source);
auto result = sql.query("SELECT id, name FROM player");
auto players = sql.query<PlayerRow>("SELECT ...", [](const QueryRow& row) {
    return PlayerRow{row};
});
```

```cpp
#include <apollo/data/orm/memory_connection.hpp>

// MemoryConnection：IConnection 内存实现（测试用），seed_query 预置查询结果
auto conn = std::make_shared<MemoryConnection>();
conn->seed_query("SELECT 1", rows);
```

```cpp
#include <apollo/data/orm/persist_journal.hpp>

// PersistJournal（P1-4）：write-behind 日志
// 行格式 seq|timestamp_ms|key|payload；快照压薄 tmp+rename 原子重写
apollo::data::journal::PersistJournal journal(path);
journal.open();
journal.append("77", payload_json, timestamp_ms);  // write-ahead 落盘
journal.drain(sink, /*max_entries=*/32);           // 定额出队 → 档案落盘
journal.compact();                                 // 快照压薄
journal.replay(sink);                              // 崩溃回放 + seq 续接
```

For the consumer-side wiring see apps/base-app (save path write-ahead + autoSaveLoop drain +
startup replay, orchestrated by the P1-5 RecoveryCoordinator).

## cache（apollo::data_cache）

In-process cache: the `ICacheProvider` abstraction + `PrimitiveCache` (map + TTL) +
the `CacheManager` singleton facade (provider injection,
get/set/remove/exists/get_or_compute).

```cpp
#include <apollo/data/cache/cache_manager.hpp>
#include <apollo/data/cache/primitive_cache.hpp>

auto& cache = apollo::data::cache::CacheManager::instance();
cache.set_provider(std::make_shared<PrimitiveCache>());
cache.set("player:12345", serialized, 3600);
std::string out;
if (cache.get("player:12345", out)) { /* 命中 */ }
cache.remove("player:12345");
```

## Dependencies

- apollo::core

## Linking

```cmake
target_link_libraries(my_app
    apollo::data_core      # INTERFACE（仅头）
    apollo::data_orm       # sql_template / memory_connection / persist_journal
    apollo::data_cache     # cache_manager / primitive_cache
)
```

## Not Implemented (Planned)

- Real MySQL/Redis connectors and connection pools (P1-4 deleted the five legacy sets: four
  `#ifdef` pieces, a legacy double copy, two connection_pool copies, the whole redis family,
  DistributedLock)
- A cross-process shared hot-data layer (Redis) — design in attribute-sync §8.4; introduction
  follows the cross-process batch
- Entity attribute ORM mapping (the Session/Query/Entity APIs of the old docs/api/data.md
  draft do not exist)
