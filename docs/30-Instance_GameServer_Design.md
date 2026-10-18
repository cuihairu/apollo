# World 与 Instance 设计

worldServer 管理 Space/Cell 与执行进程，cell 实际运行模拟；Base 管长期对象，database 管存储。按 [BigWorld 与 Apollo 对照](design/bigworld-apollo-mapping.md)使用术语，不增加同义容器或改动现有代码名。

## 进程与对象

Space 对应现有 Scene 空间语义；Cell 是空间分区，执行进程对应 BigWorld CellApp。默认一个执行进程承载多个 Space 的完整 Cell；扩容先分配完整 Space，未来可在同 Space 切分。Instance 是 Apollo 临时玩法的成员、目标、计时、结算和回收生命周期，关联 Space；持续空间不必有临时 Instance。

<ArchitectureDiagram kind="space" />

## 临时副本流程

1. Base 验证成员、资格、配置和版本，向 World 申请兼容容量。
2. World 分配 Space/Cell 与执行端，cell 创建 Instance 和运行空间，校验成员 Base 关联及准入。
3. cell 推进战斗、怪物、目标与时钟；只持有必要基础快照和运行状态。
4. 通关或失败后停止有效玩法输入，以稳定结算 ID 将结果交各原 Base 校验，由 database 持久确认。
5. 待确认、部分成功、补偿和取消有明确策略，不能销毁唯一恢复凭据；取消不发成功奖励。
6. 成员离开，回收实体、AOI、脚本与临时资源，销毁 Instance 和临时 Space，原 Base 对象继续存在。

复用 Create → Initialize → Waiting → Running → Finishing → Rewarding → Draining → Destroyed 八态，覆盖无人进入、全员断线、解散及超时。状态机不能代替跨进程持久确认。

## 跨服与更新

跨服只转移模拟归属，原 Base 与数据库归属不变；cell 向各成员原 Base 提交业务结果，不获取各服存储凭证。显式换空间与未来同空间跨 Cell 分开，现有 SceneTransfer 重建 Avatar 不能提供无缝连续。

协议更新分别交接 Base 长期对象、cell 模拟状态、World 管理拓扑和在途决定。完整活动连续迁移包括成员、实体、怪物、目标、时钟、随机与必要脚本状态。仅代码更新保持 wire，Lua 可在执行边界更新，C++ 使用进程替换；保持 gateway 与替换 gateway 分别验收。

## 实现状态

World、Scene、Instance、SceneTransfer、BattleRuntime 及发布/排空原语可复用；现有 WorldApp 仍混合管理和执行，Base/database 独立原型已具备，cell 入口、可信关联、真实路由和完整迁移待接。未来无缝地图仅预留身份、分区、拓扑、Ghost 和可见性边界，当前不启用。见[跨服场景](design/cross-server-scenes.md)、[更新边界](design/gateway-topology-survey.md)与根目录 todo.md。
