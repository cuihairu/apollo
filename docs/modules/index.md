---
title: 模块总览
---

# 模块总览

Apollo 的模块拆分围绕一条原则：

`先稳定底层能力，再按游戏域装配，不让 BigWorld 语义反向污染基础层。`

当前模块可以按职责理解为：

| 模块 | 作用 |
|------|------|
| [Base](/modules/base) | 时间、线程池、内存、字符串、ID 等基础设施 |
| [Core](/modules/core) | 配置、日志、生命周期、DI、模块注册等框架内核 |
| [Runtime](/modules/runtime) | ApplicationHost、WorldHost、ServiceHost 等宿主运行时 |
| [Data](/modules/data) | 数据访问、缓存、Repository、连接抽象 |
| [Net](/modules/net) | 传输、协议、消息编解码、会话通信 |
| [Game](/modules/game) | 实体、AOI、战斗、属性、场景与世界逻辑 |
| [BigWorld](/modules/bigworld) | Witness、Ghost、分布式空间等增强语义 |

推荐阅读顺序：

1. [Base](/modules/base)
2. [Core](/modules/core)
3. [Runtime](/modules/runtime)
4. [Net](/modules/net)
5. [Data](/modules/data)
6. [Game](/modules/game)
7. [BigWorld](/modules/bigworld)
