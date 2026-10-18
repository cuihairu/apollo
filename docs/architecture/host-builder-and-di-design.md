---
title: 宿主与依赖注入
---

# 宿主与依赖注入

本文使用当前代码中的类型说明应用装配，不额外定义同义容器或宿主。业务进程边界见[当前架构决策](./adr.md)。

## 当前部件

| 部件 | 所有权与职责 |
|---|---|
| `ApplicationContextBuilder` | 启动期注册类型、构造依赖、名称及作用域 |
| `ApplicationContext` | 创建和持有注入对象，按依赖顺序初始化、逆序释放 |
| `ApplicationHost` | 托管应用服务，处理生命周期、停止原因及关闭钩子 |
| `ServiceHost` | 应用宿主的便捷封装 |
| `WorldHost` | 固定 tick 驱动 World 服务 |
| `IHostedService` | 服务的 start / stop / is_running / tick 接口 |

依赖注入实现位于 `modules/core`，运行宿主位于 `modules/runtime`。依赖对象的创建不等于业务服务已经启动；容器与宿主的生命周期必须由应用入口明确协调。

## 注册与解析

`ApplicationContextBuilder` 支持 Singleton 和 Prototype。构造依赖使用类型键，Singleton 初始化按依赖图排序。`get<T>()` 和 `get_named<T>()` 提供解析，但不应在业务热路径和 Lua 中把容器当成任意服务定位器。

```cpp
apollo::core::di::ApplicationContextBuilder builder;
builder.add_singleton<GameClockService>().name("game_clock");
builder.add_singleton<LoginPipeline, GameClockService>().name("login_pipeline");
auto context = builder.build();
if (!context.initialize()) return 1;
```

该片段表达注册方式，示例类型由应用提供。当前实现对缺失、歧义及环依赖存在断言校验；不能宣称已经提供完善的生产错误聚合、配置验证或所有应用的统一装配。

## 启停边界

应用入口负责加载自身配置、构造共享服务、注册宿主、初始化日志与运行时设施，并定义停止顺序。`ApplicationHost` 负责其已注册服务的生命周期；没有注册的服务仍由所属应用管理。

WorldApp 使用 WorldHost 驱动模拟，LoginServer 管理 OnlineDirectory 和准入链路；长期资料及存储组件须分别接入 baseServer 和 database 的独立入口。它们不是已经完成全部容器化的程序，也不需要为统一词汇再增加 HostBuilder、ServiceCollection 等未实现类型。

停止时先关闭准入和受理，再完成已接受工作、结算/持久写入及资源释放，最后关闭依赖设施。超时强停和活动副本恢复须按业务验收，不能从析构顺序推导成功恢复。

## 脚本与更新

使用现有 Lua ScriptRuntime 和启动期具名服务注入。脚本通过受控接口访问功能，模拟变更在所属逻辑线程执行。模块热替换不是运行期修改 C++ 容器图；C++ 变更通过进程替换发布。

两种更新方案及其能力边界见[架构总览](../guide/architecture.md)。

## 关联文档

- [模块装配](./starter-and-module-assembly-design.md)
- [运行时模块](../modules/runtime.md)
- [核心模块](../modules/core.md)
