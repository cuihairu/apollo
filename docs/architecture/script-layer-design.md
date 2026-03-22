---
title: 统一脚本层设计
icon: code
order: 8
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - 脚本
  - Lua
  - Python
---

# 统一脚本层设计

这篇文档解决的是 Apollo 后续做业务逻辑扩展时，一个必须先定下来的问题：

`Apollo 到底如何支持脚本化业务，同时不把框架绑死在某一种语言上。`

这里先明确一个前提：

- 目标不是让一个项目同时启用多种业务脚本语言

而是：

- Apollo 提供统一脚本层
- 具体项目按团队习惯选择 `Lua` 或 `Python`
- 两者共享同一套宿主语义、对象模型、绑定规则和治理机制

## 一、先说结论

Apollo 的脚本层应该设计成：

```text
Apollo Script Layer
    ├── Script ABI
    ├── Script Runtime Host
    ├── Binding Generator
    ├── Hot Reload / Sandbox / Timeout
    └── Script Backend
            ├── Lua Backend
            └── Python Backend
```

也就是说，Apollo 要统一的是：

- 脚本宿主接口
- API surface
- 对象生命周期
- 绑定生成规则
- 热更和治理能力

而不是要求一个项目同时装 `Lua + Python` 双栈。

## 二、为什么要这样设计

### 1. 游戏行业里主流习惯就是“选一种脚本语言写业务”

更常见的实际情况是：

- 有的团队偏 `Lua`
- 有的团队偏 `Python`

但很少会把高频业务脚本同时拆成两种语言维护。

所以 Apollo 更合理的目标是：

- 同时吸引这两类用户

而不是：

- 强制所有项目双语言并存

### 2. 如果脚本层直接绑定具体语言，框架 API 会越来越散

如果 Apollo 直接写成：

- `Lua` 一套宿主 API
- `Python` 一套宿主 API

那后面一定会出现：

- 能力不对齐
- 行为边界不一致
- 文档和示例维护成本翻倍

所以必须先统一脚本层抽象。

### 3. KBE 的启发是“脚本桥接是主路径，不是边角插件”

从 [kbe-source-analysis.md](/C:/Users/cui/Workspaces/apollo/docs/architecture/kbe-source-analysis.md) 已经能看出来：

- KBE 把 Python 放在实体定义和业务逻辑主路径上
- 不是“顺手做个脚本扩展”

Apollo 不一定照搬 Python-only 模型，但必须吸收这个原则：

`脚本层必须是框架的一等能力。`

## 三、总体目标

Apollo 的统一脚本层建议满足 5 个目标：

### 1. 语言可替换

业务项目能够在 `Lua` 或 `Python` 之间二选一。

### 2. API 稳定

同一套宿主能力，不因为语言不同而语义漂移。

### 3. 热更可治理

不是只支持“重新加载文件”，而是要考虑：

- 状态迁移
- 回滚
- 超时
- 异常隔离

### 4. 性能可分层

高频热路径和低频扩展逻辑不能混成一个执行模型。

### 5. 工程可维护

文档、示例、绑定生成、测试都应该围绕同一套 ABI 展开。

## 四、核心设计原则

### 1. 一个项目通常只选一种主业务脚本语言

这是最重要的边界。

Apollo 可以支持：

- `Lua Backend`
- `Python Backend`

但默认假设应该是：

- 一个游戏项目只选择其中一种作为主业务脚本语言

### 2. 宿主统一，后端分离

脚本层要统一的是 `Script ABI`，不是统一解释器实现。

### 3. 脚本只扩展业务，不接管引擎底座

脚本适合写：

- 技能
- buff
- AI
- 任务
- 活动规则
- GM 逻辑

不适合写：

- 网络收发底层
- AOI 核心热路径
- replication 打包热路径
- 存储驱动底层

### 4. API 稳定优先于绑定花哨

脚本层的重点不是“语法多优雅”，而是：

- 版本可演进
- 行为稳定
- 可回归测试

## 五、统一 Script ABI

这层是整个设计的核心。

建议 Apollo 定义一套语言无关的脚本 ABI。

### `ScriptRuntime`

职责：

- 加载脚本模块
- 创建脚本对象
- 调用脚本函数
- 回收脚本实例

### `ScriptModule`

职责：

- 表示一个脚本模块
- 暴露构造入口和方法表
- 暴露模块版本信息

### `ScriptObjectHandle`

职责：

- 表示脚本世界中的对象实例引用
- 用于和 C++ 实体、服务对象关联

### `ScriptValue`

职责：

- 做统一参数和值封装
- 屏蔽 Lua/Python 参数表示差异

### `ScriptContext`

职责：

- 当前调用上下文
- trace id
- entity id
- player id
- timeout/deadline

## 六、推荐宿主接口

建议后续统一成下面这类接口：

```cpp
class IScriptBackend {
public:
    virtual ~IScriptBackend() = default;

    virtual bool initialize(const ScriptRuntimeOptions& options) = 0;
    virtual bool loadModule(std::string_view moduleName) = 0;
    virtual ScriptObjectHandle createObject(
        std::string_view moduleName,
        std::string_view className,
        const ScriptArgs& args) = 0;
    virtual ScriptResult invoke(
        ScriptObjectHandle object,
        std::string_view method,
        const ScriptArgs& args,
        const ScriptContext& ctx) = 0;
    virtual bool reloadModule(std::string_view moduleName) = 0;
};
```

这里真正重要的是：

- Lua 和 Python 都走同一组宿主语义
- 不允许每个后端各搞一套生命周期

## 七、Lua 和 Python 的角色定位

这里不是说两者要在同一个项目里同时跑，而是 Apollo 要明确支持哪两类用户。

### `Lua Backend`

适合的项目特征：

- 更强调轻量和嵌入性
- 更强调热更
- 更强调高频业务规则

推荐适用场景：

- 技能
- buff
- AI
- 任务条件
- 活动规则
- 副本脚本

### `Python Backend`

适合的项目特征：

- 更强调表达力和生态
- 更偏 KBE/BigWorld 风格用户习惯
- 更偏快速开发和复杂业务组织

推荐适用场景：

- 实体行为逻辑
- 任务与活动逻辑
- GM 和管理逻辑
- 配置驱动逻辑

### 关键点

Apollo 应支持二者，但不建议默认设计成：

- 同一项目 `Lua` 写一半业务
- `Python` 再写一半业务

这会直接抬高维护成本。

## 八、API 稳定性设计

如果 Apollo 要吸引 Lua 用户和 Python 用户，API 稳定性比语言支持数量更重要。

建议明确下面 4 层稳定边界。

### 1. 稳定宿主命名空间

例如统一暴露：

- `apollo.entity`
- `apollo.world`
- `apollo.session`
- `apollo.timer`
- `apollo.log`
- `apollo.rpc`

Lua 和 Python 看到的是同样的概念层。

### 2. 稳定对象模型

例如统一暴露：

- `Entity`
- `PlayerAnchor`
- `Proxy`
- `WorldSpace`
- `TimerHandle`

不要在不同后端里改对象语义。

### 3. 稳定错误模型

建议统一：

- 参数错误
- 调用超时
- 权限错误
- route stale
- not found

脚本语言不同，但错误语义应一致。

### 4. 稳定版本策略

建议脚本 API 采用显式版本号：

- `Script API v1`
- `Script API v2`

不要让脚本接口在小版本里悄悄改行为。

## 九、绑定生成策略

脚本层不能长期靠手写绑定撑住。

建议后续从统一 schema 生成脚本绑定：

- `EntitySchema`
- `MethodSchema`
- `PropertySchema`
- service contract

也就是：

```text
Schema
    -> Binding Generator
        -> Lua Binding
        -> Python Binding
```

这样可以保证：

- 宿主能力一致
- 文档和代码不漂
- 新增实体和服务时不重复手写

## 十、热更设计

脚本层必须支持热更，但热更不能只理解成“重新执行脚本文件”。

建议至少拆成 3 层：

### 1. 模块重载

适合：

- 纯函数逻辑
- 配置式规则

### 2. 对象升级

适合：

- 已在线实体实例
- 任务状态机
- 活动实例

这要求脚本层支持：

- 旧实例迁移到新类定义
- 旧状态保留

### 3. 回滚

热更失败时应支持：

- 模块回退
- 实例继续留在旧版本
- 明确错误报告

## 十一、超时与沙箱

这块一定要在架构层先写清楚，不然后面很难补。

### 超时治理

建议脚本执行统一支持：

- 调用 deadline
- watchdog
- 执行超时中断

### 沙箱治理

建议限制脚本直接访问：

- 任意文件系统
- 任意网络
- 任意线程
- 任意进程操作

默认应该通过宿主 API 间接访问能力。

### 权限分级

建议至少分成：

- `Gameplay`
- `Ops`
- `Tool`

不同脚本域暴露不同 API。

## 十二、性能分层

如果不做性能分层，脚本层最后一定会拖垮 world runtime。

建议明确分成 3 档：

### 1. 高频热路径

例如：

- replication
- AOI 核心判断
- pathfinding 主循环

原则：

- 保持 C++ 实现

### 2. 中频业务逻辑

例如：

- 技能判定
- buff 结算
- 任务推进
- 活动规则

原则：

- 可脚本化

### 3. 低频控制逻辑

例如：

- GM
- 运维命令
- 管理逻辑
- 数据修复

原则：

- 更适合脚本化

## 十三、和 Apollo 其他设计的关系

脚本层必须和现有文档对齐。

### 和 `EntitySchema`

脚本对象暴露的属性和方法，应该来自统一 schema，而不是脚本后端各自定义。

### 和 `RemoteEntityCall`

脚本层发起跨进程调用时，应通过统一远程调用模型，不直接拼消息。

### 和 `Replication Pipeline`

脚本可以参与决定业务状态变化，但复制包组装不应放进脚本热路径。

### 和 `PlayerAnchor / WorldHost`

脚本逻辑应运行在明确宿主上：

- `PlayerAnchor Host`
- `WorldHost`

而不是悬空执行。

同时也不应把脚本宿主误写成：

- `DBMgr / PersistenceService`
- `Gateway` 的底层连接循环
- `AOI / Replication` 的核心热路径

## 十四、推荐的目录与模块落点

建议未来补一个明确模块，例如：

- `modules/script`

并在其下分层：

```text
modules/script
    ├── core
    ├── abi
    ├── binding
    ├── runtime
    ├── lua
    └── python
```

这样 Apollo 后面才能做到：

- 宿主统一
- 后端可选
- 绑定生成统一

## 十五、结论

Apollo 的脚本层最合理的路线不是“一个项目同时启用多种业务脚本语言”，而是：

- 框架提供统一脚本层
- 项目按团队习惯选择 `Lua` 或 `Python`
- 两种后端共享统一的 Script ABI、API 语义和治理模型

这样既能吸引 Lua 用户，也能吸引 Python 用户，同时不把框架 API 做散。

真正该先收住的是：

- 统一宿主接口
- 统一对象模型
- 统一绑定生成
- 统一热更/超时/沙箱治理
- 清晰的性能分层

## 相关阅读

- [Apollo 渐进式游戏框架理论设计](./apollo-progressive-game-framework.md)
- [PlayerAnchor 设计稿](./player-anchor-design.md)
- [WorldHost 设计稿](./world-host-design.md)
- [EntitySchema 设计](./entity-schema-design.md)
- [RemoteEntityCall 设计](./remote-entity-call-design.md)
- [Replication Pipeline 设计](./replication-pipeline-design.md)
- [KBEngine 源码分析](./kbe-source-analysis.md)
