---
title: Module Manifest 与 Registry 设计
icon: boxes-stacked
order: 16
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Module
  - Manifest
  - Registry
---

# Module Manifest 与 Registry 设计

这篇文档解决的是 Apollo 装配体系往下落时，一个必须先定清楚的问题：

`模块到底如何被描述、如何被发现、如何被解析依赖。`

如果这层不先立住，后面的：

- starter
- profile
- bootstrap
- app 装配

最终都会退化成：

- 一堆硬编码初始化顺序
- 一堆 if/else
- 一堆模块间隐式耦合

## 一、设计目标

这层设计要解决 5 个问题：

1. 一个模块如何声明自己是谁。
2. 一个模块依赖哪些其他模块。
3. 一个 starter 如何找到并组合模块。
4. 一个 app 如何验证依赖图是否完整。
5. 一个 profile 如何覆盖默认配置。

## 二、参考来源

这层参考了两类思路，但都不会直接照搬：

### 1. 参考 Spring Boot / starter 思路

参考点：

- starter 组合模块
- metadata / manifest 描述能力
- profile 决定启用方案

不照搬点：

- 不做运行时反射扫描
- 不做隐式 Bean 自动注入

### 2. 参考 KBE 的显式组件边界

参考点：

- 宿主和组件边界需要明确
- 不能让运行时对象关系靠“猜”

不照搬点：

- 不把所有路由和对象关系都绑到重实体模型

## 三、为什么这样设计

Apollo 是：

- mono-repo
- 多模块
- 多种游戏形态装配

所以模块描述必须显式化。

如果没有 manifest 和 registry，starter 迟早会变成：

- “这个模块大概需要那些模块”

这种不可维护的状态。

更合理的方式是：

- 模块自己声明元信息
- registry 统一收集和校验
- bootstrap 按 registry 建依赖图

## 四、优点

- 模块边界清晰
- starter 和 profile 可组合
- app 装配顺序可验证
- 文档和代码更容易对齐
- 更适合 C++ 的显式注册风格

## 五、代价与风险

- 需要先定义 manifest 规范
- 模块初期写法会比“直接 new”更繁琐
- 如果 manifest 过度复杂，会拖慢装配层

## 六、为什么不选其他方案

### 不选“完全手工装配”

因为模块越来越多后，会非常快失控。

### 不选“Java 式自动扫描”

因为 C++ 的反射和运行时元数据能力不适合走这条路。

### 不选“全动态插件注册”

因为这会把问题推向：

- ABI 复杂
- 调试困难
- 部署复杂

Apollo 更适合：

- manifest + registry + 显式注册

## 七、核心概念

## 八、`ModuleManifest`

`ModuleManifest` 是模块的静态描述对象。

建议至少包含：

```text
ModuleManifest
    name
    version
    layer
    dependencies
    optional_dependencies
    capabilities
    conflicts
    default_config_keys
    startup_phase
```

### 字段说明

#### `name`

例如：

- `runtime`
- `runtime.ops`
- `platform.redis`
- `game.world`
- `script.lua`

#### `layer`

用于声明这个模块属于哪一层。

例如：

- `L4 Runtime Host`
- `L6 Platform Foundation`
- `L8 Domain Components`

#### `dependencies`

必须存在的依赖。

#### `optional_dependencies`

可选依赖。

例如：

- `game.world` 可选依赖 `script.lua`
- `game.world` 可选依赖 `script.python`

#### `capabilities`

模块提供什么能力。

例如：

- `world_host`
- `leaderboard`
- `script_backend_lua`

#### `conflicts`

用于声明互斥模块。

例如：

- `script.lua`
- `script.python`

在同一业务项目中通常应互斥。

#### `startup_phase`

用于帮助 bootstrap 做初始化顺序控制。

## 九、`ModuleRegistry`

`ModuleRegistry` 是运行时看到的模块清单。

它负责：

- 注册 manifest
- 按名字查模块
- 按 capability 查模块
- 校验重复注册
- 校验冲突和缺失依赖

推荐结构：

```text
ModuleRegistry
    ├── registerManifest()
    ├── findByName()
    ├── findByCapability()
    ├── validateDependencies()
    └── buildDependencyGraph()
```

## 十、推荐注册模型

Apollo 更适合：

- 编译期显式注册
- 启动期统一汇总

而不是：

- 启动时全目录扫描

### 推荐方式

每个模块提供一个静态 manifest 导出入口。

逻辑上可以接近：

```cpp
const ModuleManifest& getRuntimeModuleManifest();
const ModuleManifest& getPlatformRedisManifest();
```

然后由 starter 或 bootstrap 显式收集。

## 十一、依赖图构建

建议 bootstrap 在真正初始化前，先构建依赖图。

### 标准流程

1. 收集 starter 选中的模块
2. 加入 profile 强制启用模块
3. 展开依赖
4. 检查冲突
5. 检查缺失模块
6. 生成拓扑排序
7. 按排序初始化

### 为什么必须做拓扑排序

因为 Apollo 后续模块会跨很多层：

- runtime
- ops
- platform
- game foundation
- domain

如果没有显式依赖图，很容易顺序错乱。

## 十二、推荐 capability 模型

除了按名字依赖，Apollo 还应该支持按能力查询。

例如：

- 需要一个 `script_backend`
- 需要一个 `kv_store`
- 需要一个 `leaderboard_service`

这样有利于：

- 后端替换
- 平台组件替换
- profile 切换

### 注意

按 capability 解析时，必须保证：

- 不模糊
- 不隐式选错模块

如果有多个候选，应该要求：

- starter 或 profile 显式指定

## 十三、推荐冲突规则

Apollo 需要明确支持模块冲突声明。

典型例子：

- `script.lua` 和 `script.python`
- 两种不同 `RelationalStore` 主实现

### 处理规则

1. profile 可显式指定优先模块
2. 若仍冲突，bootstrap 启动失败
3. 错误信息必须明确指出冲突来源

## 十四、推荐 manifest 示例

```yaml
name: game.world
version: 0.1.0
layer: L8
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
conflicts: []
startup_phase: domain
```

## 十五、与 starter / profile / bootstrap 的关系

### `Starter`

starter 决定：

- 默认装哪些模块

### `Profile`

profile 决定：

- 最终启哪些模块
- 覆盖哪些配置

### `Bootstrap`

bootstrap 负责：

- 用 registry 把这些决策变成真正可初始化的依赖图

## 十六、对当前 Apollo 的直接含义

Apollo 当前还没有正式的 `ModuleManifest + ModuleRegistry` 体系。

所以当前最现实的推进顺序应该是：

1. 先定义 manifest 结构
2. 先给核心模块补 manifest
3. 再让 starter 依赖 manifest，而不是手工列模块

建议优先补 manifest 的模块：

- `runtime`
- `runtime.ops`
- `platform.redis`
- `platform.sql`
- `platform.repository`
- `game.foundation`
- `game.session`
- `game.world`
- `script.lua`
- `script.python`

## 十七、结论

Apollo 的 starter 体系要真正落地，前提不是先写更多 starter，而是先把：

- `ModuleManifest`
- `ModuleRegistry`
- `DependencyGraph`

这三层立住。

这层一旦收住，后面的 starter、profile、bootstrap 才会真正稳。

## 相关阅读

- [Starter 与模块装配设计](./starter-and-module-assembly-design.md)
- [模块目录重组设计](./module-reorganization-design.md)
- [Apollo 分层设计](./apollo-layering-design.md)
- [Configuration 与 Profile 设计](./configuration-and-profile-design.md)
