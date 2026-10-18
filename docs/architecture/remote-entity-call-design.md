---
title: RemoteEntityCall 设计
icon: arrows-turn-right
order: 21
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - RemoteEntityCall
  - Message
  - RPC
---

# RemoteEntityCall 设计

本页说明已有远程引用原语与目标架构的调用边界；同 Space 多 Cell 与 Ghost 路线仅预留接口。Base 业务与场景模拟分别寻址，规则见[会话与在线路由](../design/session-and-online-directory.md)。

## 调用边界

- gateway 向所属 Base 发基础业务，向当前 cell 发场景输入。
- Base 经 World 管理准入/分配，向 cell 提供玩法快照；cell 向各玩家 Base 发结算等请求。
- Base 向 database 请求加载或持久提交，不把数据库位置暴露给 cell。
- loginServer 托管的目录提供服务和玩家关联查询，正常消息可使用缓存路由直接通信。

## 当前实现

`modules/net/remoteentity` 是可复用库，提供 RemoteEntityRef、RemoteMethodSchema、RouteResolver 与 RpcTrace。目标调用是否经过这些原语仍须按真实接线验证；库类型存在不等于业务目录、完整 RPC 编解码或版本迁移已实现。

## 5. 引用、方法与解析

### 5.1 RemoteEntityRef

源码：`modules/net/remoteentity/include/apollo/net/remote/entity_ref.hpp`。引用包含 entity_id、domain、target_app、role、route_version 和 shard_key。数据中的目标及版本必须由可信目录与准入确定，不能采信客户端自行声明的资料所有者。

### 5.2 RemoteMethodSchema

源码：同目录 method_schema.hpp。定义方法名、别名、调用模式、目标域、参数类型、超时及幂等声明。声明幂等不自动完成去重，业务仍需持久 ID 和确认结果。

### 5.3 RpcTrace

源码：同目录 rpc_trace.hpp 与 src/rpc_trace.cpp。复用 linktrace 的上下文和 span 发射。它是自研跟踪原语，尚未接入 OpenTelemetry SDK 或 OTLP exporter，也不是性能采样 Profiler。

### 5.4 RouteResolver

源码：同目录 route_resolver.hpp 与 src/route_resolver.cpp。提供进程内路由的 resolve、update_route、clear 与版本查询。更新由所属执行者串行完成。该表不自动提供跨进程目录或分开的玩家 Base/模拟位置关联，不能取代业务目录。

## 7. 路由失效与结果语义

服务地址或代次变化后刷新引用，拒绝旧写权、旧会话及旧模拟代次。Base 替换与 cell 迁移分别更新对应路由，不能互相覆盖。

区分传输到达、业务受理、执行结果和持久确认。结果不明先查业务记录或使用同一 ID 重试；跨代次重发不能重新分配 ID 导致重复奖励。trace 可关联故障，不能证明操作已持久执行。

## 当前工作

补齐 Base/database 边界的契约、可信注册、独立路由、版本解码及失败恢复。已有实现与代码迁移见[服务器应用](../apps/index.md)，任务只维护在根目录 todo.md；不保留另一套后续模式或对象抽象路线。
