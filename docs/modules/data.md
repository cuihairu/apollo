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

Data 模块提供数据访问层。2026-10-04 对账后的现状：三个子库——core（连接抽象）、
orm（SQL 模板 + 内存连接 + write-behind 日志）、cache（进程内缓存）；外部数据库
（MySQL/Redis）客户端未接线，P1-4 已删除五套存量实现（见 todo.md P1-4）。

## core（apollo::data::core，头文件库）

连接与数据源抽象。

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

SQL 模板执行 + 测试内存连接 + write-behind 日志。

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

消费侧接线见 apps/zone-app（保存路径 write-ahead + autoSaveLoop drain + 启动
replay，P1-5 RecoveryCoordinator 编排）。

## cache（apollo::data_cache）

进程内缓存：`ICacheProvider` 抽象 + `PrimitiveCache`（map + TTL）+
`CacheManager` 单例门面（provider 注入，get/set/remove/exists/get_or_compute）。

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

## 依赖

- apollo::core

## 链接

```cmake
target_link_libraries(my_app
    apollo::data_core      # INTERFACE（仅头）
    apollo::data_orm       # sql_template / memory_connection / persist_journal
    apollo::data_cache     # cache_manager / primitive_cache
)
```

## 未实现（规划态）

- MySQL/Redis 真实连接器与连接池（P1-4 已删五套存量：`#ifdef` 四件、legacy
  双拷贝、connection_pool 两份、redis 全族、DistributedLock）
- 跨进程共享热数据层（Redis）——设计见 attribute-sync §8.4，引入随跨进程批次
- 实体属性 ORM 映射（docs/api/data.md 旧稿的 Session/Query/Entity API 不存在）
