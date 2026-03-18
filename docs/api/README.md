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

## 命名空间

所有 Apollo API 都在 `apollo` 命名空间下：

```cpp
#include <apollo/base/time.hpp>
#include <apollo/core/log.hpp>
#include <apollo/net/tcp/server.hpp>

using apollo::base::Time;
using apollo::core::LogManager;
using apollo::net::tcp::Server;
```

## 错误处理

Apollo 使用异常和错误码两种方式报告错误：

```cpp
// 异常方式
try {
    auto result = someOperation();
} catch (const apollo::Exception& e) {
    LOG_ERROR("App", "错误: {}", e.what());
}

// 错误码方式
auto result = someOperation();
if (!result.ok()) {
    LOG_ERROR("App", "错误: {}", result.error());
}
```

## 线程安全

Apollo 的 API 线程安全性分为三级：

- ✅ **线程安全** - 可以在多线程中安全调用
- ⚠️ **条件安全** - 需要外部同步
- ❌ **不安全** - 必须在同一线程调用

文档中会标注每个 API 的线程安全级别。
