---
title: 架构总览
icon: layers
order: 7
prev: /guide/configuration
---

# 架构总览

Apollo 当前面向塔防、合作玩法和小队副本，预留未来无缝地图。术语按 [BigWorld 与 Apollo 对照](../design/bigworld-apollo-mapping.md)解释：已有六个角色保持原名，增加已讨论的 cell 执行职责；worldServer 管理 cell，不再同时定义为逐帧模拟宿主。

## 目标模型

<ArchitectureDiagram kind="system" />

| 角色 | 职责与生命周期 |
|---|---|
| gateway | 客户端 socket、会话、Base 与模拟端点路由；更换后端不迁移 socket。 |
| loginServer | 认证、凭证、服务目录与玩家在线关联；保存位置投影，不裁决 Cell 拓扑。 |
| baseServer | PlayerAnchor 等长期对象、基础业务与资料写权；不随副本销毁或跨服转移。 |
| worldServer | 类似 CellAppMgr 的管理职责：cell 注册、容量、Space/Cell 分配、拓扑、排空和迁移。 |
| cell | 类似 CellApp 的执行职责：承载本地 Cell，运行 Avatar/Entity、AOI、战斗、Lua 与 tick。 |
| database | 资料加载、保存、存储路由、队列/journal 及持久结果；不制定奖励规则。 |
| machined | 按需监督本机进程；不裁决跨服、玩家资料或空间归属。 |

## 空间与玩法对象

Space 是独立运行空间，对应现有 Scene 的空间语义；Scene 是当前代码名，不新增同义 Scene→Space 容器链。Cell 是 Space 的空间分区，cell 是承载分区的进程，两者身份分开。默认仅一个执行进程，每个 Space 一个覆盖其范围的 Cell；一个进程可承载多个 Space 的 Cell。

Instance 继续表示小队、目标、计时、结算与回收的临时玩法生命周期，复用八态状态机。它关联 Space，不是另一种空间分区；持续开放的 Space 可以没有临时 Instance。

<ArchitectureDiagram kind="space" />

## 玩家与数据归属

Base 持有长期资料并提供经校验的玩法快照；cell 只修改其 Cell 内的权威模拟状态。奖励、消耗与任务结果由 cell 请求各玩家所属 Base 校验，再经 database 持久提交。接收 ACK 不等于保存成功。跨服保留原 Base、区服和数据库归属，目的地不获取原服数据库凭证。

## 注册、寻址与迁移

Base/World 服务向 loginServer 目录注册；cell 向所属 worldServer 注册并发布就绪。worldServer 是 Space/Cell 拓扑与分配的权威，在线目录保留玩家 Base、World、Space、Cell、执行端点及会话的位置投影。gateway 缓存 Base 与 cell 路由，正常消息直达对应业务端点。

源 cell 在模拟边界冻结，目标 cell 恢复为未激活状态；worldServer 按预期源与代次提交模拟归属，再更新路由投影、激活目标、清理源。跨 World 交接须有协调和持久提交依据，不能由两个管理端各自激活。结果不明先查记录，禁止双端模拟。worldServer/cell 独立部署均不要求同机 machined。

## 更新边界

支持协议更新的进程替换：启动新进程、验证新旧消息与状态兼容、恢复准备、交接权威和路由、源端清理后退出。Base 迁移长期对象及在途业务；cell 迁移完整必要模拟状态；worldServer 迁移管理拓扑及在途分配/迁移记录。三者不是同一份快照。

仅代码更新保持消息编号、字段和 wire 不变；Lua 在所属执行边界更新，C++ 使用进程替换。保留 gateway 可保留其 TCP；替换 gateway 需排空或重连恢复。完整 Instance 连续必须包含成员、怪物、目标、计时和必要脚本状态，不能只搬玩家。详见[更新边界](../design/gateway-topology-survey.md)。

## 无缝地图预留

现在不实现空间切分、Ghost 复制和跨边界战斗。设计保留同一 Space 多 Cell、全空间坐标、稳定实体 ID、邻接拓扑、权威代次、完整状态交接和连续可见性。显式换 Space 与同 Space 跨 Cell 是不同操作，不能复用离场重置来实现无缝迁移。

## 代码现状

当前 WorldApp、World、Scene 仍将管理和模拟合在一起；独立 Base/database 原型已提供远程加载/保存；cell 执行入口、目录注册、业务迁出和整体路由仍待完成。SceneTransfer 是同进程换场，不是完整 Cell 状态迁移。已有原语和测试保留其实际范围，不作为目标拓扑完成证据。见[服务器应用](../apps/index.md)与根目录 todo.md。
