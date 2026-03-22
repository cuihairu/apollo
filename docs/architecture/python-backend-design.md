---
title: Python Backend 设计
icon: flask
order: 10
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Python
  - Script
  - KBE
---

# Python Backend 设计

这篇文档回答的是另一个对 Apollo 很关键的问题：

`如果项目选择 Python 作为主业务脚本语言，Apollo 应该如何提供一套可用的后端。`

这里的重点不是单纯“能不能嵌 Python”，而是：

- 如何把 Python 变成正式业务脚本后端
- 如何兼容 KBE 风格用户习惯
- 如何避免 Python 侵入所有热路径

## 一、先说结论

Apollo 的 `Python Backend` 更适合被定义成：

- 表达力优先的业务脚本后端
- 更适合复杂业务组织和快速开发
- 更适合吸引 KBE/BigWorld 风格用户
- 但必须严格限制进入高频热路径

更准确的定位应该是：

`复杂业务脚本后端`

## 二、为什么 Apollo 仍然值得支持 Python

### 1. KBE 路线用户天然熟悉 Python

从 [kbe-source-analysis.md](./kbe-source-analysis.md) 已经能看到：

- KBE 的脚本桥接主路径就是 Python
- 实体定义、脚本模块、业务行为都和 Python 深度相关

所以如果 Apollo 想吸引这一类用户，Python 是绕不开的。

### 2. Python 的表达力更强

很多复杂业务逻辑用 Python 表达会更直接，例如：

- 多阶段任务编排
- 活动规则组合
- GM 与管理逻辑
- 复杂条件和状态机

### 3. Python 的生态更成熟

在工具和控制层面，Python 的生态优势很明显：

- 数据处理
- 配置工具
- 调试辅助
- 自动化脚本

但这不意味着 Apollo 应该把所有在线主逻辑都丢给 Python。

## 三、Python Backend 的定位边界

Apollo 需要提前把 Python 的边界收住。

### 适合放进 Python 的逻辑

- 实体业务行为
- 任务与活动逻辑
- 管理逻辑
- GM 逻辑
- 配置驱动逻辑
- 世界事件编排

### 谨慎放进 Python 的逻辑

- 高密度战斗热点
- 大量 buff 高频 tick
- 高频 AI 感知循环

### 不适合放进 Python 的逻辑

- 网络 I/O 底层
- AOI 主循环
- replication 编码热路径
- 大规模路径搜索主循环

## 四、运行时模型

建议 Apollo 的 `Python Backend` 采用：

- 明确的宿主内嵌解释器
- 按 app 或宿主域组织模块
- 严格的调用边界和错误治理

### 推荐结构

```text
PythonScriptBackend
    ├── PythonRuntimeHost
    ├── PythonModuleRegistry
    ├── PythonBindingRegistry
    ├── PythonReloadManager
    └── PythonSandboxPolicy
```

### 为什么不能把 Python 看成“外挂工具脚本”

如果 Apollo 既想支持 Python 用户，又只把 Python 放在边角工具链，就很难吸引真正的 Python 业务开发者。

所以更合理的做法是：

- Python 也作为正式业务后端支持
- 但严格规定它的性能边界

## 五、对象模型建议

Apollo 的 Python 后端应该尽量贴近 KBE 用户易理解的模型，但不能把宿主完全脚本化。

### 推荐关系

```text
C++ Host Object
    <-> ScriptObjectHandle
    <-> Python Script Object
```

### 原则

- 权威状态仍由宿主控制
- Python 负责业务行为和逻辑编排
- 生命周期由宿主驱动

### 为什么不能让 Python 全权持有实体权威状态

因为 Apollo 后续还要统一：

- `PlayerAnchor`
- `RemoteEntityCall`
- `Replication Pipeline`
- `Authority Transfer`

这些都要求宿主层拥有稳定的一致性模型。

## 六、模块与类模型

Python 后端建议鼓励使用：

- 模块
- 类
- 明确生命周期 hook

逻辑风格上更适合：

```python
class Avatar:
    def on_enter_world(self):
        pass

    def on_skill_cast(self, ctx, target):
        pass
```

### 为什么 Python 更适合类模型

因为 Python 用户通常更习惯：

- 类和对象组织
- mixin
- 模块化目录结构

Apollo 不需要强行把 Python 写成 Lua 风格 table 模型。

## 七、绑定方式建议

和 Lua 一样，Python 后端也不应该长期依赖纯手写绑定。

建议分成两层：

### 自动生成绑定

来源：

- `EntitySchema`
- `PropertySchema`
- `MethodSchema`
- service contract

### 手写宿主 API

例如：

- `apollo.log`
- `apollo.timer`
- `apollo.rpc`
- `apollo.entity`
- `apollo.world`
- `apollo.session`

### 关键点

Lua 和 Python 的绑定方式可以不同，但暴露的概念层必须一致。

## 八、热更设计

Python 后端的热更比 Lua 更复杂，必须先写清楚。

### 1. 模块重载

建议支持：

- 模块级 reload
- 依赖树检查
- 失败回退

### 2. 类实例迁移

Python 热更真正难的是：

- 旧实例怎么迁移到新类定义

Apollo 更建议支持：

- 显式迁移钩子
- 老实例可继续留在旧版本

### 3. 风险控制

不要默认全量热替换：

- 活动脚本可以热更
- GM 脚本可以热更
- 世界主实体脚本应更谨慎

## 九、错误与异常治理

Python 的一个工程优势是异常表达力强，但在线运行时不能直接把异常往外冒。

建议统一做：

- Python traceback 捕获
- 错误分类
- trace id 关联
- 宿主级 fallback

### 建议错误分类

- ScriptImportError
- ScriptInvokeError
- ScriptTimeoutError
- ScriptPermissionError
- ScriptStateMigrationError

这样比单纯打一条日志更有用。

## 十、超时与执行治理

Python 后端必须比 Lua 更重视执行治理。

### 原因

- Python 表达力强
- 也更容易写出复杂、慢、阻塞的逻辑

### 建议支持

- 调用 deadline
- watchdog
- 慢脚本统计
- 模块级耗时指标

### 原则

Python 脚本适合复杂业务表达，但不应获得“无限执行权”。

## 十一、性能策略

Apollo 要支持 Python，但不能为了支持 Python 而牺牲 world runtime。

### 适合 Python 的频率带

- 世界事件编排
- 任务状态推进
- 活动状态机
- 管理逻辑
- 较低频实体行为

### 不适合 Python 的频率带

- 帧级热点战斗循环
- 大量单位高频 buff 结算
- 高频可见集变化计算

### 性能建议

- 尽量减少跨语言往返
- 做批量调用而不是碎片调用
- 把热点数值逻辑下沉 C++
- 用脚本层做规则编排而不是内核循环

## 十二、和 KBE 兼容路线的关系

如果 Apollo 后续要继续吸收 KBE 用户，Python 后端是重要桥梁。

### 可以借鉴的方向

- Python 业务类模型
- 实体脚本入口
- 脚本驱动实体行为

### 不建议照搬的地方

- 把所有核心语义都深度绑死在 Python 上

Apollo 和 KBE 的一个关键区别应该是：

- Apollo 的宿主抽象先独立
- Python 只是其上的一种正式后端

这样 Lua 后端才不会变成二等公民。

## 十三、推荐目录

建议未来在脚本模块下形成：

```text
modules/script/python
    ├── include/apollo/script/python/
    ├── src/
    ├── binding/
    └── tests/
```

## 十四、结论

Apollo 支持 Python 是有明确价值的，尤其是在吸引 KBE 风格用户和承接复杂业务逻辑方面。

但 Python 后端的正确定位不是：

- 吞掉所有在线主逻辑

而是：

- 作为正式业务脚本后端
- 承接复杂业务表达
- 严格避开高频热路径

这样 Apollo 才能既支持 Python，又不牺牲整体架构稳定性。

## 相关阅读

- [统一脚本层设计](./script-layer-design.md)
- [Lua Backend 设计](./lua-backend-design.md)
- [KBEngine 源码分析](./kbe-source-analysis.md)
- [EntitySchema 设计](./entity-schema-design.md)
- [RemoteEntityCall 设计](./remote-entity-call-design.md)
