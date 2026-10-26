# 玩家对象模型

玩家的长期资料对象和场景实体具有不同所有者、生命周期与写入边界。术语见[概念与命名](./concept-glossary.md)。

## 行业术语与命名依据

`Avatar` 是已有的角色术语，但含义依赖引擎，不能据此推导它只在 Cell 侧存在，也不能限定为玩家实体。

| 来源 | 原有用法 | 对 Apollo 的意义 |
|---|---|---|
| [BigWorld 2.1 官方词汇表（存档）](https://howarduong.github.io/github.io/doc/html/glossary_of_terms/glossary_of_terms.html) | Avatar 是角色关联的实体，可以是玩家角色，也可以是 NPC；Entity 是可寻址的基本对象。 | 角色类型与执行位置是两个维度；Avatar 不是 CellApp 的同义词。此处引用 2.1 词义，不把其部署限制套到 Apollo。 |
| [BigWorld 2.1 官方教程 §3.4（存档）](https://howarduong.github.io/github.io/doc/html/bigworld_tutorial/bigworld_tutorial.html) | 示例为同一 Avatar 类型编写 Base 和 Cell 脚本，Base 部分继承 Proxy。 | Avatar 可以有 Base 部分；不是只存在于场景的对象，也不是引擎强制的玩家类名。 |
| [KBEngine 实体定义](https://kbengine.github.io/docs/programming/entitydef.html)、[Base Avatar 示例](https://github.com/kbengine/kbengine_demos_assets/blob/master/scripts/base/Avatar.py)、[Cell Avatar 示例](https://github.com/kbengine/kbengine_demos_assets/blob/master/scripts/cell/Avatar.py) | 按需定义 base/cell/client 部分；示例 Avatar 的 Base 部分继承 Proxy，Cell 部分继承 Entity。 | 复用“实体的 Base/Cell 部分”即可；不能把示例类名当作进程名或完整对象模型已经实现的证据。 |
| [Unreal 官方 Gameplay Framework](https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-framework-in-unreal-engine) | Pawn 表达可被玩家或 AI 控制的世界对象；Character 是其子类；PlayerState 与 Controller 是另外的职责。 | 同样区分角色实体、控制与状态，但不能把 PlayerState 当作 Apollo 的持久 Base 或直接照搬类名。 |
| [Unity 官方 Avatar/重定向说明](https://docs.unity.com/en-us/engine/6000.6/manual/animation-section/animation-mecanim/avatar-creationand-setup/retargeting) | 配置后的 Avatar 为人形动画重定向提供标准骨骼层级和名称。 | Unity 的动画 Avatar 不是这里的服务端实体；跨引擎没有一个含义完全一致的 Avatar 类。 |

命名判断：Avatar 用于角色语义是合适的，但单独一个 `Avatar` 只实现玩家的 Cell 模拟部分，范围比 BigWorld 的词义窄。曾用的 `PlayerAnchor` 在本次核对的上述官方实体术语中不是共同名称，且“Anchor”不表示玩家永远固定在某个 Base 进程——该名已随 2026-10 的命名迁移弃用，不再出现在生产代码与文档。

文档以**玩家角色实体的 Base 部分、Cell 部分**描述职责，引用实现时写 `apollo::game::base::Avatar`（Base 部分）与 `apollo::game::cell::Avatar`（Cell 部分）。不把二者宣传为上游通用的一对类型，不新增中间容器或同义类；生产代码无别名或转发头，旧类名不保留。

| 文档概念 | 当前代码对应 | 说明 |
|---|---|---|
| 玩家角色实体 | 由长期对象和模拟对象关联表达 | 是业务语义，不宣称当前已有完整通用分布式 Entity 类型。 |
| 玩家角色的 Base 部分 | `apollo::game::base::Avatar` | 独立于登录连接、Cell 对象和 Instance，可离线激活；承载进程可迁移。 |
| 玩家角色的 Cell 部分 | `apollo::game::cell::Avatar` | 当前玩家模拟对象，不代表整个逻辑角色，也不等于全部 Cell/Real 实体；NPC 和怪物可使用其他 Entity 类型。 |
| 客户端会话 | Session 及 gateway 连接 | 连接状态，不是玩家角色实体本身。 |

### Apollo 代码命名

Base 部分实现为 `apollo::game::base::Avatar`（modules/game/base），Cell 部分实现为 `apollo::game::cell::Avatar`（modules/game/cell），依赖单向 cell→base→core，用模块/命名空间表达分布式角色的不同部分。裸 `Avatar` 只在上下文明确时使用；不把两部分合并为共享内存对象，也不把所有 NPC/Entity 改为 Avatar。命名空间是 Apollo 的实现约定，依据是上游 Base/Cell 分部模型，不是声称 BigWorld 使用相同 C++ 类型。

命名迁移已完成（根目录 [todo.md 的 P0.1](https://github.com/cuihairu/apollo/blob/main/todo.md)）：`PlayerAnchor`→`base::Avatar`、`world::Avatar`→`cell::Avatar`，代码、头路径、附属符号与调用点同步清理，过渡映射已删；wire 状态串与契约字段逐字节不变，无别名或转发头。

行业术语的通用说明集中维护在 [hello-game](https://github.com/cuihairu/hello-game/blob/main/docs/glossary/11.md)，本页保留 Apollo 的职责和代码对应。

## 所有权

| 对象 | 所有者 | 状态与生命周期 |
|---|---|---|
| Base 部分（`base::Avatar`） | baseServer | 背包、装备、成长、任务和玩家基础业务；加载、在线、断线保留和卸载独立于副本。 |
| Cell 部分（`cell::Avatar`） | cell 内权威 Cell（现有 Scene 的空间语义） | 场景位置、战斗中的血量、技能运行状态和临时效果；按进出场及副本规则创建、迁移和释放。 |
| Session | gateway 连接与在线目录关联 | 绑定认证身份和当前连接，使用会话版本拒绝旧连接。 |

同一玩家的完整长期资料只有一个 Base 写入者，场景实体只有一个 cell 模拟者。cell 使用玩法所需快照，消耗或奖励通过请求交由 Base 校验；会跨对局保留的战斗结果应显式结算，不能默认所有场景字段都写入档案。

## 生命周期

目标生命周期中，登录、离线发货等可信业务均可请求所属 Base 经 database 加载或恢复长期资料；激活必须取得唯一写权，不能因为玩家离线而创建第二份写入者。已有对象则复用，未加载时经统一分配/激活机制处理。登录另行建立客户端会话，并不等于创建 Cell 部分。进入玩法时提供准入数据和只读基础属性快照，cell 创建场景实体。离开或副本结束后，Base 接收并幂等提交结算，cell 清理场景实体；销毁 Instance 不销毁长期资料。

## 跨服迁移

<ArchitectureDiagram kind="cross-server" />

跨服模拟迁移不改变长期资料与原区服的存储归属，也不搬迁完整档案或数据库凭证。Base 部分可因负载、恢复或更新迁往另一承载进程，投递按稳定玩家身份解析当前 Base 与归属代次，不把订单或业务请求永久绑定某个 Base ID。目标与源不得同时模拟同一实体；迁移结果不明先核对提交记录。对局连续迁移需同时覆盖整个 Instance 的共享状态，详见[跨服场景](./cross-server-scenes.md)。

## Base 迁移与升级

资料归属不等于固定承载进程。Base 负载迁移与升级都需交接长期资料对象、会话和在途操作，条件提交玩家到当前 Base 的归属记录并刷新路由；数据库归属保持不变。切换前排平写入，切换后拒绝旧进程写入，并处理迟到结算和重试。

## 当前实现

`base::Avatar`、`cell::Avatar`、Scene 与 Instance 类型可复用。长期资料与存储职责已由 WorldApp 迁出，由 base 独立入口持有并经 database 持久提交，WorldApp 不再宿主档案。Base 到模拟域的跨界已按类型面收口（P1）：跨域只传只读玩法快照值（`base::GameplaySnapshot`，换幕经 `SceneTransferRequest::gameplay_snapshot` 值投影 `cell::Avatar::project_from`），不再携带 `base::Avatar*` 指针；结算写权唯一路径是返回业务请求（契约 958/959），副本账本不再装配本地锚点解析。快照生产面（Base 按准入链构造并下发）与 cell 可信绑定随后续条目实施。未闭合部分：管理与执行分离（P2）。实施任务只维护在根目录 todo.md。

worldServer 管理 Space/Cell 分配，不另持一份玩家模拟对象。Base 实体对应 `base::Avatar`，Real/Cell 实体对应 `cell::Avatar`/Entity；原词与实现缺口见[体系对照](./bigworld-apollo-mapping.md)。
