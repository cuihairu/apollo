# 设计件总览

> 本目录 = apollo 的**系统设计件**（design docs）落点：每件对应一个能力域的设计/调研/裁决记录。
> 状态口径：**已交付**（代码已落）/ **设计稿**（评审中或待批）/ **前置设计**（立项前调研件，拍板点在文内列明）。
> 术语纪律服从 [term-contract](./term-contract.md)；新词先入 [concept-glossary](./concept-glossary.md) 再入契约。

## 基础与模型

| 设计件 | 状态 | 一句话定位 |
|---|---|---|
| [术语契约](./term-contract.md) | v1.0 定稿 | 全仓命名裁决表（禁用词/定稿名/先例对照） |
| [概念词汇表](./concept-glossary.md) | 活文档 | 新词登记处 |
| [玩家对象模型](./player-object-model.md) | 已交付 | Anchor/Avatar/Scene/Instance 所有权与生命周期 |
| [属性系统与同步](./attribute-sync.md) | 已交付（部分批待接线） | 长期态字段模型 + 可见性/同步域 |
| [会话与在线目录](./session-and-online-directory.md) | 已交付（骨架） | manager 域目录权威 + 跨进程镜像/恢复相位 |
| [登录流程](./login-flow.md) | 设计稿 | login/gateway/base 三段握手 |
| [时钟与时间](./clock-and-time.md) | 已交付 | 墙钟/tick/帧预算口径 |

## 战斗

| 设计件 | 状态 | 一句话定位 |
|---|---|---|
| [战斗确定性](./battle-determinism.md) | 已交付 | 四约束 + 录制/回放/hash 链 |
| [战斗实例卸载](./battle-instance-offload.md) | 前置设计 | offload 进程形态（三拍板点待裁） |
| [战斗验证服务](./battle-verification-service.md) | 设计稿（评审中） | 客户端权威战斗复算对账（verifier） |

## 网络与运维

| 设计件 | 状态 | 一句话定位 |
|---|---|---|
| [网络抽象与传输内核](./net-abstraction.md) | 设计稿 | L0-L3 分层 + M1 自研内核（nng 替换路线） |
| [网关拓扑调研](./gateway-topology-survey.md) | 调研件（「留」结论已生效，ADR-011） | 九框架对照 + 独立 gateway-app 论证 |
| [日志系统](./logging.md) | 已交付（P3-3 批 A/B/C） | 单套收口 + 结构化行 + 三禁纪律 |
| [LoggerApp 立项前置设计](./logger-app.md) | 前置设计（候选倾向已记，拍板点待裁） | 进程壳与结构化行接线的落地形态（ADR-013） |
| [崩溃采集](./crash-capture.md) | 已交付 | Crashpad out-of-process + 符号化管线 |
| [容量与基准](./capacity-and-benchmark.md) | 已交付 | 5000 CCU 容量模型 + 三模型基准 |
| [备份容灾与宕机接管](./backup-revive.md) | 已关闭（批 0 已交付，六拍板点已裁 ADR-010：案 A 先行） | 权威进程死亡后「服务不倒」候选拓扑（G-2） |

## 扩展面

| 设计件 | 状态 | 一句话定位 |
|---|---|---|
| [Cell 单写者收口](./cell-single-writer.md) | 前置设计（候选倾向已记，拍板点待裁） | RPC 受理与模拟线程的并发形态（P3-2 批 B） |
| [入站第三方对接面](./inbound-interfaces.md) | 设计稿（评审中） | 渠道回调承载进程与鉴权/幂等 |
| [脚本系统（Lua）](./scripting-lua.md) | 设计稿 | 双端共享战斗逻辑与热更纪律 |
| [SDK 契约](./sdk-contract.md) | 活文档 | 客户端/服务端共享契约（contract.lua） |
| [XML 契约生成](./xml-generation.md) | 已交付 | entities.xml → 三语言投影 |
