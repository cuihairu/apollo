# docs/architecture/ 文档状态表（参考件区）

> 状态：**登记性 README（2026-09-30 落盘，2026-10-01 清理批重写）**。判定原则不变：**现行设计权威 = docs/design/ 十一份（含 concept-glossary）**；本目录整体为参考件/历史件。
> **2026-10-01 过期设计清理（用户裁决）**：原 C 档（已被 docs/design/ 取代的历史设计稿，35 份）与 D 档（历史任务清单/路线图，22 份）共 **57 份已 git rm**（git 历史可溯，提交见当日清理批）；删除前全仓引用面核查完成，活链接四处已改指/删除（guide/index、guide/configuration、guide/quick-start、apps/index），battle-verification-service.md §8 表与 architecture-review :1800 的历史指针按「git 可溯」纪律处理。判定依据与逐份「被谁取代」对照表见 git 历史中本文件 2026-09-30 版本（design-gap-inventory §4.1 v1 口径）。

## A 档：仍被权威文档实引（4 份）

| 文件 | 引用方 | 地位 |
|---|---|---|
| remote-entity-call-design.md | net-abstraction §7、concept-glossary | **RemoteEntityCall 语义层权威**（传输底座已接 net-abstraction §5.6/§5.7——文档中 Channel/Endpoint 旧词按 M1 内核口径读） |
| observability-watcher-and-runtime-introspection-design.md | scripting-lua §7 | **attach/watcher/profile 语义层权威** |
| host-builder-and-di-design.md | architecture-review §20/§21（审计基准） | 参考件（**C-76 裁决已落档 2026-09-30**：文件头部状态注 + architecture-review §24 收口）；实现现状 = §20 十项对照 |
| starter-and-module-assembly-design.md | architecture-review §1/§4 | 装配思想历史源（现行口径 = architecture-review §17/§21 + 四 main 装配现状） |

## B 档：引擎源码分析参考件（读作「先例证据」，不读作「apollo 设计」）

bigworld.md、bigworld-lifecycle.md、kbe-source-analysis.md、kbe-reference-principles.md、kbengine-entitydef-analysis.md、base-cell-proxy-model.md、witness-ghost-design.md（ghost 已被决策 #9 否决——读作 BW 机制参考）、aoi.md、aoi-broadcast.md，以及 **mmo-frameworks/ 子目录 25 份**（agones/spatialos/nakama/photon/playfab/pragma/trinitycore/ryzom-core/mirror/fishnet/colyseus/beamable/hathora/edgegap/gamelift/accelbyte/azerothcore/o3de/smartfoxserver/unity-gaming-services/open-world-server/kbengine 等 + comparison/scorecard/adoption-guide）——第三方框架对比分析，36 号的扩展证据库。

---

**消化纪律**：C/D 档已清空（2026-10-01）；后续若 B 档件被权威稿判定失效，按同流程（引用面核查 → git rm → 本表改写）处置。已删文档一律按「git 历史可溯」读（concept-glossary §3 纪律），不复述内容、只留指针。
