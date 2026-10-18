# 当前架构决策

仅保留有效规则，完整术语与实现缺口见[体系对照](../design/bigworld-apollo-mapping.md)，任务见根目录 todo.md。

## ADR-001：玩家长期资料与模拟权威

baseServer 持有 `base::Avatar` 与基础业务，cell 承载权威 `cell::Avatar`/Entity，database 负责存储访问。各数据域独占写权；跨服和副本销毁不搬长期档案。

## ADR-002：Space、Cell 与现有 Scene

Space 为独立运行空间，对应现有 Scene 的空间语义，不叠加同义容器。Cell 是分区，cell 是承载进程；默认一进程、每 Space 一 Cell，预留一 Space 多 Cell。当前 Scene 仍为本地实现。

## ADR-003：Instance 临时玩法生命周期

Base 校验小队，World 分配空间和执行端，cell 运行 Instance 并向各所属 Base 提交结算，由 database 持久确认。复用八态，不将 Instance 定义为空间或进程；共享状态保持唯一逻辑负责人。

## ADR-004：显式换空间与 Cell 权威交接

不同 Space 显式切换与同 Space 跨 Cell 分开。现有 SceneTransfer 重建 Avatar，不实现连续跨分区。完整交接冻结、准备、条件提交、目标激活、源清理并保留原 Base；结果不明先查记录。

## ADR-005：本地 AOI 与观察者视图

SceneAoi/ViewerState 是现有本地可见性原语，目标由 cell 推进。未来邻接 Ghost、跨 Cell 可见集合并和 Witness 完整语义另行实现，不新增 AOI 业务进程。

## ADR-006：战斗与结算

cell 运行 BattleRuntime，确定随机源、顺序和时钟；稳定结算 ID 交各所属 Base 校验，再经 database 持久提交资料与去重凭据。传输、受理和内存奖励均不等于持久成功。

## ADR-007：存储独立

database 提供加载、保存、路由与持久结果；Base 制定业务规则。提取既有 JSON、SaveQueue、PersistJournal，模拟端不持有完整档案或原服数据库凭证。外部 MySQL/Redis 按需求接入。

## ADR-008：故障恢复范围

journal 和进程重启不等于恢复活动模拟。分别恢复 Base 资料、World 管理拓扑和 cell 模拟，重建权威与路由；未经验证的活动恢复须明确失败策略。

## ADR-009：模拟写入与线程

每个实体只有一个 Cell 权威写入者；逻辑执行者顺序修改，IO/后台投递有界意图。多 Cell 的共享 Instance 状态有唯一逻辑负责人，不要求共享内存或全局锁步。

## ADR-010：注册与本机监督

Base/World 向 loginServer 目录注册，cell 向所属 World 注册。World 裁决拓扑，目录保存位置投影。machined 只监督本机；独立 World/cell 不要求本机 machined。跨机热备与仲裁未立项。

## ADR-011：Gateway 与两种更新

gateway 持有 socket，分开 Base/cell 路由。协议更新替换进程并转换状态，Base、cell、World 管理端分别交接其状态；仅代码更新保持 wire，可使用 Lua 更新。TCP、会话与完整活动连续分别验收。

## ADR-012：默认部署与容量扩展

默认一个 cell 进程，承载多个 Space 的完整 Cell；不要求一副本一进程。更多执行端向 World 注册，先支持完整 Space/Cell 分配，未来再启用同 Space 分区。自动拉起、就绪和失效协议仍须实现。

## ADR-013：观测状态

复用自研日志、MetricRegistry、span/trace 与 M1 收集；收集失败不得阻塞模拟。专用 Profiler、OpenTelemetry SDK/OTLP exporter 尚未集成，外部观测部署按需求实施。

## ADR-014：固定名称与对照

gateway、loginServer、baseServer、worldServer、database、machined 保持原名；增加已讨论的 cell 执行职责。worldServer 对照 CellAppMgr 管理，cell 对照 CellApp。现有 Scene 对照 Space；Instance 为 Apollo 玩法对象。第三方名称保留原样。

## ADR-015：异步意图受理

当前 WorldApp::gameLoop 泵送和消费 acceptanceQueue_ 并推进 World。拆分后该模拟循环归 cell；World 留管理。入队不等于执行，不阻塞 IO 等待 tick。

## ADR-016：过载与停机

容量、水位、预算和拒收明确，不静默丢弃已受理工作。关闭受理、尾扫、结算和资源释放后停机；管理端排空执行分配与模拟端排空活动分别处理。

## ADR-017：回执语义

关联 request_id，区分传输、受理、执行与持久结果。消费回执不能替代业务验证、幂等结算或存档确认。
