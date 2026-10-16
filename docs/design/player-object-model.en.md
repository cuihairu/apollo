[English](player-object-model.en.md) | [中文](player-object-model.md)

# Apollo Player Object Model and Cell Slimming (player-object-model)

> Status: design draft (under review, wording finalized in the 2026-10-03 discussion, awaiting user review). Positioning: **the layering and ownership of player objects** — where the player objects live after login, who owns disconnect retention, who moves and who stays across scenes (entering an instance/switching lines); plus the **Cell slimming boundary** (what to cut/what to keep). Gap registration: design-gap-inventory #18 (added after the user identified on 2026-10-03 the contradiction "after login the player is on BaseApp and also in Zone"). Cross-references: concept-glossary §2.1 (scene/instance/baseapp granularity); session-and-online-directory (#12 registration anchor/displacement/disconnect keep-alive window); login-flow (#13 placement chain); battle-determinism (tick-boundary determinism); attribute-sync (§4.4 RO_MIRROR, L3 volatile state); No. 36 #9 (seamlessness rejected); base-cell-proxy-model.md (Grade-B reference piece — the evidence source for the KBE three-layer mechanism, not this design).

---

## 0. The Contradiction and the Ruling

**The contradiction (identified by the user on 2026-10-03, confirmed)**: three places could not be reconciled —

1. base-cell-proxy-model.md (Grade-B reference piece): "Session/Proxy, PlayerAnchor/Base, AvatarEntity/Cell **three layers must be layered**" (the KBE mechanism);
2. concept-glossary's baseapp entry: "deliberately **not splitting the cell/base two process families**" (the apollo decision face);
3. login-flow: "baseapp creates a **session entity**".

How many player objects there actually are, where they live after login, who owns disconnect retention, who moves and who stays when entering an instance — no authoritative carrier existed (#12 only governs online registration/displacement, not object-migration ownership). And the baseapp entry's "scene-instance granularity" contradicted §2.1's "hosts one or more scenes".

**The ruling (user, 2026-10-03)**: **A — the three-layer object model stays; the design focus = Cell slimming**. The user's words: "I understand the earlier redesign was meant to avoid cell complexity; in fact the main design work is on slimming the cell" — BW/KBE's cell-family complexity (ghost double-write/boundary negotiation/the full witness suite/seamless migration) is what gets cut; once cut, the Cell retains exactly one duty: "in-scene spatial authority".

**One-line closure**: **object layering ≠ process layering**. Proxy/Base/Cell are responsibility layers, the three co-located in the baseapp process (this is exactly what No. 36 #9's "don't split process families" means); BW/KBE have object layering + process separation — the separation is the tuition they paid for a seamless world; apollo takes the insight without paying the tuition.

## 1. Three-Layer Object Duties

| Object | Duty | Lifecycle | Location |
|---|---|---|---|
| **Proxy/Session** | Connection hosting: four-channel session, resume reconnect, displacement-switch remount | Lives with the connection, can be lost and remounted | The gateway connection face (logically attached to the baseapp session entity) |
| **Base (PlayerAnchor)** | Long-term logical ownership: disconnect retention, the cross-scene anchor, long-term state such as bag/progression, gameplay enter/exit coordination | Created at login → destroyed at logout | **Home baseapp** (never migrates with scene switches) |
| **Cell (Avatar)** | In-scene spatial authority: movement/combat/AOI broadcast | **Born on entering a scene, dies on leaving it** | The baseapp owning the current scene (or the instance process) |

The reason for separation (absorbing base-cell-proxy's correct insight): disconnect retention and cross-scene concurrency need an **unmoving anchor** — with a single object migrating wholesale, "who owns the main-world state while the player is in an instance" is forced back onto the persistence layer (too slow) or a global cache (superfluous); Base is that anchor. But **the three layers do not cross processes**: the cooperation completes inside the baseapp process, with no Mercury-style cross-process tax.

**Counter-plan record (single-object wholesale migration, the loser of the 2026-10-03 two-way choice)**: fewest concepts, adequate for most scenes under an instance regime; it lost because disconnect retention and state ownership during scene transfer had no natural placement, and it conflicted with the decision face's "don't split base/cell" narrative — the user's ruling chose A.

## 2. The Cell Slimming Checklist (the design's main cut)

| BW/KBE stock complexity | Disposition | Reason |
|---|---|---|
| ghost double-write/boundary negotiation | **CUT** (decision #9 already rejected seamlessness) | A Cell is bound to one scene for life, never moved across processes |
| inter-cell entity migration | **CUT** | cross-scene = "Cell scene transfer" (§3): destroy-and-rebuild, no object relocation |
| the full witness suite of priority/dual sequence numbers | **SLIMMED**: viewer set + three-channel seq one-way broadcast | the BW dumpAoI read (witness.cpp:2470-2516) stays as reference-piece evidence; the implementation carries no priority mechanism |
| disconnect handling | **RAISED to Base** (§3) | the Cell does not manage connection state — that is what Base exists for |
| combat/movement authority | **CONVERGED onto the Cell tick boundary** | battle-determinism's existing semantics (single judgment domain = the scene thread's tick boundary) unchanged |

With its duties narrowed, the Cell keeps exactly three things: **movement authority (reconcile), combat authority, AOI broadcast**. Connections, persistence, cross-scene, disconnect — none of them belong to it.

## 3. Key Sequences

**Scene transfer (entering an instance/switching lines/crossing scenes)**: Base stays put, Cell transfers —

1. gameplay admission/party start (gameplay layer) → the target scene (or instance process) is created;
2. the old scene destroys the Cell → the new scene rebuilds the Cell (**state is projected from Base**: equipment/attributes/long-term buffs);
3. in-scene volatile state (temporary buffs/CDs/coordinates) is **not carried over** — reset on entry per gameplay rules (coordinates = spawn point/entrance); settlement-type results (drops/rewards/quest progress) **fall back one-way to Base** at gameplay end (instance results falling back per the existing semantics, No. 36 #16).

**Disconnect**: the Cell idles in-scene for N seconds (Suspended state — the #12 disconnect keep-alive window, the window sharing one source value with the resume token TTL) → when the window expires the Cell is removed; **Base stays resident holding state**, directory entries handled per #12 §4, saves go through write-behind.

**Reconnect**: the Session remounts Base (the resume token restores the L2 session) → the Cell is rebuilt per scene rules (in place/revive point/main city — gameplay rules decide, the engine does not unify).

## 4. Carriage Relations (three-axis closure)

```text
Process axis   gateway(connection) / baseapp(logic server) / baseappmgr·login-app·DB·machined·logger
              │ one baseapp hosts 1..N scenes; the "instance process" = a process opened for a single scene (the offload form)
Space axis     scene = the only spatial unit (maps, partitions, instances are all scenes)
              instance = the gameplay-lifecycle alias of a scene (no longer a separate layer)
              room = the colloquial term for an instance (docs/30); line = parallel scene instances of the same map (not a first-class concept)
Object axis    Proxy/Session(gateway) + Base(Home baseapp, unmoving) + Cell(lives and dies with the scene)
```

One line (same as glossary §2.1): **a scene is "a piece of world", an instance is "one run of a piece of world", a baseapp is "the machine slot that runs the world"**; the answer to where the player "is" = **anchored in Base, spatial state in Cell**.

## 5. The Positioning Argument: No Seamless Map, Still an MMO

**The criterion of an MMO is "massive concurrent presence + persistent-world interaction", not a seamless map**. The hardest counterexamples are the industry ceiling itself: WoW (zone transitions + instanced dungeons + phased lines, never seamless), FF14 (zone-partitioned loading + the instance regime), the EQEmu family (the zone-process regime) — all recognized MMOs; truly seamless (BDO/EVE) is the minority. "Massively" speaks to the player scale and interaction density carried in the same world (persistent characters, multiplayer instances, guilds/economy/competition), not to whether the map underfoot is continuous.

Counting the other way: the price of seamlessness is exactly the whole set §2 cuts away (ghost/migration/boundary negotiation/cross-process debugging with no single-stepping); the partition/line regime buys instance fault-domain isolation, independent scaling of gameplay instances, a baseapp-granularity capacity model — more tangible for a 5000+ CCU SLO.

**The positioning sentence**: Apollo = a **lightweight MMO server engine** of the instance/line regime — covering the WoW/FF14-style mainstream MMO shape (zone worlds + instanced dungeons + lines), not chasing the seamless big world (the BDO/EVE type).

## 6. Process Model Comparison (precedent evidence)

Open-source/verifiable (the eight faces of No. 36 + direct reads of the mmo-frameworks analysis piece):

| Engine | Process topology | Where player objects live | Spatial model |
|---|---|---|---|
| BigWorld | machined (daemon) → loginapp → baseappmgr/cellappmgr → baseapp×N + cellapp×N + dbapp | Base in baseapp, spatial state in cellapp (two process families separated) | Seamless: inter-cell ghost double-write |
| KBEngine | machine → loginapp → dual mgr → baseapp×N + cellapp×N + dbmgr + interfaces | same as BW (same shape) | same ghost + entity migration |
| skynet | no preset topology: thousands of services in one process + gate/cluster across nodes | all business custom | homegrown 3×3 grid |
| TrinityCore (WoW official not public, the most verifiable emulation) | authserver + worldserver (a single world server) + the auth/characters/world three databases | objects inside the worldserver process | map/instance threads inside the world server, instances = in-process instances |
| Ryzom Core | one shard = a cluster of dedicated services (FES/EGS/AIS/GPMS/NS/WS/TS/MS/SU…13+) | spread across the dedicated services | finest service granularity, position service independent (GPMS) |
| EQEmu | one process per zone | inside the zone process | the scene-process regime |
| **Apollo** | login-app / baseappmgr / gateway-app / baseapp×N / instance / DB / machined / logger | **Base/Cell two object layers, co-located in baseapp** | scene = the only spatial unit, the instance/line regime |

Closed-source games (officials do not publish process models; only the spatial shape can be judged): WoW/FF14 = zone transitions + instanced dungeons + lines; Lineage = map switching; BDO = seamless; EVE = a single shard.

**Two lineages**: ① the Base/Cell separation school (BW/KBE) — elegant disconnect retention, the full price of seamlessness; ② the world-server/scene-server single-process school (TrinityCore/EQEmu/Ryzom) — no cross-process tax. **Apollo's hybrid stance**: object layering per BW/KBE + process merger per the EQEmu zone + service discovery per machined + AOI/position as an independent service absorbing the Ryzom GPMS idea (No. 36 #10).

## 7. Interlock with Existing Designs

| Existing piece | Interlock point |
|---|---|
| #12 session-and-online-directory | the registration anchor = Base; displacement = the Session remounts Base; the Suspended window = the Cell suspension window (one shared source value) |
| #13 login-flow | the baseappmgr placement is the Home-baseapp assignment (Base ownership settled once at login) |
| battle-determinism | the Cell tick boundary = the single judgment domain; scene-transfer rebuild does not break determinism (state projection comes from Base's long-term state) |
| attribute-sync | L3 volatile state (HP/position/buff remaining) is not persisted, not carried on scene transfer; the RO_MIRROR read-only mirror (§4.4) unchanged |
| #17 battle-verification | the authoritative side of recomputation reconciliation = the Cell (adjudication is produced at the Cell tick boundary) |
| No. 36 #9/#16 | the seamlessness rejection unchanged; instance results fall back one-way to Base |

## 8. TODO (code batches)

1. The migration surface of the legacy login-app's "preparePlayerOnline direct-calling the base trio" (already registered in login-flow §10) executes per this design's semantics — the Base/Home-baseapp concepts enter code-batch comments and naming;
2. The scene-transfer volatile-state rules (buffs/CDs not carried) land as gameplay SDK conventions (scripting-lua side);
3. The glossary's "player object model" entry has been added (§1 table); the baseapp entry's semantics fixed (2026-10-03).
