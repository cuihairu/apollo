# Apollo ApplicationContext 2.0 设计

> 目标：将现有 `ApolloApplication` 的 Starter 装配能力，与通用的 Bean 生命周期、配置、事件、任务调度、管理端点统一起来，形成一套所有服务进程可复用的运行时基础设施。

## 1. 设计范围

ApplicationContext 2.0 负责以下职责：

1. 管理 Bean 注册、依赖解析、生命周期推进与逆序关闭。
2. 提供统一的配置视图，接入文件、环境变量、命令行覆盖。
3. 提供同步/异步事件总线，承载运行时状态传播。
4. 提供统一调度器，管理延迟任务、周期任务、Cron 任务。
5. 提供管理端点挂载点，暴露健康检查、配置、指标和运维命令。
6. 与 `Apollo::Starter::ApolloApplication` 配合，由 Starter 完成模块装配，由 ApplicationContext 完成运行态管理。

不在本阶段范围内：

- 反射式自动注入。
- ORM/JPA 风格数据访问抽象。
- 分布式配置中心一致性协议。
- 热卸载插件。

## 2. 运行时分层

```text
Bootstrap / main
  -> ApolloApplication
     -> StarterRegistry / Condition evaluation
     -> register beans into ApplicationContext
        -> ConfigEnvironment
        -> LifecycleProcessor
        -> EventBus
        -> Scheduler
        -> ManagementEndpointRegistry
        -> Business Beans
```

分工原则：

- `ApolloApplication` 负责启动期选择哪些 Starter 生效。
- `ApplicationContext` 负责生效后这些 Bean 如何构造、排序、启动、停止。
- Starter 输出的是“注册动作”，不是“直接启动逻辑”。

## 3. 核心对象模型

### 3.1 BeanDefinition

```cpp
enum class BeanScope {
    Singleton,
    Prototype
};

enum class LifecyclePhase : int {
    Bootstrap = -2000,
    Infrastructure = -1000,
    Config = -500,
    Data = 0,
    CoreService = 500,
    Business = 1000,
    Gateway = 2000
};

struct BeanDefinition {
    std::string name;
    BeanScope scope = BeanScope::Singleton;
    LifecyclePhase phase = LifecyclePhase::CoreService;
    bool autoStart = true;
    bool lazyInit = false;
    std::vector<std::string> dependsOn;
    std::function<std::shared_ptr<void>(class ApplicationContext&)> factory;
    std::type_index serviceType = typeid(void);
    std::unordered_map<std::string, std::string> metadata;
};
```

约束：

- Bean 名称全局唯一。
- `dependsOn` 必须显式声明基础设施依赖，禁止依赖“注册顺序”。
- `Prototype` Bean 不参与统一生命周期，只参与按需构造。

### 3.2 Runtime Bean 接口

```cpp
enum class StartMode {
    Auto,
    Manual
};

enum class ShutdownMode {
    Graceful,
    Immediate
};

class IBean {
public:
    virtual ~IBean() = default;
    virtual std::string_view name() const = 0;
};

class ILifecycleBean : public IBean {
public:
    virtual bool configure() { return true; }
    virtual bool init() { return true; }
    virtual bool start(StartMode) { return true; }
    virtual bool ready() { return true; }
    virtual void stop(ShutdownMode) {}
    virtual void destroy() {}
};

class IReloadableBean : public IBean {
public:
    virtual bool reload(const std::vector<std::string>& changedKeys) = 0;
};

class IHealthContributor : public IBean {
public:
    virtual HealthReport checkHealth() const = 0;
};
```

说明：

- `configure -> init -> start -> ready` 是正向阶段。
- `stop -> destroy` 是逆向阶段。
- 只有实现 `ILifecycleBean` 的 Bean 才参与生命周期推进。

## 4. 生命周期状态机

```text
Registered
  -> Constructed
  -> Configured
  -> Initialized
  -> Started
  -> Ready
  -> Stopping
  -> Destroyed
```

失败语义：

1. `configure/init/start/ready` 任一阶段失败，启动立即终止。
2. 已进入成功状态的 Bean 需要按已完成顺序执行补偿关闭。
3. `Gateway` 类 Bean 必须最后 `start`、最先 `stop`，避免对外暴露半初始化服务。
4. `stop()` 不应抛异常；错误转入日志和诊断事件。

## 5. 启动流程

### 5.1 Bootstrap 顺序

1. 解析命令行参数。
2. 建立 `ConfigEnvironment`。
3. `ApolloApplication` 选择 Starter。
4. Starter 向 `ApplicationContext` 注册 BeanDefinition。
5. ApplicationContext 构建依赖图并做拓扑排序。
6. 执行 `configure -> init -> start -> ready`。
7. 启动管理端点并发布 `ApplicationReadyEvent`。

### 5.2 关闭流程

1. 发布 `ShutdownRequestedEvent`。
2. 停止接入层 Bean。
3. 停止业务层与后台任务。
4. 刷盘/回收事件总线与调度器。
5. 销毁资源 Bean。

## 6. 依赖解析与排序

排序规则：

1. 先按显式依赖构建 DAG。
2. 在无依赖冲突时按 `phase` 升序。
3. 同一 phase 内按 `name` 做稳定排序，保证结果可预测。

冲突判定：

- 存在环依赖时启动失败，并输出环路径。
- 依赖缺失时启动失败，并列出缺失 Bean 名称。
- `lazyInit=true` 的 Bean 不能被 `autoStart=true` Bean 显式依赖。

伪代码：

```cpp
std::vector<BeanDefinition*> LifecycleProcessor::sort(
    std::span<BeanDefinition> defs) {
    // Kahn + stable priority queue(phase, name)
}
```

## 7. 配置环境模型

### 7.1 配置来源优先级

```text
defaults
  < config/base
  < config/{env}
  < config/override
  < environment variables
  < command line args
  < in-memory overrides
```

统一接口：

```cpp
class IConfigEnvironment {
public:
    virtual ~IConfigEnvironment() = default;
    virtual bool has(std::string_view key) const = 0;
    virtual std::string getString(std::string_view key,
                                  std::string_view fallback = "") const = 0;
    virtual int64_t getInt(std::string_view key, int64_t fallback = 0) const = 0;
    virtual bool getBool(std::string_view key, bool fallback = false) const = 0;
    virtual std::vector<std::string> getList(std::string_view key) const = 0;
    virtual std::vector<ConfigValueOrigin> explain(std::string_view key) const = 0;
};
```

要求：

- 每个配置值必须可追踪来源，便于管理端点展示“这个值从哪里来的”。
- 配置热更新只允许作用于标记为可热更的 key。

## 8. CLI 组件

标准参数建议：

- `--config=path`
- `--env=dev|test|prod`
- `--set key=value`
- `--server.id=...`
- `--log.level=...`
- `--management.port=...`
- `--spring.profile=` 不建议保留该命名，统一使用 `--env`

接口：

```cpp
struct CommandLineOptions {
    std::string configPath;
    std::string environment = "dev";
    std::unordered_map<std::string, std::string> overrides;
};

class ICommandLineParser {
public:
    virtual CommandLineOptions parse(int argc, char** argv) = 0;
};
```

## 9. 事件总线设计

### 9.1 事件分类

- 生命周期事件：`ApplicationStartingEvent`、`ApplicationReadyEvent`、`ShutdownRequestedEvent`
- 配置事件：`ConfigReloadedEvent`
- 运行事件：`ServiceRegisteredEvent`、`TransportReadyEvent`
- 故障事件：`BeanStartFailedEvent`、`TaskExecutionFailedEvent`

### 9.2 接口

```cpp
struct EventEnvelope {
    std::string topic;
    std::chrono::system_clock::time_point timestamp;
    std::string sourceBean;
    std::any payload;
};

enum class DispatchMode {
    Sync,
    Async
};

class IEventBus {
public:
    virtual ~IEventBus() = default;
    virtual SubscriptionId subscribe(std::string topic,
                                     DispatchMode mode,
                                     EventHandler handler) = 0;
    virtual void unsubscribe(SubscriptionId id) = 0;
    virtual void publish(EventEnvelope event) = 0;
};
```

设计要求：

- 默认同 topic 内按发布顺序投递。
- 异步订阅需要线程池隔离，防止慢订阅者阻塞主路径。
- 事件总线不得作为 RPC 替代品，只用于状态传播和解耦通知。

## 10. 调度器设计

统一调度器承载三类任务：

1. `scheduleOnce(delay, fn)`
2. `scheduleAtFixedRate(interval, fn)`
3. `scheduleCron(expr, timezone, fn)`

接口：

```cpp
using TaskId = uint64_t;

class IScheduler {
public:
    virtual ~IScheduler() = default;
    virtual TaskId scheduleOnce(std::chrono::milliseconds delay, TaskFn fn) = 0;
    virtual TaskId scheduleAtFixedRate(std::chrono::milliseconds interval, TaskFn fn) = 0;
    virtual TaskId scheduleCron(std::string expr,
                                std::string timezone,
                                TaskFn fn) = 0;
    virtual bool cancel(TaskId id) = 0;
};
```

策略：

- `scheduleAtFixedRate` 适合心跳、指标采集。
- `scheduleCron` 只用于管理和运维任务，不进入高频业务 Tick。
- 同一个任务连续失败达到阈值时，发布 `TaskExecutionFailedEvent`。

## 11. 管理端点

管理端点不直接依赖业务服务，而是依赖 `ApplicationContext` 暴露的标准接口。

建议最小集：

- `/health`：整体健康度、关键 Bean 状态。
- `/metrics`：Prometheus 文本格式。
- `/config`：配置视图和来源链路。
- `/beans`：Bean 列表、phase、依赖、状态。
- `/events/recent`：最近若干关键事件。
- `/scheduler`：任务列表、下次触发时间、失败次数。

注册模型：

```cpp
class IManagementEndpoint {
public:
    virtual ~IManagementEndpoint() = default;
    virtual std::string_view path() const = 0;
    virtual EndpointResponse handle(const EndpointRequest&) = 0;
};
```

## 12. 与 ApolloApplication 的衔接

当前 `ApolloApplication` 已具备：

- Starter 条件匹配。
- 配置文件读取。
- 启动/停止入口。

建议演进为：

1. `ApolloApplication::Builder` 先构造 `CommandLineOptions` 和配置覆盖。
2. `ApolloApplication::initialize()` 中不再直接执行 Starter 逻辑，而是让 Starter 注册 `BeanDefinition`。
3. `ApolloApplication::start()` 委托 `ApplicationContext.start()`
4. `ApolloApplication::stop()` 委托 `ApplicationContext.stop()`

接口草案：

```cpp
class ApolloStarter {
public:
    virtual ~ApolloStarter() = default;
    virtual void registerBeans(ApplicationContext& ctx) = 0;
};
```

这样可以消除“Starter 已初始化，但 Bean 生命周期尚未统一管理”的灰区。

## 13. 与现有代码的映射

当前代码现状：

- [ApolloApplication.h](/Users/cui/Workspaces/apollo/include/apollo/starter/ApolloApplication.h)
  已有 Builder、配置注入、Starter 过滤，但缺少真正的上下文管理。
- [ApolloApplication.cpp](/Users/cui/Workspaces/apollo/src/starter/ApolloApplication.cpp)
  已做配置加载和 Starter 启动，但 `Fruit` 组合与统一 Bean 生命周期未完成。
- [ApplicationContext.h](/Users/cui/Workspaces/apollo/include/apollo/framework/ioc/ApplicationContext.h)
  已有基础 IoC 雏形，但未形成 phase/事件/调度/管理端点的统一模型。

推荐拆分实现顺序：

1. `ConfigEnvironment`
2. `BeanDefinition + LifecycleProcessor`
3. `EventBus`
4. `Scheduler`
5. `ManagementEndpointRegistry`
6. `ApolloApplication` 改为基于 `ApplicationContext` 驱动

## 14. 最小可交付版本

MVP 范围：

- 只支持 `Singleton` Bean。
- 只支持显式注册，不做自动扫描。
- 支持 `configure/init/start/stop/destroy`。
- 支持同步事件 + 基础异步线程池分发。
- 支持 `scheduleOnce` 和 `scheduleAtFixedRate`。
- 提供 `/health`、`/metrics`、`/beans` 三个管理端点。

先不做：

- Prototype Bean。
- Cron 表达式热更新。
- 分布式事件总线。
- 复杂 DI 自动装配。

## 15. 风险与约束

- 如果继续让 Starter 自己维护内部生命周期，最终会出现两套启动模型并存。
- 事件总线若无隔离策略，容易被日志、监控等慢消费者拖垮。
- 调度器若进入高频游戏逻辑主循环，会和场景 Tick 职责冲突，必须边界清晰。
- 管理端点默认只监听内网或本地回环地址，鉴权在 API/运维接入层处理。

---

该设计文档将 `docs/14` 中的建议收口为可编码的运行时模型，后续可直接据此拆分 `ApplicationContext`、`ConfigEnvironment`、`EventBus`、`Scheduler` 四个基础模块。***
