[English](index.en.md) | [中文](index.md)

# 设计件总览

> 本目录包含各能力域的设计稿和实现说明。Apollo 的目标架构、公开术语和进程边界以[架构总览](../guide/architecture.md)与[概念与命名](./concept-glossary.md)为准；子系统文档不能引入第二套进程拓扑。
> 状态口径：**已交付**（代码已落）/ **设计稿**（评审中或待批）/ **前置设计**（立项前调研件）。外部引擎术语用于比较，不自动成为 Apollo 概念。

## 基础与模型

| 设计件 | 状态 | 一句话定位 |
|---|---|---|
| [概念词汇表](./concept-glossary.md) | 活文档 | 新词登记处 |
| [玩家对象模型](./player-object-model.md) | 子系统实现说明 | 玩家身份、实体与场景对象；目标对局边界以 Room 为准 |
| [属性系统与同步](./attribute-sync.md) | 子系统实现说明 | 属性数据与可见性/同步机制 |
| [会话与在线目录](./session-and-online-directory.md) | 子系统实现说明 | 在线会话与当前房间路由信息 |
| [登录流程](./login-flow.md) | 子系统设计稿 | Login、Gateway 与 RoomServer 的登录/路由边界 |
| [时钟与时间](./clock-and-time.md) | 已交付 | 墙钟/tick/帧预算口径 |

## 战斗

| 设计件 | 状态 | 一句话定位 |
|---|---|---|
| [战斗确定性](./battle-determinism.md) | 已交付 | 四约束 + 录制/回放/hash 链 |
| [战斗实例卸载](./battle-instance-offload.md) | 子系统实现说明 | 战斗计算卸载实验；不改变 Room/RoomServer 目标术语 |
| [战斗验证服务](./battle-verification-service.md) | 设计稿（评审中） | 客户端权威战斗复算对账（verifier） |

## 网络与运维

| 设计件 | 状态 | 一句话定位 |
|---|---|---|
| [网络抽象与传输内核](./net-abstraction.md) | 设计稿 | L0-L3 分层 + M1 自研内核（nng 替换路线） |
| [网关拓扑调研](./gateway-topology-survey.md) | 调研件（「留」结论已生效，ADR-011） | 九框架对照 + 独立 gateway-app 论证 |
| [日志系统](./logging.md) | 已交付（P3-3 批 A/B/C） | 单套收口 + 结构化行 + 三禁纪律 |
| [LoggerApp 立项前置设计](./logger-app.md) | 已交付（L1-L4，2026-10-10/12） | 进程壳与结构化行接线的落地形态（ADR-013） |
| [崩溃采集](./crash-capture.md) | 已交付 | Crashpad out-of-process + 符号化管线 |
| [容量与基准](./capacity-and-benchmark.md) | 已交付 | 5000 CCU 容量模型 + 三模型基准 |
| [备份容灾与宕机接管](./backup-revive.md) | 已关闭（批 0 已交付，六拍板点已裁 ADR-010：案 A 先行） | 权威进程死亡后「服务不倒」候选拓扑（G-2） |

## 扩展面

| 设计件 | 状态 | 一句话定位 |
|---|---|---|
| [模拟单写者收口](./cell-single-writer.md) | 前置设计 | RPC 受理与房间模拟线程的并发形态 |
| [受理队列水位与过载](./queue-watermark.md) | 前置设计（已裁 ADR-016） | 两级水位/回调错误码/tick 预算锚定 |
| [入站第三方对接面](./inbound-interfaces.md) | 设计稿（评审中） | 渠道回调承载进程与鉴权/幂等 |
| [脚本系统（Lua）](./scripting-lua.md) | 设计稿 | 双端共享战斗逻辑与热更纪律 |
| [SDK 契约](./sdk-contract.md) | 活文档 | 客户端/服务端共享契约（contract.lua） |
| [XML 契约生成](./xml-generation.md) | 已交付 | entities.xml → 三语言投影 |
