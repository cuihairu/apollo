---
title: Configuration 与 Profile 设计
icon: sliders
order: 19
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Config
  - Profile
  - Override
---

# Configuration 与 Profile 设计

这篇文档解决的是 Apollo 整体框架继续往工程化落地时，另一个必须明确的问题：

`配置、profile、环境覆盖、模块开关到底该怎么统一。`

如果这层不单独设计，最后通常会变成：

- 每个 app 自己读自己的配置
- starter 默认值和运行时配置互相覆盖不清
- dev/test/prod 行为不一致
- 脚本后端、平台后端、分布式开关到处散

## 一、设计目标

这层设计要解决 6 个问题：

1. 配置来源如何统一。
2. profile 如何选择。
3. starter 默认配置如何和运行时配置合并。
4. 环境变量、命令行、文件配置如何覆盖。
5. 模块开关和后端选择如何表达。
6. 运维开关如何动态治理。

## 二、参考来源

### 1. 参考 Spring 风格 profile 思想

参考点：

- profile 决定运行模式
- 默认配置 + profile 覆盖
- 环境变量和命令行覆盖

不照搬点：

- 不做过度复杂的配置魔法
- 不做隐式松散绑定到不可控程度

### 2. 参考 MMO 工程常见做法

参考点：

- 配置必须支持多环境
- 配置必须支持多 app
- 配置必须支持开关分布式能力

## 三、为什么这样设计

Apollo 不是一个只有单一部署模式的框架。

它需要支持：

- 轻在线
- 普通 MMO
- BigWorld 模式
- Lua 或 Python 脚本后端
- Redis / SQL / OTel / ops 等不同开关组合

这意味着配置不能只做成“一个 app 一个 yaml 文件”。

更合理的方式是：

- `base config`
- `profile config`
- `environment overrides`
- `command line overrides`

按层合并。

## 四、优点

- 多环境行为更可预测
- starter 默认值和部署配置边界清楚
- 模块开关统一
- 后端选择统一
- 更适合自动化部署和测试

## 五、代价与风险

- 配置体系会比单文件复杂
- 需要严格规定覆盖优先级
- 如果结构设计不好，配置会迅速膨胀

## 六、为什么不选其他方案

### 不选“每个 app 自己定义配置规则”

因为这样最终会让整个框架没有统一运行模型。

### 不选“只有一份全局配置”

因为 Apollo 需要支持：

- 多 profile
- 多 topology
- 多后端

一份静态配置不够表达。

### 不选“配置全动态随便覆盖”

因为会导致行为不可预测。

Apollo 更适合：

- 有明确优先级的层次化配置

## 七、推荐配置层级

建议 Apollo 的配置按 5 层合并：

```text
1. Module Default
2. Starter Default
3. Base App Config
4. Profile Config
5. Environment / CLI Override
```

## 八、各层职责

### 1. `Module Default`

模块自身最小默认值。

例如：

- `runtime.ops.console = true`
- `platform.redis.pool_size = 8`

### 2. `Starter Default`

starter 决定的默认装配值。

例如：

- `starter-mmo` 默认启用 `game.session`
- `starter-bigworld` 默认启用 `distributed_world`

### 3. `Base App Config`

某个 app 的基础配置文件。

例如：

- `gateway-app.yaml`
- `world-app.yaml`

### 4. `Profile Config`

用于表达：

- `dev`
- `test`
- `prod`
- `mmo`
- `bigworld`

### 5. `Environment / CLI Override`

用于：

- 容器化部署
- CI/CD
- 紧急覆盖

## 九、推荐优先级

建议最终优先级为：

```text
CLI > Environment > Profile > App Config > Starter Default > Module Default
```

### 为什么必须明确优先级

因为后面这些值会经常冲突：

- 脚本后端选谁
- Redis 地址是什么
- 是否启用 distributed world
- 是否打开 OTel

没有优先级，就会出现运维层无法预测最终生效值。

## 十、推荐配置分区

建议 Apollo 的配置至少统一成下面几个顶级区段：

### `app`

- app 名称
- 实例 id
- 监听地址

### `profile`

- 当前 profile
- 拓扑模式

### `modules`

- 启用哪些模块
- 禁用哪些模块

### `script`

- backend = lua / python
- 热更开关
- sandbox 开关

### `platform`

- redis
- sql
- cache
- leaderboard
- lock

### `ops`

- console
- otel
- health
- watcher
- remote_control

### `distributed_world`

- enabled
- partition
- authority transfer

### `login`

- password verifier
- risk control
- ticket ttl

### `persistence`

- db mode
- writeback policy
- save queue / coordinator

## 十一、推荐配置示例

```yaml
app:
  name: world-app
  instance_id: world-01

profile:
  mode: mmo
  env: prod

modules:
  enable:
    - runtime
    - runtime.ops
    - platform.redis
    - platform.sql
    - game.session
    - game.world
  disable:
    - script.python

script:
  backend: lua
  hot_reload: false

platform:
  redis:
    endpoint: redis://127.0.0.1:6379
  sql:
    driver: mysql

ops:
  otel: true
  console: false
  remote_control: true

distributed_world:
  enabled: false
```

## 十二、profile 的职责边界

`Profile` 不是另一个 starter。

它更准确的职责是：

- 在既有 starter 基础上做模式覆盖

### 例如

`starter-mmo`

负责：

- 默认装什么模块

`profile=prod`

负责：

- console 关掉
- OTel 打开
- endpoint 换成生产值

`profile=bigworld`

负责：

- distributed world 相关开关打开

## 十三、动态配置边界

Apollo 后续肯定会碰到“动态配置要不要支持”的问题。

这里建议先明确边界。

### 可以动态变更的

- log level
- trace sampling
- 某些 ops 开关
- 某些活动和规则配置

### 不建议动态变更的

- 脚本后端
- 核心 platform driver
- distributed world 拓扑模式
- 关键依赖模块启停

### 为什么

这些值一旦变化，会直接影响宿主结构和依赖图，不适合热切。

## 十四、和 starter / manifest / bootstrap 的关系

### `ModuleManifest`

提供：

- module default

### `Starter`

提供：

- starter default

### `Bootstrap`

负责：

- 加载和合并配置
- 在 module resolve 前确定最终配置

## 十五、对当前 Apollo 的直接含义

Apollo 当前已经有配置系统雏形，但还没有把：

- module default
- starter default
- profile config
- environment override

整合成统一规则。

下一步更现实的推进顺序应该是：

1. 先定义配置层级和优先级
2. 先让 starter 和 bootstrap 按统一配置规则工作
3. 再让 ops 和 platform 模块接入

## 十六、结论

Apollo 如果要从“很多模块”走向“真正可运行的框架”，配置体系必须和 starter、manifest、bootstrap 一起设计。

真正需要收住的是：

- 配置来源
- 覆盖优先级
- profile 语义
- 模块开关表达方式

只有这层清楚，后面的装配和运行行为才会稳定。

## 相关阅读

- [Starter 与模块装配设计](./starter-and-module-assembly-design.md)
- [Module Manifest 与 Registry 设计](./module-manifest-and-registry-design.md)
- [App Bootstrap 生命周期设计](./app-bootstrap-lifecycle-design.md)
- [LoginApp 收口设计](./login-app-design.md)
- [持久化进程与 DBMgr 设计](./persistence-process-and-db-manager-design.md)
