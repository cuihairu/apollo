---
title: 配置
icon: settings
order: 6
prev: /guide/module-system
---

# 配置

> 2026-10-04 对账：本页按 HEAD 实况重写。旧稿的 `load()`/`loadFromEnv()`/`get<T>()`/`onChange()`/`setOverrideStrategy()`/`parseArgs()`/`defineSchema()`/`validate()` 均不存在；
> YAML/TOML 不支持。环境变量覆盖、命令行统一解析、schema 校验均未实现。

配置分三层，各有分工：

| 层 | 位置 | 能力 | 现状 |
| --- | --- | --- | --- |
| `ConfigManager`（legacy） | `modules/core/config/` | 文件加载、层级访问、监听、热重载轮询 | 已实现；仓库应用未接线，仅测试覆盖 |
| `ConfigRegistry` | `modules/core` | 进程内键值表（string/int64/bool） | 已实现；无文件 I/O |
| 应用自读 | `apps/*` | CLI 参数解析、策划表加载 | 各 app 自管，无统一入口 |

## ConfigManager（文件配置）

`apollo::core::config::ConfigManager` 单例（`include/apollo/core/config/config_manager.h`）。
格式支持 INI、JSON、XML、Lua(table) 四种，按扩展名自动检测（`.ini`/`.cfg`/`.json`/`.xml`/`.lua`）。
JSON 解析优先走 nlohmann_json（构建可用时），否则退回内置简化解析。
配置按 section 隔离（默认 `"default"`），内部 `shared_mutex` 保护，读多写少场景线程安全。

```cpp
#include <apollo/core/config/config_manager.hpp>

auto& config = apollo::core::config::ConfigManager::instance();

// 加载（format 省略时按扩展名检测）
config.loadFile("config.json");

// 层级访问，键用点号分隔；getter 均带默认值参数
int64_t port = config.getInt64("server.port", 8888);
std::string host = config.getString("server.host", "0.0.0.0");
bool verbose = config.getBool("log.verbose", false);
std::vector<std::string> nodes = config.getArray("cluster.nodes");

// 泛型版
auto timeout = config.getValue<int>("server.timeout", 30);

// 写入与回存
config.setValue("server.port", static_cast<int64_t>(9999));
config.saveFile("config.json");
```

### 变更监听

```cpp
size_t id = config.addListener("server.port", [](const std::string& key,
                                                const apollo::core::config::ConfigNode& value) {
    // 键传空串监听全部变更
});
config.removeListener(id);
```

### 热重载

`enableHotReload(enable, checkIntervalMs)` 打开文件变化轮询，但检查发生在 `update()` 里——
宿主必须周期性调用它（时间轮或主循环均可）。`reload(section)` 可手动强制重载。

```cpp
config.enableHotReload(true, 1000);

// 主循环里：
config.update();
```

仓库内的应用（apps/）目前没有接 `update()` 循环，热重载属于库能力、未在运行链路生效。
游戏侧热更规划走 tick 边界换 `ConfigSnapshot`（architecture-review §17.6），未实现。

### 便捷宏

```cpp
// 等价 ConfigManager::instance().getInt/getStr/getBool/getDouble
int port = APOLLO_CONFIG_INT("server.port", 8888);
```

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

没有统一的命令行解析层，各 app 自管。zone-app 的做法是入口处解析进 `BaseConfig` 结构
（`apps/zone-app/src/main.cpp`）：

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

统一收口列入 P3-1（配置统一，ConfigManager 收口或删除）。

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
