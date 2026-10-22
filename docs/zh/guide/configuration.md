---
title: 配置
icon: settings
order: 6
prev: /guide/module-system
---

# 配置

Apollo 当前使用进程内 `ConfigRegistry` 和各应用自己的配置结构。没有统一的配置文件、环境变量覆盖优先级或命令行解析层。

## 进程内配置

`apollo::core::config::ConfigRegistry` 提供 `set`、`get_string`、`get_int64`、`get_bool` 和 `has`。它是键值注册表，不负责文件读取、校验或变更通知。

```cpp
#include <apollo/core/config/config_registry.hpp>

apollo::core::config::ConfigRegistry config;
config.set("server.port", 8888);
config.set("server.host", std::string("0.0.0.0"));
const auto port = config.get_int64("server.port");
```

## 应用配置

配置须与目标角色对应：loginServer 配置认证与目录；gateway 配置接入及分开的 Base/cell 路由；Base 配置玩家归属和 database 客户端；World 配置执行端注册、容量、拓扑和准入；cell 配置所属 World、模拟与 Base 寻址；database 配置存储分区、连接和保存队列；machined 只配置本机监督。

逻辑服务身份与地址分开。玩家切换 World 保持原区服、Base 和存储归属。独立 World 需要服务目录配置，不需要原服数据库凭证或强制同机 machined。进程代次、玩家资料写权、场景模拟代次、会话版本与存储结构版本分别定义，不能混用。

现有 LoginConfig、GatewayConfig、WorldAppConfig 尚未完整实现上述拆分。须将资料和直接存储职责移出 World，接到 Base/database 入口。当前 CLI 以程序 --help 为准，不能作为另一套目标部署方案；代码映射和迁移任务见[服务器应用](../apps/index.md)及根目录 todo.md。

认证器仍是原型，测试档案不代表生产建号与鉴权。跨服分组、准入与返回配置为[设计预留](../design/cross-server-scenes.md)，未启用跨服部署。

## 策划表与能力边界

`include/apollo/config/table_loader.h` 中的 `TableLoader` 加载 `*|*` 分隔的文本表格：第一行描述、第二行字段名、第三行起为数据。子类通过 `onRowRead()` 消费数据。

统一 CLI、配置 schema 校验、YAML/TOML 解析和游戏配置快照热替换尚未接通。配置结构里存在数据库字段，也不表示 MySQL 或 Redis 已进入业务保存路径。热更新能力及排空边界见[架构总览](./architecture.md)。
