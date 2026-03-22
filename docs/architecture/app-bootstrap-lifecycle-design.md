---
title: App Bootstrap 生命周期设计
icon: rocket
order: 17
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Bootstrap
  - Lifecycle
  - Startup
---

# App Bootstrap 生命周期设计

这篇文档解决的是 Apollo 装配体系里的另一块关键拼图：

`一个 app 从读取配置到启动模块，再到关闭回收，这条生命周期应该如何统一。`

如果这层不统一，Apollo 很容易继续维持当前这种状态：

- 每个 app 自己写启动顺序
- 每个 app 自己写关闭顺序
- 模块初始化点散落
- 运维和调试入口接入时序不一致

## 一、设计目标

这层设计要解决 6 个问题：

1. app 启动顺序如何统一。
2. module / starter / profile 何时解析。
3. runtime、ops、platform、domain 何时初始化。
4. health / telemetry / console 何时开放。
5. 关闭流程如何统一。
6. 失败回滚如何统一。

## 二、参考来源

### 1. 参考 Spring Boot 启动链

参考点：

- bootstrap 统一入口
- 环境准备先于组件装配
- 生命周期阶段清晰

不照搬点：

- 不做反射式自动注入
- 不依赖容器隐式管理全部对象

### 2. 参考 KBE 的显式宿主初始化

参考点：

- 宿主初始化顺序很重要
- 网络、脚本、实体定义、运行时顺序不能乱

不照搬点：

- 不把全部运行时深度绑死在某一种脚本语言上

## 三、为什么这样设计

Apollo 后面的 app 已经不只是简单 main 函数：

- `gateway-app`
- `login-app`
- `base-app`
  目标语义是 `BaseApp(PlayerAnchor Host)`，不是数据库服务同义词
- `world-app`
  当前仓库阶段可先由 `apps/cell-app` 承担其装配壳
- `db-app / persistence-app`

都会逐渐依赖：

- runtime host
- ops host
- platform foundation
- game foundation
- domain components

如果生命周期不统一，starter 再漂亮也没有用。

## 四、优点

- app 启动顺序可预测
- 模块初始化时机一致
- 失败回滚可统一处理
- 运维/调试/遥测接入点清晰
- 更容易写集成测试

## 五、代价与风险

- bootstrap 设计会变成框架核心之一
- 初期会比“main 里手写几行初始化”复杂
- 如果阶段划分过细，会导致接入成本上升

## 六、为什么不选其他方案

### 不选“每个 app 自己定义生命周期”

因为这会让同一个框架里的 app 行为越来越不一致。

### 不选“只有 initialize / shutdown 两阶段”

因为 Apollo 已经有：

- config
- starter
- registry
- telemetry
- health
- runtime
- domain

两阶段不够表达。

## 七、推荐生命周期阶段

建议 Apollo 的 app bootstrap 统一成 9 个阶段。

```text
1. BootstrapArgs
2. EnvironmentPrepare
3. ProfileResolve
4. ModuleResolve
5. HostBuild
6. ModuleInit
7. ReadyExpose
8. Running
9. Shutdown
```

## 八、各阶段说明

### 1. `BootstrapArgs`

职责：

- 读取命令行参数
- 读取 app 名称
- 确定配置目录
- 确定 profile

### 2. `EnvironmentPrepare`

职责：

- 初始化最小日志
- 初始化配置加载器
- 初始化基础错误处理
- 初始化 pidfile / process guardian 的最小能力

### 3. `ProfileResolve`

职责：

- 解析 profile
- 应用环境覆盖
- 选择 starter
- 选择脚本后端
- 选择平台后端

### 4. `ModuleResolve`

职责：

- 加载 starter 默认模块
- 查询 `ModuleRegistry`
- 展开依赖
- 检查冲突
- 生成拓扑顺序

### 5. `HostBuild`

职责：

- 创建 `ApplicationHost`
- 创建 `RuntimeHost`
- 创建 `RuntimeOpsHost`
- 组装基础容器

### 6. `ModuleInit`

职责：

- 初始化 platform foundation
- 初始化 game foundation
- 初始化 domain components
- 按拓扑顺序初始化模块

### 7. `ReadyExpose`

职责：

- 打开 health check
- 注册 console 命令
- 打开 telemetry exporter
- 标记 systemd/supervisor ready

### 8. `Running`

职责：

- 进入主循环
- 处理信号
- 处理管理命令
- 处理热更/调试请求

### 9. `Shutdown`

职责：

- 停止接收新请求
- 下线 domain components
- flush 状态和日志
- 释放 host 和模块
- 删除 pidfile

## 九、推荐对象模型

如果后续要把 bootstrap 做成统一标准，还需要同步约束几件事：

- `base-app` 的 bootstrap 必须围绕 `PlayerAnchor / SessionLocator / WorldAssignment`
- `world-app` 的 bootstrap 必须围绕 `WorldHost / MapInstance / WorldSession`
- `db-app / persistence-app` 的 bootstrap 必须围绕 `Repository / UnitOfWork / SaveCoordinator`

也就是说，bootstrap 统一的是生命周期，不是把不同语义的进程又混成一个大壳。

```text
AppBootstrap
    ├── BootstrapContext
    ├── EnvironmentLoader
    ├── ProfileResolver
    ├── ModuleResolver
    ├── HostBuilder
    ├── LifecycleCoordinator
    └── ShutdownCoordinator
```

### `BootstrapContext`

职责：

- app 名称
- profile
- config 路径
- trace / run id
- 当前阶段

### `ProfileResolver`

职责：

- 解析 profile
- 覆盖配置
- 选择 starter 和后端

### `ModuleResolver`

职责：

- 查询 manifest
- 构建依赖图
- 生成初始化顺序

### `HostBuilder`

职责：

- 构建 runtime / ops / platform 基础宿主

### `LifecycleCoordinator`

职责：

- 推进阶段
- 处理初始化顺序
- 记录阶段状态

### `ShutdownCoordinator`

职责：

- 统一关闭顺序
- 失败降级
- 强制退出兜底

## 十、推荐初始化顺序

Apollo 后续的初始化顺序，建议至少遵守下面这条主线：

1. `core`
2. `runtime`
3. `runtime.ops`
4. `platform`
5. `game.foundation`
6. `domain.components`
7. `distributed.world`（如果启用）

### 为什么这样排

- 先把框架跑起来
- 再把治理和观测挂起来
- 再挂平台能力
- 最后挂游戏语义和业务域

这能保证：

- 出问题时更容易排查
- 运维入口在业务加载前就准备好

## 十一、失败回滚策略

bootstrap 不能只考虑成功启动，也必须考虑启动中途失败。

### 推荐规则

如果某阶段失败：

1. 记录失败阶段
2. 逆序回滚已初始化模块
3. 输出明确诊断信息
4. 以非零码退出

### 为什么必须逆序回滚

因为很多模块存在依赖关系：

- world 依赖 platform
- platform 依赖 runtime

不逆序回滚会导致资源释放混乱。

## 十二、关闭流程建议

Apollo 的关闭流程也应该阶段化。

### 推荐顺序

1. 标记 `Draining`
2. 停止新连接 / 新请求
3. 通知 domain components 收口
4. flush 平台状态
5. 停止 telemetry / console
6. 停止 runtime host
7. 清理 pidfile / supervisor 状态

### 为什么不能直接 `exit`

因为 Apollo 后面会逐步承载：

- 玩家在线态
- 世界运行态
- 平台状态
- tracing/logging

直接退出会让收尾质量很差。

## 十三、和当前 Apollo 仓库的关系

Apollo 当前已经有一些生命周期相关能力，但还没有统一的 bootstrap 生命周期骨架。

下一步更合理的方向是：

- app 的 `main.cpp` 逐步变薄
- 初始化收口到统一 bootstrap
- 测试围绕 bootstrap 阶段来写

## 十四、结论

Apollo 如果要从“很多原型 app”走向“统一装配框架”，`App Bootstrap 生命周期` 必须单独设计。

它是 starter、profile、manifest 真正落地的执行主链。

没有这层，前面的装配设计都会停留在概念层。

## 相关阅读

- [Starter 与模块装配设计](./starter-and-module-assembly-design.md)
- [Module Manifest 与 Registry 设计](./module-manifest-and-registry-design.md)
- [Apollo 分层设计](./apollo-layering-design.md)
