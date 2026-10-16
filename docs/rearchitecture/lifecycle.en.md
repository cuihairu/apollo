[English](lifecycle.en.md) | [中文](lifecycle.md)

# Apollo Lifecycle Map (lifecycle)

> Belongs to: companion piece 4 of the phase-one read-only architecture review. Companions: audit.md, architecture.md, object-model.md (object ownership), concurrency.md (threads/locks), persistence.md, improvement-plan.md.
> Basis: task book §9 (Instance lifecycle), §11 (Scene Transfer), §18 (Runtime/Application lifecycle, the four checks: constructor does too much / destructor implicitly releases / global singleton / cross-module implicit lifecycle), §20 (Recovery). Terminology follows existing code wording (BigWorld/KBE concepts reused first); the differences list is at the end.

---

## 1. Lifecycle Hierarchy Overview (current state)

```text
Process layer:
  ApplicationHost (Boot->ConfigLoaded->Initialized->Ready->Stopping->Stopped phase machine,
    used by apps/game-server)
    └ ServiceHost (thin forwarding) [game-server path]
    └ WorldHost (default 20Hz; note: world_host.cpp has no build target — see §1.4)
         └ cell-app: CellWorldService::tick (CellServer private, guarantee class)

Object layer (the complete set of state machines today — only two):
  PlayerAnchor six states: Loading -> Online -> Transferring -> Disconnected -> Saving -> Offline
  WorldSession seven states: Entering -> Active -> Suspended -> TransferringOut -> TransferringIn -> Leaving -> Closed

State-machine-less objects (current state): Scene / MapInstance / Entity / AOI / Avatar (no
class) / Connection / SaveQueue / DatabaseService
```

### 1.1 Per-object lifecycle state (exhaustive check)

| Object | Current lifecycle | Created by | Destroyed by | Defects |
|---|---|---|---|---|
| ApplicationHost | phase machine complete (a start failure walks StartupFailed→Stopping→Stopped) | main | main | the failure path does not call stop on already-started services (exception safety missing); the `if (!is_running()) skip stop` semantics is fragile; run_once always returns 0 with no error propagation; exceptions bubble straight up |
| ServiceHost | thin shell | main | main | no independent semantics |
| WorldHost | initialize/tick/shutdown + 20Hz fixed rate | cell_server.cpp:281 | — | **no build target** (runtime/CMakeLists compiles only application_host+runtime_manifest; 88 compile_commands entries with no world_host.cpp) — engine-correct but never built |
| PlayerAnchor | Loading (at activate) -> Online (bindSession) -> Disconnected (unbindSession) -> Saving->Offline (at deactivate) | base-app activatePlayer | base-app deactivate | Transferring never reached (no cross-process transfer); Saving has no save action (SaveQueue is fake); no flush guarantee before Offline |
| WorldSession | seven-state bookkeeping: attachPlayerWorldSession sets Entering -> resume Active; detach suspends then close | cell-app attach | cell-app detach | close_session sets Leaving then **immediately** sets Closed and erases — Leaving is unobservable; resume/complete_transfer do not validate the current state (a Closed session can be resumed directly); complete_transfer after begin_transfer completes synchronously — TransferringOut/In are always instantaneous states, at odds with the "cross-process async reconciliation" design narrative |
| Scene | none: exists from construction, dies with its owner | the MapInstance ctor chain | no explicit | no ready/running/stopped; "independent stop/resume" impossible |
| MapInstance | a registry entry of five operations | create_instance | destroy (erase) | no Initialize/Waiting/Running/Finishing/Draining/Rejoining semantics (all eight segments of task book §9 missing) |
| Entity (cell) | add->update->remove: on_spawn/on_despawn/on_update hooks | EntityManager::add (no production caller) | the container | component on_attach called twice (once by add_component, once by spawn); EntityManager::update calls on_update while holding the lock |
| Avatar | **does not exist** | — | — | no spawn/death/scene-transfer rules |
| Connection | exists from accept until disconnect | gateway raw TCP | disconnect | no session carriage, no keepalive, no reconnect; HttpServer connections are never reclaimed (http.cpp:506-549 unbounded growth) |
| SaveQueue | resident workerLoop thread | base-app ctor | process end | worker only does callback(true); no stop protocol (destructor join unwritten) |
| DatabaseService | initialize() always true; no close | base-app ctor | — | lifecycle is an empty implementation |

### 1.2 Verdicts on the four §18 checks

1. **Constructor does too much**: overall no. DI `create_bean` has no heavy-work constraint (doing dependency actions at construction is a caller-discipline issue); but the FileAppender string constructor opens the file under the default config first, then changes config_ without reopening (file_appender.cpp:36-50, in fact a bug); AnchorManager::activate does an in-place make_shared with no loading (loading happens in activatePlayer — order correct).
2. **Destructor implicitly releases business resources**: partly. ApplicationContext's destructor calls shutdown in explicit reverse order (correct); but the cell EntityManager holds raw pointers and owns no destruction; WorldSession/Anchor destructors carry no business actions (resources go with the map erase); legacy ecs.hpp `DestroyEntity->RemoveAllComponents` skips component destructors (leaks memory for components holding std::string).
3. **Global singleton**: 5+ of them (AOIManager/AttributeManager/AttributeContainerManager/BattleManager dead pieces + global_config/global_log_manager live pieces + a dual LogManager class definition, ODR). The live pieces are reasonable (process-level config/logging); dead pieces should be deleted (improvement-plan P1/P3).
4. **Cross-module implicit lifecycle**: yes. The dual-tree coexistence creates the crosscut of "one capability, two lifecycles" (e.g. the anchor in modules/game/session vs. the anchor consumed by base-app; modules/net implementing old-header APIs with no unified lifecycle contract); the ODR makes the log lifecycle entirely undefined.

---

## 2. Key Sequences (current state, item by item, against the design docs)

### 2.1 Login (login-flow's designed two-phase connection -> code)

```text
client -> login-app (stub, always succeeds) -> [token = ticket timestamp XOR hash, sessionSecret unused]
      -> base-app (9002): PLAYER_ACTIVATE -> activatePlayer -> DatabaseService.loadPlayer (997 fakes
        "player_+id") -> AnchorManager.activate -> bindSession (anchor->Online, SessionLocator
        registers) -> returns true
The landing chain the design requires (login-flow §10): baseappmgr arbitrates the Home baseapp ->
base erects the anchor -> baseapp reserves scene — the code has none of this chain
```

### 2.2 Entering a scene (the attach path)

```text
cell-app CellServer::attachPlayerWorldSession (cell_server.cpp:532-550):
  assign_map_instance(id=1 defaultMapInstanceId_); assign_space(same id); bind the avatar EntityId;
  route_version+1
  world_session Entering -> resume -> Active
None of: scene-enter validation, spawning an Avatar object, AOI registration, initial view snapshot
```

### 2.3 Scene transfer / cross-border (task book §11 focus; against player-object-model §3)

```text
Code: handleCellCrossBorder -> WorldSessionManager.transfer_session -> immediate complete_transfer (sync)
Design (contract line): admission (gameplay layer) -> target scene creation -> the old scene destroys the
                  Avatar -> the new scene rebuilds the Avatar (state projected from the Anchor)
                  -> volatile state (buffs/CDs/coordinates) not carried over -> settlement results flow
                  one-way back to the Anchor
Gaps:
  1. "prepare/admit/reject" absent (no transfer pre-validation)
  2. state transfer is field hauling (world_id/map_id/space copied into the new record), not object
     projection (no Avatar)
  3. no failure rollback path (complete has no failure branch)
  4. disconnect mid-transfer: none (the Transferring state is instantaneous)
  5. AOI rebuild: none (AOI is unrelated to scene)
  6. "target scene does not exist": none (default single scene)
```

### 2.4 Disconnect and reconnect (task book §10/§12/§13 focus)

```text
Current state: disconnect = the connection vanishes = everything vanishes (gateway has no session;
  base-app's SessionBinding is never unbound — the disconnect goes to the backend as plain text with no
  handler; reconnectWindowMs has zero references)
Design: disconnect -> stay resident in the Cell scene N seconds (Suspended window) -> the window expires
  and the Cell removes it; the Anchor stays resident keeping state; the Session can resume
  (resume_token TTL shares origin with the window)
Gap: the whole chain, zero implementation
```

### 2.5 Logout

```text
Current state: no logout message type (repo-wide grep, no logout); deactivate erases the anchor
  directly (no archive)
Design: archive on logout -> flush to DB -> deactivate
```

### 2.6 Process shutdown (task book §18 close-out)

```text
Current state: ApplicationHost stops services in reverse order (skipping !is_running ones); no
  last-frame flush semantics; cell-app has no shutdown handler; base-app's autosave thread only prints
Design: Stop -> each service stops -> dirty data flushed (persist stop-flush protocol) -> process exit;
  the recovery phase refuses new work until convergence
```

---

## 3. Recovery (task book §20) Current-State Matrix

| Recovery object | Must persist | Recomputable | Droppable | Code state |
|---|---|---|---|---|
| Player long-term state (anchored) | yes (level/inventory/progress) | — | no | load fakes data, save never flushes → **all lost** |
| Scene runtime state | — | yes (reopen the scene) | partly | no scene state file |
| Instance progress | results land one-way on the Anchor | abortable mid-flight | no (settlement) | no instance lifecycle |
| Session/connection | — | yes (rebuild within the reconnect window) | outside the window | none |
| Service topology (baseapp crash) | the baseappmgr directory is rebuildable | — | temporary loss | none (baseappmgr is a single-point in-memory node, not itself persistent) |
| Battle intermediate state | — | rematch | droppable before settlement | no battle |

**Verdict**: recovery = entirely blank; improvement plan P1-5 (Recovery) starts from the
"define must-persist / recomputable / droppable" three-tier list (task book §20's literal
requirement); its carrying mechanism = persistence.md's write-behind target model.

---

## 4. Terminology Differences (lifecycle level)

1. **The Suspended semantics gap**: original line §1.3 "the disconnect keepalive window = the Suspended state, the window and the resume token TTL share origin"; the code's world_session.hpp has the Suspended enum but zero timers (world_session.cpp:74-101) — the enum carries no window. Difference registered.
2. **Scene transfer = transfer field hauling**: original line "换幕 (scene transfer)" = Cell destroy-and-rebuild (the migration association is forbidden); the code's begin_transfer/complete_transfer is record hauling and completes synchronously — direction right (no cross-process tax) but no object rebuild/state projection (the six gaps of lifecycle §2.3). Difference registered.
3. **Leaving/Transferring unobservable states**: the code's state machine claims 7 states; in fact 2 are instantaneous and 1 is unobservable (close_session immediately sets Closed); the original line needs only the **observable window** the word Suspended maps to. Difference registered.
4. **The Anchor's Transferring/Saving**: the original line says the Anchor never migrates (Home baseapp), so the code's Transferring never being reached is fine; Saving having no save action is a **functional gap**, not a naming issue. Registered.
5. **"Stay resident N seconds after disconnect"**: player-object-model §3's Cell idle window has no corresponding implementation anywhere in the code (repo-wide grep, no idle/timeout logic) — a P1 implementation item, naming follows the original line.

---

(end)
