# Apollo 对象所有权模型（object-model）

> 所属：第一阶段只读架构审查配套件之三。配套：audit.md（仓库层）、architecture.md（架构地图/依赖）、lifecycle.md（生命周期）、concurrency.md（并发）、persistence.md（持久化）、improvement-plan.md（改进计划）。
> 依据：任务书 §5（对象所有权是本审查的重点）、§6（Player 一等对象）、§12（Player/Connection/Session 三者不混乱绑定）、§14（Entity 模型）、§15（ECS 审查）。术语按 term-contract v1.0；差异清单见文末。

---

## 1. 现状：玩家侧对象的实际分布（以代码为准）

代码中与「玩家」相关的对象有四类职责载体，但**没有一个叫 Account/Avatar 的类**：

| 代码件 | 文件 | 职责 | 状态 |
|---|---|---|---|
| PlayerAnchor | modules/game/session/player_anchor.hpp（anchor_manager.hpp 持有） | 长期归属：player_id + 六态 + SessionBinding + WorldAssignment + dirty 标记 | 唯一被 base-app 生产使用的玩家长期对象；无持久化钩子 |
| SessionLocator | modules/game/session/session_locator.hpp | player↔session 双向索引（bind 顶掉旧条目） | baseappmgr 与 base-app 使用 |
| WorldSession | modules/game/world/world_session.hpp | 七态簿记状态机（Entering…Closed）+ avatar EntityId + 四位置字段 | 仅 cell-app 使用；与网络零耦合 |
| cell::PlayerEntity | apps/cell-app/include/cell/cell_manager.hpp:42-93 | 「场景内玩家」的唯一近似物；update 空、onEnterView/onLeaveView 空 | 仅 cell-app 内部 |

另有两处命名空间混用的事实：`WorldSession` 的 player 键在验证路径用 **EntityId** 当 **PlayerId** 查（cell_server.cpp:559 `find_by_player(entity_id)`）；cell-app 把 map_instance_id 直接当 space_id 赋值（cell_server.cpp:544-545）。

**对照契约三件套（term-contract §1.2）**：Session/Proxy（连接承载，gateway 面）→ **无 Proxy 类**，gateway 侧无会话对象（只有连接 socket）；PlayerAnchor/Base（长期归属，驻 Home Zone）→ 有类、无 Home Zone 概念（存在 base-app 进程无定位字段）；Avatar/Cell（场景化身，进 scene 生、出 scene 死）→ **无类**，最接近的 cell::PlayerEntity 无生命周期规则。

---

## 2. 任务书 §5 所有权矩阵：逐对象现状

> 模板字段：Owner / Created / Destroyed / Runtime Authority / Persistence / Current Scene / Connection / Reconnect / Scene Transfer。每格填「现状」与「目标（契约/设计口径）」；「?」= 当前无法明确回答（任务书 §5：必须列为架构问题）。

### 2.1 World（世界）

| 维度 | 现状 | 目标 |
|---|---|---|
| Owner | 无 World 对象（WorldHost 是服务宿主，不起世界之职） | world = Zone 内世界容器（create_scene/scene 管理） |
| Created/Destroyed | 无 | 随 Zone 进程启停 |
| Runtime Authority | 无 | Zone 内场景集合的单一权威 |
| Persistence | — | 无持久化（世界配置属 Definition 层） |
| 结论 | **World 不是一等对象**：任务书 §27 目标 API 的第一段 `world.create_scene` 无法落代码 | — |

### 2.2 PlayerAnchor（玩家锚 ≡ Base；契约：驻 Home Zone）

| 维度 | 现状 | 目标（契约） |
|---|---|---|
| Owner | AnchorManager（base-app 内，mutex+map） | Home Zone 持有（本进程内单一权威）——语义一致，缺「Home Zone 分配」机制 |
| Created | base-app activatePlayer（DB 加载后 make_shared） | 登录落点阶段（login-flow 分配 Home Zone 时） |
| Destroyed | AnchorManager::deactivate（erase） | 登出存档完成后 |
| Runtime Authority | **是**（内存 map 是唯一真相；DB 假实现） | 是（设计决策：在线玩家内存为权威） |
| Persistence | 无钩子；dirty 标记存在但仅测试消费 needs_save；save 回路在 base-app SaveQueue（假） | dirty→write-behind journal（attribute-sync §8.2） |
| Current Scene | WorldAssignment（world/map/instance/space + route_version）——**字段正确** | 同上+悬浮态语义 |
| Connection | SessionBinding（session_id/gateway_id/gateway_addr/bind_time_ms） | 同 |
| Reconnect | 无窗口机制（Suspended 仅是另一个对象 WorldSession 的枚举） | 掉线保活窗口（Suspended 态）与 resume_token 同源 TTL |
| Scene Transfer | 无（锚不参与换幕——正确！但无任何代码体现「换幕只动 Cell」） | 锚不动、Cell 换幕 |
| 结论 | **玩家长期对象的方向是对的**（内存权威+绑定+位置字段齐备），但缺三件事：持久化回路、Home Zone 定位、保活窗口 | — |

### 2.3 Avatar（场景化身 ≡ Cell）—— 不存在

| 维度 | 现状 | 目标（契约） |
|---|---|---|
| Owner | 无（cell::PlayerEntity 归 cell::EntityManager 裸指针持有） | 当前 scene（scene 生它、scene 死它） |
| Created | 无构造路径 | 进 scene 时（状态从 PlayerAnchor 投影） |
| Destroyed | 无 | 出 scene/断线窗口满（移除） |
| Runtime Authority | — | scene 线程内移动/战斗/AOI 广播权威 |
| Persistence | — | L3 易失态不落档、换幕不带走 |
| Current Scene | — | 随 scene 存续 |
| Reconnect | — | 按场景规则重建（原地/复活点/主城由玩法定） |
| Scene Transfer | — | 「换幕」= 销毁重建，**不做对象搬迁**（决策 #9 否决无缝） |
| 结论 | **任务书 §6「Runtime Player」与契约「Cell」的落点**——这是当前模型最大的洞：无类、无构造、无生命周期 | — |

### 2.4 Entity（通用场景实体）

| 维度 | 现状 | 目标 |
|---|---|---|
| Owner | 四~五套并存（audit.md A1）：cell::EntityManager（裸指针 map）/ legacy ecs（unique_ptr+裸指针回指+悬垂风险）/ legacy battle::ecs / modules/game/core Entity / legacy ecs.h | 归 Scene 所有（scene.entities_） |
| Created | 无生产构造路径（各 Manager 的 add 仅测试/框架内部） | 场景内 spawn |
| Runtime Authority | 无 tick 阶段（EntityManager::update 持锁遍历） | scene tick 单写者 |
| 类型族 | 无 NPC/Monster/Projectile/Drop/Building/Tower 类；entities.xml 的继承链（Monster→NPC→Avatar→Player）在代码无对应 | 契约实体族（entities.xml 语义，含 Avatar；Player 名已废） |
| 结论 | 任务书 §14 的「Entity 归一管理」质问成立：**全局 Manager 拥有实体、Scene 反而不拥有** | — |

### 2.5 Scene（场景——契约：世界中一块有归属的逻辑单元）

| 维度 | 现状 | 目标 |
|---|---|---|
| Owner | 无独立所有权：Scene 是 MapInstance 的值成员（MapInstance→WorldSpace→Scene 三层空壳） | 属 world 容器（Zone 内） |
| Created | 仅 ensureDefaultMapInstance 硬编码 id=1 "default-world" | world.create_scene(...) |
| Runtime Authority | Scene::update 遍历 entities_（永远是空的） | scene tick 阶段（simulate/recalc/aoi/collect/persist） |
| 拥有 AOI | **否**（Scene 无 AOI 成员） | 是（scene_id 隔离） |
| 拥有 Player 进出 | **否**（无 enter/leave API） | 是（instance.enter/leave(player)） |
| 独立运行/停止/恢复 | 否（无状态机） | 是（生命周期状态） |
| 结论 | 任务书 §8 的设问全部命中：**Scene 只是数据结构，运行逻辑散落在 cell-server/EntityManager/AOIManager** | — |

### 2.6 Instance（副本——契约：一次玩法开启的场景实例）

| 维度 | 现状 | 目标（任务书 §9） |
|---|---|---|
| Owner | MapInstanceManager（mutex+map，shared_ptr 注册表） | scene 派生：create_instance |
| Created | MapInstanceManager::create_instance（唯一调用点 = ensureDefaultMapInstance 的硬编码路径） | 玩法准入创建 |
| Lifecycle | **无状态机**：五操作（create/find/destroy/update/count），销毁=从 map 擦除 | Create→Initialize→Waiting→Running→Finishing→Rewarding→Draining→Destroyed 八段 |
| Runtime Authority | MapInstanceManager::update 持锁跑内层 tick 树（map_instance.cpp 未编译） | scene 独立 tick |
| 结论 | 任务书 §9/§10（Instance 一等对象、Scene≠Instance≠Map）双命中：**代码是「每实例一个 Scene」的字面三层层叠+无生命周期**；Map（静态资产）在代码中无独立概念（只有 map_name 字符串） | — |

### 2.7 Session / Connection（会话与连接——任务书 §12 焦点）

| 维度 | 现状 | 目标 |
|---|---|---|
| 关系 | **Connection = Session 混同**：gateway 无会话对象（裸 TCP 连接即一切）；两套 Session 域（apps 各自）与 game 层零耦合 | Connection→Session→PlayerAnchor 严格分层 |
| Created | 连接接受即存在 | 鉴权通过创建 |
| 断线 | 无（disconnect 以纯文本发后端、无 handler；无重连代码） | Session 可失可重挂（resume token） |
| 顶号 | SessionLocator::bind 顶掉旧条目（进程内雏形） | 顶号预裁在 manager 域 |
| 结论 | 任务书 §12 命中：**Player 强绑定 Connection** 的现状不存在 Player，但绑定方向是错的（连接即一切） | — |

### 2.8 AOI

| 维度 | 现状 | 目标 |
|---|---|---|
| Owner | 两套（legacy AOIGrid 仅测试；cell::AOIManager 属 cell-server）——**Scene 均无** | 归 scene（scene_id 隔离） |
| Runtime Authority | cell::AOIManager 网格迁移；事件只分发 LEAVE；ENTER/SYNC 未填充；ViewerState 不存在 | viewer set + 增量广播（四通道） |
| 结论 | 任务书 §13 命中：AOI 未成为 Scene 的兴趣管理，且两套并存 | — |

### 2.9 Guild / Party（公会/队伍——任务书 §5 清单件）

- **代码中不存在**（全仓 grep 无 guild/party 类）；entities.xml 的 Player 实体注释「含社交面」也只是注释。设计侧 player-object-model 把社交归 Base 长期态。→ 列入 P2（social 域），当前无对象可审计。

---

## 3. 目标所有权模型（改进计划的锚点）

> 契约 §1.2 + player-object-model §4 三轴收口 + 任务书 §5/§6/§10 的交集；本表即 improvement-plan P0 的验收骨架。

```text
进程轴：  gateway-app（连接面） | Zone/逻辑服进程（1..N scene；Home Zone 驻 PlayerAnchor）
空间轴：  world ──1..N──> scene ──1..N──> instance（= scene 的玩法生命周期别名）
          scene 拥有：entities / AOI / tick 阶段 / 玩家进出
          instance 拥有：玩法八态生命周期 + scene（或引用）
对象轴：  Connection（gateway）→ Session（可失可重挂）→ PlayerAnchor（Home Zone，不动）
          │                                                            │
          └──────── 换幕：旧 scene 销毁 Avatar，新 scene 重建 Avatar（状态从 Anchor 投影，易失态重置）
```

| 对象 | Owner | Runtime Authority | Thread | Persistence | Visibility | Transfer | Recovery |
|---|---|---|---|---|---|---|---|
| Account | 账号库（DB 侧） | 登录取用 | — | 是（建号/鉴权） | — | — | 鉴权重试 |
| PlayerAnchor | Home Zone | Zone 内单线程（主页线程） | 单写者 | 在线内存权威 + write-behind | 无（不广播） | 不迁移 | 进程重启 → 从 DB 加载重建 |
| Avatar | 当前 scene | scene 线程 | scene 线程单写者 | L3 易失态不落档 | scene AOI 内广播 | 换幕销毁重建 | 断线窗口内重建 |
| Session | gateway（逻辑挂 Anchor） | gateway 线程 | 连接线程 | 无 | — | resume 重挂 | resume_token |
| Scene | world（Zone） | scene 线程 | 单写者 | 运行时态持久化归玩法结果单向落 Anchor | AOI 边界 | — | 崩溃重开实例 |
| Instance | scene 派生 | scene 线程 | 同上 | 结果落 Anchor | — | — | abort/restore |
| Entity | scene | scene 线程 | 同上 | 若需要随玩法 | AOI | 场景内仅 | 场景内重建 |
| AOI | scene | scene 线程 | 同上 | 无 | 自身即兴趣分发表 | — | 重建 |

---

## 4. 所有权问题清单（现状 → 问题编号）

| # | 问题 | 证据 | 级别 |
|---|---|---|---|
| O-1 | Avatar 无类、无构造路径、无生命周期 | §2.3 | P0 |
| O-2 | Scene 不拥有实体/AOI/玩家进出——运行逻辑在 cell-server 私有组件 | §2.5 | P0 |
| O-3 | Instance 无玩法生命周期状态机；Map=Scene=Instance 三概念互混（MapInstance→WorldSpace→Scene 三层空壳，三层无行为差异） | map_instance.hpp/world_space.hpp；audit.md §5 | P0 |
| O-4 | PlayerId/EntityId/space 三命名空间混用 | cell_server.cpp:544-545,559 | P0 |
| O-5 | 全局 Manager 拥有实体（EntityManager/MapInstanceManager），与任务书 §14 的「Entity→Scene」目标相反 | cell_server.cpp:40 | P1 |
| O-6 | Connection/Session/Player 绑定颠倒：无 Session 层、断线即失 | gateway_server.cpp | P1 |
| O-7 | 保活窗口/恢复无对象承载（Suspended 枚举无窗口） | world_session.cpp:74-101 | P1 |
| O-8 | guild/party/social 无对象（设计有、代码无） | 全仓 grep | P2 |
| O-9 | 五套 Entity/ECS 互不通用 | audit.md A1 | P2 |
| O-10 | 两套 AOI 无归属 | audit.md A2 | P1 |
| O-11 | 属性两套无归属、无同步管线 | audit.md A4 | P2 |

---

## 5. 术语契约差异清单（object-model 级）

1. **「Player」概念落点**：任务书 §6 要求 Player 拥有一等对象身份（Identity/Attributes/Inventory/Equipment/Quest/Progress/Social/Guild/Runtime State）；按契约修文为**承载者 = PlayerAnchor（持久态）+ Avatar（运行时态）**，Inventory/Equipment/Quest 等字段在代码**完全不存在**（base-app PlayerData 只有 playerId/username/level/exp/hp/maxHp/mp/maxMp/x/y/z 9 字段，database_service.hpp:22-39）——差异登记：任务书功能面（背包/装备/任务）在契约语义下归属 Anchor 持久态，P0 Player 模型重建时需补字段模型，不是代码命名问题。
2. **entities.xml 四层继承链（Monster→NPC→Avatar→Player）与契约冲突**：契约裁决 Player 已废、Avatar 为唯一玩家空间对象；entities.xml:8 `<entity id="Player" parent="Avatar"/>` 必须按契约修文——本阶段登记，具体修订（删 Player 或并入 Avatar、NPC 是否继承怪物族）列入 P2 契约修订批次（需走「先 glossary 后 contract」新术语流程）。
3. **WorldSession 命名**：契约 Session=连接承载（gateway 面）；代码 WorldSession 是「场景会话簿记」——职责与契约 Session 不同、命名冲突；随 P0 session 模型重建更名（建议 `SceneSession` 或并入 WorldAssignment 记录，见 improvement-plan P0-3）。
4. **WorldSpace 命名含「Space」**（契约禁 Space 裸用）：随三层空壳消解散件更名。
5. **MapInstance = Instance 的代码名**：保留（契约 `instance` 义），但语义必须从「注册表条目」升为「八态玩法生命周期」（O-3）。
6. **cell::PlayerEntity**（PascalCase cell 前缀）是临时名，Avatar 落地时删除；不把 cell 前缀写入契约。

---

（完）