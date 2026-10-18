# Apollo 概念与命名

名称按职责固定：gateway、loginServer、baseServer、worldServer、database、machined，以及 worldServer 管理的 `cell` 执行进程。空间和实体语义尽量复用 BigWorld。完整的上游含义、Apollo 对应、代码类型与缺口见[BigWorld 与 Apollo 对照](./bigworld-apollo-mapping.md)。

## 进程与执行边界

| Apollo 名称 | 对照职责 | 所有权 |
|---|---|---|
| gateway | 独立连接层，承担部分 Proxy/会话相关职责 | 客户端 socket 与投递，缓存 Base 和模拟目标路由。 |
| loginServer | LoginApp 加 Apollo 的服务/在线目录职责 | 认证、Base/World 服务寻址、玩家归属与位置投影。 |
| baseServer | BaseApp 的长期非空间逻辑 | `base::Avatar` 等长期对象、基础业务、结算校验。 |
| worldServer | CellAppMgr 的空间/执行进程管理职责 | cell 注册、就绪、负载、Space/Cell 分配与拓扑、迁移和排空。 |
| cell 执行进程 | CellApp | 本地 Cell 的实体、AOI、战斗、脚本和 tick，向各所属 Base 提交业务结果。 |
| database | DBApp 的存储访问职责 | 资料加载、保存、存储路由、持久结果；不制定业务奖励。 |
| machined | 本机监督职责 | 拉起、停止、死亡检测和重启，不裁决跨服或玩家归属。 |

cell 自注册到所属 worldServer；独立部署不要求同机 machined。worldServer 是拓扑权威，loginServer 的位置投影与 gateway 缓存随版本刷新，不重复裁决分区所有权。

## 空间与玩法边界

| 概念 | Apollo 含义 |
|---|---|
| 地图资源 | 静态几何/配置；现有 SceneDescriptor 引用它，不表示一次运行空间。 |
| Space | 一份有独立 ID 和坐标的运行空间，对应现有 Scene 的空间语义；两者不叠加为同义容器。现有 Scene 是单进程实现映射。 |
| Cell | Space 的空间分区，记录范围与权威实体，分配到执行进程；不等于 cell 进程。 |
| Instance | Apollo 的临时玩法生命周期，管理小队、目标、计时、结算和回收；关联 Space，不冒称 BigWorld 原生八态类型。 |
| Base 实体 | baseServer 的长期非空间对象，玩家侧现有代码为 `base::Avatar`。 |
| Cell/Real 实体 | cell 进程中拥有模拟写权的实体，玩家侧复用 `cell::Avatar`。 |
| Ghost | 邻接分区的非权威投影，设计预留；不执行独立业务写入。 |
| Witness | 面向客户端的可见世界上下文；现有 ViewerState、SceneAoi 与属性同步只承担部分职责。 |

默认一个 cell 执行进程，每个 Space 一个 Cell；同一进程可以承载多个 Space 的 Cell。扩展先分配完整 Space 到多个执行进程，未来允许一个 Space 多 Cell。Space 身份和坐标不因执行进程或分区变化而改变。

## 角色实体与 Avatar

Avatar 在 BigWorld 词汇中可包含玩家和 NPC；BigWorld/KBEngine 的角色实体可按需有 Base、Cell、客户端部分。Apollo 的玩家角色由 `base::Avatar`（Base 部分）与 `cell::Avatar`（Cell 模拟部分）两个实现类共同表达，命名是 Apollo 的实现约定，不是上游通用术语，也不表示固定 Base 进程。文档使用 Base/Cell 部分描述职责，代码引用保持真实名称；行业原文与命名判断见[玩家对象模型](./player-object-model.md)。

## 数据归属与生命周期

长期资料只由所属 Base 校验修改并经 database 保存；模拟实体只由一个权威 Cell 推进。跨服、跨 Space、跨 Cell 的模拟迁移不改变长期资料和原存储归属；Base 承载进程可按负载或更新独立迁移，玩家到当前 Base 的目录记录和写权代次必须同步交接。Instance 销毁只释放临时玩法和空间资源，不销毁长期资料。

小队请求 → Base 校验 → worldServer 分配 → cell 创建关联 Space/Cell 与玩法 → 成员运行 → 各所属 Base/database 持久确认结算 → 离开并回收。复用既有 `Create → Initialize → Waiting → Running → Finishing → Rewarding → Draining → Destroyed`。

## 迁移、更新与无缝预留

跨 Space 换场与同 Space 跨 Cell 连续交接分别定义；后者保持实体 ID、位置、血量、技能和必要脚本状态，不触发一次完整离场重置。共享玩法有唯一逻辑负责人，不能把整个多 Cell 空间锁定在一个进程内对象或线程。

协议更新分别处理 Base、worldServer 管理状态和 cell 模拟状态的进程替换；仅代码更新保持 wire 契约不变，可使用 Lua 模块发布。当前不启用无缝地图，预留空间分区、邻接可见性、Real/Ghost、权威交接和跨 Cell 交互边界。实现和验收只维护在根目录 todo.md。
