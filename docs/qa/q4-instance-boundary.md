# Q4：副本如何创建、完成与销毁？

Base 验证小队，worldServer 分配 Space/Cell 与执行进程，cell 创建并运行临时 Instance。Space 对应现有 Scene 空间语义，Instance 管成员、目标、计时、结算与回收；不是额外空间层或进程。

通关或失败结果交各所属 Base 校验，经 database 持久确认后退出并释放临时资源；长期 Base 对象继续存在。取消、无人进入、掉线和超时均有明确收尾策略。

World/Scene/Instance 原语已存在，管理/执行拆分和完整跨进程接线仍待完成。默认每 Space 单 Cell，未来同 Space 多 Cell 与无缝地图仅预留设计。见[体系对照](../design/bigworld-apollo-mapping.md)、[副本设计](../30-Instance_GameServer_Design.md)和根目录 todo.md。
