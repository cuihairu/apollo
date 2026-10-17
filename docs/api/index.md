---
title: API 参考
icon: code
order: 1
---

# API 参考

本节包含 Apollo 的 API 参考文档。

## 模块 API

- [Base API](./base.md) - 基础工具类 API
- [Core API](./core.md) - 核心框架 API
- [Runtime API](./runtime.md) - 运行时 API
- [Network API](./net.md) - 网络通信 API
- [Data API](./data.md) - 数据访问 API
- [Game API](./game.md) - 游戏逻辑 API
- [BigWorld API](./bigworld.md) - BigWorld 兼容层 API

## 命名空间

所有 Apollo API 都在 `apollo` 命名空间下：

```cpp
#include <apollo/base/time.hpp>
#include <apollo/core/log/log_manager.hpp>
#include <apollo/runtime/application_host.hpp>

using apollo::base::Time;
using apollo::runtime::ServiceHost;
```

## 错误处理

Apollo 无统一异常基类（`apollo::Exception` 不存在），按域分三种惯例：

```cpp
// 1) 异常：宿主/容器类在编程错误路径抛 std::runtime_error
//    （如 ThreadPool 已 stop 后 submit）
try {
    pool.submit(task);
} catch (const std::runtime_error& e) {
    APOLLO_LOG()->error("submit failed: {}", e.what());
}

// 2) 哨兵返回值：池件耗尽返回 UINT32_MAX / nullptr
uint32_t id = pool.allocate();
if (id == UINT32_MAX) { /* 池耗尽 */ }

// 3) 结果结构：数据层 QueryResult 携带 ok/error/affected_rows/rows
auto result = conn->execute_query(sql);
if (!result.ok) {
    APOLLO_LOG()->error("query failed: {}", result.error);
}
```

日志宏只有 `APOLLO_LOG()`（默认 logger）与 `APOLLO_LOG_GET(name)`（具名
logger）两个，格式化走 logger 方法链（`->info/warn/error(...)`）——
`LOG_ERROR(logger, ...)` 一类宏不存在。

## 线程安全

Apollo 的 API 线程安全性分为三级：

- **安全** - 可以在多线程中安全调用
- **条件安全** - 需要外部同步
- **不安全** - 必须在同一线程调用

文档中会标注每个 API 的线程安全级别。
