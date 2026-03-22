---
title: Capability 与 Feature Flag 设计
icon: toggle-on
order: 20
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Capability
  - FeatureFlag
  - Config
---

# Capability 与 Feature Flag 设计

这篇文档解决的是 Apollo 整体框架继续往工程化推进时，一个很容易被低估的问题：

`模块能力、功能开关、运行时开关，到底应该如何统一表达。`

如果这层不清楚，后面会很快出现这些问题：

- starter 选了模块，但 app 不知道模块具体提供什么能力
- profile 想切换功能，但没有稳定的能力层表达
- 运维开关和架构能力开关混在一起
- BigWorld 增强能力和普通 MMO 默认能力边界不清

## 一、设计目标

这层设计要解决 6 个问题：

1. 模块提供什么能力，如何被声明。
2. app 依赖的是模块名，还是能力名。
3. 功能开关和架构能力开关如何区分。
4. 运行时可动态变更的开关有哪些。
5. 哪些能力必须静态决定，哪些可以运行期调整。
6. 如何避免配置越长越乱。

## 二、参考来源

### 1. 参考 feature flag 思想

参考点：

- 通过开关控制行为
- 支持环境差异
- 支持灰度和运营治理

不照搬点：

- 不把所有架构决策都做成动态开关

### 2. 参考 capability 描述思路

参考点：

- 模块不只按名字依赖，还可以按能力声明
- 模块可替换时，能力比实现名更重要

### 3. 参考 MMO 架构实际边界

参考点：

- 有些能力必须启动前决定
- 有些能力适合运行期调整

## 三、为什么这样设计

Apollo 后面会同时面对：

- 多模块装配
- 多种脚本后端
- 多种平台后端
- 普通 MMO / BigWorld 模式切换
- ops 开关和调试开关

如果不把“能力”和“开关”拆开，最后会出现：

- 逻辑判断到处散
- 配置项互相覆盖不清
- 某些本应静态决定的东西被错误地做成运行时开关

更合理的方式是：

- `Capability`
  - 表达模块提供的稳定能力
- `Feature Flag`
  - 表达某个能力或某段行为是否启用

## 四、优点

- 模块装配语义更清楚
- 配置表达更稳定
- 动态开关边界更清楚
- 后端替换更容易
- 更适合 profile、starter、registry 协同工作

## 五、代价与风险

- 需要先定义能力词汇表
- 开关太多会导致治理复杂
- 如果能力粒度切得太碎，会适得其反

## 六、为什么不选其他方案

### 不选“只认模块名，不认能力”

因为这会让模块替换和后端切换变得困难。

### 不选“所有东西都做成 feature flag”

因为架构决策和运行时调优不是一回事。

### 不选“完全没有统一开关体系”

因为 Apollo 后面会有太多模式差异，迟早会失控。

## 七、Capability 的定义

`Capability` 表达的是：

- 某模块稳定提供的能力契约

例如：

- `script_backend`
- `world_host`
- `kv_store`
- `relational_store`
- `leaderboard_service`
- `distributed_world`

### 为什么能力要先于实现

因为很多时候 app 真正关心的是：

- 我需要一个脚本后端

而不是：

- 我一定要 `script.lua`

这给 Apollo 留出了：

- 实现替换
- profile 选择
- starter 默认组合

的空间。

## 八、Feature Flag 的定义

`Feature Flag` 表达的是：

- 某种行为、策略或功能是否开启

例如：

- `ops.console.enabled`
- `ops.otel.enabled`
- `script.hot_reload.enabled`
- `distributed_world.enabled`
- `battle.experimental_damage_pipeline`

### 为什么它和 capability 不同

因为 capability 回答的是：

- 有什么能力

feature flag 回答的是：

- 这个能力或这段行为现在要不要打开

## 九、推荐分类

建议 Apollo 把 flag 至少分成 4 类。

### 1. Architecture Flag

用于：

- 普通 MMO / BigWorld 模式切换
- script backend 选择
- distributed world 开关

### 2. Platform Flag

用于：

- Redis / SQL 后端开关
- leaderboard enable
- lock strategy

### 3. Ops Flag

用于：

- console
- otel
- health
- remote control

### 4. Business Flag

用于：

- 活动开关
- 实验性规则开关
- 某业务域功能灰度

## 十、哪些必须静态决定

下面这些建议在启动前决定，不应运行期随便切：

- 脚本后端
- 关键 platform driver
- distributed world 模式
- 核心 host 结构
- 核心模块启停

### 理由

这些值一旦变化，影响的是：

- 依赖图
- 宿主结构
- 生命周期对象

它们不是普通行为开关。

## 十一、哪些可以运行期调整

下面这些更适合做运行时 flag：

- log level
- trace sampling
- console enable
- watcher detail level
- 某些活动开关
- 某些调试开关

### 理由

这些变化不会改写核心依赖图和宿主结构。

## 十二、推荐表达方式

建议 Apollo 配置里显式拆两块：

```yaml
capabilities:
  script_backend: lua
  kv_store: redis
  relational_store: mysql

features:
  ops.console: true
  ops.otel: true
  script.hot_reload: false
  distributed_world.enabled: false
```

### 为什么不建议混成一块

因为：

- `script_backend = lua`
  是结构能力选择
- `script.hot_reload = false`
  是功能行为开关

这两个不是一个层级的问题。

## 十三、和 Manifest / Profile / Bootstrap 的关系

### `Manifest`

定义：

- 模块提供哪些 capability

### `Profile`

选择：

- 需要哪些 capability
- 默认 feature flag 怎么配

### `Bootstrap`

负责：

- 校验 capability 是否满足
- 应用 feature flag

## 十四、和 KBE 参考原则的关系

Apollo 参考 KBE 时，也要避免把“能力选择”和“实现形态”绑死。

例如：

- 保留 `Proxy / Base / Cell` 思路
- 但不意味着必须所有模式都启用 KBE 风格 distributed world

这正是 capability / feature flag 分离的意义。

## 十五、对当前 Apollo 的直接含义

Apollo 现在已经在文档里定义了很多能力，但还没有统一的：

- capability vocabulary
- feature flag taxonomy

下一步更现实的顺序应该是：

1. 先给核心模块补 capability
2. 先给 ops、script、distributed world 补 feature flag
3. 再让 profile 和 bootstrap 使用这套词汇表

## 十六、结论

Apollo 的整体框架如果要真正稳定下来，能力表达和功能开关必须拆开。

更合理的做法是：

- capability 描述“是什么”
- feature flag 描述“是否启用”

只有这样，starter、manifest、profile、bootstrap、ops 才会形成统一语言。

## 相关阅读

- [Configuration 与 Profile 设计](./configuration-and-profile-design.md)
- [Module Manifest 与 Registry 设计](./module-manifest-and-registry-design.md)
- [Apollo 分层设计](./apollo-layering-design.md)
