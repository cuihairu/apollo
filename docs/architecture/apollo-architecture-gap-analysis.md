---
title: Apollo 架构差距分析
icon: list
order: 5
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - 差距分析
  - 重构路线
---

# Apollo 架构差距分析

这篇文档不是再讲目标，而是对照当前仓库现状，回答三个问题：

1. Apollo 现在已经有什么。
2. 距离“普通 MMO 模式”还缺什么。
3. 距离“BigWorld 模式”还缺什么。

这里的判断基于当前仓库结构和已有实现：

- `apps/`
- `modules/`
- `include/`
- 已有架构文档

术语基线见：

- [进程语义重定义](./process-semantics-redefinition.md)

## 一、当前仓库的总体判断

当前 Apollo 已经有了几个不错的基础：

- `runtime` 有统一宿主雏形
- `core` 有服务发现接口
- `game/core` 有基础实体对象
- `game/world` 有单机 `Scene` 和 AOI 基础实现
- `base-app` / `cell-app` / `gateway-app` / `login-app` 已经有原型进程
- `bigworld` 有兼容层雏形

但整体仍然处于：

`有若干方向正确的模块原型，但还没有收敛成统一世界运行时`

也就是说，现在的问题不是“完全没有代码”，而是：

- 这些模块之间还没有形成稳定分层
- 各个 app 仍然偏独立 demo
- 普通 MMO 模式还没有被明确定义成默认形态
- BigWorld 模式更没有形成真正的分布式空间骨架

还需要同步记住一个语义前提：

- 当前 `apps/base-app` 不是最终语义上的 `BaseApp`
- 当前 `apps/cell-app` 不是最终语义上的 `CellApp`
- `DBMgr / PersistenceService` 还没有被单独收成稳定进程角色

## 二、按层看当前状态

### 1. `modules/runtime`

现状：

- 已有 `ApplicationHost`
- 已有 `ServiceHost`
- 生命周期测试比较完整

结论：

- 这一层方向是对的
- 但它还只是通用宿主
- 还没有 `WorldHost`
- 还没有“游戏 tick + world runtime”这一层

判断：

- `普通 MMO 模式`: 半成品
- `BigWorld 模式`: 明显不够

### 2. `modules/core`

现状：

- 有 `service_discovery`
- 有配置、日志、生命周期等基础设施

结论：

- 可以支撑通用框架层
- 但还没有真正的控制面抽象
- watcher / registry / load query 还没形成统一体系

判断：

- `普通 MMO 模式`: 基础够用，但还缺运维面
- `BigWorld 模式`: 不够

### 3. `modules/game/core`

现状：

- 有 `Entity`
- 有 `IEntity`
- 有 component 风格扩展接口

结论：

- 已经有基础实体抽象
- 但它更像本地对象模型
- 还没有统一的 schema 系统
- 还没有 remote entity ref
- 还没有 replication / persistence 描述

判断：

- `普通 MMO 模式`: 勉强可做简单业务
- `BigWorld 模式`: 核心缺失

### 4. `modules/game/world`

现状：

- 有 `Scene`
- 有 AOI 实现
- 但 world 公开接口很薄

结论：

- 这是一个单机场景层起点
- 目前更接近“scene + aoi demo”
- 还不是完整 world runtime

缺少：

- `WorldSpace`
- 地图实例生命周期
- world tick 接口
- replication 语义
- witness 语义

判断：

- `普通 MMO 模式`: 有基础，但还没收口
- `BigWorld 模式`: 还没开始

### 5. `modules/bigworld`

现状：

- 有 BigWorld 兼容层
- 有 runtime / entity / timer / callback 包装

结论：

- 这是 API facade
- 不是分布式空间运行时
- 目前更适合承接兼容接口，而不是承接真正的 `Base/Cell` 语义

判断：

- `普通 MMO 模式`: 可选兼容层
- `BigWorld 模式`: 不能指望它单独完成架构能力

### 6. `apps/login-app`

现状：

- 已有登录流程
- 已有 gateway 分配
- 已有 session 管理雏形

结论：

- 方向是对的
- 但目前更像独立登录 demo 服务
- 还没有接入统一 world/runtime 语义

判断：

- `普通 MMO 模式`: 可作为起点
- `BigWorld 模式`: 还没和 BaseAppMgr 串起来

### 7. `apps/gateway-app`

现状：

- 已有客户端连接入口
- 已有 login/base/cell 转发逻辑
- 已有简单 cell 负载选择

结论：

- 当前 gateway 已经强行感知了太多后端角色
- 这在原型期没问题
- 但后面要避免它变成“知道全世界路由细节”的胖网关

判断：

- `普通 MMO 模式`: 可运行雏形
- `BigWorld 模式`: 路由语义还不够稳

### 8. `apps/base-app`

这里必须严格区分两个概念：

- `KBE 语义里的 BaseApp`
- `Apollo 当前仓库里的 apps/base-app`

它们现在不是一回事。

#### KBE 语义里的 `BaseApp`

在 KBEngine/BigWorld 里，`BaseApp` 更接近：

- 玩家长期归属节点
- 会话锚点
- 与 `Proxy` 强相关
- 非空间权威逻辑承载点

它不是单纯数据库服务。

这一点从 KBE 的 `proxy.h`、`baseapp.h` 就能看出来。

#### Apollo 当前 `apps/base-app` 的实现

从当前代码看：

- [apps/base-app/include/base/base_server.hpp](/C:/Users/cui/Workspaces/apollo/apps/base-app/include/base/base_server.hpp)
  - 直接暴露 `handleDbLoadRequest` / `handleDbSaveRequest` / `handleDbQueryRequest`
  - 持有 `DatabaseService` 和 `SaveQueue`
- [apps/base-app/src/base_server.cpp](/C:/Users/cui/Workspaces/apollo/apps/base-app/src/base_server.cpp)
  - 启动时初始化数据库
  - 对外处理 `DB_LOAD_REQUEST` / `DB_SAVE_REQUEST` / `DB_QUERY_REQUEST`
  - 当前核心行为是玩家数据加载和保存

所以基于“当前实现”来判断，它更接近：

- 一个数据服务原型
- 或者说“带玩家数据语义的 DB 服务”

而不是 KBE 语义上的 `BaseApp`。

#### 正确结论

因此不能说：

- `Apollo 里的 BaseApp 只能被理解成数据服务`

这在 KBE 语义上是错的。

更准确的说法应该是：

- `Apollo 当前 apps/base-app 的实现，行为上更像数据服务`
- `但 Apollo 将来如果要做真正的 BaseApp，语义应该向 KBE 的玩家锚点模型靠拢`

这是一个非常关键的差距。

判断：

- `当前实现`: 偏数据服务
- `目标语义`: 应该演进为玩家锚点型 `BaseApp`

### 9. `apps/cell-app`

现状：

- 有实体管理器
- 有 AOI 管理器
- 有基础位置与玩家实体

结论：

- 当前更像“单地图逻辑服原型”
- 还不是分布式 `CellApp`
- 缺少 ghost / witness / teleport / authority transfer

判断：

- `普通 MMO 模式`: 其实已经可以当 world server 原型
- `BigWorld 模式`: 还差很多核心能力

## 三、当前最重要的一个判断

Apollo 现在最接近的，不是 KBE 式 `BaseApp + CellApp` 架构，而是：

`Login + Gateway + World/Cell 原型 + DB 原型`

这意味着什么？

意味着 Apollo 现在最合理的默认模式，其实应该是：

`普通 MMO 模式`

而不是强行把当前 `base-app` 和 `cell-app` 解释成已经具备 KBE 语义。

## 四、距离“普通 MMO 模式”还缺什么

这部分才是 Apollo 最应该优先补的。

### 1. 缺统一 `WorldHost`

现在有 `ApplicationHost` / `ServiceHost`，但没有专门的游戏世界宿主。

缺少：

- world tick
- scene/instance 生命周期
- 统一 world service 装配
- 游戏进程主循环语义

### 2. 缺统一 world 对象模型

现在有：

- `Entity`
- `Scene`
- AOI

但缺：

- `WorldSpace`
- `MapInstance`
- `PlayerSession`
- `PlayerAnchor`
- `WorldEntityRuntime`

### 3. 缺登录到世界的统一流转

当前 `login-app`、`gateway-app`、`cell-app` 之间已有原型交互，但没有统一模型约束：

- 玩家什么时候绑定 session
- 玩家什么时候进入 world
- 玩家切图时锚点归谁

### 4. 缺统一消息边界

现在有协议层，但 world 内部消息和组件边界还没有真正收口。

需要补：

- app 间消息模型
- world 内事件模型
- 玩家路由模型

### 5. 缺统一控制面

现在虽然有 `service_discovery`，但还没有：

- watcher tree
- app load report
- world/scene query
- session query

### 对应优先级

这部分应该是最高优先级，因为它决定 Apollo 能不能先成为一个可用的普通 MMO 框架。

## 五、距离“BaseApp 模式”还缺什么

这里说的不是当前 `apps/base-app`，而是 KBE 意义上的 `BaseApp`。

### 当前缺口

#### 1. `BaseApp` 语义不对

当前 `base-app` 更像数据库服务，不像玩家锚点服务。

#### 2. 缺 `PlayerAnchor`

Apollo 还没有一个清晰对象代表：

- 玩家长期归属
- 跨图稳定身份
- 非空间权威逻辑

#### 3. 缺 `BaseAppMgr`

当前没有一个真正的 base 控制面来做：

- 玩家分配
- base 负载选择
- 基于玩家锚点的路由

### 结论

Apollo 如果下一阶段想比“普通 MMO world server”再往前一步，最值得补的是：

- `PlayerAnchor`
- `BaseApp`
- `BaseAppMgr`

而不是直接跳到 `CellApp` 分布式空间。

## 六、距离“BigWorld 模式”还缺什么

这部分差距最大。

### 1. 缺真正的 distributed `CellApp`

当前 `cell-app` 只有：

- 本地实体
- 本地 AOI

缺少：

- 跨 cell authority
- entity transfer
- ghost replica
- witness
- cross-cell replication

### 2. 缺 `CellAppMgr`

当前没有真正的空间控制面来管理：

- space -> cell 映射
- cell 负载
- 空间迁移
- 空间查询

### 3. 缺空间运行时对象

当前 `Scene` 太薄，不能承接 KBE 意义上的 space。

缺：

- geometry/runtime space data
- distributed partition
- observer state
- cell-local world state

### 4. 缺 remote entity model

当前实体系统还是本地对象中心，缺：

- `RemoteEntityRef`
- `EntitySchema`
- `ReplicationSchema`
- `AuthorityState`

### 5. 缺 BigWorld 模式下的控制面

BigWorld 模式至少需要：

- space query
- cell load
- migration status
- entity ownership query

这部分在 Apollo 里基本还没成型。

## 六点五、`apps/base-app` 应该怎么演进成真正的 `BaseApp`

这是当前 Apollo 架构里最容易混淆、也最值得尽早定下来的点。

### 1. 当前 `apps/base-app` 已经承担了什么

基于当前实现，它已经承担了这些职责：

- 玩家数据加载
- 玩家数据保存
- 数据查询
- 异步保存队列
- 基于 RPC 的数据访问入口

也就是说，它已经有了“数据相关后端服务”的起点。

### 2. 但它还没有真正的 `BaseApp` 语义

如果要接近 KBE/BigWorld 风格的 `BaseApp`，它还缺这些核心职责：

- 玩家长期在线锚点
- 玩家登录后稳定归属
- `Session -> PlayerAnchor` 绑定
- 非空间逻辑归属
- 跨地图/跨实例时玩家状态持续存在
- 与 world/cell 的远程实体协作

换句话说，当前 `apps/base-app` 更像：

- `PlayerDataService`

而不是：

- `PlayerAnchorService`

### 3. Apollo 后续有两种可行路线

#### 路线 A：保留 `base-app` 名字，内部逐步升级

这条路线的思路是：

- 保留现在的 `apps/base-app`
- 先继续保留数据加载/保存职责
- 再逐步往里补玩家锚点语义

最终让 `base-app` 同时承接：

- 玩家长期状态
- 玩家归属与会话锚点
- 数据持久化协调

优点：

- 迁移成本低
- 名字与目标语义最终一致
- 适合当前仓库演进式重构

缺点：

- 在过渡阶段会出现“数据服务职责”和“玩家锚点职责”混在一起
- 需要很明确地控制模块边界，否则容易继续膨胀

#### 路线 B：拆出独立数据服务，再重新实现真正的 `BaseApp`

这条路线的思路是：

- 把当前 `apps/base-app` 的数据库职责拆出来
- 形成独立的 `db-app` 或 `data-service`
- 再单独实现真正的 `BaseApp`

此时：

- 数据服务专注存取
- `BaseApp` 专注玩家锚点与长期权威

优点：

- 语义最清晰
- 更接近 KBE 分工
- 后续扩展更稳

缺点：

- 当前阶段改动更大
- 需要新增一个 app 和更多基础设施

### 4. 对 Apollo 当前阶段的建议

更现实的建议是先走路线 A，再视复杂度决定是否过渡到路线 B。

原因很简单：

- 当前 Apollo 还没有先把普通 MMO world runtime 收住
- 这时直接把 `base-app` 再拆一次，收益未必最大
- 更合理的顺序是先把 `base-app` 的目标语义定义清楚，再决定是否拆分

也就是说：

#### 短期

先保留 `apps/base-app`，但明确它未来不是“纯 DB 服务”，而是要演进成真正的 `BaseApp`。

#### 中期

在 `base-app` 内补出：

- `PlayerAnchor`
- `SessionBinding`
- 玩家归属管理
- world 切换时的长期状态协调

#### 后期

如果发现数据职责和玩家锚点职责耦合过重，再把数据访问部分拆成独立 `db-app`。

### 5. 这意味着代码层面要补什么

如果 Apollo 选择“保留 `base-app` 并逐步升级”，那下一步最该补的不是数据库代码，而是以下对象：

- `PlayerAnchor`
- `AnchorManager`
- `PlayerResidence`
- `SessionLocator`
- `WorldAssignment`

这些对象的职责大致如下：

#### `PlayerAnchor`

表示玩家长期归属对象，应该承载：

- 玩家唯一身份
- 当前在线状态
- 当前绑定 session
- 当前所在 world/space
- 长期业务状态引用

#### `AnchorManager`

负责：

- 创建/查找/销毁锚点
- 玩家登录后分配锚点
- 断线重连时恢复锚点

#### `SessionLocator`

负责：

- 某个玩家当前在哪个 gateway/session
- 某个 session 对应哪个玩家锚点

#### `WorldAssignment`

负责：

- 玩家当前应路由到哪个 world 实例
- 切图/切实例时更新路由

### 6. 一个关键判断

Apollo 现在如果直接把当前 `base-app` 理解成“已经是 BaseApp”，会误判成熟度。

但如果直接把它否定成“只是 db-app”，又会错失一个自然演进点。

更准确的判断是：

- 它现在的实现重心偏数据服务
- 但它最合适的未来方向，是演进成真正的 `BaseApp`

这才是对当前代码和目标架构都比较准确的表述。

## 六点七五、`apps/cell-app` 现在更像 world 原型，而不是真正的 `CellApp`

`apps/cell-app` 这个名字现在也很容易造成误解。

和 `base-app` 一样，这里也必须区分两个层次：

- `KBE/BigWorld 语义里的 CellApp`
- `Apollo 当前仓库里的 apps/cell-app`

### 1. KBE 语义里的 `CellApp` 是什么

在 KBEngine 里，`CellApp` 不是普通地图服，而是：

- 空间内实时权威节点
- 承担位置、移动、战斗、AOI
- 承担 `Witness`
- 承担 `Ghost`
- 承担跨 Cell 迁移
- 和 `CellAppMgr` 一起形成分布式空间

也就是说，真正的 `CellApp` 是：

`分布式空间运行时节点`

### 2. Apollo 当前 `apps/cell-app` 的实现是什么

从当前代码看：

- [apps/cell-app/include/cell/cell_manager.hpp](/C:/Users/cui/Workspaces/apollo/apps/cell-app/include/cell/cell_manager.hpp)
  - 有本地 `Entity`
  - 有 `PlayerEntity`
  - 有 `AOIManager`
  - 有 `EntityManager`
- [apps/cell-app/src/cell_server.cpp](/C:/Users/cui/Workspaces/apollo/apps/cell-app/src/cell_server.cpp)
  - 主要是单进程内的实体管理、AOI 更新和消息处理

这说明当前 `cell-app` 已经具备了一个很重要的起点：

- 单机 world / map server 原型

但是它还没有完整 `CellApp` 的几个关键语义：

- 没有 `Ghost`
- 没有 `Witness`
- 没有跨 cell authority transfer
- 没有 `CellAppMgr`
- 没有 distributed space
- 没有 cross-cell teleport 协议

### 3. 正确结论

因此更准确的说法应该是：

- `Apollo 当前 apps/cell-app 的实现，更像单机 world server 原型`
- `它还不是 KBE 语义上的分布式 CellApp`

这个判断并不是否定它，反而是在明确它当前真正的价值。

因为从架构演进上看，这种“单机 world 原型”恰恰是 Apollo 最应该先做扎实的层。

### 4. 这意味着什么

如果 Apollo 现在就把 `cell-app` 当成“已经实现了 CellApp”，会带来两个问题：

- 高估当前分布式空间能力
- 让后续设计在错误前提上继续叠加

但如果把它理解为：

- `普通 MMO 模式下的 world-app 原型`

那很多事情就顺了：

- 它现在的 AOI 是合理的
- 它当前的单进程实体管理是合理的
- 它可以先成为 Apollo 默认世界服
- 将来再按需进化成真正的 `CellApp`

### 5. Apollo 后续也有两条路线

#### 路线 A：保留 `cell-app` 名字，逐步向真正 `CellApp` 升级

这条路线的思路是：

- 保留当前 app 名称
- 先继续做单机 world runtime
- 后续逐步补 distributed space 能力

优点：

- 名字连续
- 未来若真的做 BigWorld 模式，不需要迁移 app 名称

缺点：

- 在当前阶段很容易误导阅读者，以为已经有完整 `CellApp`
- 文档和设计沟通成本更高

#### 路线 B：先把它明确重定位成 `world-app`，以后再引入真正 `CellApp`

这条路线的思路是：

- 承认当前实现本质上是单机世界服
- 先把普通 MMO 模式收敛为 `world-app`
- 等 distributed space 真正开始做时，再新建真正的 `cell-app`

优点：

- 语义最清晰
- 当前能力边界很明确
- 更适合 Apollo 先走普通 MMO 模式

缺点：

- 名称需要调整
- 已有认知需要迁移

### 6. 对 Apollo 当前阶段的建议

从架构清晰度来说，我更倾向于路线 B。

原因是：

- Apollo 当前最该先完成的是“普通 MMO 模式”
- 当前 `cell-app` 已经非常接近这个模式里的 `world-app`
- 这时继续坚持 `cell-app` 这个名字，收益不大，误导很大

也就是说，较合理的方向是：

- 当前 `apps/cell-app` 在架构上按 `world-app` 理解
- 文档中明确它是单机 world/runtime 原型
- 等真正做 distributed space 时，再决定是否恢复 `cell-app` 这一术语

### 7. 如果暂时不改名，至少也要在文档里改语义

如果当前不想动目录名，那至少要统一文档语义：

- 当前 `apps/cell-app` = `单机 world server 原型`
- 未来 `CellApp` = `分布式空间节点`

这两者不能继续混用。

### 8. 当前 `cell-app` 下一步最该补什么

不是 ghost，不是跨 cell，同样也不是先造 `CellAppMgr`。

当前最该补的是普通 MMO 世界服真正缺的能力：

- `WorldHost`
- `MapInstance`
- 统一 world tick
- 玩家进入/离开世界流程
- `Session -> Player -> AvatarEntity` 映射
- 场景级广播与 replication
- scene lifecycle

这些补齐后，Apollo 才算真正有了稳定的普通 MMO 模式。

### 9. 真正的 `CellApp` 以后要额外补什么

等 Apollo 真的进入 BigWorld 模式，再在当前 world runtime 之上增加：

- `Witness`
- `GhostReplica`
- authority transfer
- cross-cell teleport
- space partition
- `CellAppMgr`

这样演进路径会清楚很多。

## 七、Apollo 的实际重构顺序应该是什么

按照收益和风险排序，我建议是：

### 第一阶段：把 Apollo 先收成“普通 MMO 模式”

先完成：

- `WorldHost`
- `Scene/MapInstance` 收口
- `Session -> World` 统一流转
- `gateway/login/world` 路由关系稳定
- 控制面最小闭环

这是最现实也最有产出的阶段。

### 第二阶段：把 `BaseApp` 做对

先完成：

- `PlayerAnchor`
- `BaseApp`
- `BaseAppMgr`

此时 Apollo 就能形成：

- 登录入口
- 玩家锚点
- 世界实例

这已经比普通 MMO 架构更稳了。

### 第三阶段：按需补 `CellApp`

先完成：

- distributed `WorldSpace`
- `GhostReplica`
- `Witness`
- authority transfer
- `CellAppMgr`

只有项目明确需要时才进入这一阶段。

## 八、建议的模块调整

### 1. `apps/cell-app` 应考虑重命名或重新定位

以当前实现看，它更像：

- `world-app`
- 或 `map-app`

而不是 KBE 意义上的 `CellApp`。

如果继续沿用 `cell-app` 这个名字，很容易让后续设计误判现状成熟度。

### 2. `apps/base-app` 应重新定义职责

不是说它应该直接改名成 `db-app`。

更准确地说，当前实现承担的是偏数据服务职责，而名字使用了 `base-app`。

因此后续有两个选择：

- 方案 A：保留 `base-app` 名字，但逐步把它补成真正的玩家锚点型 `BaseApp`
- 方案 B：把当前实现拆成独立数据服务，再重新实现真正的 `BaseApp`

这两种都可以，但不能继续让“当前实现职责”和“KBE 术语语义”长期错位。

如果走方案 B，更合理的名字应是：

- `db-app`
- `persistence-app`
- `PersistenceService`

而不是继续把数据库执行层写成 `BaseApp`。

### 3. `modules/game/world` 应升级为真正的 world runtime

建议它成为普通 MMO 模式的核心层，而不是只保留 `Scene + AOI`。

### 4. `modules/bigworld` 应保持兼容层定位

不要把真正的 distributed world runtime 全堆到兼容层里。

更合理的是：

- world runtime 在 `modules/game/world` 或未来新的 distributed world 模块
- `modules/bigworld` 只做 facade / adapter

## 九、一个简化的现状评估表

| 能力 | 当前状态 | 结论 |
|------|----------|------|
| 统一宿主 | 已有基础 | 可继续扩展 |
| 普通 MMO world runtime | 半成品 | 应优先补齐 |
| 登录/网关原型 | 已有 | 需要和 world 收口 |
| 基础实体模型 | 已有 | 还缺 schema 和 remote ref |
| BaseApp 玩家锚点 | 基本缺失 | 下一阶段重点 |
| CellApp 分布式空间 | 基本缺失 | 暂不优先 |
| 控制面/观测面 | 雏形不足 | 需要统一设计 |

## 十、结论

Apollo 当前最合理的架构判断是：

- 还没有形成真正的 BigWorld/KBE world runtime
- 但已经具备演进到“普通 MMO 模式”的良好基础
- 下一步最应该做的是把普通 MMO 模式收拢
- 再把 `BaseApp` 做成玩家锚点
- 最后按需升级到 `CellApp` 分布式空间

如果顺序反过来，Apollo 很容易在还没有收住基础 world runtime 时，就陷入复杂的 BigWorld 名词体系，结果两边都做不扎实。

## 相关阅读

- [Apollo 世界架构路线](./apollo-world-architecture.md)
- [Apollo 重构路线图](./apollo-refactor-roadmap.md)
- [进程语义重定义](./process-semantics-redefinition.md)
- [进程重组与实施路线](./process-reorganization-and-rollout-plan.md)
- [持久化进程与 DBMgr 设计](./persistence-process-and-db-manager-design.md)
