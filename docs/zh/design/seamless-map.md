# 无缝地图与同 Space 多 Cell 设计

> 状态：目标设计，分阶段实施。现有运行时仍是本地 Scene/单一执行权威；本文件描述 Apollo 的目标能力和可审查的实现边界。

## 1. 目标与术语

无缝地图指一个稳定 Space 可以由多个 Cell 分区共同承载，实体跨 Cell 边界时客户端观察连续，且同一实体在任一时刻只有一个可写模拟权威。Space 是逻辑空间，Cell 是其空间分区，cell 是执行角色/进程，World 是拓扑和分配管理者；Instance 仍只表达临时玩法生命周期。Scene 是当前本地模拟容器，不新造一层同义空间对象。

本设计复用 BigWorld 的 Real/Ghost 与邻接 Cell 可见性模型，以及 KBEngine 中 Cell/Space/实体迁移的实践方向；不复刻其进程拓扑或协议。Ghost 是只读、带来源代次的邻接投影，不是第二个可写 Entity。稳定 ID、分配版本、模拟权威代次和观察者视野水位分别解决身份、拓扑、写权与可见事件顺序，不能合并成一个版本字段。

## 2. 空间切分与负载

Space 地图坐标保持全局连续，分区以稳定 CellID、边界几何、邻接表、承载 cell 实例/启动代次和拓扑版本表达。首个多 Cell 版本使用轴对齐矩形分区，边界采用半开区间 `[min,max)`；最外侧地图边界另行闭合，避免边界实体双重归属。跨地图传送仍是显式 Space 切换。

World 是分配与拓扑权威，cell 是分区内模拟写者。负载采样按 Cell 统计 tick CPU、实体数、AOI/消息量和队列水位，并带采样窗口与过载阈值。初期只支持静态配置分区与人工迁移；自动拆分、在线重平衡必须等迁移事务、负载观测、容量预留和回滚证据齐备后再开。热点不能通过复制写权解决。

Cell 拓扑变更以 `topology_version` 原子发布。准备阶段不改变路由；提交后目录和 Gateway 只接受新版本，旧版本请求返回明确 stale/migrating 结果。邻接关系必须对称，范围无缝隙且不重叠；拓扑校验失败时拒绝发布。

## 3. Ghost 生命周期与一致性

实体在一个 Cell 有且仅有一个 Real。实体进入某邻接 Cell 的 Ghost 距离带时，源 Cell 创建 Ghost 快照并发送 `GhostCreate(entity_id, source_cell, authority_epoch, topology_version, position, visible_state, sequence)`；离开距离带或拓扑提交后发送带相同身份/代次的删除。目标按 `(entity_id, source_cell, authority_epoch)` 幂等应用，序号单调；旧代次、旧拓扑和重复消息不得覆盖新状态。Ghost 只含 AOI、碰撞/战斗判定确实需要的最小只读字段，不含 Base 私有资料、存储凭证或任意 Lua 指针。

生命周期为 `Absent → Preparing → Active → Removing → Absent`。目标先准备缓存，再确认可用；源仍是唯一写者。移交提交后目标 Real 激活、权威代次递增，源 Real 降级/删除，邻居按新代次刷新 Ghost。提交前失败由源继续运行并撤销准备态；提交结果不明时查询 World 持久操作记录，不猜测、不双活。幽灵更新可合并，只保留最新连续水位；缺口超过阈值需请求完整快照，超时则将该 Ghost 标为不可用于权威判定并告警。

Ghost 更新与模拟线程采用有界意图队列；World 管理链路不可逐帧转发。`topology_version`、`authority_epoch`、实体更新序号、操作 ID 均要在内部契约中显式表达。消息接收端验证来源 Cell 当前分配和代次，防止失联旧进程复活旧投影。

## 4. 连续可见性与 AOI

AOI 观察者查询先由本地空间索引取候选，再从本 Cell Real 与邻接 Cell Ghost 候选合并，按实体 ID 去重，最后使用精确距离和可见规则过滤。Ghost 距离至少覆盖 AOI 半径加上同步延迟期间最大移动距离；该上界须按移动速度、tick/网络延迟配置并校验。观察者越过边界时，Witness/视野水位不能清空重建：在新旧 Cell 同时准备视野，按同一实体 ID 合并 ENTER/SYNC/LEAVE 差集，提交点只切换数据来源，避免重复 ENTER 或瞬时 LEAVE/ENTER 闪烁。

每个观察者维护已知实体集合和每实体最后应用序号。进入观察范围先发完整可见快照，再应用增量；离开只发一次 LEAVE。跨 Cell 同一实体的源切换若 ID 和代次接续，应继续 SYNC，不得先 LEAVE。缺失快照、序号缺口、幽灵过期分别触发快照恢复、重同步或暂时隐藏；客户端不能据此成为权威。

阶段 0 的 AOI 查询改进只保证当前单 Scene 的候选覆盖完整，不创建进程间 Ghost，也不宣称边界连续性已实现。

## 5. 跨边界战斗归属与移交

实体所在 Real Cell 对其移动、状态和个人战斗结果负责。攻击者与目标分属不同 Cell 时，攻击请求发送到目标实体当前权威 Cell；由目标权威校验攻击者身份、输入序号、距离/遮挡所需可信位置、冷却和伤害规则。所需对方状态通过带代次的 Ghost 只读快照提供，超过允许新鲜度则拒绝或延迟判定，不用双方分别结算。

同一战斗事件使用稳定 `combat_event_id` 和目标 `authority_epoch` 去重。多人共享目标、团队效果或 Instance 级胜负需指定一个逻辑负责人（World 分配的 owner Cell 或专用玩法权威）；拆分后的 Cell 不各自提交最终奖励。结果仍由原 Base 校验并由 database 持久确认。实体边界移交期间，源端冻结该实体的新战斗副作用，排清/记录已发事件；World 持久提交唯一目标与新代次后目标恢复输入。迟到事件依代次和稳定事件 ID 判定，不能让旧端继续写入。

## 6. 故障接管与计划回滚

Cell 进程故障先由 World 通过租约/启动代次确认旧写者已失效；只靠心跳超时不足以证明网络分区一侧停止写入。没有 fencing 或等价写权栅栏时，不启用自动接管。World 保存分区分配、拓扑版本、操作阶段与唯一提交决定；cell 周期性提交可恢复检查点，必要的战斗/结算副作用另有幂等账本。

接管顺序：隔离旧代次 → 预留候选 Cell → 从最后完整检查点恢复 → 校验实体/Instance/计时器/随机状态和待处理事件 → 持久提升 authority epoch → 发布路由 → 允许新输入。无法证明检查点完整或在途结算归属时，冻结受影响 Cell 并走明确恢复/补偿流程；不从邻居 Ghost 推导完整模拟状态。

计划迁移按冻结、快照、目标准备、持久提交、目标激活、路由更新、源清理执行。提交前可取消并由源恢复；提交后只能向前完成，回滚是新的反向迁移操作，不能删除原决定或同时恢复源写权。World 管理进程回滚/重启必须恢复管理日志和代次、重新关联 cell 并逐项对账；缺少旧管理进程 fencing 时仅允许停机接管。跨机仲裁和自动热备暂不在本设计承诺内。

## 7. 与现有体系的接缝

以下行号以本文编写时 `main` 为准；源码路径均为仓库相对路径。

| 现有边界 | 现状出处 | 本设计接法 |
|---|---|---|
| 30-Instance GameServer | `docs/30-Instance_GameServer_Design.md:7` 定义 Space/Cell/Instance；`:24-26` 区分换空间、Base 归属与更新 | 保持 Space/Cell/Instance 语义；同 Space Cell 拆分不复用 Instance 八态，也不迁 Base |
| World/Cell 管理 | `docs/design/world-cell-management.md:34-45` 规定 World 管拓扑、cell 写模拟、Gateway 直达 cell；`:49-63` 列身份/版本 | 增加稳定 Cell 分区几何、邻接与拓扑版本；沿用操作 ID、启动代次和独立 authority epoch |
| 当前 ADR | `docs/architecture/adr.md:9-23` ADR-002/004/005 规定 Scene、连续迁移和本地 AOI 边界；`:37-51` ADR-009/010/012 规定单写者、目录和分配 | 本设计是 ADR-002/004/005/009/010/012 的后续扩展；新增可写跨 Cell 能力前需单独 ADR/协议决策 |
| Entity/Cell 生命周期 | `modules/game/world/include/apollo/game/world/scene.hpp:20-63` Scene 持有 Avatar/Entity/AOI；`apps/world/src/world_app.cpp:716-800` 处理创建、销毁、移动和跨界意图 | 先保留 Scene 本地容器；迁移层按稳定 ID/epoch 映射源目标，成功提交后再换承载，不复制 Entity 指针 |
| 本地 AOI | `modules/game/world/include/apollo/game/world/scene_aoi.hpp:13-25,57-81` 定义 SceneAoi 与事件/索引；`:175-200` 是候选查询 | 将候选索引扩为分区邻接查询，再接入跨 Cell Ghost；现有 ViewerState 作为水位原语，需扩充来源代次 |
| Gateway 下行 | `apps/world/src/world_app.cpp:1533-1564` 按实体所在 Scene 查询 viewer 并下发；`apps/gateway/src/m1_egress.cpp:330-380` 处理 Cell 创建/销毁/移动/跨界帧 | 保留 Gateway socket 与下行出口；World/cell 推送增加拓扑/实体代次和源切换连续语义，Gateway 仅按已提交路由转发 |
| Instance 释放 | `apps/common/src/squad_instance.cpp:150-175` 关闭后清理实体/AOI/临时 Scene | 多 Cell Instance 结束需等待各分区 drain 确认，再统一释放玩法资源；持续 Space 不受临时 Instance 回收影响 |

## 8. 分阶段实施与验收

每阶段均应有可留档证据。阶段之间存在明确依赖；后续阶段未满足前提时不得启用生产流量。

| 阶段 | 范围与前提 | 验收证据形态 |
|---|---|---|
| 0：单进程空间查询基线 | 空间格查询覆盖任意配置视野半径；不涉及跨进程迁移 | 新增边界/多格距离单测，既有 AOI/Scene 全套用例通过；查询结果与朴素全量距离 oracle 一致 |
| 1：拓扑与分区身份 | 建立 Space/Cell 几何、邻接校验、拓扑版本及静态配置；依赖 World 管理记录和稳定 ID 规则 | 拓扑合法/重叠/空隙/非对称邻接测试；持久重启后拓扑摘要一致；配置错误明确拒绝 |
| 2：跨进程 Ghost 与边界 AOI | 实现消息身份、生命周期、序号/快照恢复、Witness 连续差集；依赖阶段 1、契约生成、cell 间有界传输和 fencing | 多进程确定性场景测试含边界往返、重复/乱序/断链；同实体无双 Real、无重复 ENTER/LEAVE；负载/队列指标证据 |
| 3：实体与战斗移交 | 单实体冻结、快照、目标准备、提交和清理；战斗 owner、事件去重、Base 结算边界 | 故障注入覆盖每个迁移阶段；提交前后权威唯一；跨界战斗重放无重复伤害/奖励，客户端观察连续 |
| 4：故障接管和受控计划迁移 | 管理日志、检查点、fencing、恢复对账、向前完成及反向迁移；依赖阶段 1-3 | 杀进程/网络隔离/重启矩阵；恢复摘要与输入/结算账本对账；旧代次写请求被拒；滚动升级全链路记录 |
| 5：动态负载与运维启用 | 热点检测、容量预留、自动分区调整与回滚策略；依赖稳定迁移和观测基线 | 可复现实测报告给出 tick 延迟、队列、迁移成本/中止率；策略仿真无抖动循环；线上开关与回滚演练记录 |

阶段 0 先行内容只改善单 Scene 空间查询，是独立可验收的正确性切片；它不假设 Cell 拓扑、Ghost 或远程实体容器。

## 9. 风险、暂缓项与明确不做

- **暂缓同 Space 多 Cell 上线**：目前 Scene 和 WorldApp 仍是本地模拟/管理混合路径，未有拓扑持久提交、fencing、远程写权移交和跨 Cell Witness。
- **暂缓自动动态切分/负载迁移**：没有迁移一致性与测量数据时自动搬迁会扩大故障面，先用静态分区和人工操作积累证据。
- **暂缓 Ghost 承载完整实体状态**：Ghost 只读且最小化；Base 私有字段、可变 Lua 状态、任意共享指针不跨分区镜像。
- **不做双写/战斗双方独立裁决**：会产生重复伤害、奖励和不可收敛状态；每个目标及共享玩法结果必须有唯一权威。
- **暂缓无日志的自动故障恢复和跨机热备**：无法判断提交结果或隔离旧写者时，自动恢复可能双活；先冻结并人工对账。
- **不把网关或 AOI 抽成业务进程**：Gateway 继续持 socket 与转发；AOI 查询属于模拟空间数据面，管理面只发布拓扑/路由决定。
- **不改变长期对象/存储归属、不扩大 Instance 职责、不宣称阶段 0 即完整无缝**：这些会违反当前 Base、database、Scene 与 Instance 的已定边界。

## 10. 外部资料

外部资料用于验证成熟引擎的机制，不作为 Apollo 当前功能证明。检索日期：2026-10-19。

- BigWorld Server Overview, “CellApp” / Ghost 与邻接 Cell 机制：[官方手册镜像](https://howarduong.github.io/github.io/doc/html/server_overview/ch05.html)。资料描述 Real/ghost 分布、邻接 Cell ghost 创建和更新节奏。
- BigWorld Server Programming Guide, “Implementing Entity Extras”：[`BigWorld-Engine-1.9.1` 手册源文件](https://github.com/v2v3v4/BigWorld-Engine-1.9.1/blob/master/bigworld/doc/generated_html/server_programming_guide/ch23.html)。Ghost 域控制器和可见性查询边界参考。
- KBEngine 官方配置文档：[ghostDistance 配置](https://kbengine.github.io/docs/configuration/kbengine_defaults.html)。仅用于邻接复制距离配置概念；具体行为需以对应 KBEngine 版本源码核验。
- Apollo 仓内对照：[BigWorld 与 Apollo 映射](./bigworld-apollo-mapping.md)、[KBEngine 分析](../architecture/mmo-frameworks/kbengine-analysis.md)。
