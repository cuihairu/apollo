[English](object-model.en.md) | [中文](object-model.md)

# Apollo Object Ownership Model (object-model)

> Belongs to: companion piece 3 of the phase-one read-only architecture review. Companions: audit.md (repository level), architecture.md (architecture map/dependencies), lifecycle.md (lifecycle), concurrency.md (concurrency), persistence.md (persistence), improvement-plan.md (improvement plan).
> Basis: task book §5 (object ownership is this review's focus), §6 (Player as a first-class object), §12 (Player/Connection/Session not confusedly bound), §14 (Entity model), §15 (ECS review). Terminology follows existing code wording (BigWorld/KBE concepts reused first); the differences list is at the end.

---

## 1. Current State: the Actual Distribution of Player-Side Objects (code is authoritative)

The code has four classes of responsibility carrier related to "player", but **not one class named Account/Avatar**:

| Code piece | File | Responsibility | State |
|---|---|---|---|
| PlayerAnchor | modules/game/session/player_anchor.hpp (held by anchor_manager.hpp) | long-term ownership: player_id + six states + SessionBinding + WorldAssignment + dirty flag | the only player long-term object used in production by base-app; no persistence hook |
| SessionLocator | modules/game/session/session_locator.hpp | the player↔session bidirectional index (bind displaces the old entry) | used by baseappmgr and base-app |
| WorldSession | modules/game/world/world_session.hpp | seven-state bookkeeping state machine (Entering…Closed) + avatar EntityId + four position fields | cell-app only; zero coupling with the network |
| cell::PlayerEntity | apps/cell-app/include/cell/cell_manager.hpp:42-93 | the only approximation of "an in-scene player"; update empty, onEnterView/onLeaveView empty | cell-app internal only |

Two further facts of namespace confusion: `WorldSession`'s player key is looked up using **EntityId** as **PlayerId** on the validation path (cell_server.cpp:559 `find_by_player(entity_id)`); cell-app assigns map_instance_id directly as space_id (cell_server.cpp:544-545).

**Comparison (the long-term-state / scene-avatar two layers)**: Session/Proxy (connection carriage, gateway side) → **no Proxy class**, no session object on the gateway side (only connection sockets); PlayerAnchor/Base (long-term ownership, resident at a Home baseapp) → class exists, no Home baseapp concept (the base-app process exists but has no placement field); Avatar/Cell (the scene avatar, born entering a scene, dying leaving it) → **no class**, and the nearest thing, cell::PlayerEntity, has no lifecycle rules.

---

## 2. Task Book §5 Ownership Matrix: per-object current state

> Template fields: Owner / Created / Destroyed / Runtime Authority / Persistence / Current Scene / Connection / Reconnect / Scene Transfer. Each cell holds "current state" and "target (contract/design line)"; "?" = currently unanswerable (task book §5: must be listed as an architecture problem).

### 2.1 World

| Dimension | Current state | Target |
|---|---|---|
| Owner | no World object (WorldHost is a service host, it does not do the world's job) | world = the world container inside a baseapp (create_scene/scene management) |
| Created/Destroyed | none | starts and stops with the baseapp process |
| Runtime Authority | none | the single authority over the scene set within a baseapp |
| Persistence | — | none (world configuration belongs to the Definition layer) |
| Verdict | **World is not a first-class object**: the first segment of task book §27's target API, `world.create_scene`, cannot land in code | — |

### 2.2 PlayerAnchor (player anchor ≡ Base; contract: resident at a Home baseapp)

| Dimension | Current state | Target (contract) |
|---|---|---|
| Owner | AnchorManager (inside base-app, mutex+map) | held by the Home baseapp (single in-process authority) — semantics agree, the "Home baseapp assignment" mechanism is missing |
| Created | base-app activatePlayer (make_shared after DB load) | at the login-landing stage (when login-flow assigns the Home baseapp) |
| Destroyed | AnchorManager::deactivate (erase) | after logout archiving completes |
| Runtime Authority | **yes** (the in-memory map is the single truth; DB is a fake implementation) | yes (design decision: online player memory is authoritative) |
| Persistence | no hook; the dirty flag exists but only tests consume needs_save; the save loop is base-app's SaveQueue (fake) | dirty→write-behind journal (attribute-sync §8.2) |
| Current Scene | WorldAssignment (world/map/instance/space + route_version) — **fields correct** | same + suspended-state semantics |
| Connection | SessionBinding (session_id/gateway_id/gateway_addr/bind_time_ms) | same |
| Reconnect | no window mechanism (Suspended is only an enum of another object, WorldSession) | the disconnect keepalive window (Suspended state) shares TTL origin with resume_token |
| Scene Transfer | none (the anchor takes no part in scene transfer — correct! but no code anywhere expresses "scene transfer moves only the Cell") | the anchor stays, the Cell transfers |
| Verdict | **the direction of the player long-term object is right** (memory authority + binding + position fields all present), but three things are missing: the persistence loop, Home baseapp placement, the keepalive window | — |

### 2.3 Avatar (scene avatar ≡ Cell) — does not exist

| Dimension | Current state | Target (contract) |
|---|---|---|
| Owner | none (cell::PlayerEntity is held by cell::EntityManager as a raw pointer) | the current scene (the scene births it, the scene kills it) |
| Created | no construction path | on entering a scene (state projected from PlayerAnchor) |
| Destroyed | none | on leaving the scene / the disconnect window expiring (removal) |
| Runtime Authority | — | authority for movement/combat/AOI broadcast within the scene thread |
| Persistence | — | L3 volatile state not archived, not carried across scene transfers |
| Current Scene | — | lives with the scene |
| Reconnect | — | rebuilt per scene rules (in place / at the revival point / in the main city, as gameplay decides) |
| Scene Transfer | — | "scene transfer" = destroy and rebuild, **no object relocation** (decision #9 rejects seamlessness) |
| Verdict | **the landing point of task book §6's "Runtime Player" and the contract's "Cell"** — the biggest hole in the current model: no class, no construction, no lifecycle | — |

### 2.4 Entity (the general scene entity)

| Dimension | Current state | Target |
|---|---|---|
| Owner | four~five coexisting (audit.md A1): cell::EntityManager (raw-pointer map) / legacy ecs (unique_ptr + raw-pointer back-references + dangling risk) / legacy battle::ecs / modules/game/core Entity / legacy ecs.h | owned by the Scene (scene.entities_) |
| Created | no production construction path (each Manager's add is test/framework-internal only) | in-scene spawn |
| Runtime Authority | no tick phases (EntityManager::update traverses under lock) | scene-tick single writer |
| Type family | no NPC/Monster/Projectile/Drop/Building/Tower classes; the entities.xml inheritance chain (Monster→NPC→Avatar→Player) has no code counterpart | the contract entity family (entities.xml semantics, incl. Avatar; the Player name is deprecated) |
| Verdict | task book §14's "unified Entity management" challenge holds: **global Managers own the entities while the Scene does not** | — |

### 2.5 Scene (contract: a logical unit with an owner within the world)

| Dimension | Current state | Target |
|---|---|---|
| Owner | no independent ownership: Scene is a value member of MapInstance (the MapInstance→WorldSpace→Scene three-layer empty shell) | belongs to the world container (inside a baseapp) |
| Created | only ensureDefaultMapInstance hardcoding id=1 "default-world" | world.create_scene(...) |
| Runtime Authority | Scene::update traverses entities_ (forever empty) | scene tick phases (simulate/recalc/aoi/collect/persist) |
| Owns AOI | **no** (Scene has no AOI member) | yes (scene_id isolation) |
| Owns player enter/exit | **no** (no enter/leave API) | yes (instance.enter/leave(player)) |
| Independent run/stop/resume | no (no state machine) | yes (lifecycle states) |
| Verdict | every challenge of task book §8 hits: **Scene is a data structure only; the runtime logic is scattered across cell-server/EntityManager/AOIManager** | — |

### 2.6 Instance (contract: a scene instance opened by one run of gameplay)

| Dimension | Current state | Target (task book §9) |
|---|---|---|
| Owner | MapInstanceManager (mutex+map, shared_ptr registry) | scene-derived: create_instance |
| Created | MapInstanceManager::create_instance (the only call site = ensureDefaultMapInstance's hardcoded path) | created on gameplay admission |
| Lifecycle | **no state machine**: five operations (create/find/destroy/update/count), destroy = erase from the map | the Create→Initialize→Waiting→Running→Finishing→Rewarding→Draining→Destroyed eight segments |
| Runtime Authority | MapInstanceManager::update runs the inner tick tree under lock (map_instance.cpp not compiled) | independent scene tick |
| Verdict | task book §9/§10 (Instance a first-class object, Scene≠Instance≠Map) both hit: **the code is a literal three-layer stack of "one Scene per instance" + no lifecycle; Map (static assets) has no independent concept in code (only the map_name string)** | — |

### 2.7 Session / Connection (task book §12 focus)

| Dimension | Current state | Target |
|---|---|---|
| Relationship | **Connection = Session conflated**: the gateway has no session object (a raw TCP connection is everything); the two Session domains (per-app) are zero-coupled with the game layer | the strict Connection→Session→PlayerAnchor layering |
| Created | exists from connection accept | created on auth success |
| Disconnect | none (the disconnect goes to the backend as plain text, no handler; no reconnect code) | the Session can be lost and re-attached (resume token) |
| Displacement (same-account relogin) | SessionLocator::bind displaces the old entry (an in-process prototype) | displacement pre-arbitration in the baseappmgr domain |
| Verdict | task book §12 hits: the current state is not "Player strongly bound to Connection" — no Player exists, but the binding direction is wrong (the connection is everything) | — |

### 2.8 AOI

| Dimension | Current state | Target |
|---|---|---|
| Owner | two sets (legacy AOIGrid is test-only; cell::AOIManager belongs to cell-server) — **neither belongs to Scene** | belongs to the scene (scene_id isolation) |
| Runtime Authority | cell::AOIManager grid migration; events dispatch LEAVE only; ENTER/SYNC unfilled; ViewerState does not exist | viewer set + delta broadcast (four channels) |
| Verdict | task book §13 hits: AOI never became the Scene's interest management, and two sets coexist | — |

### 2.9 Guild / Party (task book §5 checklist pieces)

- **Do not exist in code** (repo-wide grep finds no guild/party class); the entities.xml Player entity's note "includes the social face" is only a note. The design side's player-object-model puts social with Base long-term state. → listed under P2 (social domain); no object to audit at present.

---

## 3. Target Ownership Model (the improvement plan's anchor)

> Contract §1.2 + player-object-model §4's three-axis close-out + the intersection of task book §5/§6/§10; this table is improvement-plan P0's acceptance skeleton.

```text
Process axis:  gateway-app (connection plane) | baseapp/logic-server process (1..N scenes;
               the Home baseapp hosts the PlayerAnchor)
Spatial axis:  world ──1..N──> scene ──1..N──> instance (= a gameplay-lifecycle alias of scene)
               the scene owns: entities / AOI / tick phases / player enter-exit
               the instance owns: the gameplay eight-state lifecycle + the scene (or a reference)
Object axis:   Connection (gateway) → Session (losable, re-attachable) → PlayerAnchor
               (Home baseapp, immovable)
               │                                                            │
               └──────── scene transfer: the old scene destroys the Avatar, the new scene
                 rebuilds it (state projected from the Anchor, volatile state reset)
```

| Object | Owner | Runtime Authority | Thread | Persistence | Visibility | Transfer | Recovery |
|---|---|---|---|---|---|---|---|
| Account | the account store (DB side) | fetched at login | — | yes (account creation/auth) | — | — | auth retry |
| PlayerAnchor | the Home baseapp | single-threaded within the baseapp (the home thread) | single writer | online-memory authority + write-behind | none (not broadcast) | never migrates | process restart → reload and rebuild from DB |
| Avatar | the current scene | the scene thread | scene-thread single writer | L3 volatile state not archived | broadcast within scene AOI | scene transfer destroys and rebuilds | rebuilt within the disconnect window |
| Session | the gateway (logically hung off the Anchor) | the gateway thread | the connection thread | none | — | re-attached on resume | resume_token |
| Scene | the world (baseapp) | the scene thread | single writer | runtime-state persistence = gameplay results landing one-way on the Anchor | AOI boundary | — | crash reopens the instance |
| Instance | scene-derived | the scene thread | same | results land on the Anchor | — | — | abort/restore |
| Entity | the scene | the scene thread | same | as gameplay requires | AOI | in-scene only | rebuilt in-scene |
| AOI | the scene | the scene thread | same | none | is itself the interest-dispatch table | — | rebuilt |

---

## 4. Ownership Problem List (current state → problem number)

| # | Problem | Evidence | Level |
|---|---|---|---|
| O-1 | Avatar has no class, no construction path, no lifecycle | §2.3 | P0 |
| O-2 | The Scene does not own entities/AOI/player enter-exit — the runtime logic lives in cell-server private components | §2.5 | P0 |
| O-3 | Instance has no gameplay lifecycle state machine; Map=Scene=Instance three concepts confused (the MapInstance→WorldSpace→Scene three-layer empty shell, no behavioral difference between layers) | map_instance.hpp/world_space.hpp; audit.md §5 | P0 |
| O-4 | PlayerId/EntityId/space three-namespace confusion | cell_server.cpp:544-545,559 | P0 |
| O-5 | Global Managers own the entities (EntityManager/MapInstanceManager), opposite to task book §14's "Entity→Scene" target | cell_server.cpp:40 | P1 |
| O-6 | The Connection/Session/Player binding inverted: no Session layer, loss on disconnect | gateway_server.cpp | P1 |
| O-7 | The keepalive window/recovery has no object carrying it (the Suspended enum has no window) | world_session.cpp:74-101 | P1 |
| O-8 | guild/party/social have no objects (design yes, code no) | repo-wide grep | P2 |
| O-9 | Five Entity/ECS sets, mutually incompatible | audit.md A1 | P2 |
| O-10 | Two AOI sets with no ownership | audit.md A2 | P1 |
| O-11 | Two attribute sets with no ownership, no sync pipeline | audit.md A4 | P2 |

---

## 5. Terminology Differences (object-model level)

1. **The landing point of "Player"**: task book §6 requires Player to have first-class object status (Identity/Attributes/Inventory/Equipment/Quest/Progress/Social/Guild/Runtime State); per the original line, the rework splits it into **carriers = PlayerAnchor (persistent state) + Avatar (runtime state)**; Inventory/Equipment/Quest and the like **do not exist at all** in code (base-app's PlayerData has only the 9 fields playerId/username/level/exp/hp/maxHp/mp/maxMp/x/y/z, database_service.hpp:22-39) — difference registered: the task book's functional faces (inventory/equipment/quests) belong, under the original line's semantics, to Anchor persistent state; the field model must be supplied at the P0 Player-model rebuild — not a code-naming issue.
2. **The entities.xml four-layer inheritance chain (Monster→NPC→Avatar→Player) conflicts with the original line**: the original line ruled Player deprecated and Avatar the only player spatial object; entities.xml:8 `<entity id="Player" parent="Avatar"/>` must be reworked per the original line — registered this phase; the concrete revision (delete Player or fold into Avatar; whether NPC inherits the monster family) goes into the P2 original-line revision batch (must run the "glossary first, then contract" new-term process).
3. **The WorldSession name**: the original line's Session = connection carriage (gateway side); the code's WorldSession is "scene-session bookkeeping" — the responsibility differs from the original line's Session and the names clash; renamed at the P0 session-model rebuild (suggestion: `SceneSession`, or fold into the WorldAssignment record; see improvement-plan P0-3).
4. **WorldSpace's name contains "Space"** (the original line forbids bare use of Space): renamed as the three-layer empty shell is dissolved.
5. **MapInstance = the code name for Instance**: retained (the original line's `instance` sense), but the semantics must rise from "registry entry" to "eight-state gameplay lifecycle" (O-3).
6. **cell::PlayerEntity** (PascalCase cell prefix) is a temporary name, deleted when Avatar lands; the cell prefix does not go into the original line.

---

(end)
