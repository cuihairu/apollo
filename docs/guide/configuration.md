---
title: 配置
icon: settings
order: 6
prev: /guide/module-system
---

# 配置系统

Apollo 提供灵活的配置管理，支持多种格式和热更新。

## 配置文件格式

支持 JSON、YAML、TOML 格式。

### config.json

```json
{
  "server": {
    "host": "0.0.0.0",
    "port": 8888
  },
  "database": {
    "host": "localhost",
    "port": 3306,
    "name": "apollo",
    "user": "root",
    "password": "123456"
  },
  "log": {
    "level": "info",
    "file": "logs/server.log"
  }
}
```

## 使用配置

### 加载配置

```cpp
#include <apollo/core/config.hpp>

// 加载配置文件
auto& config = core::ConfigManager::instance();
config.load("config.json");

// 或从环境变量
config.loadFromEnv();
```

### 读取配置

```cpp
// 读取简单值
int port = config.get<int>("server.port");
std::string host = config.get<std::string>("server.host");

// 读取嵌套值
std::string dbUser = config.get<std::string>("database.user");

// 带默认值
int timeout = config.get<int>("server.timeout", 30);
```

### 配置监听

```cpp
// 监听配置变化
config.onChange("server.port", [](int newPort) {
    LOG_INFO("Config", "端口已更改为: {}", newPort);
});
```

## 热更新

配置文件修改后自动重新加载：

```cpp
// 启用热更新
config.enableHotReload("config.json");

// 热更新回调
config.onReload([]() {
    LOG_INFO("Config", "配置已重新加载");
});
```

## 环境变量覆盖

环境变量可以覆盖配置文件中的值：

```bash
export APOLLO_SERVER_PORT=9999
export APOLLO_DATABASE_HOST=192.168.1.100
```

```cpp
// 优先级：环境变量 > 命令行参数 > 配置文件
config.setOverrideStrategy(ConfigOverride::EnvFirst);
```

## 命令行参数

```cpp
// 解析命令行参数
config.parseArgs(argc, argv);

// 支持:
// --server.port=8888
// --server.host=0.0.0.0
// -p 8888
```

## 配置验证

```cpp
// 定义配置schema
config.defineSchema("server", {
    {"host", ConfigType::String, true},
    {"port", ConfigType::Int, true, [](const Variant& v) {
        return v.toInt() > 0 && v.toInt() < 65536;
    }}
});

// 验证配置
if (!config.validate()) {
    LOG_ERROR("Config", "配置验证失败");
}
```

## 下一步

查看 [架构文档](/architecture/overview.md) 了解更多。
