---
title: KBEngine 源码分析
icon: code
order: 3
category:
  - 架构
  - KBEngine
tag:
  - KBEngine
  - 源码分析
  - BigWorld
  - MMO
---

# KBEngine 源码分析


这篇文档基于本地源码目录 `C:\Users\cui\Workspaces\kbengine` 整理，目标不是做概念介绍，而是回答两个更实际的问题：

1. KBEngine 这套架构在源码里到底是怎么落下来的。
2. Apollo 现在还不完善时，哪些设计最值得直接吸收，哪些只适合借鉴思想而不适合照搬。

如果只看 BigWorld/KBEngine 的架构图，很容易把它理解成“多进程 MMO 服务器”。但看完源码之后会发现，它真正的价值不在于进程多，而在于它把以下几件事收成了一套统一运行时：

- 进程宿主与生命周期
- 网络消息与跨进程调用
- 实体定义、脚本绑定与同步元数据
- 空间权威、AOI、Witness、Ghost
- 集群调度、观测和运维

这几件事是一体的。

## 一、建议的阅读顺序

第一次系统读 KBEngine，建议按下面顺序走：

1. 启动宿主
2. 公共服务端骨架
3. 网络与消息
4. 实体定义系统
5. BaseApp / CellApp
6. Space / Witness / Ghost
7. BaseAppMgr / CellAppMgr / Machine / Watcher

不要一上来就扎进 `cellapp/entity.cpp`。那样很快会淹没在细节里。

## 二、源码分层

KBEngine 的源码分层很清楚，目录本身就在表达架构。

### 1. 进程层

目录：`kbe/src/server/`

关键子目录：

- `baseapp`
- `baseappmgr`
- `cellapp`
- `cellappmgr`
- `dbmgr`
- `loginapp`
- `machine`
- `tools/logger`
- `tools/interfaces`

这一层表达的是“集群中的进程角色”。

### 2. 公共运行时层

目录：`kbe/src/lib/`

最关键的几个库：

- `server`
- `network`
- `entitydef`
- `helper`
- `pyscript`
- `resmgr`
- `thread`
- `db_interface`

这一层表达的是“所有进程共享的基础运行时”。

### 3. 资源与脚本层

目录：`kbe/res/`

核心子目录：

- `server`
- `scripts`
- `client`

KBEngine 不是纯 C++ 引擎，脚本资源是运行时的一等输入。

## 三、启动入口是统一模板，不是每个进程各写一套

先看各进程的 `main.cpp`：

- `kbe/src/server/baseapp/main.cpp`
- `kbe/src/server/cellapp/main.cpp`
- `kbe/src/server/baseappmgr/main.cpp`
- `kbe/src/server/cellappmgr/main.cpp`
- `kbe/src/server/dbmgr/main.cpp`
- `kbe/src/server/loginapp/main.cpp`
- `kbe/src/server/machine/main.cpp`

这些入口都很薄，核心都是：

1. 包含本进程会用到的接口定义头。
2. 从 `g_kbeSrvConfig` 读取本进程的监听参数。
3. 调用 `kbeMainT<T>()` 启动具体服务。

真正的总入口在：

- `kbe/src/lib/server/kbemain.h`

这里定义了 `kbeMainT<SERVER_APP>()`。

### `kbeMainT` 做了什么

从源码看，它至少统一处理了下面这些事：

- 校验和生成 `componentID`
- 设置环境变量，例如 `KBE_COMPONENTID`
- 初始化日志与内存检测
- 加载密钥文件
- 初始化 `Resmgr`
- 创建 `EventDispatcher`
- 创建 `NetworkInterface`
- 初始化 `Components`
- 构造具体 `SERVER_APP`
- 调用 `app.initialize()`
- 最后调用 `app.run()`

这说明 KBEngine 的第一个关键设计是：

`进程差异在组件类，不在宿主框架`

这点对 Apollo 非常重要。Apollo 现在如果每个 app 都自己拼配置、网络、生命周期、信号、服务发现，后面一定会散。

## 四、公共服务端骨架：`ServerApp` 和 `EntityApp`

真正把各个进程统一起来的，不是 `main.cpp`，而是这两个类：

- `kbe/src/lib/server/serverapp.h`
- `kbe/src/lib/server/entity_app.h`

### 1. `ServerApp`

`ServerApp` 是所有服务进程的公共父类。

从 `serverapp.h` 可以直接看出它的职责：

- `SignalHandler`
- `TimerHandler`
- `ShutdownHandler`
- `ChannelTimeOutHandler`
- `ChannelDeregisterHandler`
- `ComponentsNotificationHandler`

这不是普通的业务基类，它是整个进程运行时的框架类。

#### `ServerApp::initialize()`

在 `serverapp.cpp` 里，这个初始化流程基本串起了整个宿主生命周期：

1. 安装信号处理
2. 初始化线程池
3. 加载配置
4. `initializeBegin()`
5. `inInitialize()`
6. `initializeEnd()`
7. 初始化网络与 watcher

也就是说，具体组件只需要填三个阶段钩子：

- `initializeBegin`
- `inInitialize`
- `initializeEnd`

这是一种很稳定的分层方式。

#### `ServerApp::run()`

`run()` 最核心的一句是：

`dispatcher_.processUntilBreak();`

因此从运行模型上看，KBEngine 本质上是：

`事件循环 + 定时器 + 网络消息 + 组件回调`

而不是“每个服务自己控制主循环”。

### 2. `EntityApp<E>`

`EntityApp` 在 `ServerApp` 上再加了一层实体运行时。

从 `entity_app.h` 能看到它增加了这些关键能力：

- 脚本初始化
- `EntityDef` 安装
- `EntityCall` 绑定
- `Entities<E>` 容器
- `EntityIDClient`
- `PY_CALLBACKMGR`
- game tick 驱动

#### `EntityApp` 最关键的两件事

第一件：

它在构造时把 `EntityDef::setGetEntityFunc(...)` 和 `EntityCallAbstract::setFindChannelFunc(...)` 接到当前 app 上。

这意味着：

- `entitydef` 不直接依赖具体进程
- `entitycall` 也不直接依赖具体实体容器
- 具体 app 通过函数绑定把这两者接起来

这是一个很成熟的解耦点。

第二件：

它在 `initialize()` 里注册了固定频率的 game tick：

- `dispatcher().addTimer(...)`
- 频率来自 `gameUpdateHertz`

所以 KBEngine 不是所有逻辑都靠消息驱动，它有明确的 tick 驱动核心。

## 五、网络运行时：KBE 的消息模型为什么长期可维护

网络核心入口：

- `kbe/src/lib/network/network_interface.h`

相关基础类：

- `channel.h`
- `endpoint.h`
- `bundle.h`
- `packet.h`
- `message_handler.h`

### 1. `NetworkInterface`

`NetworkInterface` 是每个进程的网络中枢。

从头文件能直接看出它负责：

- 维护外网 TCP/UDP 与内网 TCP 端点
- 管理 `ChannelMap`
- 绑定 `EventDispatcher`
- 处理 `Channel` 注册/注销
- 处理 channel timeout
- 统一 `processChannels`

KBEngine 的一个关键点是区分：

- 外部连接
- 内部连接

这不只是多开一个端口，而是明确区分两种语义：

- 外部连接要处理 client 协议与安全边界
- 内部连接要处理组件间通信和服务编排

### 2. `Channel`

`Channel` 是逻辑连接抽象，不只是 socket。

它承接了：

- 地址绑定
- 发送队列
- 超时管理
- 和消息处理器的协作

### 3. `Bundle`

`Bundle` 很值得 Apollo 认真参考。

它的价值是：

- 将多条逻辑消息聚合为一次发送
- 支持按协议写入 message header 和 payload
- 避免业务层直接操作裸 buffer

这让“远程实体方法调用”“属性同步”“广播”最终都能收敛到一个统一的发送载体里。

### 4. `MessageHandler`

KBEngine 并不是用一套通用字符串路由做 RPC，而是有显式的消息声明与处理器。

优点是：

- 调度稳定
- 序列化边界明确
- 性能和内存布局可控

代价是：

- 可读性一般
- 扩展时需要改接口定义和宏

## 六、`entitydef` 是 KBEngine 最核心的壁垒

关键目录：

- `kbe/src/lib/entitydef/`

关键文件：

- `entitydef.h`
- `scriptdef_module.h`
- `property.h`
- `method.h`
- `datatype.h`
- `entity_call.h`
- `entity_component.h`
- `volatileinfo.h`

如果只看 `BaseApp` 和 `CellApp`，会误以为 KBEngine 的核心是“分布式服务编排”。实际不是。

它最深的壁垒是把“实体定义”做成了统一元数据系统。

### 1. `EntityDef`

从 `entitydef.h` 看，`EntityDef` 负责：

- 初始化实体定义系统
- 加载全部脚本模块
- 加载 property / method / interface / component 描述
- 建立 `script module -> utype` 映射
- 维护 defs 的 MD5

这是实体系统的全局注册中心。

#### 它不是只做配置加载

`EntityDef` 的意义在于把下面几个问题绑定在一起：

- 这个实体有哪些属性
- 属性属于 base 还是 cell 还是 client
- 哪些属性持久化
- 哪些方法可远程调用
- 哪些属性支持 alias 压缩
- 哪些数据是 volatile 的

这就是它为什么重，但也为什么强。

### 2. `ScriptDefModule`

`ScriptDefModule` 是单个实体脚本模块的运行时描述对象。

从 `scriptdef_module.h` 能看到它维护：

- `PROPERTYDESCRIPTION_MAP`
- `METHODDESCRIPTION_MAP`
- component 描述
- detail level
- volatile info
- base / cell / client 三侧能力标记

这意味着一个实体定义，不是“一张表”，而是一整套跨边界描述：

- 本地对象结构
- 远程方法表
- 客户端同步裁剪规则
- 组件化扩展点

### 3. `EntityCall`

`entity_call.h` 很关键。

它不是普通 RPC 代理，而是“分布式实体引用”。

它携带的信息包括：

- 实体类型
- 目标地址
- 组件 ID
- 实体 ID
- call 类型

也就是说，KBE 里的远程调用模型不是“请求某个服务上的某个方法”，而是“拿着实体引用去访问实体侧的方法”。

这和传统微服务完全不是一套语义。

### 4. `entity_component`

源码里确实有 `entity_component.h/.cpp`，但不能把它误读成 ECS。

更准确地说，它解决的是：

- 实体定义的复用
- 子组件级别的 property/method 描述
- 组件内属性同步与脚本描述整合

它不是为了数据导向批处理，也不是为了高频系统遍历。

## 七、脚本桥接是主路径，不是插件

关键目录：

- `kbe/src/lib/pyscript/`
- `kbe/src/lib/python/`

以及：

- `entitydef/py_entitydef.*`
- `entitydef/scriptdef_module.*`

从 `EntityApp` 的初始化流程也能看出来：

- 先装 Python 环境
- 再装 Python 模块
- 再装 `EntityDef`

这个顺序很说明问题。

### 设计含义

KBEngine 的业务对象不是“C++ 里创建完了，再顺便给 Python 一份映射”。

它更接近：

- C++ 提供宿主、网络、定时器、持久化和实体运行时
- Python 提供业务实体的脚本定义和行为扩展

这就是为什么实体定义、脚本模块、远程方法、属性同步会绑在一起。

## 八、BaseApp：它不是数据库层，而是玩家会话锚点

关键文件：

- `kbe/src/server/baseapp/baseapp.h`
- `kbe/src/server/baseapp/entity.h`
- `kbe/src/server/baseapp/proxy.h`

### 1. `Baseapp`

从 `baseapp.h` 可以直接看出，它负责：

- 创建实体
- 从 DB 创建实体
- `createEntityAnywhere`
- `createEntityRemotely`
- 创建 cell entity
- 管理客户端 proxy
- 转发消息
- 处理备份与归档

这已经说明它绝不是单纯的数据服务。

### 2. `Proxy`

`proxy.h` 非常关键，它基本暴露了会话锚点的真实语义：

- 保存客户端地址
- 向客户端发送 bundle
- 统计 RTT
- 获取 `timeSinceHeardFromClient`
- 是否有客户端附着
- `giveClientTo`
- `onGetWitness`
- `kick`

这说明 KBEngine 把“玩家连接”和“玩家实体”之间放了一个明确的代理对象。

这个代理对象负责：

- 连接状态
- 带宽与加密
- 登录数据
- 客户端转移
- 与 witness 的衔接

### 为什么这点重要

很多项目里会把“玩家在线状态”“socket 连接”“角色实体”揉成一个对象。前期简单，后期会非常难扩展。

KBEngine 明确把这些概念拆开：

- `Proxy`: 会话与客户端承载
- `BaseEntity`: 玩家长期权威逻辑
- `CellEntity`: 空间内实时权威逻辑

## 九、CellApp：它不是逻辑服，而是空间权威节点

关键文件：

- `kbe/src/server/cellapp/cellapp.h`
- `kbe/src/server/cellapp/entity.h`
- `kbe/src/server/cellapp/spacememory.h`
- `kbe/src/server/cellapp/witness.h`
- `kbe/src/server/cellapp/ghost_manager.h`
- `kbe/src/server/cellapp/view_trigger.h`

### 1. `Cellapp`

从 `cellapp.h` 里能直接看到它承担的能力：

- 创建和销毁 cell entity
- 处理 entity call
- 处理 client -> cell 的转发消息
- 处理 ghost property / volatile data 更新
- 处理 `reqTeleportToCellApp`
- 持有 `GhostManager`
- 管理 `Updatable`

所以 `CellApp` 的本质是：

`空间内实时权威模拟节点`

### 2. `cellapp::Entity`

`cellapp/entity.h` 是 MMO 语义最浓的文件之一。

从头文件能直接看到：

- `isReal`
- `hasGhost`
- `realCell`
- `ghostCell`
- `baseEntityCall`
- `clientEntityCall`
- `controlledBy`
- `position`
- `direction`
- `teleport`
- `setViewRadius`
- `getWitnesses`
- `backupCellData`

这说明 Cell 侧实体同时承载：

- 权威状态
- 客户端控制关系
- 空间位置
- 传送与迁移
- AOI 可视范围
- 持久化前的 cell 数据回写

这已经不是普通 ECS 或普通 scene entity 的概念了，而是“分布式空间实体”。

## 十、`SpaceMemory`：空间不是 ID，而是运行时世界容器

关键文件：

- `kbe/src/server/cellapp/spacememory.h`

从头文件可以看出 `SpaceMemory` 至少管理：

- 当前 space 的 `SPACE_ID`
- 实体集合
- 当前所属 `Cell`
- `CoordinateSystem`
- `NavigationHandle`
- `spaceData`

### 它负责什么

#### 1. 空间内实体生命周期

- `addEntity`
- `removeEntity`
- `onEnterWorld`
- `onLeaveWorld`

#### 2. 空间几何和导航

- `loadSpaceGeometry`
- `unLoadSpaceGeometry`
- `NavigationHandle`

#### 3. 空间级共享数据

- `setSpaceData`
- `getSpaceData`
- `delSpaceData`

#### 4. 空间更新

- `update()`

### 设计结论

KBE 的 Space 不是“地图 ID”，而是一个实际参与运行时调度的对象。

对 Apollo 的对应边界是既有 Scene：跨服场景仍由 Scene 持有实体、AOI 与 tick，不能因参考 SpaceMemory 再引入同义运行时对象。

## 十一、`Witness`：客户端看到的世界不是直接从实体表里扫出来的

关键文件：

- `kbe/src/server/cellapp/witness.h`
- `kbe/src/server/cellapp/view_trigger.h`
- `kbe/src/server/cellapp/entityref.h`

### 1. `Witness` 是什么

从 `witness.h` 看，它维护：

- `viewRadius`
- `viewHysteresisArea`
- `ViewTrigger`
- `viewEntities`
- `clientViewSize`
- 发送 bundle 缓冲

它还提供：

- `onEnterView`
- `onLeaveView`
- `sendToClient`
- `addUpdateToStream`
- `resetViewEntities`

所以 `Witness` 本质上是：

`客户端视野上下文`

它不是实体本身，而是“这个实体对应的客户端应该看见什么”的运行时状态机。

### 2. 为什么有 `ViewTrigger`

`view_trigger.h` 显示 `ViewTrigger` 继承自 `RangeTrigger`，负责：

- 坐标节点进入范围
- 坐标节点离开范围

这说明 AOI 在 KBE 里不是简单遍历，而是：

- 坐标系统负责空间组织
- Trigger 负责边界事件
- Witness 负责客户端视图集合

### 3. `EntityRef` 的价值

KBE 没有直接把 view 集合做成 `Entity*` 列表，而是有显式的 `EntityRef`。

这通常意味着它需要在“实体存在”“别名 ID”“同步状态”“延迟移除”之间做更多管理。

## 十二、`GhostManager`：分布式空间里最容易被忽视但最关键的设计

关键文件：

- `kbe/src/server/cellapp/ghost_manager.h`

从头文件可以直接读出它的职责：

- 管理 `realEntities_`
- 管理 `ghost_route_`
- 暂存待广播 `messages_`
- 同步 ghost
- 维护路由

### 为什么需要它

当一个实体迁移、切 Cell 或临时跨边界协作时，会出现这样的问题：

- 某些消息还在飞
- 权威实体位置已经切换
- 某些 Cell 上还保留 ghost 投影

如果没有一层专门的 ghost 路由和转发管理，跨 Cell 世界会非常脆弱。

### 这也是 KBE 和普通场景服最大的分水岭之一

普通副本制服务端里，实体通常只在一个进程里活着。

KBE 里不是。

KBE 假设实体可能：

- 在一个 Cell 上是 real
- 在另一个 Cell 上是 ghost
- 在迁移窗口里还需要 route

这才是真正意义上的分布式世界。

## 十三、CellAppMgr：它管理的不是进程列表，而是空间拓扑

关键文件：

- `kbe/src/server/cellappmgr/cellappmgr.h`
- `kbe/src/server/cellappmgr/cellapp.h`
- `kbe/src/server/cellappmgr/space.h`

### 1. `Cellappmgr`

从头文件能直接看到它负责：

- `findFreeCellapp`
- `updateBestCellapp`
- `reqCreateCellEntityInNewSpace`
- `reqRestoreSpaceInCell`
- `updateCellapp`
- `querySpaces`
- `updateSpaceData`
- `setSpaceViewer`

这说明 `CellAppMgr` 不是简单注册中心，而是空间管理者。

### 2. 管理视角里的 `Cellapp`

`cellappmgr/cellapp.h` 里每个 cellapp 记录：

- 当前 load
- entity 数量
- space 集合
- global/group order

这是一个很典型的调度视图。

### 3. 管理视角里的 `Space`

`cellappmgr/space.h` 记录：

- `SPACE_ID`
- `Cells`
- 几何映射路径
- 脚本模块名

注意，这里管理层关心的是：

- 这个 space 在哪些 cell 上展开
- 它的脚本和几何定义是什么

这说明 KBE 的空间系统不是只在 Cell 本地存在，管理层也持有一份空间拓扑视图。

## 十四、BaseAppMgr：它管的是玩家入口和非空间负载

关键文件：

- `kbe/src/server/baseappmgr/baseappmgr.h`
- `kbe/src/server/baseappmgr/baseapp.h`

### 1. `Baseappmgr`

从头文件能直接看到它负责：

- `findFreeBaseapp`
- `updateBestBaseapp`
- `registerPendingAccountToBaseapp`
- `sendAllocatedBaseappAddr`
- `reqCreateEntityAnywhere`
- `reqCreateEntityRemotely`
- `queryAppsLoads`

这说明 `BaseAppMgr` 至少承担了两类职责：

- 玩家接入分配
- base 侧负载调度

### 2. 管理视角里的 `Baseapp`

`baseappmgr/baseapp.h` 记录：

- `numEntitys`
- `numProxices`
- `load`
- `initProgress`
- `flags`

很明显，Base 侧调度重点不是空间，而是：

- 会话数
- entity 数
- 负载

## 十五、Machine：KBE 从一开始就把运维体系内建了

关键文件：

- `kbe/src/server/machine/machine.h`

从头文件能直接看到它负责：

- 广播接口发现
- 查询全部接口信息
- 查询机器
- 启动进程
- 停止进程
- 杀进程
- 设置 flags

而且它同时维护：

- 广播地址
- 多个 `EndPoint`
- UDP packet receiver
- 组件 ID 记录

### 设计意义

这表明 KBE 把“集群部署与进程管理”当作引擎内建能力，而不是完全依赖外部脚本。

现代系统当然可以把这些工作交给 k8s、nomad 或 systemd，但 KBE 这个设计提醒了一点：

`运维面不是额外的，它也是架构的一部分`

## 十六、Watcher：KBE 不是只有日志，它有结构化观测树

关键文件：

- `kbe/src/lib/helper/watcher.h`
- `kbe/src/lib/helper/watcher.cpp`

从 `watcher.h` 可以直接看出它提供了：

- `WatcherObject`
- `WatcherValue`
- `WatcherFunction`
- `WatcherMethod`
- `Watchers`
- `WatcherPaths`

并且有统一的：

- `addToInitStream`
- `addToStream`
- path 树组织
- 远程 query 机制

### 这套东西为什么重要

它意味着 KBE 的观测不是“打日志 + grep”，而是：

- 运行时指标可挂到路径树
- 这些路径可远程查询
- 值、函数、方法都能挂进去

在 `ServerApp::initializeWatcher()` 里也能看到它统一挂了：

- `stats/stampsPerSecond`
- `uid`
- `username`
- `componentType`
- `componentID`
- `globalOrder`
- `groupOrder`
- `gametime`

这是一套很完整的自省体系。

## 十七、KBE 的主循环本质上是什么

把 `ServerApp`、`EntityApp`、`NetworkInterface`、`Watcher`、各进程组件放到一起看，KBE 的运行模型可以概括成：

1. `kbeMainT` 构建统一宿主
2. `ServerApp` 初始化通用运行时
3. `EntityApp` 初始化脚本与实体系统
4. `EventDispatcher` 驱动主循环
5. `NetworkInterface` 驱动网络事件
6. `Timer` 驱动 game tick
7. `BaseApp/CellApp` 在 tick 和消息之间推进实体状态
8. `Watcher/Machine/Tools` 暴露运维入口

所以它不是“多进程 + 网络”这么简单，而是一套完整的：

`实时世界宿主`

## 十八、Apollo 的采用边界

Apollo 采用 Login、Gateway、World 三项业务进程职责。World 内的 Scene 拥有模拟状态，Instance 承载临时玩法生命周期；World 还托管玩家资料与持久化，Login 托管在线目录、落点与迁移编排。

借鉴 KBE 的共同宿主、契约元数据、网络分层、可见性和结构化观测，不照搬 Base/Cell 分布式空间拓扑。当前架构不提供 Ghost、无缝地图或另一种 BigWorld 运行模式，不能由本篇外部源码分析推导新的 Apollo 进程角色。

跨服场景是明确的设计预留：原区服归属与运行位置分开，多区服玩家可进入同一 Scene，临时副本继续使用 Instance。跨 World 必须显式冻结、准备目标、条件提交权威、更新路由并清理源对象；跨服不意味着同时运行两份玩家对象。

实现状态与验收以[服务器应用](../apps/index.md)、[当前架构决策](./adr.md)和[跨服场景设计](../design/cross-server-scenes.md)为准。本文只说明外部引擎的机制，不维护另一份迁移路线或任务清单。

## 参考入口

- `C:\Users\cui\Workspaces\kbengine\kbe\src\lib\server\kbemain.h`
- `C:\Users\cui\Workspaces\kbengine\kbe\src\lib\server\serverapp.h`
- `C:\Users\cui\Workspaces\kbengine\kbe\src\lib\server\entity_app.h`
- `C:\Users\cui\Workspaces\kbengine\kbe\src\lib\network\network_interface.h`
- `C:\Users\cui\Workspaces\kbengine\kbe\src\lib\entitydef\entitydef.h`
- `C:\Users\cui\Workspaces\kbengine\kbe\src\lib\entitydef\scriptdef_module.h`
- `C:\Users\cui\Workspaces\kbengine\kbe\src\lib\entitydef\entity_call.h`
- `C:\Users\cui\Workspaces\kbengine\kbe\src\server\baseapp\baseapp.h`
- `C:\Users\cui\Workspaces\kbengine\kbe\src\server\baseapp\proxy.h`
- `C:\Users\cui\Workspaces\kbengine\kbe\src\server\cellapp\cellapp.h`
- `C:\Users\cui\Workspaces\kbengine\kbe\src\server\cellapp\entity.h`
- `C:\Users\cui\Workspaces\kbengine\kbe\src\server\cellapp\spacememory.h`
- `C:\Users\cui\Workspaces\kbengine\kbe\src\server\cellapp\witness.h`
- `C:\Users\cui\Workspaces\kbengine\kbe\src\server\cellapp\ghost_manager.h`
- `C:\Users\cui\Workspaces\kbengine\kbe\src\server\cellappmgr\cellappmgr.h`
- `C:\Users\cui\Workspaces\kbengine\kbe\src\server\baseappmgr\baseappmgr.h`
- `C:\Users\cui\Workspaces\kbengine\kbe\src\server\machine\machine.h`
- `C:\Users\cui\Workspaces\kbengine\kbe\src\lib\helper\watcher.h`
