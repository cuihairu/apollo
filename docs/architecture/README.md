
# 架构参考

Apollo 的目标架构权威说明见[架构现状与目标模型](../analysis/architecture-review.md)、[架构总览](../guide/architecture.md)和[副本制 GameServer 设计](../30-Instance_GameServer_Design.md)。

本目录中的 BigWorld、KBEngine、AOI 和其他 MMO 框架材料是第三方实现参考。它们描述外部系统的术语和机制，不代表 Apollo 采用对应的进程模型或承诺相同能力。特别是 `BaseApp`、`CellApp`、`BaseAppMgr`、`Witness`、`Ghost` 和 `Space` 都不是 Apollo 的目标架构概念。

Apollo 的目标术语只有：

| 名称 | 含义 |
|---|---|
| `Login` | 账号认证和登录凭证。 |
| `Gateway` | 客户端连接、会话和消息路由。 |
| `World` | 承载副本并运行权威对局逻辑的进程。 |
| `Instance` | 一场对局及其临时状态。 |

仓库中的 `login-app`、`gateway-app`、`world-app`、`base-app` 和 `baseappmgr` 是当前原型程序名；应用实现与目标职责的映射见[服务器应用](../apps/index.md)。
