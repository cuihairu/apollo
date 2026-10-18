# 架构总览

Apollo 角色保持 gateway、loginServer、baseServer、worldServer、database、machined，模拟执行使用已讨论的 cell。worldServer 对应 CellAppMgr 的管理职责，cell 对应 CellApp；长期对象由 Base 管理，经 database 保存。

Space 对应现有 Scene 的空间语义，Cell 是空间分区，Instance 是临时玩法生命周期。默认一个执行进程、每 Space 一个 Cell；预留同 Space 多 Cell，无缝切分、Ghost 与跨边界战斗暂未实现。

cell 向 World 注册，World 管理拓扑和迁移；loginServer 托管服务/在线目录的位置投影，gateway 直接转发到 Base/cell。machined 只监督本机进程，独立 World/cell 不要求同机部署它。跨服保留原 Base 和数据库归属，不搬完整档案。

协议更新分别交接 Base 长期对象、cell 模拟状态和 World 管理状态，再切换权威与路由。仅代码更新保持 wire，可采用 Lua 更新；C++ 使用进程替换。TCP、会话与完整活动连续分别验收。

## 临时副本生命周期

小队申请 → Base 校验 → World 分配 Space/Cell → cell 创建并运行 Instance → 原 Base/database 持久结算 → 离场与回收。复用八态，持续 Space 可以没有临时 Instance。

## 文档入口

- [BigWorld 与 Apollo 完整对照](../design/bigworld-apollo-mapping.md)：原词、目标角色、代码类型和实现缺口。
- [架构总览](../guide/architecture.md)、[玩家对象](../design/player-object-model.md)、[跨服](../design/cross-server-scenes.md)与[更新边界](../design/gateway-topology-survey.md)。
- [服务器应用](../apps/index.md)：当前 WorldApp 尚未拆分管理与执行，Base/database 独立原型已具备，cell 入口及整体接线待完成；任务见根目录 todo.md。

外部引擎源码保持真实名称，不构成另一套 Apollo 目标拓扑。
