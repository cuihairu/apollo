[English](README.en.md) | [中文](README.md)

# docs/architecture/ Documentation Status Table (Reference Area)

> Status: **registry README (written 2026-09-30, rewritten in the 2026-10-01 cleanup batch)**. The judging principle is unchanged: **the current design authority = the eleven docs in docs/design/ (including concept-glossary)**; this directory as a whole is reference/historical material.
> **2026-10-01 stale-design cleanup (user ruling)**: the former tier C (historical design drafts superseded by docs/design/, 35 docs) and tier D (historical task lists/roadmaps, 22 docs) — **57 docs in total, git rm'd** (traceable in git history; see the same-day cleanup batch commit); before deletion the repo-wide reference surface was fully checked, and the four live links were retargeted/removed (guide/index, guide/configuration, guide/quick-start, apps/index); the historical pointers in battle-verification-service.md §8's table and architecture-review :1800 were handled per the "traceable in git" discipline. The judging basis and the per-doc "superseded by whom" mapping table are in this file's 2026-09-30 version in git history (design-gap-inventory §4.1 v1 semantics).

## Tier A: Still Cited by Authoritative Docs (4 docs)

| File | Cited by | Standing |
|---|---|---|
| remote-entity-call-design.md | net-abstraction §7, concept-glossary | **RemoteEntityCall semantic-layer authority** (the transport base is already wired to net-abstraction §5.6/§5.7 — read the doc's old Channel/Endpoint terms per the M1 kernel semantics) |
| observability-watcher-and-runtime-introspection-design.md | scripting-lua §7 | **attach/watcher/profile semantic-layer authority** |
| host-builder-and-di-design.md | architecture-review §20/§21 (audit baseline) | Reference piece (**C-76 ruling on record 2026-09-30**: the file-header status note + architecture-review §24 closure); implementation status = the §20 ten-item comparison |
| starter-and-module-assembly-design.md | architecture-review §1/§4 | The historical source of the assembly idea (current semantics = architecture-review §17/§21 + the four main assembly states) |

## Tier B: Engine Source-Analysis Reference Pieces (read as "precedent evidence", not as "apollo design")

bigworld.md, bigworld-lifecycle.md, kbe-source-analysis.md, kbe-reference-principles.md, kbengine-entitydef-analysis.md, base-cell-proxy-model.md, witness-ghost-design.md (ghost was already rejected by decision #9 — read it as a BW mechanism reference), aoi.md, aoi-broadcast.md, plus **25 docs in mmo-frameworks/** (agones/spatialos/nakama/photon/playfab/pragma/trinitycore/ryzom-core/mirror/fishnet/colyseus/beamable/hathora/edgegap/gamelift/accelbyte/azerothcore/o3de/smartfoxserver/unity-gaming-services/open-world-server/kbengine etc. + comparison/scorecard/adoption-guide) — third-party framework comparative analysis, the extension evidence library of #36.

---

**Digestion discipline**: tiers C/D are already empty (2026-10-01); if a tier-B piece is later judged void by an authoritative draft, handle it by the same process (reference-surface check → git rm → rewrite this table). Deleted docs are always read as "traceable in git history" (concept-glossary §3 discipline): do not restate their content, keep pointers only.
