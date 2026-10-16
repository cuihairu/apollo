[English](architecture.en.md) | [中文](architecture.md)

# Apollo Architecture Map (architecture)

> Belongs to: companion piece 2 of the phase-one read-only architecture review. Companions: audit.md (repository analysis/build/terminology), object-model.md (object ownership), lifecycle.md (lifecycle), concurrency.md (concurrency), persistence.md (persistence), improvement-plan.md (improvement plan).
> Basis: task book §4 (produce a complete architecture map), §22/§23 (Service boundaries/Manager sprawl), §27 (API usability), §31 (performance analysis), §32 (complexity review). Terminology follows existing code wording (BigWorld/KBE concepts reused first); the differences list is at the end.

---

## 1. The Actual Architecture Today (code is authoritative, not the docs' line)

### 1.1 Process topology: 6 app skeletons vs. the design-target roles

| Process (code) | Directory | Design-target role (task book/contract word) | Code current state |
|---|---|---|---|
| login-app | apps/login-app | account auth, token issuance (short connection) | skeleton complete; under stub transport mode login "always succeeds" (`return true`); sessionSecret unused, ticket = timestamp XOR hash |
| gateway-app | apps/gateway-app | token verification, session carriage, zone_hint-based passthrough | skeleton complete; client ingress unwired (NullClientIngressServer idling); sendToClient a placeholder; resolveRoute's landing port mismatched 9002 vs 9003 |
| base-app | apps/base-app | anchor carriage (PlayerAnchor resident at a Home baseapp) | AnchorManager+SessionLocator in-memory directory usable; activatePlayer/bindSession/unbindSession have handlers; no Home baseapp concept; the DB layer is a fake implementation |
| baseappmgr | apps/baseappmgr | online directory and placement arbitration (the baseappmgr domain, serialized) | assignments_+SessionLocator in-process directory + PLAYER_ASSIGN_WORLD/PLAYER_RESOLVE_ROUTE handlers — **the highest-quality app extant**; displacement pre-arbitration/write-off/event family unimplemented |
| cell-app | apps/cell-app | the baseapp/scene runtime (1..N scenes) | the only app wired to WorldHost; EntityManager+cell::AOIManager+MapInstanceManager+WorldSessionManager; ensureDefaultMapInstance(id=1); **game/world's 5 implementation files uncompiled → guaranteed link failure (when GAME_MODULE=ON)** |
| game-server | apps/game-server | full-pipeline demo container | a 137-line demo: MemoryConnection+SqlTemplate+IdPool+ThreadPool+DI (login pipeline)+ServiceHost; no game loop; link failure (C-95) |

One-line verdict: **6 process skeletons complete, message protocols complete, data models complete — but the default build compiles zero apps; each of the three mainlines has end-to-end breaks** (see §2.2).

### 1.2 Module map (create/hold/call/lifecycle — item by item per task book §4)

**The new tree modules/ (149 files, 31,947 lines)**

| Module | Contents | Creates what | Holds what | Calls what | Notes |
|---|---|---|---|---|---|
| core | DI (ApplicationContext), manifest, config_registry, LogManager | beans per the BeanDefinition graph | type index + singleton table | — | DI is compile-time type-keyed + named; ctor injection for singletons only; topological initialization, reverse-order destruction; eager by default |
| runtime | ApplicationHost/ServiceHost/WorldHost | service containers and the host | services_/shutdown_hooks_/console+signal sources | the various IHostedService/IWorldService | the phase machine Boot→ConfigLoaded→Initialized→Ready→Stop; WorldHost defaults to 20Hz (tick_rate_hz=20) |
| base | base pieces (the boundary with core needs checking per archive) | — | — | — | no local implementation-detail report (see batch material) |
| contract | XML+XSD contract parsing | the contract model | the in-memory model | — | sdks/contract's parsing side; the most complete test family (the contract double gate) |
| data | cache (in-memory cache) + core (connection interface) + orm (MemoryConnection/SqlTemplate) + redis | see persistence.md §2 | see persistence.md §2 | — | **one of the 5 parallel persistence stacks**; zero ability to reach a real medium |
| net | tcp/reactor/rpc/http (EventLoop/RestTemplate)/websocket/protocol (Channel/Endpoint) | see §1.4 network | see §1.4 network | — | modules/net's .cpp files are the old-tree headers' **sole implementers**; the default build compiles all 5 net libraries |
| protocol | (commented out, retired) the nng-family RPC/nng_wrapper/RepSocket | — | — | — | modules/CMakeLists.txt:28-30 commented "has issues"; never adapted to nng 1.11 |
| game | world (scene/aoi/map_instance/world_session/…) + session (anchor/locator) + core (entity) + attributes + battle | see object-model.md | see object-model.md | — | GAME_MODULE=OFF by default; the world library compiles only aoi+scene |
| bigworld | the BW compatibility-layer runtime | — | — | — | tests compile src/runtime.cpp directly: 11 tests run by default |
| starter | two empty INTERFACE shells, core/net | — | — | — | no headers (ls-verified); comprehensive tests reference the nonexistent module_registry.hpp |

**The old tree include/ (107 files, 32,883 lines) + src/ (29 files, 12,394 lines)**

- The header side is the **live API layer**: include/apollo/net/* (Connection/Session/NetworkManager/MessageCodec/packet/websocket/http/adapters) is implemented by modules/net's .cpp; include/apollo/network/* is only modules/net/tcp's private spec layer.
- The implementation side is wholesale "written but unbuilt": the legacy apollo library target's source list references 30+ **nonexistent files** (src/network/*, src/apollo/net/*, src/apollo/bw/runtime.cpp, 9 under src/apollo/actor/*, src/apollo/redis/redis_template.cpp etc.) — once the legacy layout is turned on, configure passes and the build necessarily fails (audit.md §3); it has stayed latent since modular=ON became the default.
- include/apollo/game/*: legacy game headers such as battle/ecs/aoi/attribute/comval — mostly zero consumers or tests only (audit.md §5 table).

### 1.3 Layer stack (the task book §4 Client→Persistence stack, backfilled per actual code)

```text
Client (the GametestClient example, a standalone project, not in the main build)
  ↓ raw TCP (gateway socket bind/listen)
Gateway-app (NullClientIngress idling / sendToClient placeholder)
  ↓ netproto::Channel (the new nng family, stub) / apollo::protocol::RpcClient (the old nng family, module retired)
Login-app / Base-app / Baseappmgr (RepSocket handler tables; base-app port 9002, baseappmgr 9003)
  ↓ message types as text (the disconnect is plain text with no matching handler)
Cell-app (WorldHost 20Hz tick)
  ↓
EntityManager (cell::Entity/PlayerEntity, held as raw pointers) + MapInstanceManager (tick tree inside a lock)
  ↓
MapInstance → WorldSpace → Scene (a three-layer empty shell, zero entities injected in production)
  ↓
Persistence: base-app DatabaseService (in-memory unordered_map, load fakes data, save always calls back success)
```

### 1.4 Network-layer facts (the network agent read all 28 old headers + all implementations, verbatim)

**Key architecture facts**:
1. The migration moved only the implementation files: modules/net's .cpp files are the sole implementers of the old-tree headers (include/apollo/net/*, include/apollo/network/*); the old apollo library target is structurally unbuildable for lack of 30+ source files.
2. Of the 5 net libraries in the current default build, apollo_net_protocol (Channel/Endpoint) is a **pure stub** without an nng environment (connect/send/receive all false); RestTemplate compiles to a pure stub without curl; net/http and net/websocket take the built-in implementations (Drogon unused).
3. Three-way namespace collisions in-system: `MessageHeader` in three flavors (4B host-order packed / 12B big-endian / "BWTR" 24B); `Channel`/`Message`/`ChannelConfig` defined separately and unrelated at net::protocol, ipc, and net; `RpcClient/RpcServer` with at least three implementations (the protobuf-family placeholder shell, the net family's relatively complete but cleanupTimeouts-empty one, the nng family never compiled).
4. The IPC family (include/apollo/ipc + src/apollo/ipc) is an offline side branch: zero code shared with net; APOLLO_ENABLE_IPC=OFF; the classes declared in socket_transport.h have no implementation files at all.

**Verified functional defects (with line numbers)**:
- SessionManager self-deadlock: onDisconnected calls getSession under the lock (getSession locks the same non-recursive mutex again) — session_manager.cpp:273-280.
- TemplateSessionFactory: make_shared paired with bare delete — session_factory.h:50-57.
- native_adapter: the factory member stored by setSessionFactory differs from the member used by handleAccept → the setting never takes effect, and the created NativeSession raw pointers are never held → session leak — native_adapter.cpp:413-415/534-542.
- The nng family: NngSocket::receive leaks messages and misuses the four-arg nng_recv (nng_wrapper.hpp:105-113); RepSocket::stop joins before closing the socket → deadlock (modules/protocol/src/socket.cpp:127-133).
- HttpServer: a detached thread per connection + unbounded vector growth + removeConnection never called + stop hangs — http.cpp:506-549.
- httpGet/httpPost hardcode localhost:80 with path `/` (http.cpp:777-812); the crc32 table initializes only 8 entries (packet.h:488-493); getLocalAddresses returns only 127.0.0.1.
- Fake async: NativeNetworkManager's processEvents is pure poll looping; epollFd_/ioPort_ never used (native_adapter.cpp:778-790 vs 1033-1087).
- sdnet_adapter.h is wholesale unusable: SSCP is a locally forged fake, CreateListener commented out, getConnectionCount returns 0 — the corresponding .cpp does not exist (consistent with the net-abstraction design doc's G-1/G-2 plan direction of "retire the NNG wrapper, delete sdnet_adapter": this piece should go).

### 1.5 The design-vs-code correspondence surface (docs/design ↔ code)

| Design-doc capability | Code landing | State |
|---|---|---|
| Four channels (movement/attributes/events/control) | no implementation of any | unlanded |
| FrameFilter/CryptoFilter | none (the frame layer has only MessageCodec's 12B header) | unlanded |
| InterServerLink | none (inter-process goes over the legacy RepSocket/nng stub) | unlanded |
| session_key/login_token/resume_token | ticket = timestamp XOR hash; sessionKey/resumeToken zero implementation | partial (fake)/unlanded |
| OnlineDirectory (baseappmgr authority + mirror) | baseappmgr's in-process assignments_+SessionLocator | prototype |
| Displacement/keepalive window/Suspended | the WorldSession Suspended state enum; no timers; SessionLocator::bind displacement | enum-level |
| Attribute sync L1-L4/RO_MIRROR/viewer set | neither attribute container has a network pipeline; dirty exists at the API layer only | unlanded |
| write-behind journal (8.2) | none; base-app SaveQueue idles at the application level every 60s | unlanded |
| PCG32 substreams/battle determinism | battle's two empty shells | unlanded |
| Contract schema_hash/descriptor.bin | the sdks contract family complete (XML+XSD+gen+golden check) | **landed and running in CI** |
| Scripting, Lua 5.5 | no scripting module | unlanded |
| Service discovery G-1 (machined two-layer) | none | unlanded |
| Hot standby G-2 (reviver) | none | unlanded |

> **Post-hoc errata (2026-10-10)**: this table is a review-period snapshot; several
> rows have since changed — G-1 is delivered (P3-1 batches B-F + the closing
> batch: machined + the UDP two-layer + supervision + mirroring + the recovery
> phase); **G-2 is not chartered** (ADR-010: plan A goes first, restart-only, no
> reviver); the write-behind journal is delivered (PersistJournal, P2 batch);
> PCG32 substreams/battle determinism is delivered (P2-2). The four
> channels/FrameFilter/InterServerLink remain unlanded (net M1; ADR-011 ruled,
> on the books).

The contract system is the **only design-code-test closed loop**; most other capabilities remain at "the design doc has promised, the code is archived as stub/dead".

---

## 2. End-to-End Data Flows (the task book §4's object relations, per code read)

### 2.1 The login chain (login-flow design §10 already has the "preparePlayerOnline calls base directly" migration registered)

Code current state (login-app + base-app + baseappmgr cross-checked):
1. client → login-app (tcp stub) → `login()` always succeeds in stub mode (login_server.cpp:536-541).
2. base-app's `PLAYER_ACTIVATE` → activatePlayer: DatabaseService (in-memory) loads the faked "player_+id" profile → AnchorManager::activate → bindSession.
3. login-app's assignWorld message hits base-app 9002 → base-app has no handler for it (Unknown message type swallowed by catch-and-accept); the real handler PLAYER_ASSIGN_WORLD_REQUEST is at baseappmgr 9003.
4. gateway resolveRoute sends PlayerResolveRouteRequest to the base-app URL (9002) → none → falls back to buildDefaultRoute.

**Verdict: the real login→placement→scene-entry closed loop cannot run under any configuration**; the in-memory anchor directory is the only in-process link covered by unit tests (and explicitly "no start(), directory methods only").

### 2.2 The three mainlines' break points (consolidated ledger)

| Break | Location | Consequence |
|---|---|---|
| Client inbound | gateway_server.cpp:356-362 ("not wired yet") | no client can connect |
| login→base assignWorld port/semantic mismatch | login→9002 no handler vs baseappmgr 9003 | placement arbitration is forever the default |
| gateway→baseappmgr route | gateway has no --baseappmgr flag | routes always go buildDefaultRoute |
| cell-app game loop | game/world uncompiled + broadcast placeholder + combat hardcodes -10 | the scene/combat chain does not exist |
| Persistence | DatabaseService in-memory fake + SaveQueue always-callbacks-success | a restart loses all data |

### 2.3 Movement→broadcast→combat (task book §13/§16 focus)

- Movement: no player movement input path (no intent upstream, no movement channel).
- Broadcast: cell-app's `broadcastToViewers` is an explicit placeholder (comment: "the real implementation needs to find the player's corresponding Gateway connection"); of the AOI events only LEAVE is dispatched, ENTER/SYNC never filled (aoi.cpp:95-102).
- Combat: cell-server's handleCombatSkillCast hardcodes a damage -10 broadcast, no damage lands; the new BattleSystem has zero production consumers; legacy battle is a dead header.
- Determinism (the battle-determinism four constraints): zero implementation in code (no PCG substreams, no four-tuple replay).

---

## 3. Service Boundaries and Manager Sprawl (task book §22/§23)

### 3.1 Service/Manager/Context inventory, reviewed one by one

| Piece | Manages what | Owns whom | Modifies what | Depends on what | Should it exist |
|---|---|---|---|---|---|
| AnchorManager | player→anchor | Anchor (shared_ptr) | state/bindings | none | keep (carried by base-app, semantics correct) |
| SessionLocator | player↔session | two maps | bindings | none | keep but fold into the directory |
| MapInstanceManager | instance registry | MapInstance | add/remove/find | none | simplify (no state machine) |
| WorldSessionManager | session state machines | WorldSession | seven states | none | rework (see object-model §6) |
| EntityManager (cell) | entity registry | raw pointers | update (inside a lock) | — | rework (in-scene entities belong to Scene) |
| AOIManager (the cell one) | fixed-length grid | entity references | grid migration | — | merge into Scene AOI |
| AOIGrid (legacy) | the 3x3 grid | copies of itself | — | — | merge, keeping one |
| DatabaseService | player profiles | unordered_map | read/write | none (in-memory) | rewrite (a real persistence stack) |
| SaveQueue | save tasks | the task queue | callback(true) | — | rewrite (no flush branch exists) |
| RpcManager/ServiceManager | message dispatch | handler tables | — | — | keep (correct) |
| RpcServiceManager (net/tcp) | a TODO placeholder | — | — | — | delete the placeholder |
| The AttributeManager/ContainerManager singletons | attribute containers | singletons | references inside locks + used outside | — | converge (two sets into one) |
| BattleManager singleton (legacy dead header) | — | — | — | — | delete |
| LocalServiceDiscovery | a local registry | unordered_map | — | — | delete or fold into G-1 |
| RedisDistributedLock | distributed lock (fake) | a void* client | 90% random success | — | delete (zero consumers) |

### 3.2 God Manager/God Context check verdicts

- **No** typical God Manager (every manager has a single responsibility), but there is "Manager never far from Registry" inflation — the production path's construction chain "Manager→Context→Registry→Factory→Dispatcher" is direct-connected rather than chained; **the complexity is not here but in the dual-tree duplication and unwiredness** (audit.md §5, table A1-A9).
- Task book §23's criterion ("whether it truly owns an explicit lifecycle"): MapInstanceManager/WorldSessionManager have lifecycles but no state-machine backing; EntityManager has no lifecycle (raw pointers); DatabaseService's lifecycle is an empty implementation.

---

## 4. API Usability Assessment (task book §27 + §34)

### 4.1 The target API vs. current facts

Target (the §27 recommended shape): `world.create_scene → scene.create_instance → instance.enter(player_id) → player.send(...)`.

Current facts:
1. **No world/scene/instance/player object chain exists**: WorldHost only manages service ticks; Scene has no creator API (ensureDefaultMapInstance hardcodes id=1); a player entering a scene must pierce cell-server's private attachPlayerWorldSession (which internally assigns two sets of ids, map+space).
2. Layers traversed by a simple operation, measured: `CellServer::handleCellCrossBorder → world_session_manager → WorldSession → (looked up by entity_id as player_id)` — the mere "suspend + transfer + complete" grouping is 3 layers of bookkeeping + namespace confusion.
3. Examples today: 16 demos (ECS/RPC/session/game_server/benchmark/serializer etc.) all gated off, never run by CI; benchmark.cpp does not even link apollo; all_features_demo/ioc_example referenced by README/examples-README are deleted (tombstones).
4. The minimal runnable example (task book §29: login→lobby→create instance→enter→spawn→AOI→battle→reward→leave→lobby) **cannot complete under any configuration of the current code** (the five breaks of §2.2).

### 4.2 The app configuration/startup surface

- No unified config file; each app carries hand-written config.hpp fields (~20 dead fields: ports/timeouts/switches written but read by no one); login's sessionSecret unused; reconnectWindowMs zero references; chatAppUrl 9003 collides with baseappmgr's default 9003.

---

## 5. Complexity Review (task book §32: code size/dependencies/interfaces/threads/locks/lifecycles)

### 5.1 Core-module complexity table

| Piece | Code size | External deps | Public interface | Threads | Locks | Lifecycle complexity | Does the complexity solve the problem |
|---|---|---|---|---|---|---|---|
| modules/game/world | 639+231 | none | ≈56 + 7 states | 0 (tick-driven) | one mutex per mgr | seven-state machine but Leaving unobservable, transfer made synchronous | **No**: three-layer empty shell + uncompiled |
| modules/game/session | 302 | none | ≈33 | 0 | 1 | the anchor's six states correct | partly (semantics right, persistence hook missing) |
| modules/game/core+attributes+battle | 741 | none | ≈35+ | 0 | attribute-container locks | the double-attach bug (on_attach called twice) | No: one of the five ECS sets |
| include/apollo/game legacy, 9 headers | 3,054 | phantom headers | ≈300+ (incl. 290 constants) | 0 | 0 | — | **No**: ~1/3 purely dead |
| modules/net + old headers | ~9,600 | nng (stub)/curl (stub)/Drogon (unused) | ≈340 | many (a detached thread per connection, send/recv threads) | self-deadlock risk | HttpServer connections never reclaimed | partly distorted: three sets coexisting |
| modules/protocol (retired) | uncompiled | nng | — | worker threads | — | RepSocket::stop deadlock | No |
| modules/core DI + runtime | 1,500 | none | ≈80 | 0 (outside the host) | 0 | correct (topological/reverse-order) | **Yes** (highest quality) |
| src/legacy persistence, 9 files | 0 compiled | mysql/redis++ (off) | ≈90 | pool threads | pool locks (correct) | CreateConnection empty | No |
| apps (6) | 4,822 | nng (stub) + game (broken) | message tables ≈35 | 1 main thread + 1 save thread per app | directory locks | each has a lifecycle but no state machine | partly: skeletons right, wiring broken |

### 5.2 Premature complexity inventory (the other half of task book §14/§15/§19)

- **Two AOI sets** (the legacy 3x3 grid + the cell fixed-length grid), neither wired to Scene — all waste.
- **The whole IPC family** (40 pure-virtual Channel/BackpressureManager/ReconnectConfig/service discovery) default OFF, zero consumers, no implementation — a textbook case of "pre-embedded complexity".
- **The distributed lock** (RedisDistributedLock, 90% random success) has zero consumers.
- **The queue family**: an inherited hand-written pthread version/ThreadPool/SPSC/lock-free queues/memory pools — several sets coexisting (tests cover them, production consumes none).
- **Cut-or-keep verdicts**: EntityId wrapper (keep), DI (keep), WorldHost phase machine (keep), the contract family (keep); the 3x3 grid vs. the cell grid (merge into one), IPC (archive), the three protocol sets (converge to one), the two attribute sets (merge into one).

---

## 6. Performance Analysis (task book §31, based on the real models)

> Premise declared: the current code **cannot run any game load** (three broken mainlines + cell-app link failure + placeholder broadcast); the following are projections for "after the existing structure is made to run" + future benchmark design; they do not constitute measured data.

### 6.1 Model A: 1 Scene / 1000 Players (the big scene)

- Structure: 1 MapInstance→WorldSpace→Scene + cell::EntityManager (1000 entities) + AOI (fixed-length grid).
- Tick: WorldHost at 20Hz, single-threaded; EntityManager::update **traverses 1000 entities holding the lock** (calling on_update inside it).
- Broadcast: AOI interest management with O(1) grid lookup + neighborhood (the current broadcast is a placeholder; assume N viewers each get 1 message) → theoretically ~1000 messages/tick outbound; at 64B per message ≈ 1.28MB/s/tick direction → the gateway connection face is the bottleneck (and the gateway itself is unwired).
- Verdict direction: a single scene with a thousand players is feasible under the **current single-threaded model** but with no headroom — entity updates should be de-locked (scene-thread exclusive), and the AOI grid should guard against `GetOrCreateCell` side-effect creation (aoi.cpp:20,35: range queries write the grid).

### 6.2 Model B: 10 Scenes / 100 Players

- Structure: cell-app single process, 10 instances (MapInstanceManager holding a 10-fork tree) — **currently no multi-scene path** (ensureDefaultMapInstance hardcodes 1; MapInstanceManager::create_instance exists but has no production caller).
- A 100-entity-per-scene tick is 1/10 of model A's load; the lock granularity is still global (one MapInstanceManager lock covers all instances) → after 10×100 the cross-instance lock contention face is still one lock.
- Feasibility verdict: single-process multi-scene is structurally sound, but **the lock granularity must drop from "manager-level" to "scene-level"** (concurrency.md §4).

### 6.3 Model C: 100 Instances / 10 Players (the typical instanced load)

- Structure: 100 MapInstances (10 players each) → 1,000 entities in one process.
- Total tick budget: 1,000 entities × 20Hz; if the whole tick tree is on one thread, at ≤ 50µs per entity update 1,000 entities = 50ms → **exceeds the 20Hz frame budget (50ms)**; entity sharding/scene parallelism or lower per-instance update cost is needed; if sharded per instance across threads, the 100 instances' lock contention and migration boundaries appear immediately — there is no sharding mechanism today.
- The instance lifecycle (create/wait/run/settle/destroy, 5 states) does not exist today → the instanced load's **scale-up/down and reclamation** path is empty.
- Memory: 100×10 entities at a few hundred B each is no pressure; the problem is that the broadcast cost runs away while AOI has no per-session viewer state (the ViewerState design is unimplemented).

### 6.4 Frame budget and benchmark design (written into improvement-plan P3)

- The target frame budget (the capacity-and-benchmark doc is chartered: 5000 CCU, a frame-budget table); on the code side benchmark.cpp is not linked against apollo (examples/CMakeLists.txt:17-18), and the benchmark form (unit benchmarks/stress tools/long-run) is all zero; the Redis hot-data cross-process channel is zero implementation.
- Constraint reminder: performance targets should serve "many scenes + many instances", not "one giant seamless world" (task book §31, original text).

---

## 7. Task Book §4's Questions Answered One by One (current code facts)

1. **What objects are created**: see the §1.3 layer diagram and object-model.md; the production path actually creates only "the anchor directory, session records, the instance registry, the entity table"; game objects (Avatar/NPC/Monster) have no construction path.
2. **What is held**: managers hold maps; MapInstance holds WorldSpace by value holding Scene by value; the cell EntityManager holds raw pointers.
3. **What is called**: the host→service→manager→entity chain (see lifecycle.md's call-chain diagram).
4. **Lifecycle**: app-level has the Host phase machine; at the business-object level only WorldSession's seven states and the Anchor's six; Scene/Instance/Entity have no state machines.
5. **Who owns the objects**: see object-model.md's table; current state = "the managers own everything".
6. **Who modifies the objects**: the holders; cross-thread modification facts in concurrency.md (EntityManager's update inside a lock, AttributeContainer's outside-lock references).
7. **Who destroys the objects**: container erase/destructor; the Avatar cleanup path is zero implementation.
8. **Cross-thread**: app main thread + save thread + network threads mixed (concurrency.md).
9. **Cross-process**: yes (by design), currently zero (the only cross-process path is a stub).
10. **Persistent**: no (in-memory authority and nothing flushed).
11. **Depends on other modules**: the dependency graph in §8.

---

## 8. Dependency Map

```text
apps/game-server ──> apollo::game_core ──> modules/runtime ──> modules/core
apps/cell-app ──> apollo::game_world + game_core + apollo_protocol(retired) ──> runtime ──> core
apps/base-app, baseappmgr ──> apollo::game_session + apollo_protocol(retired) ──> runtime ──> core
apps/gateway-app ──> apollo::net_protocol + apollo_protocol(retired) (double gate) ──> net ──> core
apps/login-app ──> apollo_protocol(retired) ──> net ──> core

modules/net (tcp/http/websocket/rpc/protocol) ──> old-tree headers (include/apollo/net, include/apollo/network)
modules/game ──> legacy headers (apollo/game/aoi/aoi.hpp etc.) + runtime
modules/data ──> (itself) + (duplicates copied from legacy)
tests/net_tests ──> the apollo aggregate (INTERFACE: the five net libs + base)
the legacy apollo library ──> references 30+ missing source files → structurally unbuildable (only in the NOT-modular layout)
sdks/gen ──> pugixml (XML parsing, vcpkg) ──> generates C++/JSON/proto/lua/contracts
```

Brace note: no dependency cycles exist (core has no reverse dependencies), **but the module-directory dual tree creates an implicit "one header, two shelvings" dependency** (e.g. memory_connection.cpp exists twice, in data/orm and src/apollo/database); four namespaces coexist — apollo::storage::database / apollo::database / apollo::db / apollo::data::{core,orm} (audit.md's A7 table).

---

## 9. Terminology Differences (architecture level)

1. **baseapp has no independent code identifier**: the design line says baseapp = the logic-server process (carrying 1..N scenes); the only approximation in code is apps/cell-app (a single-scene process prototype); the names do not clash but the responsibility falls short (the cell-app name is a BW root; the original line forbids "cellapp" — see audit.md §2.3 item 4; rename at migration).
2. **Home baseapp / PlayerAnchor semantics**: PlayerAnchor has a class but no Home baseapp placement (anchors live in base-app's in-process map, with no field/mechanism corresponding to "assign a Home baseapp at login, never migrate"); WorldAssignment's four ids (world/map/instance/space) + route_version are the field-level prototype of the "scene transfer" mechanism, but cell-app uses map_instance_id as space_id (cell_server.cpp:544-545) — the two id namespaces are crossed and confused, and the original line's "scene is the only spatial unit" clashes with the code's three-layer ids (detailed table in object-model.md §4).
3. **Avatar has no class**: the nearest carriers are cell::PlayerEntity (cell_manager.hpp:42-93) + world_session's avatar EntityId field; the original line's inheritance chain (entities.xml Monster→NPC→Avatar→Player) has no code counterpart (no Monster/NPC classes).
4. **instance has no state machine**: the original line's instance = the instanced gameplay lifecycle (Create→Initialize→Waiting→Running→Finishing→Rewarding→Draining→Destroyed; task book §9 recommends unifying); the code's MapInstance has five stateless operations (object-model.md §5).
5. **Suspended has no window**: the original line's Suspended = the disconnect keepalive window; the code has only the enum value (detailed in lifecycle.md §3).
6. **OnlineDirectory has no code**: the original line's baseappmgr authority + mirror; the code has only baseappmgr's in-process table (audit.md §2.2 row 8).
7. **The network narrative surface**: the gap between the net-abstraction design (L0-L3/four channels/InterServerLink/FrameFilter) and the code (three MessageHeader sets, three RPC sets, two Session specs) is exactly §1.4; the original line's RemoteEntityCall/InterServerLink must appear after the convergence — currently **no code to align against** (registered, no forced naming).
8. **"换幕 (scene transfer)" vs. the code's transfer**: world_session's begin_transfer/complete_transfer field hauling is the instant-synchronous version of "scene transfer" — the right direction (no cross-process tax) but without state projection (Base long-term state → the new Cell) or the volatile-state discard rule (contract §1.2: L3 is not archived); see lifecycle.md §4.
9. **battle's usable semantics**: the original line's "battle = the dedicated battle-verification domain"; the code's two battle empty shells are nominally "the battle system" — not in conflict with the original line's semantics (battle = the combat-logic domain, verification/settlement) but both are empty shells; registered.

---

(end)
