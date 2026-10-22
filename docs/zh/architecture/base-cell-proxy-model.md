---
title: Base Cell Proxy 对象模型
icon: project-diagram
order: 19
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - BaseApp
  - CellApp
  - Proxy
---

# Base Cell Proxy 对象模型

本页解释 KBEngine 的外部对象职责，不定义 Apollo 的另一种运行模式。Apollo 使用 gateway、loginServer、baseServer、worldServer、cell、database、machined，权威规则见[玩家对象模型](../design/player-object-model.md)。

## KBEngine 中的分工

KBEngine 的 Proxy 处理与客户端关联的能力，Base 承载非空间实体和长期逻辑，Cell 承载空间模拟。数据库访问与这些运行对象有独立边界。这些是第三方的真实名称，不能直接等同 Apollo 的进程或类。

可以分别阅读上游 [proxy.h](https://github.com/kbengine/kbengine/blob/master/kbe/src/server/baseapp/proxy.h)、[base.h](https://github.com/kbengine/kbengine/blob/master/kbe/src/server/baseapp/base.h) 和 [cellapp/entity.h](https://github.com/kbengine/kbengine/blob/master/kbe/src/server/cellapp/entity.h)，理解连接、非空间逻辑和模拟之间的协作。具体版本字段以源码为准。

## Apollo 的对应职责

| 边界 | Apollo 所有者 |
|---|---|
| 客户端连接与投递 | gateway |
| 账号认证和目录 | loginServer |
| 长期玩家对象与基础业务 | baseServer；复用 `base::Avatar` |
| Space/Cell 管理 | worldServer；对照 CellAppMgr |
| 场景运行实体 | cell 内的权威 Cell；复用 Avatar/Entity |
| 资料加载和持久保存 | database |
| 本机进程监督 | machined |

跨服场景只迁移模拟归属，原 Base 与数据库归属保持稳定。Instance 销毁不销毁长期玩家对象；Base 升级可以独立交接长期对象和会话。会话、长期资料和场景状态分别管理，不把它们合成一份随副本移动的完整档案。

## 生命周期与调用

Base 经 database 加载玩家长期资料，向 cell 提供玩法快照和准入信息。worldServer 管理分配，cell 运行场景实体，结算时提交稳定业务 ID 给所属 Base；Base 校验并经 database 持久确认。cell 无需原服数据库凭证，目录维护玩家 Base 与模拟位置的独立关联。

现有类型和路由原语需接入上述边界；引用外部引擎不代表代码已完成迁移。实现映射见[服务器应用](../apps/index.md)，跨服与更新规则见[跨服场景](../design/cross-server-scenes.md)和[更新边界](../design/gateway-topology-survey.md)。
