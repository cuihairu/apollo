# 副本运行与可选进程隔离

默认模型是在 World 中按小队请求创建 Instance。已有 World、Scene、Instance 和 BattleRuntime 应优先复用，副本完成玩法和结算后释放资源并销毁。完整流程见[World 与 Instance 设计](../30-Instance_GameServer_Design.md)。

若后续玩法确需更强的故障或资源隔离，可选择一个 World 进程只运行一个 Instance。这是部署方式，不增加目标进程名。当前 ADR-012 的边界见[当前架构决策](../architecture/adr.md)。

现有 offload 模块和内存传输桩不等于生产级自动拉起：启动请求、就绪通知、成员准入、落点发布、进程退出、超时和崩溃失败仍需接通并验收。结算沿既有持久化权威路径进行，不能因隔离部署增加第二写入方。

自动扩缩与一副本一进程不作为默认必需能力；先完成同一 World 多副本隔离、小队创建、目标完成与销毁的完整验证。
