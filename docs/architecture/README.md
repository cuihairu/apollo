# docs/architecture/ 文档状态表（参考件区）

> 状态：**登记性 README（2026-09-30 文档重整理批）**——本目录 70 份 + mmo-frameworks/ 25 份与权威稿的关系对照。判定原则：**现行设计权威 = docs/design/ 十份（含 concept-glossary）**；本目录整体降为参考件/历史件，其中 4 份仍被权威稿实引（A 档）。**本表只登记状态、不删档**——删除候选需用户逐批裁决（此前两批删除：docs 根 B 级清理 37 份 + docs/05，均 git 可溯）。
> 判定依据：design-gap-inventory §4.1 v1 口径 + 2026-09-30 引用面清查（design/analysis → architecture/ 实引）；三档之外新增「已被取代的历史设计稿」「历史任务清单」两档以便逐档消化。

## A 档：仍被权威文档实引（4 份）

| 文件 | 引用方 | 地位 |
|---|---|---|
| remote-entity-call-design.md | net-abstraction §7、concept-glossary | **RemoteEntityCall 语义层权威**（传输底座已接 net-abstraction §5.6/§5.7——文档中 Channel/Endpoint 旧词按 M1 内核口径读） |
| observability-watcher-and-runtime-introspection-design.md | scripting-lua §7 | **attach/watcher/profile 语义层权威** |
| host-builder-and-di-design.md | architecture-review §20/§21（审计基准） | 参考件（C-76 状态裁决随 §4.1 盘点批——本表即其输入）；实现现状 = §20 十项对照 |
| starter-and-module-assembly-design.md | architecture-review §1/§4 | 装配思想历史源（现行口径 = architecture-review §17/§21 + 四 main 装配现状） |

## B 档：引擎源码分析参考件（读作「先例证据」，不读作「apollo 设计」）

bigworld.md、bigworld-lifecycle.md、kbe-source-analysis.md、kbe-reference-principles.md、kbengine-entitydef-analysis.md、base-cell-proxy-model.md、witness-ghost-design.md（ghost 已被决策 #9 否决——读作 BW 机制参考）、aoi.md、aoi-broadcast.md，以及 **mmo-frameworks/ 子目录 25 份**（agones/spatialos/nakama/photon/playfab/pragma/trinitycore/ryzom-core/mirror/fishnet/colyseus/beamable/hathora/edgegap/gamelift/accelbyte/azerothcore/o3de/smartfoxserver/unity-gaming-services/open-world-server/kbengine 等 + comparison/scorecard/adoption-guide）——第三方框架对比分析，36 号的扩展证据库。

## C 档：已被 docs/design/ 取代的历史设计稿

| 文件 | 被谁取代 / 现行权威 |
|---|---|
| entity-schema-design.md | sdk-contract（XML+XSD 契约） |
| replication-pipeline-design.md | attribute-sync（三代属性容器/差分/水位） |
| script-layer-design.md、lua-backend-design.md | scripting-lua（Lua 5.5 白名单、原生 C API） |
| python-backend-design.md | 已否决（决策 #12——攻击面） |
| gateway-session-design.md、gateway-ingress-facade-design.md、login-app-design.md | 未取代——**设计缺口 #13/#14 域**（登录链路/入站对接，design-gap-inventory） |
| player-anchor-design.md、base-app-evolution.md、world-host-design.md | cell/base 拆分族——已被决策 #9 否决（Zone 制，不拆两族进程） |
| distributed-space-design.md、space-partition-topology-design.md、authority-transfer-design.md、app-manager-design.md | 无缝世界/实体迁移族——已被 #9 否决；接管语义现行 = net-abstraction §7 G-2/manager 域 |
| shard-zone-instance-match-topology-design.md | 术语已被 concept-glossary 取代（副本 instance/Zone/scene 三粒度） |
| world-entry-transfer-design.md | todo 批次 3（跨 Zone 显式 handoff） |
| domain-event-and-message-bus-design.md、internal-service-client-and-envelope-design.md | net-abstraction §5.7（InterServerLink）/§7（RemoteEntityCall、internal 域信封） |
| entity-lifecycle-and-state-machine-design.md | attribute-sync §10 六阶段 + ECS 域（随代码批次细化） |
| interest-management-and-aoi-pipeline-design.md | 决策 #10（AOI 独立服务） |
| navigation-movement-and-physics-boundary-design.md | **设计缺口 #16**（地图与空间数据管线）+ todo 批次 5 |
| combat-runtime-and-ecs-boundary-design.md | battle-determinism + concept-glossary 副本/战斗验证词条 |
| persistence-process-and-db-manager-design.md、persistence-repository-unitofwork-design.md | attribute-sync §8（journal/DDL）+ todo 批次 2（db-app/storage.xml） |
| reliability-failover-and-recovery-design.md | net-abstraction §7 G-2（backup-hash 链/reviver/恢复相位） |
| runtime-ops-host-design.md、platform-foundation-design.md、configuration-and-profile-design.md、capability-and-feature-flag-design.md、module-manifest-and-registry-design.md、module-reorganization-design.md、app-bootstrap-lifecycle-design.md | 装配/宿主族——现行权威 = architecture-review §17/§20/§21（HostBuilder/容器/starter/profile 全族零实现的审计结论 + di 显式装配路线）；实现随代码批次 |
| testing-and-verification-strategy.md | 随代码批次（golden/ctest 双闸已立，todo 批次纪律） |
| world-tick-scheduling-and-job-model.md | clock-and-time（主循环 10Hz、消息段、tick 边界） |

## D 档：历史任务清单/路线图（使命已完成或被取代）

apollo-architecture-gap-analysis、apollo-final-blueprint、apollo-implementation-checklist、apollo-layering-design、apollo-progressive-game-framework、apollo-refactor-roadmap、apollo-world-architecture、process-reorganization-and-rollout-plan、process-semantics-redefinition、technology-convergence-and-replacement-plan、technology-convergence-execution-plan、distributed-world-task-checklist、distributed-world-topology-implementation-plan、mmo-code-task-mapping、mmo-component-assembly-catalog、mmo-module-rollout-plan、mmo-topology-scope-and-composition-design、standard-mmo-task-checklist、player-online-flow、topology-comparison-and-login-flow-design、lightweight-mmo-and-tower-defense-fit（定位文——现行定位见 docs/index.md + docs/30）、overview（目录页——已被 docs/index.md 取代）。

---

**消化纪律**（design-gap-inventory §4.1 原口径延续）：后续设计批次顺手核与其主题相关的 C 档稿——新权威稿落盘时，对应 C 档行从「被谁取代」列改注权威指针已闭环；D 档随任务实际完成逐批判废。删除候选（建议优先级：D 档 roadmap 族 → C 档装配族）待用户裁决后 git rm。
