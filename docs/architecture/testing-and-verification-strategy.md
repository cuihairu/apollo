---
title: Testing 与 Verification 策略
icon: vials
order: 21
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Testing
  - Verification
  - QA
---

# Testing 与 Verification 策略

这篇文档解决的是 Apollo 整体框架继续往下走时，另一个必须尽早定下来的问题：

`这套分层、装配、脚本、平台、BigWorld 增强能力，最终到底怎么验证。`

如果没有统一验证策略，后面很容易出现：

- 文档写得很多，但没法证明设计可落地
- starter、profile、manifest、bootstrap 各写各的，没有整体回归
- BigWorld 增强层一加入，就把普通 MMO 主线打坏

## 一、设计目标

这层策略要解决 6 个问题：

1. 如何验证分层边界没有被破坏。
2. 如何验证 starter / profile / bootstrap 真的能组合工作。
3. 如何验证脚本后端替换不破坏宿主 API。
4. 如何验证 platform foundation 抽象没有漂。
5. 如何验证 distributed world 增强层不会污染默认模式。
6. 如何让文档设计和测试策略相互对应。

## 二、参考来源

### 1. 参考工程分层测试思路

参考点：

- 单元测试
- 集成测试
- 装配测试
- 端到端验证

### 2. 参考 KBE 兼容层的启示

Apollo 当前已有一条很好的方向：

- `API + tests`

这个原则应该扩展到整套框架设计。

### 3. 参考新的设计原则

既然 Apollo 现在要求每个设计点都写：

- 理由
- 优点
- 代价
- 不选方案

那测试层也应该能对应验证这些取舍。

## 三、为什么这样设计

Apollo 后面不是单一模块，而是整套组合框架。

所以验证不能只停留在：

- 某个类能不能 new 出来

更合理的验证体系应该覆盖：

- 分层
- 模块
- 装配
- profile
- script backend
- platform backend
- 普通 MMO 主线
- BigWorld 增强层

## 四、优点

- 能尽早发现分层污染
- 能尽早发现 starter/profile 配置冲突
- 能证明脚本和平台抽象不是空文档
- 普通 MMO 主线更稳
- BigWorld 增强层更不容易反向打坏基础层

## 五、代价与风险

- 测试体系会明显变大
- 需要定义标准测试夹具和假模块
- 集成测试和装配测试成本高于普通单测

## 六、为什么不选其他方案

### 不选“只写单元测试”

因为 Apollo 最大的风险不在单类逻辑，而在装配和边界。

### 不选“只跑 end-to-end”

因为出问题后很难快速定位是哪层坏了。

### 不选“等实现差不多了再补测试策略”

因为越晚定验证分层，越容易让坏依赖和坏边界长进去。

## 七、推荐测试层级

建议 Apollo 的验证体系至少分 6 层。

### 1. Layer Rules Test

验证：

- 分层依赖规则
- 模块目录约束
- 禁止反向依赖

### 2. Module Unit Test

验证：

- 单个模块内部行为
- 纯函数逻辑
- 数据结构和算法

### 3. Contract Test

验证：

- `Script ABI`
- `Platform Foundation` 抽象
- `RemoteEntityCall`
- `Replication Pipeline`

### 4. Assembly Test

验证：

- starter
- manifest
- registry
- bootstrap
- host builder

### 5. Mode Test

验证：

- 轻在线组合
- 普通 MMO 组合
- BigWorld 组合

### 6. Scenario Test

验证：

- 登录
- 进入世界
- 切图
- 重连
- 排行榜
- 热更
- distributed world 边界观察

## 八、推荐重点测试对象

### 1. 分层边界测试

理由：

- Apollo 最大的长期风险是分层被侵蚀

优点：

- 能很早发现错误依赖

代价：

- 需要维护依赖规则白名单/黑名单

### 2. 装配链测试

理由：

- starter / profile / bootstrap 是整个框架的主线

优点：

- 能验证整套装配是否真的可运行

代价：

- 需要 mock 一批最小模块

### 3. 脚本后端一致性测试

理由：

- Lua / Python 是可替换后端

优点：

- 能验证统一 Script ABI 没有漂

代价：

- 需要双后端对照用例

### 4. 平台层契约测试

理由：

- Redis / SQL / leaderboard / lock 这类抽象很容易漂

优点：

- 能保护平台层稳定性

代价：

- 需要 fake backend 和真实 backend 双测

## 九、推荐测试矩阵

建议 Apollo 至少维护下面这组验证矩阵：

```text
Mode x Script Backend x Platform Backend x Ops Flag
```

例如最小矩阵可以是：

1. `mmo + lua + redis/mysql + ops-basic`
2. `mmo + python + redis/mysql + ops-basic`
3. `bigworld + lua + redis/mysql + ops-full`

### 为什么不建议一开始矩阵爆炸

因为组合太多会把 CI 压垮。

更合理的是：

- 先维护最小代表性矩阵
- 再逐步增加覆盖

## 十、文档和测试如何对应

Apollo 现在文档已经越来越多，必须防止它们只停留在概念层。

建议后续每篇核心设计文档都至少映射到：

- 一类 contract test
- 一类 assembly test
- 一类 scenario test

### 例如

`统一脚本层设计`

应对应：

- Script ABI contract test
- Lua/Python backend parity test

`Starter 与模块装配设计`

应对应：

- starter expansion test
- bootstrap assembly test

`Platform Foundation 设计`

应对应：

- leaderboard contract test
- distributed lock contract test

## 十一、推荐目录思路

建议测试目录不要只按“单元测试/集成测试”分，还要按架构层补标签。

例如：

```text
tests/
  unit/
  contract/
  assembly/
  mode/
  scenario/
```

### 对应关系

- `unit`
  - 模块内部逻辑
- `contract`
  - 抽象接口和后端一致性
- `assembly`
  - starter/bootstrap/hostbuilder
- `mode`
  - mmo/bigworld/profile 组合
- `scenario`
  - 登录、进世界、切图、重连等链路

## 十二、对当前 Apollo 的直接含义

Apollo 当前已经有一些测试基础，但整体还没有升到“框架验证策略”层。

更现实的推进顺序应该是：

1. 先给 starter/manifest/bootstrap 补 assembly test
2. 先给 script/platform 抽象补 contract test
3. 再给普通 MMO 主线补 scenario test
4. 最后再给 BigWorld 增强层补 mode/scenario test

## 十三、结论

Apollo 后续如果要把整套框架做稳，测试不能只围绕类和函数，而必须围绕：

- 分层
- 模块
- 装配
- 模式
- 场景

这样这套架构文档才会真正对应到可验证的工程体系。

## 相关阅读

- [Module Manifest 与 Registry 设计](./module-manifest-and-registry-design.md)
- [App Bootstrap 生命周期设计](./app-bootstrap-lifecycle-design.md)
- [Host Builder 与 DI 设计](./host-builder-and-di-design.md)
- [Configuration 与 Profile 设计](./configuration-and-profile-design.md)
