[English](audit.en.md) | [中文](audit.md)

# Apollo Architecture Audit Report (audit)

> Belongs to: phase one (read-only architecture review; this phase modified no source code/build scripts/config files).
> Basis: docs/rearchitecture/architecture-review-plan.md (the task book, commit ce91a739, 40 sections, hereinafter "the task book").
> Terminology: existing code wording; naming reuses existing BigWorld/KBE concepts first.
> Scope: README, docs/, examples/, tests/, src/, include/, sdks/, modules/, apps/, the build system and CI.
> Method: all code and documents read in full; the pre-existing review artifacts (repo-root BIGWORLD_AUDIT.md, ipc_audit_results.md) served only as cross-validation references, their conclusions not adopted by default; this report's data reflects the 2026-10-04 workspace.
> Companion files: architecture.md (architecture map) / object-model.md (object ownership) / lifecycle.md (lifecycle map) / concurrency.md (concurrency model) / persistence.md (persistence) / improvement-plan.md (improvement plan). This document is the overview and repository-level evidence.

---

## 1. Repository Overview and Analysis (Repository Analysis)

### 1.1 Positioning Baseline

Apollo's repositioning (task book §1): **a server framework for lightweight MMOs, scene-based multiplayer online games, and instanced multiplayer games** — persistent online players, Scene, Instance, AOI, combat, player state, scene switching, social, guilds, instances, arenas, tower defense, idle MMOs, lightweight MMORPGs, data persistence, disconnect reconnect, service recovery; it explicitly **does not pursue a seamless large world** (design decision #9, docs/analysis #36).

The two fact baselines on the code side:

- **The design side** has converged: the 12 design documents under docs/design are chartered; design-gap-inventory 15/17 items CLOSED (only gap #15 Bots and #16 map pipeline remain OPEN).
- **The code side** is still a dual-structure tree of "legacy silos coexisting with new modules" (§5), and the default build compiles only the base libraries — **all 6 apps compile nothing by default** (§3.3), with numerous "design-declared, code-unlanded" gaps (see §7's problem list and the companion documents).

### 1.2 Code Size Statistics (measured 2026-10-04, wc -l)

| Tree | Files | Lines | Notes |
|---|---|---|---|
| modules/ | 149 | 31,947 | the new kernel (base/contract/core/runtime/data/net/protocol/game/bigworld/starter) |
| include/ | 107 | 32,883 | legacy public headers (net/net-message/thread/battle/ecs/aoi/attribute/comval…) |
| src/ | 29 | 12,394 | legacy implementations (storage/db_mysql/db_mock/memory_connection/redis/network/net-message/thread…) |
| tests/ | 41 | 16,422 | legacy tests |
| examples/ | 24 | 9,670 | examples (most depend on legacy APIs) |
| apps/ | 33 | 4,822 | 6 app skeletons (gateway/login/base/baseappmgr/cell/game-server) |
| sdks/ | 10 | 3,947 | contracts (contract XML+apollo.xsd), generator gen/src, generated C++/JSON/proto/lua |
| skds/ | 18 | 3,523 | the Unity C# SDK board (directory-name spelling is a historical leftover) |
| **Total** | **411** | **~115,600** | excluding docs/ (3,097 files / 310,325 lines, incl. design docs, analysis records, and the VitePress site) |

Of which legacy (include/+src/) is ~45k lines, new modules (modules/+apps/) ~37k lines, tests 16k lines, examples 9.7k lines. **The scale of "the same capability existing as 2~3 parallel implementations in the repo" due to the dual-tree coexistence is this audit's most important structural finding** (§5, §6).

### 1.3 Build Topology and Gating

- CMake + Ninja + vcpkg; `APOLLO_ENABLE_MODULAR_LAYOUT=ON` (default) enables the modules/ new tree; legacy src/+include/ participates evolutionarily when `APOLLO_BUILD_LEGACY=ON`.
- **Module gating**:
  - `APOLLO_BUILD_GAME_MODULE=OFF` by default — the libraries under modules/game (incl. apollo::game_world) do not compile in the default build.
  - `modules/protocol` (apollo_protocol) disabled by default — the CMake comment concedes "has issues"; that target carries `APOLLO_USE_NNG=1` and has never compiled against nng 1.11 (breakage registered in the repo).
  - `modules/data`'s and `modules/net`'s submodules degrade per find_package results (Trantor/redis++ optional, built-in stubs/degraded implementations on a miss; "using built-in implementation" in the apollo_runtime CMake).
- **The app gating chain** (apps/CMakeLists.txt): all 6 apps depend on `apollo_protocol` (and login/base/baseappmgr additionally require apollo::game_session under `APOLLO_BUILD_GAME_MODULE=ON`; cell-app requires apollo::game_core+apollo::game_world). **The default build (GAME_MODULE=OFF, protocol disabled) → zero apps compiled**; all fall into the else branch printing disabled.
- Registered breakages (repo architecture-review.md defect register C-95 etc.): the examples link breaks; modules/protocol was never adapted to nng 1.11; game-server has undefined symbols under GAME_MODULE=ON (LogManager::clear/set_console_enabled/snapshot, SqlTemplate constructor/query).
- **Latent link breaks newly confirmed by this audit (the six-source uncompiled family)**:
  1. `modules/game/world/CMakeLists.txt` compiles only `aoi.cpp; scene.cpp` into `apollo_game_world`, while `map_instance.cpp / map_instance_manager.cpp / world_space.cpp / world_session.cpp / world_session_manager.cpp` — five implementation files — are in no build target (double-checked via compile_commands.json and build.ninja); yet `apps/cell-app` calls their API unconditionally (cell_server.cpp:523-563 ensureDefaultMapInstance/assign_map_instance/transfer_session etc.).
  2. `modules/runtime/src/world_host.cpp` (109 lines) is not in runtime/CMakeLists.txt (only application_host.cpp + runtime_manifest.cpp are compiled) — cell-app also depends on WorldHost (cell_server.cpp:281).
  → Verdict: **with APOLLO_BUILD_GAME_MODULE=ON, cell-app necessarily fails to link with undefined references to 6 missing sources** (it is latently OFF by default today). Meanwhile legacy `src/apollo/storage/database/db_mysql.cpp`, `redis.cpp`, `src/apollo/config/table_loader.cpp` and 9 other files compile 0 times in the 88-file real build list (compile_commands) — the legacy persistence stack has a complete "written-but-unbuilt" layer; the root CMakeLists' legacy branch also references 30+ **nonexistent source files** (src/network/*, src/apollo/net/*, src/apollo/bw/runtime.cpp, src/apollo/core/log/*, src/apollo/redis/redis_template.cpp etc.); once the legacy layout is turned on, configure passes but the build necessarily fails.
- **ODR violation (base-framework layer)**: `apollo::core::log::LogManager` is **defined twice under the same name** — in the old header log_manager.hpp (39 lines, four methods declared only) and the new header log_manager.h (106 lines); and `global_log_manager()` is double-declared with only one implementation — two class definitions in one link graph = ODR UB (application_host.cpp hits the old header by -I order and links the new header's implementation; write() semantics split); the two log_manager.h versions have drifted 291 lines vs 106 lines (diff confirms they are not the same file).

### 1.4 Repository Health and Delivery Discipline

- Untracked workspace files: BIGWORLD_AUDIT.md, ipc_audit_results.md, Testing/Testing/ (the first two are pre-existing review material, excluded from this deliverable per instructions).
- Git history: the main branch shows a single-author commit style dominated by "docs:" and "refactor(apps):"; the recent architecture-review history spans 25 rounds with defects C-1..C-95 registered (architecture-review.md §31-§34, 3,150 lines).

---

## 2. Terminology Cross-reference and Differences

> Rule (coordinator constraint + the task book's front matter): this document set uses existing code wording throughout; where they differ, the body is reworked per the code read, and a "differences list" is appended at **the end of each document**, item by item.

### 2.1 The Aftermath of the "Player" Ruling (an important preface)

The original-line v1.0 user ruling (docs/design/player-object-model.md and the analysis records): **the player object = the Account/Session/Avatar triple, "Player is deprecated"** — the player entity has only the word Avatar, and the Player/Avatar split is regarded as re-invented layering and abolished; Avatar = the scene avatar/Cell, resident in a scene with the scene's life and death; PlayerAnchor = Base, resident at the Home Zone (assigned once at login).

Yet the task book's §2/§6/§39 use "Player must become a first-class object" throughout as the acceptance mainline (Account→Player→Runtime Player; Player owns Identity/Attributes/Inventory/Equipment/Quest/Progress/Social/Guild/Runtime State). The two **do not conflict but need rework to align**: this document set lands the task book's "Player first-class object" as the original line's anchor-avatar two-body model — **the original line's persistent-state carrier = PlayerAnchor (long-term ownership, memory authority, flushed at logout), the task's "Runtime Player" = Avatar (in-scene spatial authority)**; in some contexts (e.g. §6's Player persistent/runtime state dividing line) "Player" is retained as a narrative umbrella term, while code identifiers always use the original line's words.

### 2.2 Original-line Terms vs. Code Current State, Master Table (details in the companions)

| Original-line term (code identifier) | Code current state | Location and evidence |
|---|---|---|
| `Scene` (scene, a logical unit within the world) | exists but wrapped by the MapInstance→WorldSpace→Scene three-layer empty shell; zero entities in production code, no AOI, no player enter-exit | modules/game/world/src/scene.cpp, world_space.cpp, map_instance.cpp |
| `Instance` (instance, gameplay lifecycle) | only a create/find/destroy/update five-operation registry, no state machine, no gameplay lifecycle | modules/game/world/map_instance_manager.hpp |
| `Zone` (logic-server process, carrying 1..N scenes) | no code identifier; cell-app is the de facto single-scene process prototype | apps/cell-app |
| `PlayerAnchor` (≡ Base, resident at the Home Zone) | exists (six states + SessionBinding + WorldAssignment); **no Home Zone concept**, no persistence hook | modules/game/session/player_anchor.hpp; base-app/base_server.cpp |
| `Session`/`Proxy` (connection carriage) | no Proxy class; WorldSession is a pure bookkeeping state machine, zero-coupled with the network layer; two Session domains coexist (app-level) | modules/game/world/world_session.hpp; apps/* |
| `Avatar` (scene avatar) | **no such class**. The nearest are cell-app's internal `cell::PlayerEntity` and world_session's avatar EntityId field | apps/cell-app/src/cell_manager.hpp |
| `AOI` | **two implementations**: legacy `apollo::AOIGrid` (3x3 grid, consumed by tests only) and cell-app's own `cell::AOIManager` (fixed-length array grid); Scene has no AOI member | modules/game/world/src/aoi.cpp; apps/cell-app/src/cell_server.cpp:61-120 |
| `OnlineDirectory` (manager authority + mirror) | only the baseappmgr in-process assignments_+SessionLocator prototype; no cross-process directory, no event family | apps/baseappmgr/src/baseappmgr.cpp |
| `Suspended` (keepalive window) | WorldSession has a Suspended state **enum**, with no timer/expiry logic at all | modules/game/world/src/world_session.cpp:74-101 |
| `PersistJournal` (write-behind journal) | none; base-app has an application-level SaveQueue (autosave thread, prints at 60s) | apps/base-app/src/base_server.cpp |
| `RemoteEntityCall` / `InterServerLink` | no code; inter-process communication goes over the legacy RepSocket (nng stub) | modules/net · legacy net |
| `login_token`/`session_key`/`resume_token` | login-side ticket = timestamp XOR hash (sessionSecret unused); resume_token unimplemented | apps/login-app; modules/game/session |
| `FrameFilter`/four channels | none; the net-abstraction design doc's L0-L3 has no code counterpart | — (see architecture.md) |
| `VerifierApp`/`LoggerApp`/`Machined`/`InterfacesApp` | no code for any | design docs inbound-interfaces.md, battle-verification (gap #17) |

### 2.3 Retired Words Appearing in Existing Code (must be registered)

| Forbidden word | Locations |
|---|---|
| `Battle` (as a space/instance name: a historical misnomer, ruling 1) | the full legacy include/apollo/game/battle/ecs/battle_system.hpp set (BattleWorld/BattleManager/BattleSystem); docs/25's semantics is actually instance |
| `cellapp / baseapp` | apps/base-app's and apps/baseappmgr's process names and CMake target names (process names are historical leftover naming; whether to convert back to BW vocabulary is undecided; in-code classes do not use baseapp naming) |
| `Space` | world_space.cpp's WorldSpace class name contains the Space root — a partial violation, registered (2026-10-07 erratum: **the modules/protocol message family additionally has `SpaceID`/`spaceId`/`fromSpace`/`toSpace` and the `CellEntityMove`/`CellEntityProperty` struct names** — BigWorld-compatible wire-semantic carriers (the #33 facade face; the wire format carries no names), part of the same net M1/protocol convergence clearance chain as the nng foundation — M1 content is ruled (ADR-011); code clearance lands with the M1 implementation batch) |
| `Mailbox / EntityCall / witness / ghost` | no code occurrences (cleared at design time); they remain in design docs' comparison sections (allowed). 2026-10-07 recheck: zero residue in non-exempt document narration (guide/apps/design outside comparison sections) — all other occurrences are KBE source line references, ruling-record references, or literal factual statements of third-party library names |
| `nng` | modules/protocol (the apollo_protocol dependency), modules/net's nng_wrapper/socket.cpp/channel.cpp stubs, the legacy net stack — **the original line has banned nng (retired, #36 item 2), yet the code still depends on/residues it**: see architecture.md §Network |
| `registry` (no etcd/consul) | no external dependency (naming guidance only) |
| "self-developed / far ahead" style | none |

---

## 3. Build and Runnability Measurements

### 3.1 Default Build Artifacts

Under the default configuration (APOLLO_ENABLE_MODULAR_LAYOUT=ON; GAME_MODULE=OFF; protocol disabled), the actual output is: the new tree's base libraries (the usable parts of core/runtime/base/contract/data/net) + tests + the legacy targets still hanging in the build. **At the executable level: zero apps**. In other words, the repo today "installs but cannot run any service process".

### 3.2 If GAME_MODULE Is Turned On

- apollo::game_world contains only aoi+scene; the 5 world implementation files cell-app depends on are missing → link failure (newly registered by this audit, not yet in the defect book).
- game-server fails to link (C-95 registered: LogManager/SqlTemplate undefined symbols).
- modules/protocol fails to compile (never adapted to nng 1.11; its CMake guesses paths from InstalledDir).

### 3.3 Three Mainline End-to-End Breaks (app level, cross-checked by the apps agent)

1. **client→gateway**: NullClientIngressServer idles (acceptNewConnections commented "not wired yet"), sendToClient is a placeholder.
2. **login→base assignWorld**: base-app replies Unknown message type to assign messages; login under "stub transport mode" always returns true (login "always succeeds").
3. **Route resolution**: the gateway routes PlayerResolveRouteRequest to base-app port 9002, while the real handler lives only in baseappmgr (default 9003) — after the port mismatch it falls back to buildDefaultRoute; the gateway has no --baseappmgr flag.

Also: login's qr config `chatAppUrl=tcp://127.0.0.1:9003` collides with baseappmgr's default 9003 listener; the disconnect goes as plain text with no matching backend message type; two Session domains coexist (each app doing its own thing); no reconnect code anywhere (reconnectWindowMs zero references).

### 3.4 Test-State Overview (details in §6 and the companions)

- tests/, 41 files, 16,422 lines: mostly the legacy stack (net/thread/storage/db_mock) and in-memory directory/anchor unit tests; **the baseappmgr and PlayerAnchor directory tests explicitly "do not start(), test in-memory methods only" (test_baseappmgr.cpp:24-25 comments)**; gateway/login/cell have no tests.
- New-module side: modules/*/tests exist but mostly directly test the few sources they compile themselves; with the modular switch off these tests may not enter the build (check per measured gating).
- CI: workflows exist (a remembered four-gate push build + test: build/unit/integration/examples), but constrained by 3.1-3.2.

---

## 4. Documentation Current State (docs/)

(This section is an audit conclusion; the detailed revision plan is improvement-plan.md P4.)

- The site: docs/ is a VitePress site (docs/.vitepress), 310k lines including node_modules.
- Mature-structure areas: docs/design/ (12 design docs + contracts, high quality, decisions complete), docs/analysis/ (the #36 decision set, 25 rounds of architecture-review history, design-gap-inventory).
- Distortion areas (mismatched with code):
  - docs/guide/quick-start.md cites nonexistent APIs (dispatcher/ServiceRegistry etc.); the sample code does not compile.
  - docs/apps/BigWorld服务器应用实现.md differs from the code in 9 ways (the doc assumes NNG ready, the topology omits baseappmgr, the WorldApp name mismatches cell-app, Pub/Sub unimplemented, GatewayApp "optional" contradicts the code).
  - Several capabilities claimed in docs/architecture/ and the design docs (the attribute sync pipeline, the online directory, the four channels, the write-behind journal) have no implementation in code (item-by-item differences in architecture.md §7).

---

## 5. Dual-Structure Tree and Parallel-Implementation Master List (the structural finding)

The repo's **multi-implementation parallelism for the same capability** is the greatest maintainability burden; registered item by item (locations/sizes/consumers) for improvement-plan decisions:

| # | Capability | Parallel implementations | Consumer current state |
|---|---|---|---|
| A1 | Entity/ECS | (a) modules/game/core `Entity` (string-keyed component map) (b) legacy `apollo::ecs` (ecs.h, header-only) (c) legacy `apollo::battle::ecs` (ecs.hpp, compiled into apollo_game_battle) (d) cell-app `cell::Entity/PlayerEntity` (e) another set in legacy include/apollo/game/ecs.h. **4~5 sets in total**, id types mutually incompatible (u64 wrapper/uint64/uint32) | (a) only cell-app as the avatar id carrier; (b) only examples/ecs_demo; (c) only tests/test_game; (d) cell-app internal; (e) zero |
| A2 | AOI | (a) legacy `apollo::AOIGrid` (3x3 uv grid, aoi.cpp 256 lines) (b) cell-app `cell::AOIManager` (fixed-length array grid) — mutually unrelated; Scene has no AOI | (a) tests only; (b) cell-app, but viewer broadcast is a placeholder |
| A3 | Battle | (a) modules/game/battle `BattleSystem` (vector+update, not ECS, 75 lines) (b) the full legacy include/apollo/game/battle/ecs/battle_system.hpp set (BattleWorld/BattleManager, 185-line header) — (b) zero references, pure dead header | (a) zero production; (b) zero |
| A4 | Attribute system | (a) the variant-based AttributeContainer/AttributeManager (attribute.hpp, singleton, references fetched under lock) (b) the hand-written union ComVal (comval.h 587 lines + attribute_value.h, two copies: one under modules and one under src/, byte-identical, the latter unbuilt); attribute_id.h with ~290 constants | no production consumers; serialize has a per-attribute 1-byte underestimate bug (attribute_value.cpp:247 estimated vs 286 actual), tail attributes silently dropped on write |
| A5 | Session/directory | (a) modules/game/session (anchor/locator) (b) modules/game/world WorldSession (seven-state machine) (c) two in-app Session domains (gateway/login each) | (a) base-app+baseappmgr; (b) cell-app; (c) each to itself; interop only via message text |
| A6 | Transport | (a) modules/net tcp/rpc + apollo_protocol (nng stub) (b) legacy include/apollo/net + src/apollo/network (RepSocket etc.) (c) apps directly on the RepSocket/nng_wrapper stubs | under the default build, paths (a)/(c) are stubs (connect false, else empty branches) |
| A7 | Persistence | (a) modules/data (cache/core/orm/redis) (b) legacy src/apollo/storage (db_mysql/db_mock/memory_connection) (c) base-app's DatabaseService+SaveQueue (in-memory fake) — 9 files in (a)(b) at 0 compiles | real chain: only memory_connection (file-flush behavior to be verified) + the in-memory anchor |
| A8 | Configuration | (a) modules/core config_registry's ConfigRegistry (global KV, **live**) (b) legacy ConfigManager/ConfigNode (file + hot-reload + listeners, **zero production consumers**: parseXml/parseLua always return false, notifyListeners is a dead end, hot-reload has no caller, `getValue<T>` declared without definition) (c) each app's hand-written config.hpp fields (~20 dead fields) | Config/Definition/Runtime State mixed (setValue writes configs_ directly); two mutually unaware entry points for the same data |
| A9 | Logging | (a) the old in-memory log_manager.hpp/log_manager.cpp (four methods declared only/dead source, no build target) (b) the spdlog/built-in log_manager.h (**live**, different API) (c) utils/logging's third CamelCase set — **three sets coexisting + an ODR violation** (LogManager double-defined same-named class) | game-server's referenced snapshot/clear/set_console_enabled symbols do not exist anywhere in the graph (C-95); no unified async logging architecture |

> "Zero" in the table means a repo-wide grep finds no consumer (tests excepted). This table expands into the object-level ownership and dependency maps in object-model.md / architecture.md.

---

## 6. Test Coverage and the Four-Gate Gaps

Total test code: tests/ 40 files 16,422 lines + modules/*/tests/ 14 files 8,091 lines ≈ 24,513 lines. But governed by three switches, **the actual default build runs only 16 targets**:

1. **Tier 1 (default / what CI actually builds)**: the tests/CMakeLists.txt else branch registers 13 targets (bigworld_api/memory/string/terminal/thread_pool/time/core_lifecycle/core_log/core_config/runtime/data/net/game) + the module-side contract double gate (apollo_contract_tests 788 lines, apollo_contract_gen_compile_test) + the sdks/gen golden check. CI (ci.yml) passes only `-DAPOLLO_BUILD_TESTS=ON`.
2. **Tier 2 (the GTest branch, 16 targets)**: `APOLLO_BUILD_GTESTS=OFF` by default + CI passes nothing → protobuf/network/log/config/redis/database/sql_template/rest_template/integration/channel/… all unbuilt; channel_tests additionally needs APOLLO_ENABLE_IPC=ON (off by default). **README's claimed "database/Redis operation tests" are actually zero in CI**. And the GTest branch and else branch are mutually exclusive — each tier loses half the assets (game_tests, session_world_tests live only in the else branch).
3. **Tier 3 (module-level, 12 targets, 7,194 lines)**: `BUILD_TESTING` permanently false (the root CMake has no include(CTest) repo-wide) → the various *tests and *_comprehensive_tests under base/core/runtime/data/net/game/starter/bigworld/protocol never compile; several suites would not compile even if allowed (net_comprehensive references phantom headers websocket.hpp/message_codec.hpp; starter_comprehensive references the empty-shell module's module_registry.hpp; bigworld_comprehensive's BigWorld.h has 20 compile errors — C-64 registered).
4. **App-level, 4 targets** (base_anchor/session_world/baseappmgr/gateway_route/protocol_bootstrap types, compiling apps sources directly into test binaries): all guarded by targets turned off by default (apollo_protocol, game_session) → all skipped by default; **they live and die with the apps themselves**. The apps tree has no standalone tests/ directory.
5. **Three tombstone documents**: `./examples/all_features_demo` referenced at README.md:317 was deleted with ce5f07e8 (the extant test_all_features.cpp depends on deleted headers, necessarily fails to compile); tests/README.md is an entire Actor-suite tombstone (the referenced test_actor_framework.cpp does not exist, the coverage table has no backing); examples/README.md's ioc_example etc. do not exist. All examples are gated `APOLLO_BUILD_EXAMPLES=OFF` (CI sets it off explicitly) — **CI never compiles/runs any example** (benchmark.cpp does not even link apollo).

Task book §28 coverage matrix check (precise version):

| Domain | Default-build coverage | Gap |
|---|---|---|
| Runtime lifecycle | test_runtime.cpp 26 tests (Host start/stop/run_once/hooks/failure rollback) | no tick-exception recovery, no fault injection; module-level callback cases never compile |
| Player login/logout/reconnect | none | no end-to-end login, no logout, no reconnect (repo-wide, the socket-connected reconnect has zero references) |
| Scene/Instance | test_game.cpp:568-624 Scene construction/spawn/update | MapInstance only create/update/destroy (test_session_world.cpp:107-125); no start/finish state machine; the instance production path is uncompiled |
| AOI | test_game.cpp:304-432 legacy 3x3 grid, 9 tests (enter/leave/move/visibility) | cell::AOIManager untested |
| Battle | test_game.cpp:434-568 legacy ECS | the new BattleSystem zero-instantiated (comprehensive only includes) |
| Persistence | test_data.cpp 20 tests (memory connection/cache) | real DBs untested; base-app DatabaseService untested |
| Network | test_net.cpp 76 tests (header-level: packets/checksums/HTTP/WS headers) | the real-socket loopback tests (test_network.cpp) are in the GTest branch → CI never runs them |

Infrastructure: the assertion body is a self-written zero-dependency macro (TEST_ASSERT across ~30 files, re-implementing TEST_CASE/INTEGRATION_TEST registrars); real GTest in only 9 files; **no sanitizer/valgrind wiring whatsoever** (the -DSANITIZE_* claimed in tests/README is undefined repo-wide); the coverage option exists but codecov.yml ignores tests/**; the contract test family (test_contract.cpp + xmllint XSD gate + golden check) is the repo's most completely wired, CI-actually-run test family.

---

## 7. The Most Important Architecture Problems (severity-ordered master outline)

> Detailed argumentation and location evidence in architecture.md §8 and object-model.md, lifecycle.md, concurrency.md, persistence.md; the remediation grading in improvement-plan.md.

1. **[P0] The player object model is out of step with the code**: contract/design (PlayerAnchor+Avatar two bodies, scene transfer, the Suspended window) vs code (no Avatar, no Home Zone, WorldSession's field-hauling transfer, zero disconnect handling). The full focus of task book §2/§5/§6/§12.
2. **[P0] Scene/Instance never became the runtime boundary**: the three-layer empty-shell wrapper (MapInstance→WorldSpace→Scene), no AOI, no tick phases, no lifecycle; production entities all go through cell::EntityManager. Task book §8/§9/§10/§11.
3. **[P0] Object ownership/execution context unassigned**: the mutex+map manager crowd, tick trees inside locks (MapInstanceManager::update runs entities while holding the lock), container references used outside locks (AttributeManager), EntityId/PlayerId namespace confusion (cell_server.cpp:559 find_by_player(entity_id)); task book §5/§26.
4. **[P1] The transport/protocol dual trees unconverged**: nng not yet retired (contract forbids), NNG stubs idling, three mainline breaks (3.3), port collisions; task book §21.
5. **[P1] The persistence stack is a fake implementation**: DatabaseService::initialize always true, loadPlayer fakes data, fromJson/toJson asymmetric (missing x/y/z), SaveQueue's direct callback(true), no write-behind/journal; task book §7/§19.
6. **[P1] Recovery/reconnect unimplemented**: no reconnect code, no keepalive window, no resume_token; task book §20.
7. **[P1] AOI dual implementations, neither wired into Scene**; task book §13.
8. **[P2] Four~five Entity/ECS sets in parallel** (A1); task book §14/§15.
9. **[P2] Battle's two-layer empty shell** (the new BattleSystem with no production, the legacy battle a pure dead header); task book §16.
10. **[P3] The default build has zero apps, three latent link breaks** (incl. the newly registered cell-app); the CI's four gates cannot cover the real chains; task book §3/§29.
11. **[P3] Documentation distortion** (quick-start not compilable, the apps doc's 9 differences, design-doc capabilities unlanded); task book §33/§34.
12. **[P3] Excess legacy complexity**: ~1/3 dead interfaces and 4 global singletons (AOIManager/AttributeManager/AttributeContainerManager/BattleManager), the hand-written comval union, attribute_id's 290 constants; task book §23/§32/§37.

---

## 8. Terminology Differences (audit-level summary)

> Each companion appends its own local list per its topic; here is the cross-document master table.

1. **The `Player` code identifier remains in sdks/contract/entities.xml:8 and the generated contract.lua/apollo_contract.h** (`<entity id="Player" parent="Avatar">`), with the Monster→NPC→Avatar→Player four-layer chain; the player entity goes by Avatar only (Player deprecated) → **the generated artifacts and existing docs need reworking to the Avatar-only line (the semantically equivalent landing: Avatar carries the social face / long-term state ownership goes to the Anchor; the `Player` entity should be folded into Avatar or deleted, to be decided by later code batches — registered this phase only)**.
2. **`Battle` (space sense) residue**: include/apollo/game/battle/ecs/battle_system.hpp (the full BattleWorld/BattleManager set); forbidden by the original line (ruling 1), and the file itself has zero references.
3. **`nng` not retired**: modules/protocol's CMake/code, modules/net's socket.cpp/channel.cpp/nng_wrapper.hpp stubs, legacy net references; the original line §2 bans "nng" (retired, #36 item 2) — the code has not followed the design (net-abstraction.md's G-1 self-contained four layers not landed).
4. **Process naming base-app/baseappmgr**: the original line forbids "baseapp/cellapp" as apollo process names (Zone instead); the two extant app directory/CMake target names conflict with the original line and should be renamed after migration (e.g. anchor-app/directory-app, exact names set by improvement plan P1/P3 via the new-term process).
5. **The WorldSpace class name contains the "Space" root**: the original line forbids bare Space; world_space.hpp's `WorldSpace` (and the design calls it "the three-layer empty shell of one Scene per instance") should be dissolved when the Scene model unifies.
6. **`line/room` etc.**: no code occurrences, compliant.
7. **Old words in design docs (docs/25 etc.)**: historical-document comparison sections, allowed under original-line rule 2, to be unified at document revision (P4).
8. **The `Instance` code identifier = MapInstance**: the original line requires the `instance` sense (instance gameplay lifecycle); the extant MapInstance has no state machine — not a naming error but **semantics not yet at spec** (detailed in object-model.md); the name is retained.
9. **The contract system's (entities.xml) client/internal domain split vs the sdk-contract doc**: the doc is chartered (1-899/900+); whether the XML and generator enforce the split needs code cross-reference (see architecture.md §original line; the sdks/gen-side data was verified with in-batch material).
10. **"Player" appears in this document set's narrative text** (e.g. "the online Player's in-memory object is the runtime authority", a task book §7 original sentence): per original-line rules, code/concept identifiers are always written PlayerAnchor/Avatar; where the narration quotes the task book verbatim it is marked with quotes, not used as a code identifier.

---

(end)
