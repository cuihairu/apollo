# BigWorld 与 Apollo 概念对照

本文从 Apollo 的进程、对象和代码出发，对照 BigWorld 14.4.1 服务端原词，区分相近职责、项目扩展和尚未实现的能力。进程名称沿用已确认的 gateway、loginServer、baseServer、worldServer、database、machined；模拟执行进程使用已讨论的 `cell`，其职责对应 BigWorld 的 CellApp。大写 `Cell` 专指空间分区。

## 1. 进程职责对照

| BigWorld 概念 | Apollo 对应 | 职责与差异 | 当前实现边界 |
|---|---|---|---|
| LoginApp | loginServer 的认证部分 | 验证账号和登录入口；Apollo 另由该进程托管服务/在线目录。 | login 已有认证和目录原语，最终 Base/cell 接线待完成。 |
| BaseApp | baseServer | 承载长期、非空间实体逻辑；Apollo 玩家资料对象使用 `base::Avatar`。Base 不是数据库进程。 | apps/base 已提供 base 独立入口，Avatar 经远程 database 激活；基础业务与结算受理已接入，目录归属未完成。 |
| BaseAppMgr 的相关职责 | loginServer 的 Base 注册、落点与目录部分 | 只借鉴注册和分配边界，不宣称已复刻完整 BaseAppMgr。 | Base 服务注册、玩家 Base 归属及恢复待补。 |
| CellAppMgr 的相关职责 | worldServer | 管理 cell 执行进程、Space/Cell 分配、容量、拓扑版本、排空与迁移；不运行每个实体的逐帧逻辑。 | 当前 WorldApp 仍直接推进 Scene，管理/执行拆分待做。 |
| CellApp | cell 执行进程 | 实际承载一个或多个 Space 的本地 Cell，推进实体、AOI、战斗、脚本与 tick。 | 独立执行入口、注册、就绪与远程调度未实现。 |
| DBApp / DBAppMgr 的存储职责 | database | 提供资料加载、保存、存储路由和持久结果；不处理基础业务奖励规则。 | apps/database 已提供独立入口、M1 加载/保存与文件 journal；写入代次、存储分区及完整队列仍待补。 |
| bwmachined 的本机监督职责 | machined | 拉起、停止、监督及重启本机进程；不决定玩家 Base 或 Space 归属。 | 可复用监督原语；服务就绪与业务恢复分别验收。 |
| Proxy 的连接/会话相关职责 | gateway 加 Base/会话关联 | BigWorld Proxy 属于 Base 侧实体模型。Apollo 使用独立 gateway 持有 socket，Base 保留业务关联，不能把 gateway 直接称为完整 Proxy 实现。 | 连接与部分会话原语已在，分开的 Base/cell 路由待接。 |

这是职责对照，不是把 Apollo 进程改成上游名称。worldServer 负责 cell 的管理，cell 向所属 worldServer 注册；loginServer 保存 World 服务地址和玩家归属/位置投影。拓扑分配以 worldServer 为准，目录投影和 gateway 缓存不成为第二个拓扑写入者。

BigWorld 的 [CellAppMgr](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellappmgr/cellappmgr.hpp) 与 [BaseAppMgr](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/baseappmgr/baseappmgr.hpp) 提供上述管理边界的源码依据；[Base](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/baseapp/base.hpp) 和 [Proxy](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/baseapp/proxy.hpp) 说明对象与进程的区别。

## 2. 空间与玩法对照

| 概念 | BigWorld 含义 | Apollo 对应与边界 |
|---|---|---|
| 地图资源 | 几何等静态资源供运行空间加载 | 现有 SceneDescriptor 引用地图资产；资源不是一次副本，也不是进程。 |
| Space | 有独立身份和坐标体系的运行空间，可由多个 Cell 承担模拟 | 对应 Apollo 当前 Scene 的空间语义。Scene 是现有代码类型名，当前只实现单进程容器；扩展时补逻辑空间与分区执行边界，不叠加同义的 Scene→Space 容器链。 |
| Cell | Space 的一个空间分区，拥有该范围的权威实体 | Apollo 复用此含义，记录 Space、分区 ID、范围、承载进程及拓扑版本；不是执行进程名。 |
| CellApp | 承载 Cell 的进程 | Apollo 的 cell 执行进程；一个进程可承载不同 Space 的多个 Cell。 |
| Instance | 本文核对的空间主干没有与 Apollo 八态玩法生命周期一一对应的内置层 | 保留 Apollo 现有 Instance：小队、目标、计时、玩法阶段、结算和回收。它关联运行空间，不是 Space/Cell 的另一种名字，也不声称来自 BigWorld 原生类型。 |

默认一个 cell 执行进程；每个 Space 只有一个覆盖其范围的 Cell。多个副本可以各有独立 Space/Cell，但由同一个执行进程承载。扩容先将完整 Space/Cell 分配到更多进程；未来无缝模拟允许同一个 Space 包含多个 Cell。

<ArchitectureDiagram kind="space" />

Instance 不是空间层级：一场临时塔防活动管理其目标、结算及关联 Space；持续开放的 Space 可以没有临时 Instance。两个队伍可复用同一地图资源创建不同运行空间。当前项目的 Instance 与 Scene 一对一约束属于现有玩法实现，不是 BigWorld 的通用限制。

BigWorld 的 [Space](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellappmgr/space.hpp) 管理 Cell 集合和按位置查找；[Cell](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellapp/cell.hpp) 关联空间、范围和实体；[CellApp](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellapp/cellapp.hpp) 是进程宿主。

## 3. 玩家、可见性和调用对照

| BigWorld 概念 | Apollo 对应 | 权威与实现边界 |
|---|---|---|
| Base 实体 | `base::Avatar` 等长期对象，归 baseServer | 基础业务和长期资料由 Base 校验修改，跨 Cell/Space/服的模拟迁移不改变存储归属，Base 承载可独立迁移；经 database 保存。base::Avatar 是当前玩家侧实现，不代表完整通用 Base 实体体系已实现。 |
| Cell 实体 / Real 实体 | Avatar 与参与模拟的 Entity，归 cell 内的权威分区 | 一个实体只有一个有效模拟写入者。Avatar 是玩家玩法类型，不增加一层独立进程。实体 ID 在迁移时保持稳定。 |
| Ghost 实体 | 邻接 Cell 的非权威投影，设计预留 | 用于跨边界可见性/查询，不能独立扣血、发奖或写档。当前没有完整复制、去重和失效实现；迁移快照不等于长期 Ghost。 |
| Witness | 当前 SceneAoi、ViewerState、属性同步承担部分可见集职责 | Witness 是观察者视图语义，Ghost 是跨分区投影语义。现有本地 AOI 不等于完整 Witness 或跨 Cell 可见性。 |
| Mailbox / EntityCall | RemoteEntityRef、方法元数据、RouteResolver 与已有消息契约 | 地址解析、权威版本和回执需要真实接线；现有本地引用/路由表不等于完整分布式调用或旧协议解码。 |
| Proxy / 客户端关联 | gateway 的连接、Session 与 Base 关联 | 连接、长期资料对象和空间实体分别管理；更换模拟进程不要求更换客户端 socket。 |

BigWorld 的 [Cell Entity](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellapp/entity.hpp) 描述 Real/Ghost 权威区别；[Witness](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellapp/witness.hpp) 提供观察者视图参考。这些源码支持机制对照，不证明 Apollo 已实现对应能力。

### Avatar 名称的适用范围

Avatar 是角色语义，不是 Cell 执行位置。BigWorld 2.1 官方词汇表还包含 NPC，教程和 KBEngine 示例均展示角色的 Base/Cell 部分。Apollo 以 `base::Avatar` 对应玩家 Base 部分，`cell::Avatar` 对应玩家 Cell 部分；它们是当前实现映射，不能据此宣称上游 Avatar 只存在于 Cell 或两者是行业共同类型。完整引用、其他引擎用法和命名判断见[玩家对象模型](./player-object-model.md)。

## 4. Apollo 的请求与归属

<ArchitectureDiagram kind="system" />

Base 资料写权与 Cell 模拟写权属于不同数据域；玩家不是一份随地图整体搬迁的档案。跨服目的地只接收玩法所需快照、运行状态和可信 Base 关联，不获得原服数据库凭证。Base 更新独立交接长期对象，cell 更新交接模拟状态，worldServer 更新交接管理拓扑和在途迁移记录。

## 5. 无缝地图预留及当前范围

当前基础实施目标为单执行进程、每 Space 单 Cell 的路径，不启用无缝地图。设计保留一个 Space 多 Cell 的表达，不把“当前不做”写成永久禁止。

现在确定接口边界：Space/Cell/执行进程身份分开；全空间坐标与稳定实体 ID；分区范围、邻接、拓扑版本；单实体权威代次；Real/Ghost 与观察者视图边界；完整模拟状态快照；跨 Cell 请求裁决、时序、去重及失败对账。跨 Cell 的共享 Instance 目标/结算须有唯一逻辑负责人，不要求所有分区共享一个内存对象或全局锁步。

以后实现实际空间切分、邻接复制、跨边界战斗、动态负载均衡和故障接管。默认模式不必运行这些算法，但协议和对象模型不能假定 Space 永远等于一个进程。

## 6. 当前代码对应与迁移边界

| 当前 Apollo 代码 | 对应语义 | 需要补齐 |
|---|---|---|
| WorldApp、World、WorldHost | 目前将管理和模拟装在一起 | worldServer 管理与 cell 执行拆分；旧类名只标识当前实现。 |
| Scene、SceneDescriptor | 单进程空间容器及静态资源引用 | 统一 Space 语义、空间 ID 与全局坐标；本地分区状态与逻辑空间身份分开。 |
| Instance | 临时玩法生命周期 | 保留八态，补跨进程结算和共享状态唯一负责人。 |
| `base::Avatar`、`cell::Avatar` | Base 长期资料与玩家 Cell 实体 | 独立宿主、快照和可信关联，禁止跨界直接指针调用。 |
| SceneAoi、ViewerState | 本地分区可见性原语 | 邻接复制、全空间可见性合并、去重与连续观察。 |
| WorldAssignment、RemoteEntityRef | 路由/引用原语 | 分开 Base、World、Space、Cell 和执行进程，带有效代次和版本。 |
| SceneTransfer | 当前显式换场景，销毁后重建 Avatar | 同 Space 跨 Cell 保留血量、技能、计时和必要脚本状态，不走离场重置。 |

本次整理不重命名 C++ 类、可执行文件或协议字段，不把迁移计划写成代码已完成。后续代码按根目录 todo.md 实施，文档只保留这套语义和清晰的实现映射。

管理/执行拆分的具体方案见 [World 与 cell 管理设计](./world-cell-management.md)（评审草案）：拟统一短名 `world`，无参数启动管理角色、`--role=cell` 启动执行角色，本机自动扩容、自然排空缩容；命名与接线迁移尚待实施。
