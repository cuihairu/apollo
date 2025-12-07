# 插件化与动态加载设计（存储/扩展模块）

> 目的：以 Spring Boot 的“组件/Starter”理念为参考，为 Apollo C++ 框架定义动态加载/插件化机制，支持 MySQL/MongoDB/Redis 等可插拔数据源以及其他扩展模块。

## 1. 设计目标
1. **按需加载**：不同进程仅加载所需插件，减少主程序体积与依赖。
2. **统一接口**：插件通过标准接口注册 Bean，业务逻辑无需关注具体实现。
3. **跨平台**：在 Windows（DLL）与 Linux/macOS（SO/DYLIB）上使用统一的加载封装。
4. **热插拔（可选）**：后续可支持运行时卸载/替换，但初期以启动期加载为主。
5. **安全/稳定**：限定导出接口，避免 ABI 不兼容；必要时提供签名/校验。

## 2. SharedLibrary 封装

```cpp
class SharedLibrary {
public:
    SharedLibrary() = default;
    ~SharedLibrary() { Unload(); }

    bool Load(const std::string& path);
    void* GetSymbol(const std::string& name) const;
    void Unload();

private:
#ifdef _WIN32
    HMODULE handle_ = nullptr;
#else
    void* handle_ = nullptr;
#endif
};
```

- Windows: `LoadLibraryA`, `GetProcAddress`, `FreeLibrary`, 并记录 `GetLastError()`。
- Linux/macOS: `dlopen`, `dlsym`, `dlclose`, 错误通过 `dlerror()`。
- 搭配 `std::filesystem` 实现插件目录扫描。

## 3. 插件接口约定

每个插件需导出一个标准 C 函数，供主程序调用。例如：

```cpp
extern "C" bool RegisterBeans(ApplicationContext& ctx, const PluginConfig& cfg);
```

- `PluginConfig` 由主程序解析配置后传入（包含运行环境、连接字符串等）。
- 插件在 `RegisterBeans` 里创建并注册自己的 Bean（例如 `MySQLDatabaseBean`、`RedisClientBean`）。
- 若需要自定义生命周期，可在 Bean 的 `OnInit/OnStart/OnStop` 内实现。

插件示例（MySQL）：

```cpp
extern "C" bool RegisterBeans(ApplicationContext& ctx, const PluginConfig& cfg) {
    auto mysqlBean = std::make_shared<MySQLDatabaseBean>(cfg.mysql);
    ctx.RegisterBean("MySQLDatabase", mysqlBean, /*deps=*/{"LoggerBean"});
    return true;
}
```

## 4. 启动流程
1. 解析配置文件/命令行，确定需要加载的插件列表（例如 `plugins/database/mysql_plugin.so`）。
2. 对每个插件路径调用 `SharedLibrary::Load`，查询 `RegisterBeans` 符号。
3. 调用 `RegisterBeans` 将 Bean 注册到 `ApplicationContext`。
4. 完成所有插件注册后，进入 `ApplicationContext::Startup()`（按依赖/优先级启动）。

可选：支持目录扫描和自动加载，以 `*.so`/`*.dll` 命名规则识别。

## 5. 数据源插件应用

| 插件 | 提供 Bean | 依赖 | 说明 |
|------|-----------|------|------|
| `mysql_plugin` | `MySQLDatabaseBean` | Logger, ConfigLoader | 内部链接 `libmysqlclient` |
| `redis_plugin` | `RedisClientBean` | Logger, ConfigLoader, Transport | 链接 `hiredis` |
| `mongo_plugin` | `MongoClientBean` | Logger, ConfigLoader | 链接 `mongocxx` |
| `clickhouse_plugin` (可选) | `ClickHouseClientBean` | Logger, ConfigLoader | 等 |

主程序只需面向 `IDatabase`/`ICache` 接口编程，插件将具体实现注入即可。

## 6. 安全/版本控制
- 插件需遵守约定的 ABI（例如只有纯虚接口或 C 函数），避免 STL/模板跨边界。
- 可选：在插件包中附带版本、签名信息，主程序校验后才加载。
- 记录插件加载日志，便于诊断。

## 7. Roadmap
1. 完成 `SharedLibrary` 封装与基本注册流程。
2. 抽象 `IDatabase`, `ICache`, `ISocialModule` 等接口。
3. 实现首批插件：MySQL、Redis、Mongo（根据项目优先级）。
4. 扩展自动扫描/热加载、插件配置热更新。
5. 引入安全/权限控制（签名、白名单）。

---

通过该机制，可以实现类似 Spring Boot Starter 的按需模块化，既满足“全部进程共享基础设施”的需求，也让不同服务能自由组合所需能力。***
