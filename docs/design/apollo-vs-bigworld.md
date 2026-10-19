# Apollo 能否比 BigWorld 更完善：可核对判断

> 判断口径：比较 Apollo 对副本制多人游戏服务端的适配与可验证性，不比较产品总功能或历史生产规模。本文是开发者笔记；“能超过”指存在可验收的改进方向，不表示已经超过。仓内状态按当前 `main` 核对，外部资料检索日期：2026-10-19。

## 结论

**可以在限定维度上做得更贴合目标、证据更完整；目前没有依据宣称 Apollo 整体比 BigWorld 更完善。** BigWorld 已公开实现分布式 Cell、Ghost、CellAppMgr 与负载调整机制；Apollo 当前重点是副本制玩法、确定性/回放、工程可观测性，并为未来多 Cell 预留约束。目标不同，必须逐项验收。

## A. Apollo 可以争取超过的维度

以下条目是可争取的工程优势，当前证据只证明 Apollo 有实现或设计基础。只有达到右栏判据，才可在该维度写“超过”。BigWorld 事实依据见每项外链及本文外部资料表。

| # | 维度与 Apollo 现状证据 | BigWorld 对应现状 | 可验收的“超过”判据 |
|---|---|---|---|
| A1 | **副本玩法生命周期与结算边界。** Apollo 已实现 `Instance` 八态和相邻状态转换校验：[instance.hpp](https://github.com/cuihairu/apollo/blob/main/modules/game/world/include/apollo/game/world/instance.hpp#L22)、[instance.cpp](https://github.com/cuihairu/apollo/blob/main/modules/game/world/src/instance.cpp#L8)、[test_instance.cpp](https://github.com/cuihairu/apollo/blob/main/tests/test_instance.cpp#L1)；玩法目标见[30 号设计 §Instance 状态机](../30-Instance_GameServer_Design.md#instance-状态机)。 | BigWorld 空间主干以 Space/Cell 表达运行空间，源码映射没有找到与 Apollo 临时玩法八态一一对应的内置层；CellAppMgr 管理 Space/Cell，不等于玩法实例状态机。[14.4.1 Space](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellappmgr/space.hpp) | 对 Create 至 Destroyed 全状态、非法跳转、断线/取消/重复结算与资源回收做状态组合测试；结算 ID 幂等，故障重启对账无重复奖励。通过后只能说“副本生命周期覆盖优于所核对的 BigWorld 空间管理模型”，不延伸为整体优越。 |
| A2 | **战斗复算与确定性证据。** 实现已有输入顺序稳定性、种子敏感性及 hash 链复算测试：[test_battle_runtime.cpp](https://github.com/cuihairu/apollo/blob/main/tests/test_battle_runtime.cpp#L150)；四约束和回放规格见[战斗确定性 §1–5](./battle-determinism.md)。离线完整复算器仍是计划项（该文 §7）。 | 核对的 BigWorld 14.4.1 Cell/Witness/CellAppMgr 源码提供空间模拟和管理机制；本次公开源码/手册检索未找到等价的战斗输入录制 + 确定性复算契约。此处只陈述“所核对资料未见”，不推断所有 BigWorld 项目均无此能力。 | 固定构建、种子、快照和输入，重复运行与跨重启复算得到逐 tick 一致 hash；修改一条输入可定位首个分歧 tick；归档工具版本、录制和报告。基准输入集全通过后，才可称该复算工作流优于所核对的参考资料。 |
| A3 | **可复现的容量与回归基准。** [容量与基准 §2.1–2.4](./capacity-and-benchmark.md)记录机器、负载、统计口径和多模型实测；已明确网络发送、全容量与混合业务仍有测量边界。 | BigWorld 运维指南描述按 CellApp CPU load 调整分区/平衡，但所核对公开材料没有给出与 Apollo 同口径、可直接复跑的同负载对照报告。[Server Operations Guide §CellAppMgr](https://howarduong.github.io/github.io/doc/html/server_operations_guide/ch02.html) | 固化环境、负载种子、采样与完整路径；对指定目标场景报告 p50/p95/p99、吞吐、丢拍、队列、网络与资源成本，并公开原始结果和复跑命令；同目标硬件/语义对照 BigWorld 数据，重复跑差异在预设范围内且关键指标更优。无同口径数据前不宣称性能超过。 |
| A4 | **故障可诊断性与工程留证。** Apollo 已有本机 Crashpad 采集、符号化流程和指标/日志约定：[崩溃采集 §1–4](./crash-capture.md)、[容量与基准 §5](./capacity-and-benchmark.md)。上传、配额、各平台运行验证仍列为边界。 | BigWorld 有 BWMachined/Reviver 监督及 CellApp 故障容错机制；[Server Overview §5.1、§5.8–5.9](https://howarduong.github.io/github.io/doc/html/server_overview/ch05.html)与 14.4.1 进程源码可查。 | 在相同故障注入集上，采集率、符号化成功率、定位时间和恢复决策均有量化结果；每个崩溃能关联进程代次、日志、指标及副本/结算记录。与对照运行结果逐项比较且诊断覆盖更高、恢复无数据错误，才可称该运维面超过。 |
| A5 | **面向现代客户端的协议与可观测契约。** Apollo 网络设计将协议版本、边界、指标和压测纳入治理：[网络抽象 §4–5](./net-abstraction.md)、[Gateway 职责与更新边界](./gateway-topology-survey.md)；具体兼容窗口和业务协议仍需按场景验收。 | BigWorld 14.4.1 公开源码使用 Mercury 消息接口（例：[CellAppMgr interface](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellappmgr/cellappmgr_interface.hpp)）；它有成熟消息/进程机制，但本次没有同口径的现代客户端协议演进与遥测验收数据可比。 | 至少覆盖旧/新客户端握手、未知字段/消息、滚动升级、拒绝原因和端到端 trace；协议兼容矩阵自动执行，关键在线状态有指标与告警。用相同升级/故障场景证明兼容窗口更明确、定位信息完整且无消息错解，才可称协议治理面超过。 |

## B. 不能声称超过的维度

这些属于长期运行和生产规模事实。设计稿、单测、仿真或单机基准都不能替代真实规模证据；现阶段最多说“设计已具备相应边界/方案”。

| # | 维度 | Apollo 当前证据与限制 | BigWorld 对应能力/出处 | 解除限制所需证据 |
|---|---|---|---|---|
| B1 | **万人规模 Ghost/Witness 与无缝连续可见性** | 当前 Scene AOI 是本地原语；跨 Cell Ghost 创建/删除、序号修复、Witness 连续视野均为目标设计。[Witness/Ghost](../architecture/witness-ghost-design.md)；[无缝地图 §3–4、§8](./seamless-map.md)明确尚未实现。 | BigWorld 文档描述多个 Cell/CellApp、Real/Ghost 复制与邻接可见性：[Server Overview §5.1](https://howarduong.github.io/github.io/doc/html/server_overview/ch05.html)。 | 多机高并发长稳运行；明确同时在线、实体密度、AOI/消息负载、抖动与故障条件；记录边界往返无重复/闪烁、无双 Real，并与 BigWorld 同负载实测对照。 |
| B2 | **跨 Cell 迁移实战与故障接管** | 有冻结、快照、提交、代次、fencing 的方案，没有完成的跨进程真实迁移链路或线上演练。[无缝地图 §5–8](./seamless-map.md)；[映射 §1、§6](./bigworld-apollo-mapping.md)。 | BigWorld CellAppMgr 源码包含 CellApp 故障、恢复、Cell 更新和分配管理接口；Cell 实体模型支持分区迁移相关机制：[CellAppMgr](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellappmgr/cellappmgr.hpp)、[Cell](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellapp/cell.hpp)。 | 跨进程/跨机故障矩阵覆盖提交前后、网络分区、旧进程复活、管理端重启；证明单写者、结算不重不漏及活动状态连续；附多轮运行记录。 |
| B3 | **CellAppMgr 长期均衡与成熟运维规模** | Apollo 目前 WorldApp 仍直接推进 Scene；World/cell 分离、容量调度、动态负载迁移没有生产运行数据。[mapping §1、§6](./bigworld-apollo-mapping.md)；[World/cell 管理](./world-cell-management.md)。 | BigWorld CellAppMgr 接收 CellApp 负载并调整 Cell 边界；官方手册镜像描述以 CPU load 为依据的负载平衡。[Server Operations Guide §2.8](https://howarduong.github.io/github.io/doc/html/server_operations_guide/ch02.html) | 长期多机运行含热点迁移、节点增减、故障恢复与升级；报告负载偏斜、迁移成本/失败率、稳定时间和人工介入量，并有同环境对照。 |

## C. 现在落后或缺失的维度

以下按[BigWorld–Apollo 映射 §1–6](./bigworld-apollo-mapping.md)的“当前实现边界”归并列项。它们是当前能力差距，不代表永远做不到。

| # | BigWorld 已有、Apollo 当前缺失/不完整的能力 | Apollo 当前边界出处 |
|---|---|---|
| C1 | 独立 CellApp 执行进程，以及 World 管理与 Cell 模拟执行分离。 | [映射 §1、§6](./bigworld-apollo-mapping.md)：WorldApp 仍直接推进 Scene，独立 cell 入口/注册/远程调度未完成。 |
| C2 | 多 Cell Space 的生产分区、拓扑/边界管理与自动负载调整。 | [映射 §2、§5](./bigworld-apollo-mapping.md)：当前单进程单 Cell；[无缝地图 §2、§8](./seamless-map.md)：初期静态设计阶段。 |
| C3 | 完整 Ghost 复制、邻接 Cell 可见集合合并及跨 Cell Witness 连续性。 | [映射 §3、§5](./bigworld-apollo-mapping.md)；[Witness/Ghost](../architecture/witness-ghost-design.md)：当前只有本地 AOI 部分职责。 |
| C4 | 跨 Cell 实体权威迁移、fencing、故障接管和状态恢复闭环。 | [映射 §5–6](./bigworld-apollo-mapping.md)；[无缝地图 §6、§9](./seamless-map.md)：设计约束未转为运行能力。 |
| C5 | 完整 BaseAppMgr 等价的 Base 注册、玩家归属、恢复和分配服务。 | [映射 §1](./bigworld-apollo-mapping.md)：loginServer 有部分原语，Base 归属和恢复待补。 |
| C6 | 完整通用 Base/Cell 实体体系、Mailbox/EntityCall 分布式路由和属性自动复制。 | [映射 §3](./bigworld-apollo-mapping.md)：RemoteEntityRef 等是局部原语，权威版本、回执、跨进程接线未完成。 |
| C7 | BigWorld Python 脚本兼容、官方客户端网络协议兼容及完整 EntityDefs/IDL 行为。 | [BigWorld 兼容层 §尚未实现](../33-BigWorld_Compatibility.md)：当前是单线程 C++ API facade，明确不提供以上兼容。 |
| C8 | 经规模运行验证的 CellApp 故障容错/长期均衡能力。 | [映射 §1、§5–6](./bigworld-apollo-mapping.md)：具备借鉴边界与方案，不具备实战证据；与 B2/B3 的生产成熟度要求相关。 |

## 判据表：最小可证伪验收

每项实验至少保留输入、环境、判定规则和结果。失败即推翻相应“超过”判断；文档和架构图不能替代运行证据。

| 对应维度 | 最小验收形态 | 可证伪条件 |
|---|---|---|
| A1 副本生命周期 | 状态转换全表 + 重复 settle/重复 destroy + 故障后账本对账测试；断言奖励唯一且终态资源清零。 | 存在非法跳转成功、奖励重复/丢失，或活动副本不能收敛到终态。 |
| A2 确定性复算 | 固定 binary/seed/快照/输入重复复算；再改动单条输入，检查逐 tick 首个 hash 分歧。 | 同输入 hash 不一致，或故意变更不能定位分歧。 |
| A3 容量与回归 | 提交可复跑负载、硬件/编译参数、原始样本和 p95/p99/吞吐/丢拍/队列报告；同口径对照 BigWorld。 | 无法复跑、关键数据缺失、目标指标不优，或结果超预定波动范围。 |
| A4 故障诊断 | 对同一故障注入清单采集 dump、符号栈、代次、日志/指标关联和恢复决策；与基线逐项对比。 | 崩溃无证据、栈无法还原、代次无法关联，或恢复出现状态/结算差错。 |
| A5 协议治理 | 旧/新协议组合、滚动升级、未知字段、断连重试的自动兼容矩阵 + trace/告警记录；对照 BigWorld 同场景。 | 合法旧端被静默误解、升级期间消息错路由，或故障无法定位。 |

## 推进次序建议（已代决）

单一推荐顺序：**先补齐 A1/A2 的自动化测试与持久结算/录制证据 → 建立 A3/A4 基线（测量与故障留证）→ 完成 A5 协议兼容和观测闭环 → 再拆 World/cell 并实现静态多 Cell → Ghost/Witness 与跨界 AOI → fenced 迁移/故障接管 → 最后做自动均衡和大规模长稳对照。**

依赖理由：迁移和自动均衡依赖拓扑身份、单写者、可复算状态、容量观测及故障对账；在这些证据之前扩大分布式运行面无法证明正确性。前四项偏向副本制目标的可验收完善，多 Cell 后续是追平 BigWorld 空间能力的工程阶段，不应拿设计预留算作已超越。

## 外部资料（检索日期：2026-10-19）

- BigWorld 14.4.1 公开源码：[CellAppMgr](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellappmgr/cellappmgr.hpp)、[Space](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellappmgr/space.hpp)、[Cell](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellapp/cell.hpp)、[Cell Entity](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellapp/entity.hpp)、[Witness](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellapp/witness.hpp)、[CellAppMgr interface](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellappmgr/cellappmgr_interface.hpp)。源码是实现机制出处，不是商业部署规模证明。
- BigWorld Server Overview（公开手册镜像，章节含 CellApp、BaseAppMgr、DBMgr、BWMachined 与 Ghost）：[Chapter 5](https://howarduong.github.io/github.io/doc/html/server_overview/ch05.html)。该手册版本较早，用于机制背景；涉及 14.4.1 当前行为时以仓内已锁定源码链接为准。
- BigWorld Server Operations Guide（公开手册镜像，CellAppMgr 配置与按负载均衡）：[Chapter 2](https://howarduong.github.io/github.io/doc/html/server_operations_guide/ch02.html)。同为历史手册资料，不能替代指定版本运行实测。
- Apollo 仓内基准、确定性、崩溃采集与边界出处均在 A/B/C 各行链接；设计状态与已实现状态已明确区分。
