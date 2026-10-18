# 玩家对象模型

玩家的长期资料对象和场景实体具有不同所有者、生命周期与写入边界。术语见[概念与命名](./concept-glossary.md)。

## 所有权

| 对象 | 所有者 | 状态与生命周期 |
|---|---|---|
| PlayerAnchor | baseServer | 背包、装备、成长、任务和玩家基础业务；加载、在线、断线保留和卸载独立于副本。 |
| Avatar / Entity | cell 内权威 Cell（现有 Scene 的空间语义） | 场景位置、战斗中的血量、技能运行状态和临时效果；按进出场及副本规则创建、迁移和释放。 |
| Session | gateway 连接与在线目录关联 | 绑定认证身份和当前连接，使用会话版本拒绝旧连接。 |

同一玩家的完整长期资料只有一个 Base 写入者，场景实体只有一个 cell 模拟者。cell 使用玩法所需快照，消耗或奖励通过请求交由 Base 校验；会跨对局保留的战斗结果应显式结算，不能默认所有场景字段都写入档案。

## 生命周期

登录后由所属 Base 经 database 加载或恢复长期资料。进入玩法时提供准入数据和只读基础属性快照，cell 创建场景实体。离开或副本结束后，Base 接收并幂等提交结算，cell 清理场景实体；销毁 Instance 不销毁长期资料。

## 跨服迁移

<ArchitectureDiagram kind="cross-server" />

跨服保留所属 Base 身份和原区服，不搬迁完整档案、数据库归属或数据库凭证。目标与源不得同时模拟同一实体；迁移结果不明先核对提交记录。对局连续迁移需同时覆盖整个 Instance 的共享状态，详见[跨服场景](./cross-server-scenes.md)。

## Base 升级

固定资料归属不等于绑定某个永不替换的进程。Base 升级可交接长期资料对象、会话和在途操作，更新 Base 身份到地址的映射；数据库归属保持不变。切换前排平写入，切换后拒绝旧进程写入，并处理迟到结算和重试。

## 当前实现

PlayerAnchor、Avatar、Scene 与 Instance 类型可复用。需要从当前 WorldApp 模拟宿主迁出长期资料与存储职责，接入 Base/database 边界；本轮文档更新不代表代码已完成该迁移。实施任务只维护在根目录 todo.md。

worldServer 管理 Space/Cell 分配，不另持一份玩家模拟对象。Base 实体对应 PlayerAnchor，Real/Cell 实体对应 Avatar/Entity；原词与实现缺口见[体系对照](./bigworld-apollo-mapping.md)。
