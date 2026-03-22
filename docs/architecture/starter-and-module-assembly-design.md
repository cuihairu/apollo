---
title: Starter 与模块装配设计
icon: plug
order: 13
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Starter
  - 装配
  - MonoRepo
---

# Starter 与模块装配设计

这篇文档解决的是 Apollo 整体框架继续往下走时，一个必须明确的问题：

`Apollo 应该如何像 Spring Boot 那样做模块化装配，但又不照搬 Java 那套运行时反射模型。`

这里先把结论说清楚：

- Apollo 很适合学 `Spring Boot` 的 starter 思想
- 但不适合照搬 Java 的运行时自动扫描和隐式注入

更合理的路线是：

- `mono-repo + 多模块 + starter 装配 + profile 配置 + 显式注册`

## 一、先说结论

Apollo 的装配层建议采用下面这套模型：

```text
Module
    -> Starter
    -> Profile
    -> App Bootstrap
    -> Topology
```

也就是说：

- `Module` 负责提供能力
- `Starter` 负责组合能力
- `Profile` 负责选择默认装配方案
- `App Bootstrap` 负责启动进程
- `Topology` 负责决定最终部署形态

这比“每个 app 手写一堆初始化代码”更稳，也比“搞一套重反射自动装配”更适合 C++。

## 二、为什么 Apollo 适合这种思路

### 1. Apollo 本身就是渐进式框架

你现在已经明确 Apollo 不是一套固定形态的 MMO 框架，而是：

- 轻在线项目可用
- 普通 MMO 可用
- BigWorld 模式可增量升级

这种目标天然要求：

- 能按项目类型装配

### 2. Apollo 已经不再是单一 world server

Apollo 后面会逐步包含：

- runtime
- ops
- platform
- script
- world
- session
- battle
- social
- activity
- distributed world

如果没有统一装配模型，最终会变成：

- 每个 app 都手写一份“我需要哪些模块”

这会很快失控。

### 3. C++ 更适合显式装配

和 Java 不同，C++ 不擅长：

- 大量运行时扫描
- 大量反射注入
- 大量隐式装配

但它非常适合：

- 显式依赖
- 编译期注册
- manifest
- factory
- starter 组合

## 三、Apollo 不该照搬 Spring Boot 的部分

这点需要先说死。

### 不建议照搬的

- 包扫描
- 反射式自动发现
- 重度运行时注入
- 每个组件都可独立动态热插拔

### 建议吸收的

- starter
- profile
- auto-config 思想
- 约定优于配置
- 默认装配 + 显式覆盖

Apollo 应该做的是：

- `C++ 风格的显式装配框架`

而不是：

- `C++ 版 Spring Boot 克隆`

## 四、核心概念

## 五、`Module`

`Module` 是最小能力单元。

它表示：

- 某个明确职责的代码模块

例如：

- `base`
- `core`
- `runtime`
- `runtime.ops`
- `platform.redis`
- `platform.sql`
- `game.session`
- `game.world`
- `game.battle`
- `script.lua`
- `script.python`

### `Module` 应具备的元信息

```text
ModuleManifest
    name
    version
    dependencies
    optional_dependencies
    capabilities
    default_config
```

### `Module` 的职责边界

- 提供能力
- 不负责最终装配
- 不直接决定部署拓扑

## 六、`Starter`

`Starter` 不是能力模块，而是装配模板。

它负责：

- 选择一组模块
- 提供默认配置
- 约定模块组合方式

例如可以有：

- `starter-core`
- `starter-gateway`
- `starter-login`
- `starter-world`
- `starter-mmo`
- `starter-bigworld`

### `Starter` 的价值

它解决的是：

- 使用者不需要每次从零拼装所有模块

### 例子

`starter-mmo` 可以默认装配：

- `runtime`
- `runtime.ops`
- `platform.redis`
- `platform.sql`
- `game.session`
- `game.world`
- `game.battle`
- `script.lua` 或 `script.python`

### `Starter` 不该做什么

- 不直接写业务逻辑
- 不直接承载 app 状态

## 七、`Profile`

`Profile` 是运行配置维度的装配方案。

它回答的不是“有哪些模块”，而是：

- 这些模块在什么模式下启用

例如：

- `dev`
- `test`
- `prod`
- `single-world`
- `mmo`
- `bigworld`

### `Profile` 负责什么

- 打开或关闭某些模块
- 选择脚本后端
- 选择存储后端
- 选择运维开关
- 选择分布式能力开关

### 例子

```yaml
profile: mmo

script:
  backend: lua

distributed_world:
  enabled: false

ops:
  otel: true
  console: true
```

## 八、`App Bootstrap`

这是每个可执行程序真正的启动入口。

它负责：

- 读取 profile
- 加载 starter
- 解析 module manifest
- 组装依赖图
- 启动宿主

### 推荐结构

```text
AppBootstrap
    ├── ConfigLoader
    ├── StarterResolver
    ├── ModuleRegistry
    ├── DependencyGraph
    └── HostBuilder
```

### 关键点

Apollo 最终 app 不应该再各自手写一堆初始化顺序，而应该收口到统一 bootstrap。

## 九、`Topology`

`Topology` 是部署与运行形态层。

它负责：

- 最终拆成哪些 app
- 每个 app 装哪些 starter
- 每个 app 跑哪些 profile

例如：

### 轻在线项目

- `gateway-app`
- `login-app`
- `game-app`
- `platform-app`

### 普通 MMO

- `gateway-app`
- `login-app`
- `base-app`
  目标语义是 `BaseApp(PlayerAnchor Host)`
- `world-app`
  当前仓库可先由 `apps/cell-app` 承担其装配壳
- `chat-app`
- `db-app / persistence-app`

### BigWorld 模式

- `gateway-app`
- `login-app`
- `base-app`
  目标语义是 `BaseApp(PlayerAnchor Host)`
- `base-app-mgr`
- `cell-app`
  这里才表示真正的 `CellApp(Distributed World Node)`
- `cell-app-mgr`
- `db-app / persistence-app`

## 十、推荐装配流程

建议 Apollo 的标准装配流程是：

1. 选择 `app`
2. 加载 `profile`
3. 解析 `starter`
4. 展开 `module dependencies`
5. 构建 `host builder`
6. 初始化 `runtime host`
7. 初始化 `ops host`
8. 初始化 `platform foundation`
9. 初始化 `game foundation`
10. 初始化 `domain components`

如果启用 BigWorld，再继续：

11. 初始化 `distributed world extension`

## 十一、推荐 manifest 结构

建议后续模块至少拥有下面这类 manifest：

```yaml
name: game.world
version: 0.1.0
dependencies:
  - runtime
  - runtime.ops
  - game.foundation
optional_dependencies:
  - script.lua
  - script.python
capabilities:
  - world_host
  - map_instance
  - aoi
```

### 为什么 manifest 必须显式

因为 Apollo 未来模块会越来越多。

如果没有显式 manifest，starter 最终会退化成：

- 一堆手工 if/else

## 十二、推荐 starter 分类

### 基础 starter

- `starter-core`
- `starter-runtime`
- `starter-ops`
- `starter-platform`

### 游戏 starter

- `starter-session`
- `starter-world`
- `starter-battle`
- `starter-social`

### 形态 starter

- `starter-online`
- `starter-mmo`
- `starter-bigworld`

### 后端 starter

- `starter-script-lua`
- `starter-script-python`
- `starter-platform-redis`
- `starter-platform-sql`

## 十三、和脚本后端的关系

这层也应该和统一脚本层设计对齐。

### 原则

- 一个项目通常只选择一种主业务脚本后端

所以 starter 层应支持：

- `starter-script-lua`
- `starter-script-python`

但不建议默认同时启用。

## 十四、和当前 Apollo 仓库的关系

Apollo 当前已经有：

- `modules/starter`
- 多个 `apps/*`

但还没有形成真正统一的 starter + bootstrap 体系。

当前更像：

- 原型 app 自己装自己

下一步更合理的是：

- 把 app 退化成薄装配壳
- 把 starter 和 bootstrap 升级成正式装配层

## 十五、结论

Apollo 如果继续往整体框架推进，最合理的装配路线就是：

- 一个 mono-repo
- 多个清晰模块
- starter 负责组合模块
- profile 负责选择模式
- bootstrap 负责统一启动

这就是最适合 C++ 的“Spring Boot 思想落地版”。

## 相关阅读

- [Apollo 分层设计](./apollo-layering-design.md)
- [Apollo 渐进式游戏框架理论设计](./apollo-progressive-game-framework.md)
- [统一脚本层设计](./script-layer-design.md)
