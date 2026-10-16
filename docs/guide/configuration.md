---
title: 配置
icon: settings
order: 6
prev: /guide/module-system
---

[English](configuration.en.md) | [中文](configuration.md)

# 配置

> 2026-10-04 对账：本页按 HEAD 实况重写。旧稿的 `load()`/`loadFromEnv()`/`get<T>()`/`onChange()`/`setOverrideStrategy()`/`parseArgs()`/`defineSchema()`/`validate()` 均不存在；
> YAML/TOML 不支持。环境变量覆盖、命令行统一解析、schema 校验均未实现。

配置分三层，各有分工：

| 层 | 位置 | 能力 | 现状 |
| --- | --- | --- | --- |
| `ConfigRegistry` | `modules/core` | 进程内键值表（string/int64/bool） | 现役；无文件 I/O |
| 应用自读 | `apps/*` | CLI 参数解析、策划表加载 | 各 app 自管，无统一入口 |

## ConfigRegistry（进程内键值）

`apollo::core::config::ConfigRegistry`（`modules/core`）是更薄的进程内注册表：
`set` / `get_string` / `get_int64` / `get_bool` / `has`，不做文件 I/O、不发事件。
适合代码内默认值集中登记。

```cpp
#include <apollo/core/config/config_registry.hpp>

apollo::core::config::ConfigRegistry config;
config.set("server.port", 8888);
config.set("server.host", std::string("0.0.0.0"));

int64_t port = config.get_int64("server.port");
bool has = config.has("server.port");
```

## 应用自读

### CLI 参数

没有统一的命令行解析层，各 app 自管。base-app 的做法是入口处解析进 `BaseConfig` 结构
（`apps/base-app/src/main.cpp`）：

```cpp
BaseConfig loadConfig(int argc, char* argv[]) {
    BaseConfig config;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--db" && i + 1 < argc) {
            config.dbName = argv[++i];
        }
    }
    return config;
}
```

P3-1 已收口（配置统一，legacy `ConfigManager` 删除，`d36529c5`）；现役配置面 = `ConfigRegistry` + 应用自读。

### 策划表

`include/apollo/config/table_loader.h`：`TableLoader` 基类加载 `*|*` 分隔的文本表格
（第 1 行描述、第 2 行字段名、第 3 行起数据行），子类重写 `onRowRead()` 逐行消费。

## 未实现清单

以下能力在旧稿出现过、仓库中不存在，按需另行立项：

- 环境变量覆盖与优先级策略（EnvFirst 之类）
- 统一命令行解析（`--server.port=8888` 风格）
- schema 定义与校验
- YAML/TOML 格式（CMake 探测 yaml-cpp，但解析器未接线）
- 游戏热路径的 `ConfigSnapshot` 热更（规划见 architecture-review §17.6）
