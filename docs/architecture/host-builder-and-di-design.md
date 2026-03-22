---
title: Host Builder 与 DI 设计
icon: sitemap
order: 18
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - HostBuilder
  - DI
  - Bootstrap
---

# Host Builder 与 DI 设计

这篇文档解决的是 Apollo 装配体系继续往下落时，一个非常核心的问题：

`starter、profile、manifest、bootstrap 都有了之后，模块和对象到底如何真正组起来。`

如果这层不定义，前面的装配设计最后还是会退化成：

- main 里手工 new 一堆对象
- 模块初始化顺序写死
- 依赖传递靠构造函数层层向下塞

这会让 Apollo 很难真正成为一套可组装框架。

## 一、设计目标

这层设计要解决 6 个问题：

1. app 启动时对象如何统一构建。
2. 模块如何注册服务和工厂。
3. 宿主如何组装 runtime / ops / platform / game / domain。
4. 模块之间如何拿到依赖。
5. 生命周期对象如何托管。
6. 如何避免 C++ 下过度复杂的 IoC 容器。

## 二、参考来源

### 1. 参考 Host Builder 思想

参考点：

- 启动时先构建宿主
- 宿主再承载服务和模块
- 配置、日志、生命周期先于业务域

### 2. 参考依赖注入思想

参考点：

- 模块不直接到处 new 依赖
- 依赖通过容器和构建器注入

不照搬点：

- 不做 Java 式运行时反射注入
- 不做大规模隐式自动装配

### 3. 参考 KBE 的宿主边界

参考点：

- 登录、连接、world、script、entity 这些能力必须挂在明确宿主上

## 三、为什么这样设计

Apollo 是 C++ 框架，不适合做：

- 重反射 IoC
- 重注解容器
- 模糊的运行时依赖注入

但它非常适合做：

- 显式 Host Builder
- 轻量 DI 容器
- 生命周期托管
- 模块注册工厂

也就是说，Apollo 更合理的路线不是：

- “做一个 Spring Container”

而是：

- “做一个 C++ 风格的显式宿主构建器”

## 四、优点

- 依赖关系更清楚
- 启动过程更可验证
- 模块装配更稳定
- app `main` 可以明显变薄
- 更适合和 manifest / starter / bootstrap 配合

## 五、代价与风险

- 初期要先设计容器和注册 API
- 需要约束模块写法，不能继续随意 new
- 如果做得太重，会把 C++ 项目变成难以维护的“伪 Java IoC”

## 六、为什么不选其他方案

### 不选“完全手工构造”

因为模块和 app 增长后会迅速失控。

### 不选“重型 IoC 容器”

因为 C++ 不适合这条路，排错和可维护性都会恶化。

### 不选“所有模块自己维护局部单例”

因为这样会让依赖边界越来越隐蔽。

Apollo 更适合：

- 显式 builder
- 轻量服务注册
- 生命周期托管

## 七、核心概念

## 八、`HostBuilder`

`HostBuilder` 是整个启动装配的核心对象。

它负责：

- 创建宿主
- 装配配置、日志、事件总线
- 加载模块注册
- 构建服务容器
- 推进生命周期阶段

建议结构：

```text
HostBuilder
    ├── BootstrapContext
    ├── ServiceCollection
    ├── ModuleRegistry
    ├── HostParts
    └── BuildResult
```

### 为什么要有 `HostBuilder`

因为 Apollo 后续所有 app 最终都应该走统一构建路径，而不是每个 app 各玩各的。

## 九、`ServiceCollection`

这层相当于 Apollo 的轻量注册容器。

它负责：

- 注册 singleton
- 注册 scoped/app-lifetime service
- 注册 factory
- 注册接口到实现

但它不应承担：

- 复杂运行时反射注入
- 模糊的自动解析魔法

### 推荐支持的注册方式

```text
addSingleton<T>()
addSingleton<TInterface, TImpl>()
addFactory<T>()
addInstance<T>()
```

### 推荐原则

- 显式优先
- 生命周期清楚
- 类型安全优先

## 十、`ServiceProvider`

`ServiceProvider` 是构建完成后的只读容器。

它负责：

- 解析服务
- 提供依赖查询
- 维持生命周期对象

### 为什么要区分 `Collection` 和 `Provider`

因为：

- 注册阶段和解析阶段应该分离

这样更容易做：

- 启动校验
- 重复注册检查
- 依赖缺失检查

## 十一、推荐生命周期模型

Apollo 这里建议只支持少量明确生命周期。

### 1. `Singleton`

适合：

- config manager
- logger
- telemetry bridge
- module registry

### 2. `HostScoped`

适合：

- world host
- runtime ops host
- script runtime host

### 3. `Factory`

适合：

- domain object creator
- entity builder
- repository builder

### 为什么不建议搞更多复杂 scope

因为 Apollo 当前重点是进程级装配，不是 Web 请求级别生命周期。

## 十二、模块如何接入 DI

建议每个模块提供一个显式注册入口。

逻辑上接近：

```cpp
void registerRuntimeServices(ServiceCollection& services);
void registerPlatformRedisServices(ServiceCollection& services);
void registerWorldServices(ServiceCollection& services);
```

### 这样设计的理由

- 模块边界清楚
- 不需要运行时扫描
- 更适合 C++ 构建和调试

## 十三、推荐构建顺序

建议 `HostBuilder` 至少按下面顺序组装：

1. core services
2. runtime host
3. runtime ops host
4. platform foundation
5. game foundation
6. domain components
7. distributed world extension（如果启用）

### 为什么这样排

因为：

- 运维和观测应先于业务域
- 平台层应先于游戏域
- 游戏基础应先于业务组件

## 十四、推荐对象模型

```text
HostBuilder
    ├── buildCore()
    ├── buildRuntime()
    ├── buildOps()
    ├── buildPlatform()
    ├── buildGameFoundation()
    ├── buildDomain()
    └── buildDistributed()
```

### `BuildResult`

建议包含：

```text
BuildResult
    service_provider
    application_host
    runtime_host
    runtime_ops_host
    initialized_modules
```

## 十五、和 starter / manifest / bootstrap 的关系

### `Manifest`

告诉 builder：

- 有哪些模块
- 依赖图是什么

### `Starter`

告诉 builder：

- 默认选哪些模块

### `Bootstrap`

驱动 builder：

- 在正确阶段构建宿主和服务

所以 `HostBuilder` 实际上是：

- starter / manifest / bootstrap 的执行中枢

## 十六、和脚本层的关系

脚本后端也应通过同样的注册方式接入。

例如：

- `registerLuaScriptServices`
- `registerPythonScriptServices`

### 原则

- 同一项目通常只启用一个主脚本后端
- builder 负责根据 profile 和冲突规则选择后端

## 十七、对当前 Apollo 的直接含义

Apollo 当前已经有一些 host 和 lifecycle 基础，但还没有正式的：

- `HostBuilder`
- `ServiceCollection`
- `ServiceProvider`

下一步更现实的推进顺序应该是：

1. 先定义最小服务注册 API
2. 先让 runtime / ops / platform 模块接入
3. 再让 app `main.cpp` 逐步变薄

## 十八、结论

Apollo 如果要真正落到“可装配框架”，`HostBuilder + 轻量 DI` 是不可缺的一层。

它不是为了模仿 Java，而是为了让：

- starter
- profile
- manifest
- bootstrap

这些设计最终能真正执行起来。

## 相关阅读

- [Starter 与模块装配设计](./starter-and-module-assembly-design.md)
- [Module Manifest 与 Registry 设计](./module-manifest-and-registry-design.md)
- [App Bootstrap 生命周期设计](./app-bootstrap-lifecycle-design.md)
