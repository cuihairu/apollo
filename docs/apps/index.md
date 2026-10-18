---
title: 服务器应用
icon: server
order: 5
category:
  - 应用
tag:
  - 服务器
---

# 服务器应用

目标职责和代码是两个层面。以下按 [BigWorld 与 Apollo 对照](../design/bigworld-apollo-mapping.md)列出真实缺口，不把尚未拆分的入口当成另一套设计。

## 目标职责与代码映射

| 目标角色 | 负责 | 当前入口与待迁移边界 |
|---|---|---|
| gateway | 连接、会话、Base 与 cell 转发 | gateway 已有接入、旧 World 出口和排空；独立 Base/cell 路由待接。 |
| loginServer | 认证、服务/在线目录 | login 已托管 OnlineDirectory；Base 归属、管理端与执行端地址投影待补。 |
| baseServer | Avatar、长期业务与结算校验 | apps/base 构建 base，复用 PlayerStore/Avatar 并注入远程存储；结算受理已接，目录归属接线待完成。 |
| worldServer | cell 注册、Space/Cell 分配、拓扑与迁移 | world / WorldApp 当前还直接运行 Scene，管理与执行必须拆开。 |
| cell | Cell 内实体、AOI、战斗、脚本与 tick | 独立入口未实现；复用现有 WorldHost/Scene/Avatar 等执行原语，默认一个进程。 |
| database | 加载、保存、存储路由与持久结果 | apps/database 构建 database，承载文件档案、journal 和 M1 加载/保存；完整排队、写权与分区待完成。 |
| machined | 本机进程监督与重启 | 可复用监督原语，不负责跨服发现或玩家归属。 |

apps/common 是共享实现库，不是业务进程；logger/admin/fleetctl 是辅助工具。上表只列真实入口；默认编队和业务迁移按根目录 todo.md 推进。

## 数据与请求链路

<ArchitectureDiagram kind="system" />

Scene 对应 Space 的现有单进程实现，Cell 为未来分区边界，Instance 为临时玩法生命周期。一个 cell 进程可承载多个 Space 的本地 Cell；不要求一副本一进程。跨服保留原 Base 和数据库归属。

## 已有测试的范围

HostedServicesTests、SquadInstanceTests、SquadWireE2eTests、UpdateContinuityTests 及恢复测试可证明已有代码行为。独立 Base/database、worldServer/cell 分离、真实路由、持久结算、完整 Instance 迁移及双版本协议须按目标拓扑重新验收。旧 World 间完整档案搬迁不能充当新边界的证据。

## 独立 Base 与 database 原型

构建 `base` 和 `database` target 后，可单独启动此存储链路（当前 M1 TCP 为 POSIX 路径）：

```sh
./build-examples/apps/database/database --port 9200 --data-dir data/database/players --journal data/database/persist.log
./build-examples/apps/base/base --port 9201 --database-host 127.0.0.1 --database-port 9200
```

两者支持 `--route-json` 指定现有契约路由表；Base 支持 `--id` 设置执行身份。Base 通过 PlayerStorage 注入远程客户端，按需加载 Avatar，不扫描共享数据库的全部玩家，不打开本地档案或存储 journal。database 复用现有 DB_LOAD/DB_SAVE 消息，验证档案身份，以串行执行和文件 journal 完成保存后回执；启动先恢复未应用记录，存储或监听不可用不发布就绪，Base 无法连接 database 时启动失败，无本地回退。

这是独立链路原型：长期资料归属已切到 base，有界保存队列、存储路由与写入代次已落地；登录/gateway 与 Base 所属目录、完整跨进程结算链、存储分区仍待迁移。现有 PlayerData 仍是旧档案适配类型，含位置/运行字段；不能据此把它定义为最终 Base 业务模型，字段契约拆分与旧保存路径清理属于后续任务。当前文件实现为 flush/rename 和进程重启恢复，不承诺掉电级 fsync 耐久性。协议升级与活动连续迁移也尚未完成。

BaseDatabaseTests 覆盖真实 TCP、Base 无本地目录、关闭/重启后加载、档案身份拒绝、不可达和 journal 恢复。BaseDatabaseProcessSmoke 单独启动两个可执行文件，验证分别重启与存储隔离；不作为完整编队验收。
