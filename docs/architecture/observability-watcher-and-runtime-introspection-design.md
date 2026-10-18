---
title: Observability、Watcher 与 Runtime Introspection 设计
icon: binoculars
order: 30
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Observability
  - Watcher
  - Introspection
---

# 运行观测与内省边界

当前已具备自研日志、MetricRegistry、span/trace、日志/指标推送及部分管理查询原语。尚未集成专用 Profiler、OpenTelemetry SDK 或 OTLP exporter，不能把诊断设计稿写成已部署能力。

## 当前实现

| 能力 | 源码位置与边界 |
|---|---|
| 本地日志 | modules/core/log；输出、格式及推送见[日志说明](../design/logging.md)。 |
| 指标 | modules/core/metrics 的 MetricRegistry；指标不是 CPU/堆栈采样。 |
| span/trace | modules/net/linktrace、remoteentity 的 RpcTrace；自研上下文与采样发射，不是 OTel 集成。 |
| 推送与管理出口 | modules/net/metricspush、apps/admin 的汇聚与 HTTP 指标入口；是否配置部署须单独验收。 |
| 崩溃采集 | 见[崩溃说明](../design/crash-capture.md)；dump 不等于在线 Profiler。 |

## 目标角色观测范围

loginServer 记录认证、目录注册/失效与归属变化；gateway 记录连接、会话和 Base/cell 路由；Base 记录基础业务、结算及待确认存储；World 记录注册、拓扑、分配和迁移决定；cell 记录 Cell、实体、Instance、tick 和状态交接；database 记录存储受理、持久结果、失败与重试；machined 记录本机进程生命周期。

关联数据使用玩家 ID、Base/World/Cell/执行进程身份、会话和路由版本、迁移/结算 ID、进程与业务代次，避免将机器地址当作玩家归属。不在日志中输出登录凭证或数据库密码。

## 诊断与写入边界

运行态读取通过所属执行边界形成快照，管理线程不直接遍历正在变更的场景或资料容器。变更类管理请求仍走认证、授权与业务意图，不能用调试接口绕过 Base/World/cell/database 写权。

结构化状态查询与 Watcher 式树形展示是诊断设计方向，不代表现有统一 Watcher Tree 或完整 Runtime Introspection 平台已经实现。性能工具、OTel 及外部采集后端按具体需求接入，不增加游戏业务角色。

## 验收

分别验证进程存活、服务就绪、业务受理、执行完成和持久结果。记录跨服 Base 关联、场景迁移和更新的每个结果，不以一次 trace、重启或日志成功替代无感升级证据。Profiler 开销、采集频率、exporter 背压和故障降级在实际接入后再验收。

当前实施任务只维护在根目录 todo.md，架构与更新规则见[架构总览](../guide/architecture.md)和[更新边界](../design/gateway-topology-survey.md)。
