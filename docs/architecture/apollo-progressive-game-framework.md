---
title: Apollo 渐进式游戏框架理论设计
icon: layer-group
order: 7
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - 渐进式架构
  - MMO
  - BigWorld
  - 模块化
---

# Apollo 渐进式游戏框架理论设计

这篇文档回答的是一个更上层的问题：

`Apollo 能不能不是只服务一种 MMO 形态，而是做成一套可渐进、可组装、可复用的游戏服务器框架。`

结论是可以，但前提很明确：

- 不能先按“进程名”组织框架
- 不能先按“某一类游戏玩法”固化框架
- 必须先把共性底座、业务域组件、分布式增强层拆清楚

也就是说，Apollo 最合理的方向不是：

- 做一套只能跑 BigWorld 的框架

而是：

- 做一套默认可支撑普通在线游戏
- 再按项目复杂度逐步装配到大世界模式

## 一、先给出总判断

从理论上看，这条路线是可行的，而且比一开始就把系统做成固定进程拓扑更合理。

原因有 4 个。

### 1. 大多数在线游戏共享一批稳定公共能力

不管是：

- 回合制
- 房间制战斗
- ARPG
- 开放世界 MMO
- 轻社交游戏

都会反复需要这些能力：

- 前后端交互协议
- 账号登录
- 在线会话
- 实体与属性
- 聊天
- 背包
- 技能与 buff
- 任务
- 活动
- 匹配
- 社交
- 支付
- 敏感词过滤
- 日志与风控
- 进程间通信

这说明框架层一定有大量共性。

### 2. 不同游戏的差异主要不在“有没有这些能力”，而在“这些能力怎么组合”

例如：

- 房间制游戏更强调匹配、房间、战斗局管理
- MMO 更强调在线锚点、世界路由、地图实例、AOI
- 大世界 MMO 更强调 distributed space、authority transfer、replication

所以差异更像组合方式不同，而不是底层完全不同。

### 3. BigWorld 更像最终增强态，不应成为默认起步态

BigWorld 模式确实是更完整的终态，但它不是所有项目都必须立即启用的默认形态。

更合理的表达应该是：

`BigWorld = Apollo 渐进式框架的一种高阶装配形态`

而不是：

`Apollo = 只能按 BigWorld 运行`

### 4. 模块化和进程化应该分开讨论

很多系统一开始最容易犯的错是：

- 业务模块一旦存在，就立刻独立成一个进程

这会导致过度拆分。

更合理的原则应该是：

- 先在模块边界上拆清楚
- 再根据负载、隔离性、可运维性决定是否独立部署

## 二、Apollo 应该拆成三层能力

建议 Apollo 从理论上分成这三层：

```text
通用底座层
    -> 业务域组件层
    -> 分布式世界增强层
```

## 三、第一层：通用底座层

这层不依赖具体游戏品类，应该尽量稳定。

### 1. 交互协议与连接底座

这是前端和服务端的共同入口。

建议 Apollo 把协议层拆成 4 个子层：

```text
Client Protocol
    -> Session Protocol
    -> Internal Envelope
    -> Replication Protocol
```

#### `Client Protocol`

职责：

- 客户端请求/响应定义
- 版本号与能力协商
- 请求序号
- 错误码
- 压缩与加密协商

推荐原则：

- 对外协议要稳定
- 业务 opcode 不直接和内部消息耦合
- 允许 WebSocket / TCP / HTTP 网关映射到同一业务协议

#### `Session Protocol`

职责：

- 登录票据
- 会话接管
- 心跳
- 重连恢复
- 网关切换

#### `Internal Envelope`

职责：

- 进程间内部消息信封
- trace id
- request id
- route version
- source/target app

这一层已经在 [RemoteEntityCall 设计](./remote-entity-call-design.md) 定义了方向。

#### `Replication Protocol`

职责：

- 实体进入视图
- 增量更新
- 离开视图
- alias/detail level

这一层已经在 [Replication Pipeline 设计](./replication-pipeline-design.md) 定义了方向。

### 2. 运行时与宿主底座

这一层负责：

- `ApplicationHost`
- 生命周期
- 配置
- 日志
- 监控
- watcher
- 热更新边界
- 服务注册与发现

这层应该尽量和业务解耦。

### 3. 实体与数据底座

这一层负责：

- `EntitySchema`
- `PropertySchema`
- `MethodSchema`
- `PersistenceSchema`
- `RemoteEntityRef`
- 基础组件系统

这层一旦立住，很多业务域都能复用。

### 4. 进程间通信底座

这一层负责：

- 内部消息总线
- request/reply
- one-way event
- publish/subscribe
- 可靠投递
- route refresh

它不应该直接等于一个 RPC 框架。

更准确地说，它应该是：

- 统一内部消息模型
- 统一路由解析
- 统一追踪和错误边界

### 5. 基础平台组件

这层建议直接视作所有游戏共用平台能力：

- 敏感词过滤
- 配置中心
- 灰度与开关
- 账号与认证
- 支付回调
- 风控
- 审计日志
- 运维观测

这些能力虽然不一定都是“游戏逻辑”，但它们几乎总是项目级刚需。

## 四、第二层：业务域组件层

这层是 Apollo 最需要做成“可组装”的部分。

也就是说：

- 这些能力应该是标准组件
- 但不要求每个项目全部启用

## 五、核心业务域建议

### 1. 登录域

职责：

- 认证
- 票据发放
- 角色选择
- 区服选择
- 登录限流
- 顶号策略

部署建议：

- 小项目可并入统一接入层
- 中大型项目建议独立 `login-app`

### 2. 网关与会话域

职责：

- 客户端连接
- 协议编解码
- 心跳
- 会话绑定
- 上下行转发

部署建议：

- 几乎总是独立 `gateway-app`

### 3. 玩家锚点域

职责：

- 玩家长期在线态
- 断线恢复
- world assignment
- route 主索引
- 跨系统玩家主状态收口

部署建议：

- 普通在线游戏和 MMO 都有价值
- 可作为 `BaseApp(PlayerAnchor Host)` 的目标语义

### 4. 聊天域

职责：

- 世界聊天
- 私聊
- 频道聊天
- 聊天限流
- 敏感词过滤
- 禁言

拆分建议：

- 小项目可内嵌业务服
- 中大型项目建议独立 `chat-app`

### 5. 战斗域

职责：

- 技能
- buff
- 属性计算
- 伤害结算
- 状态效果
- 战斗事件流

这一层要注意拆成两个子层：

- `规则层`
- `执行层`

因为不同游戏会复用不同程度的战斗规则。

### 6. 匹配与房间域

职责：

- 排队
- 匹配池
- ELO/MMR
- 房间创建
- 局生命周期

适用类型：

- 房间制竞技
- 回合制
- 战术竞技
- 部分副本制玩法

### 7. 社交域

职责：

- 好友
- 黑名单
- 邮件
- 工会
- 队伍
- 师徒
- 社交推荐

这一层通常适合独立成相对稳定的社交服务。

### 8. 任务与成就域

职责：

- 条件跟踪
- 事件订阅
- 状态推进
- 奖励发放

这层适合做成事件驱动组件，而不是塞进单个世界进程。

### 9. 活动域

职责：

- 定时活动
- 节日活动
- 限时商店
- 排行榜活动
- 跨服活动调度

这一层很适合做成可插拔活动引擎。

### 10. 支付与商城域

职责：

- 支付回调
- 订单状态
- 发货
- 防重
- 风险审计

这一层必须和实时世界逻辑弱耦合。

### 11. 世界域

职责：

- 地图实例
- AOI
- 场景实体
- 移动
- NPC
- 副本
- 世界事件

它是 MMO 相关项目的核心域，但不应该侵入所有其他业务域。

## 六、业务域应该如何装配

建议 Apollo 在理论上把项目装配分成 4 档。

### 档位 1：轻在线游戏

适合：

- 轻社交
- 放置
- 卡牌
- 小型回合制

推荐装配：

- login
- gateway
- player anchor
- chat
- task/achievement
- payment
- social

一般不需要：

- 复杂 world runtime
- distributed space

### 档位 2：房间制或局内战斗游戏

适合：

- FPS 房间制
- MOBA 房间制
- 棋牌
- 回合制对战

推荐装配：

- login
- gateway
- player anchor
- match
- room/battle
- chat
- social
- payment

关键点：

- 世界域弱
- 房间域强

### 档位 3：普通 MMO

适合：

- 分线 MMO
- 地图切换 MMO
- 副本型 RPG

推荐装配：

- login
- gateway
- player anchor
- world host
- chat
- task
- activity
- guild
- friend
- payment

关键点：

- 要有统一 `WorldHost`
- 要有玩家锚点
- 但不要求 distributed cell

### 档位 4：大世界 MMO

适合：

- 连续大世界
- 热点区域密度高
- 空间分片扩容需求强

推荐装配：

- 普通 MMO 全部能力
- distributed space
- ghost/witness
- authority transfer
- app manager
- advanced replication

这就是 BigWorld 增强态。

## 七、进程拆分应该从哪些角度判断

理论上建议按 6 个维度判断是否独立进程。

### 1. 连接性质

例如：

- gateway
- login
- chat gateway

凡是公网连接密集的，一般适合独立。

### 2. 状态归属性质

例如：

- player anchor
- room instance
- world instance

凡是需要明确主状态归属的，也适合独立成清晰宿主。

### 3. 吞吐模型

例如：

- chat 是高扇出
- match 是高排队吞吐
- battle 是高实时计算
- payment 是低频但高可靠

吞吐模型不同，就不应混在一个进程里。

### 4. 故障隔离

例如：

- 支付
- 敏感词
- 活动脚本

这些出问题不应直接拖死世界主进程。

### 5. 扩容模式

例如：

- gateway 按连接数扩
- chat 按广播量扩
- match 按排队量扩
- world 按地图实例扩
- distributed cell 按热点区域扩

扩容方式不同，就该分开。

### 6. 一致性要求

例如：

- 支付订单
- 发奖
- 玩家主状态

这类强一致要求高的能力，不适合混入高频世界 tick 进程。

## 八、推荐的理论进程版图

Apollo 最终可以支持一套“逐级升级”的理论进程版图。

### 最小版图

```text
gateway
login
game-app
platform-app
```

适合轻在线项目。

### 标准在线版图

```text
gateway
login
base-app (target: BaseApp / PlayerAnchor Host)
world-app (or current cell-app as WorldHost shell)
chat-app
social-app
activity-app
db-app / persistence-app
platform-app
```

适合普通 MMO 和多数在线 RPG。

### 大世界增强版图

```text
gateway
login
base-app (target: BaseApp / PlayerAnchor Host)
base-app-mgr
cell-app (distributed world node)
cell-app-mgr
chat-app
social-app
activity-app
db-app / persistence-app
platform-app
```

适合 BigWorld 模式。

这里要注意：

- `platform-app` 不是具体名字要求
- 它表示支付、风控、敏感词、公告、配置、运营等平台类能力

## 九、前后端交互协议建议

这部分必须尽早统一，因为它会影响所有业务域。

### 1. 外部协议要稳定，内部协议要可演进

不要让：

- 客户端 opcode
- 内部 app 间 message id

直接绑定成同一套编号。

### 2. 协议建议分三类

- `Client API Protocol`
- `Internal Service Protocol`
- `Replication Protocol`

### 3. 业务协议建议按领域拆包

例如：

- login
- player
- world
- battle
- chat
- guild
- friend
- activity
- payment

这有利于前后端协同和版本管理。

### 4. 所有协议都应支持版本协商

因为渐进式框架天然要求：

- 不同项目启用不同模块
- 同一项目在阶段演进中启用更多能力

没有版本边界，协议会越来越脆。

## 十、这种渐进式框架的主要风险

这条路线可行，但也有风险。

### 1. 过度抽象

如果一开始就把每个功能都抽成超泛化组件，会导致：

- 接口空泛
- 业务落不了地

### 2. 过早微服务化

如果每个业务域一开始都独立进程，复杂度会远超收益。

### 3. 协议层失控

如果没有统一 envelope、schema、版本约束，模块越多越乱。

### 4. 世界域和平台域边界混乱

例如：

- 支付回调直接改 world 内对象
- 聊天系统直接依赖 world 进程本地状态

这些都会让架构失稳。

## 十一、理论上的落地原则

如果 Apollo 走这条路线，建议坚持 5 条原则。

### 1. 先做模块边界，再决定进程边界

### 2. 先做普通 MMO 可用态，再做 BigWorld 增强态

### 3. 所有业务域都通过统一协议和事件边界协作

### 4. 让 `PlayerAnchor` 成为跨域主状态收口点之一

### 5. 让 world 只负责世界实时态，不吞掉全部业务

## 十二、结论

Apollo 做成一套渐进式、可组装、可复用的在线游戏框架，这条路在理论上完全可行。

而且从工程上看，这比“先固定成 BigWorld 全家桶”更现实。

更准确的目标应该是：

- Apollo 先沉淀一套通用在线游戏底座
- 在此之上装配登录、聊天、社交、任务、活动、支付等业务域组件
- 对 MMO 项目再叠加 `WorldHost + PlayerAnchor`
- 对连续大世界项目最后叠加 BigWorld 增强层

这样 Apollo 才不是“某一种 MMO 的实现”，而是一套能够覆盖多种在线游戏形态的服务器框架。

## 相关阅读

- [Apollo 世界架构路线](./apollo-world-architecture.md)
- [Apollo 架构差距分析](./apollo-architecture-gap-analysis.md)
- [Apollo 重构路线图](./apollo-refactor-roadmap.md)
- [PlayerAnchor 设计稿](./player-anchor-design.md)
- [Gateway 会话设计](./gateway-session-design.md)
- [EntitySchema 设计](./entity-schema-design.md)
- [RemoteEntityCall 设计](./remote-entity-call-design.md)
- [Replication Pipeline 设计](./replication-pipeline-design.md)
