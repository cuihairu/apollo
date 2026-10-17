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

| 应用 | 配置结构 | 职责 |
|---|---|---|
| Login | `login::LoginConfig` | 认证、Gateway 分配、World 玩家资料链路和在线目录 |
| Gateway | `gateway::GatewayConfig` | 客户端监听、准入链路、在线目录查询和 World 消息链路 |
| World | `world_app::WorldAppConfig` | 模拟、脚本、玩家档案与持久化 |

每个入口自行解析 CLI；以对应程序的 `--help` 为准。配置结构中的测试默认值与程序入口启用的服务默认值可能不同，不能把库配置的端口 0 当成应用入口的部署端口。

```bash
world-app --world-id 1 --m1-port 9101 --player-store-m1-port 9104 \
  --data-dir data/world-1/players --journal data/world-1/journal/persist.log
login-app --m1-port 9103 --world-store-m1-port 9104 --directory-port 9105 \
  --directory-epochs data/login/epochs.json --gateway tcp://127.0.0.1:9002
gateway-app --port 9002 --world-m1-port 9101 \
  --login-m1-port 9103 --directory-m1-port 9105
```

World 默认按 `worldId` 分开档案和 journal 目录，避免不同 World 共写同一个本地存储。Login 仅保存目录权威代次水位，不承载玩家游戏档案。当前认证器仍是测试账号原型；成功登录还要求 World 中已有对应玩家档案，不能把这组命令当成生产建号和鉴权方案。

跨服场景的区服分组、准入和返回策略是[设计预留](../design/cross-server-scenes.md)，尚未提供可启用的部署配置。

## 策划表与能力边界

`include/apollo/config/table_loader.h` 中的 `TableLoader` 加载 `*|*` 分隔的文本表格：第一行描述、第二行字段名、第三行起为数据。子类通过 `onRowRead()` 消费数据。

统一 CLI、配置 schema 校验、YAML/TOML 解析和游戏配置快照热替换尚未接通。配置结构里存在数据库字段，也不表示 MySQL 或 Redis 已进入业务保存路径。热更新能力及排空边界见[架构总览](./architecture.md)。
