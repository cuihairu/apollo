# Apollo 架构现状与目标模型

只保留当前目标，职责和真实代码缺口以[BigWorld 与 Apollo 体系对照](../design/bigworld-apollo-mapping.md)为准。

## 当前结论

Apollo 优先塔防和小队临时玩法，预留未来无缝地图；不宣称已经具备完整 BigWorld 能力。保留六个既定角色名称，增加已讨论的 cell 执行职责，Cell 表示分区。

worldServer 对照 CellAppMgr 管理注册、Space/Cell 分配、拓扑和迁移；cell 对照 CellApp 执行模拟。Base 长期对象与 Cell 实体分开，database 承担存储。loginServer 托管认证、服务和玩家位置投影，gateway 持有连接并直达 Base/cell；machined 只监督本机。

Space 对应当前 Scene 的空间语义，Instance 是临时玩法生命周期，不增加 Scene→Space 同义层。默认一个执行进程、每 Space 一个 Cell；预留同空间多 Cell、Real/Ghost、连续观察和完整权威交接。

## 待完成边界

WorldApp、World、Scene 当前混合管理和执行，PlayerStore 仍需拆分资料和存储。独立 Base/database 原型已有远程加载/保存与文件 journal；cell、注册路由、完整业务持久提交、跨服关联、协议双版本及三类交接仍待实现。现有 SceneTransfer 重建 Avatar，只能作为显式换场原语，不能实现同 Space 无缝迁移。

跨服保留原 Base 与存储归属，结算经原 Base/database 幂等持久确认。Base、cell 和 World 管理端分别交接长期对象、模拟状态和管理记录。保持 gateway 与替换 gateway 的 TCP 结果分别验收；活动连续要求完整必要 Instance 状态。

专用 Profiler、OpenTelemetry SDK/OTLP exporter 未集成，自研日志/MetricRegistry/span 原语不构成集成证据。

## 实施入口

[架构](../guide/architecture.md)、[应用](../apps/index.md)、[对象](../design/player-object-model.md)、[在线路由](../design/session-and-online-directory.md)、[跨服](../design/cross-server-scenes.md)、[更新](../design/gateway-topology-survey.md)同步此结论。代码工作与真实验收维护在根目录 todo.md，文档不将规划写成已实现。
