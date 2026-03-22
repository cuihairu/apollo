---
title: 进程语义重定义
icon: align-left
order: 4
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - BaseApp
  - WorldHost
  - DBMgr
  - 进程语义
---

# 进程语义重定义

这篇文档只解决一件事：

`Apollo 当前仓库里哪些进程名和目标语义是错位的，后面应该按什么语义重新理解。`

这个问题必须先说清楚，否则越往后设计，错误会越固化。

## 一、先说结论

Apollo 后续应该统一按下面这组语义理解进程角色：

- `LoginApp`
  - 认证入口
  - 登录票据发放
  - 不持有长期在线主状态
- `GatewayApp`
  - 公网连接入口
  - 协议适配与消息转发
  - 不持有玩家长期归属
- `BaseApp`
  - `PlayerAnchor / Proxy / Online Authority Host`
  - 持有玩家长期在线归属
  - 负责 session 绑定、world 路由、重连恢复、持久化协调
- `WorldHost`
  - 单节点世界运行时宿主
  - 承接地图、实例、AOI、世界内表现态
- `CellApp`
  - `WorldHost + distributed space extension`
  - 是空间内实时权威节点
  - 不是普通地图服同义词
- `DBMgr` 或 `PersistenceService`
  - 数据库访问与持久化执行者
  - 不是玩家在线主状态宿主

## 二、当前错位点在哪里

Apollo 当前仓库里最明显的错位有两个。

### 1. `apps/base-app`

从当前实现看：

- [base_server.hpp](/C:/Users/cui/Workspaces/apollo/apps/base-app/include/base/base_server.hpp)
- [base_server.cpp](/C:/Users/cui/Workspaces/apollo/apps/base-app/src/base_server.cpp)
- [database_service.hpp](/C:/Users/cui/Workspaces/apollo/apps/base-app/include/base/database_service.hpp)

它当前主要承担的是：

- `DB_LOAD_REQUEST`
- `DB_SAVE_REQUEST`
- `DB_QUERY_REQUEST`
- `DatabaseService`
- `SaveQueue`

也就是说，当前 `apps/base-app` 在行为上更像：

- 数据服务原型
- 或“带玩家语义的持久化原型”

但它的名字使用了 `base-app`。

这就产生了错位：

- `名字` 指向 KBE/BigWorld 语义里的 `BaseApp`
- `实现` 却更接近 `DBMgr / DataService`

### 2. `apps/cell-app`

从当前实现看：

- [cell_server.hpp](/C:/Users/cui/Workspaces/apollo/apps/cell-app/include/cell/cell_server.hpp)
- [cell_server.cpp](/C:/Users/cui/Workspaces/apollo/apps/cell-app/src/cell_server.cpp)
- [cell_manager.hpp](/C:/Users/cui/Workspaces/apollo/apps/cell-app/include/cell/cell_manager.hpp)

它当前主要承担的是：

- 单进程 world loop
- 本地实体管理
- 本地 AOI
- 世界内消息处理

所以它当前更接近：

- 单机 `world-app` 原型

而不是：

- KBE 语义里的真正 `CellApp`

## 三、为什么 `BaseApp` 不能再被理解成数据库服务

这个点必须严格统一。

从本地 KBEngine 源码可以直接看到：

- [baseapp.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/server/baseapp/baseapp.h#L254)
  - `createClientProxies`
- [baseapp.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/server/baseapp/baseapp.h#L288)
  - `loginBaseapp`
- [baseapp.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/server/baseapp/baseapp.h#L304)
  - `reloginBaseapp`
- [baseapp.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/server/baseapp/baseapp.h#L324)
  - `onClientEntityEnterWorld`
- [proxy.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/server/baseapp/proxy.h#L65)
  - `hasClient`
- [proxy.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/server/baseapp/proxy.h#L82)
  - `onClientEnabled`
- [proxy.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/server/baseapp/proxy.h#L142)
  - `giveClientTo`

同时真正的数据角色是：

- [dbmgr.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/server/dbmgr/dbmgr.h#L32)

所以后续 Apollo 文档和实现都必须遵守这个判断：

- `BaseApp != 数据库服务`
- `BaseApp = 玩家长期归属 + Proxy + 在线主状态宿主`

## 四、Apollo 后续应该按哪条链路理解

更合理的主链应该是：

```text
Client
  -> LoginApp
  -> GatewayApp
  -> BaseApp(PlayerAnchor Host)
  -> WorldHost
  -> Distributed CellApp(按需增强)
```

以及单独的持久化链：

```text
BaseApp / WorldHost
  -> Repository / PersistenceService / DBMgr
  -> MySQL / PostgreSQL / Redis / SQLite
```

这两条链必须分开理解：

- 在线权威链
- 持久化执行链

## 五、推荐的术语落地

为了后续不再反复混淆，建议统一使用下面这些词。

### 1. 当前实现描述

对于现有 `apps/base-app`，统一描述为：

- `当前是偏数据服务原型`
- `不是最终语义上的 BaseApp`

对于现有 `apps/cell-app`，统一描述为：

- `当前是 WorldHost 装配壳`
- `不是最终语义上的 distributed CellApp`

### 2. 目标语义描述

后续统一描述为：

- `BaseApp(PlayerAnchor Host)`
- `WorldHost`
- `CellApp(Distributed World Node)`
- `DBMgr / PersistenceService`

### 3. 文档中的禁止混用

后续文档应避免把下面几组写成同义词：

- `BaseApp` 和 `DBMgr`
- `CellApp` 和 `普通地图服`
- `WorldHost` 和 `玩家锚点宿主`

## 六、对当前仓库的直接指导意义

这份语义重定义，不只是文档修词。

它会直接影响后面代码和模块落点。

### 1. `apps/base-app`

短期：

- 可以继续保留目录名
- 但不能继续把它当成“正确的 BaseApp 已经存在”

中期：

- 要么逐步补成真正的 `PlayerAnchor Host`
- 要么把当前数据服务职责拆出去，再重新实现真正 `BaseApp`

### 2. `apps/cell-app`

短期：

- 继续当 `WorldHost` 装配壳推进

中期：

- world loop、map instance、world session、entry/transfer 全部收口到 `WorldHost`

后期：

- 再在 `WorldHost` 之上叠加 distributed space 扩展
- 到那时它才配叫真正的 `CellApp`

### 3. `modules`

模块层应该同步按这个方向组织：

- `modules/runtime`
  - `ApplicationHost / ProcessHost / WorldHost`
- `modules/game/session`
  - `PlayerAnchor / SessionLocator / WorldAssignment`
- `modules/game/world`
  - `WorldSpace / MapInstance / WorldSession`
- `modules/platform` 或 `modules/data`
  - `Repository / Persistence / Cache / DB Adapter`
- `modules/bigworld`
  - `Ghost / Witness / AuthorityTransfer / AppMgr`

## 七、近期执行原则

从现在开始，建议按下面几个原则推进。

### 原则 1

不再把当前 `apps/base-app` 的实现，当成 KBE 语义里的 `BaseApp` 已完成。

### 原则 2

不再把当前 `apps/cell-app` 的实现，当成 KBE 语义里的 `CellApp` 已完成。

### 原则 3

所有文档都优先使用：

- `BaseApp(PlayerAnchor Host)`
- `WorldHost`
- `DBMgr / PersistenceService`

### 原则 4

代码落地顺序优先：

1. `WorldHost`
2. `PlayerAnchor`
3. `BaseApp` 语义收口
4. `DBMgr / PersistenceService`
5. distributed `CellApp`

## 八、结论

Apollo 当前最需要的不是继续给错误语义打补丁，而是先统一正确语义。

这份重定义文档的核心判断可以压缩成三句话：

- `BaseApp` 不是数据库服务
- `当前 base-app` 只是偏数据服务原型
- `当前 cell-app` 应先按 `WorldHost` 理解，再按需进化成真正 `CellApp`

后面的设计、重构、实现计划，都应该以这三条为基准。

## 相关阅读

- [Apollo 架构概述](./overview.md)
- [Apollo 架构差距分析](./apollo-architecture-gap-analysis.md)
- [Apollo 重构路线图](./apollo-refactor-roadmap.md)
- [BaseApp 演进设计](./base-app-evolution.md)
- [WorldHost 设计稿](./world-host-design.md)
- [KBEngine 源码分析](./kbe-source-analysis.md)
