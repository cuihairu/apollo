---
title: Lua Backend 设计
icon: bolt
order: 9
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Lua
  - Script
  - HotReload
---

# Lua Backend 设计

这篇文档是在 [统一脚本层设计](./script-layer-design.md) 之下，继续回答一个更具体的问题：

`如果项目选择 Lua 作为主业务脚本语言，Apollo 应该怎么实现这一套后端。`

这里的定位先说清楚：

- `Lua Backend` 不是可有可无的小插件
- 也不是把一堆 C API 暴露给脚本就结束

它应该是一套可以承接业务脚本、热更和宿主调用的正式后端。

## 一、先说结论

Apollo 的 `Lua Backend` 应该被设计成：

- 默认面向高频业务规则
- 默认支持热更
- 默认强调轻量嵌入和宿主可控
- 默认通过统一 `Script ABI` 对外暴露能力

更准确地说，Lua 在 Apollo 里的目标角色应该是：

`高频业务脚本后端`

## 二、为什么 Lua 适合作为主业务脚本后端

### 1. 嵌入成本低

Lua 的核心优势是：

- 运行时小
- 宿主可控
- 嵌入成熟

这对 Apollo 很重要，因为 Apollo 不是单纯工具链，而是要把脚本运行时嵌进：

- `WorldHost`
- `PlayerAnchor Host`
- 各类活动和任务宿主

### 2. 热更路径成熟

Lua 在游戏领域最常见的价值之一就是：

- 模块热更
- 规则快速调整
- 配表驱动逻辑更新

这和 Apollo 的渐进式框架路线是契合的。

### 3. 宿主约束更容易做

和 Python 相比，Lua 更容易在宿主里实现：

- 裁剪标准库
- 限制全局环境
- 控制内存分配
- 控制执行上下文

这让 Lua 更适合放进在线运行主路径附近。

## 三、Lua Backend 的定位边界

Apollo 需要明确 Lua 做什么，不做什么。

### 适合放进 Lua 的逻辑

- 技能
- buff
- AI 行为树节点逻辑
- 任务条件与任务推进
- 活动规则
- 副本事件脚本
- NPC 对话和交互

### 不适合放进 Lua 的逻辑

- 网络底层收发
- AOI 主循环
- replication 打包热路径
- 大量内存敏感的数据结构维护
- 核心持久化驱动

### 原则

Lua 应该写的是：

- 规则
- 组合
- 状态转换

而不是：

- 高频引擎底层

## 四、运行时模型

建议 Apollo 的 `Lua Backend` 采用 `多宿主实例 + 单解释器上下文隔离` 的设计。

### 推荐结构

```text
LuaScriptBackend
    ├── LuaRuntimePool
    ├── LuaModuleRegistry
    ├── LuaBindingRegistry
    ├── LuaHotReloadManager
    └── LuaSandboxPolicy
```

### `LuaRuntimePool`

职责：

- 管理多个 `lua_State`
- 按宿主类型分配运行时
- 控制生命周期

### 为什么不要全局只用一个 `lua_State`

如果所有逻辑都塞进一个全局状态，会出现：

- 模块污染
- 热更影响面过大
- 故障隔离差
- 并发调度困难

更合理的方式应该是：

- 按 app 或宿主域分配 runtime
- 每个 runtime 内部再加载该域脚本模块

## 五、脚本模块模型

建议 Lua 业务脚本统一采用模块返回 table 的模型。

例如逻辑上应接近：

```lua
local Skill = {}

function Skill:on_cast(ctx, target)
end

return Skill
```

这样做的优点是：

- 生命周期清晰
- 热更替换容易
- 容易和 `ScriptModule`、`ScriptObjectHandle` 对齐

### 不建议的模型

不建议把大量业务逻辑直接挂到全局命名空间。

原因很直接：

- 全局冲突
- 热更覆盖不清楚
- 调试困难

## 六、对象绑定模型

Lua 后端建议坚持“轻脚本对象 + 重宿主对象”的绑定模型。

### 推荐关系

```text
C++ Entity / Service Object
    <-> ScriptObjectHandle
    <-> Lua Table/UserData
```

### 原则

- 核心数据在 C++
- Lua 持有受控引用
- 生命周期由宿主主导

### 为什么不能把实体完整状态全塞进 Lua

因为 Apollo 后面要走到：

- `PlayerAnchor`
- `WorldHost`
- `RemoteEntityCall`
- `Replication Pipeline`

这些都要求宿主层拥有稳定的一致性和路由能力。

Lua 更适合作为行为扩展层，不适合作为唯一权威存储。

## 七、绑定方式建议

建议 Apollo 的 Lua 后端采用：

- 自动生成绑定为主
- 手写高阶辅助 API 为辅

### 自动生成的部分

- `EntitySchema`
- `PropertySchema`
- `MethodSchema`
- Service contract

### 手写封装的部分

- `apollo.log`
- `apollo.timer`
- `apollo.rpc`
- `apollo.world`
- `apollo.session`

### 为什么不建议大规模纯手写绑定

纯手写绑定早期看着快，后面会出现：

- 接口不一致
- 文档和代码漂移
- 修改代价越来越高

## 八、热更设计

Lua 后端必须把热更当一等能力。

### 1. 模块级热更

支持：

- 按模块重载
- 依赖追踪
- 失败回滚

### 2. 实例级升级

如果脚本对象已经在线运行，热更后要明确：

- 老实例继续跑旧逻辑
- 还是迁移到新模块

Apollo 更建议采用：

- 支持显式迁移
- 不做隐式全量替换

这样更稳。

### 3. 热更窗口控制

建议支持：

- 指定宿主域热更
- 指定地图实例热更
- 指定活动脚本热更

不要默认全服一起刷。

## 九、超时与资源治理

Lua 后端一定要有强治理能力。

### 执行超时

建议支持：

- 指令计数 hook
- deadline 检查
- watchdog 上报

### 内存治理

建议支持：

- 自定义 allocator
- runtime 级内存上限
- 模块级内存统计

### 沙箱治理

建议默认限制：

- 文件系统
- 原生 socket
- 非宿主授权模块

## 十、性能策略

Lua 后端的关键不是追求“所有逻辑都脚本化”，而是把脚本放在正确层级。

### 适合 Lua 的频率带

- 每次技能释放
- 每次 buff tick
- 每次任务事件
- 每次活动触发

### 不适合 Lua 的频率带

- 每帧大规模 AOI 判定
- 每帧海量复制包编码
- 高频路径搜索主循环

### 性能优化建议

- 减少跨语言边界往返
- 预绑定热点 API
- 使用稳定对象句柄而不是频繁动态查找
- 尽量批量传参，而不是碎片化调用

## 十一、和 Apollo 其他模块的关系

### 和 `EntitySchema`

Lua 脚本能看到的属性和方法应来自统一 schema。

### 和 `RemoteEntityCall`

Lua 发起跨进程调用时，不直接拼消息，而是走统一远程调用层。

### 和 `PlayerAnchor / WorldHost`

Lua 脚本实例应挂在明确宿主之下：

- `PlayerAnchor`
- `AvatarEntity`
- `WorldSpace`
- `ActivityInstance`

### 和 `Replication Pipeline`

Lua 可以改变业务状态，但不应直接承担复制打包主链。

## 十二、推荐目录

建议未来在脚本模块下形成：

```text
modules/script/lua
    ├── include/apollo/script/lua/
    ├── src/
    ├── binding/
    └── tests/
```

## 十三、结论

如果 Apollo 要提供一个真正可用的业务脚本后端，Lua 是最适合优先落地的选择。

它最适合承接的是：

- 高频业务规则
- 热更逻辑
- 轻量嵌入式脚本

但前提是不能把 Lua 后端做成零散绑定集合，而必须做成：

- 有统一运行时模型
- 有统一绑定生成
- 有统一热更和治理机制
- 有明确性能边界

## 相关阅读

- [统一脚本层设计](./script-layer-design.md)
- [EntitySchema 设计](./entity-schema-design.md)
- [RemoteEntityCall 设计](./remote-entity-call-design.md)
- [Replication Pipeline 设计](./replication-pipeline-design.md)
