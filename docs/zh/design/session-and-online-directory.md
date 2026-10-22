# 会话与在线路由

目录分别回答资料归哪个 Base、模拟实体在哪个 Space/Cell 和执行进程、连接在哪个 gateway。完整职责对照见[BigWorld 与 Apollo](./bigworld-apollo-mapping.md)。

## 权威与位置投影

loginServer 托管服务目录及玩家在线关联；worldServer 独占所属 Space/Cell 的拓扑和分配写权。cell 向所属 worldServer 注册，不向 machined 请求业务归属。目录保存已提交位置投影，gateway 缓存转发信息，两者不成为第二个拓扑裁决者。正常请求直达 Base 或 cell，不逐条代理目录。

## 路由记录

| 映射 | 作用 |
|---|---|
| Base 身份 → 地址、就绪、进程代次 | 解析资料业务目标及独立 Base 升级。 |
| World 身份 → 管理地址、就绪、容量与版本 | 选择普通或跨服 World 管理域。 |
| 执行进程 ID → 所属 World、地址、代次、就绪与负载 | 由 worldServer 管理注册和排空；进程可承载多个 Cell。 |
| Space/Cell → 范围、邻接、执行进程、拓扑版本 | worldServer 权威记录；默认每 Space 一个 Cell。 |
| 玩家 → 原区服、Base 身份与归属版本 | 长期资料所有者，跨服不改变。 |
| 玩家 → World、Space、Cell、执行进程、可选 Instance、模拟代次 | 已提交模拟位置与执行路由的投影。 |
| 玩家 → session/gateway、会话代次、路由版本 | 连接、重复登录与重连。 |
| 迁移 ID → 源/目标、预期代次、准备与提交结果 | 恢复结果不明的交接；跨 World 也需唯一提交依据。 |

Space、Cell、执行进程 ID 不混用；进程代次、实体权威代次、拓扑版本与会话版本分别表达不同变化。现有 WorldAssignment 缺少这些完整字段，不能把设计表写成 wire 已实现。旧 home_zone_id 不直接当业务区服 ID；IP 和数据库凭证不成为玩家归属。

## 模拟交接

Base 校验准入并提供基础快照；World 选择目标 cell。源冻结、目标准备但不激活，管理权威按预期版本条件提交；目标凭提交依据激活，目录更新投影，gateway 刷新路由，源清理。投影传播迟到必须可刷新、重定向或拒绝旧代次，不能重新授予源模拟权。

提交前取消须确认未提交，结果不明先查持久记录，禁止双端解冻。资料和结算仍交原 Base，经 database 确认。

## Base 与 Gateway 替换

Base 单独交接长期对象、在途业务与写入代次；数据库归属不变，旧写必须在业务和存储边界拒绝。cell/gateway 刷新 Base 缓存。

后端路由切换不迁移 socket。gateway 保持运行可保留 TCP；替换 gateway 需排空、重连恢复或显式连接交接。重连恢复原 Base 和当前模拟位置。

## 实现状态

OnlineDirectory、SessionLocator、PlayerDirectory、WorldAssignment 是可复用原语。管理/执行分离、Base 归属、分开的代次、跨进程目录投影及持久提交均待接线，现有完整档案迁移测试不作为新模型验收。
