[English](todo.en.md) | [中文](todo.md)

# TODO Task Plan (task book P0–P4 rearrangement, 2026-10-04)

> Basis for the rearrangement: `docs/rearchitecture/architecture-review-plan.md` (40-section task book) + the first-phase audit seven-piece set.
> Terminology discipline: naming prefers reusing existing BigWorld/KBE concepts; new terms first enter the glossary with a mapping and rationale.

## Completed (audit phase 2026-10-04)

- [x] Task book checked in (ce91a739)
- [x] First-phase read-only audit seven-piece set committed and pushed (aac84a25)
- [x] User approval, thawed into the code phase (2026-10-04)

## P0 — Object Ownership and Core Model Rebuild

- [x] P0-1 Execution context base: WorldHost compile completion + fixed-timestep abstraction + ThreadAffinity/SceneContext annotations (fd61ff7f)
- [x] P0-2 Player object model three-piece set: Avatar class + PlayerAnchor fields + PlayerId/EntityId strong-typed distinctions
- [x] P0-3 Scene/Instance model rebuild: Scene runtime container + Instance eight-state machine + `world.create_scene`/`scene.create_instance`/`instance.enter`
- [x] P0-4 Lifecycle closure: WorldSession Leaving observable + transfer async + failure rollback + application shutdown protocol
- [x] P0-5 Build base hardening: 6 previously-uncompiled sources attached to targets + zero dangling app links
- [x] P0-6 Dual-tree convergence prep: LogManager ODR elimination + first batch of dead singletons
- [x] PORT-1 Cross-platform portability small batch: config_value.h add `<cstdint>` + net_util.h setKeepAliveParams three-platform branches

## P1 — Scene Transfer / Directory / AOI / Persistence / Recovery / Reconnect

- [x] P1-1 Scene transfer: prepare→detach→attach→state sync→resume protocol + failure rollback + disconnect handling
- [x] P1-2 PlayerDirectory/online directory: modules/game/session PlayerDirectory + contracts 900-905 + behavior-contract unit tests, 7 groups
- [x] P1-3 AOI convergence and Scene integration: SceneAoi single implementation + ViewerState watermark + test_game 9-item migration (15/10 all green)
- [x] P1-4 Persistence stack convergence + minimal disk write: five sets→one set + SaveQueue real disk write + PersistJournal skeleton + write→restart→read-back
- [x] P1-5 Recovery: three-tier inventory + RecoveryCoordinator startup sequence + journal replay + recovery_tests 5 groups
- [x] P1-6 Reconnect/keepalive window: contracts 906-908 + WorldSession idle window + gateway structured disconnect (819cd25e; ON 30/30, OFF 27/27)
- [x] Full documentation reconciliation: README/docs whole site vs HEAD real implementation

## P2 — Battle / ECS / Social / Guild / Party

- [x] P2-1 ECS convergence: five sets→one set within scenes (2bbae2a3)
- [x] P2-2 Battle Runtime: determinism four constraints + battle tick integration + reward one-way interface (battle_runtime_tests 6 + rng_substream_tests 6)
- [x] P2-3 Contract system revision: entities.xml inheritance-chain revision + 12 new protocol messages + golden check (15→27)
- [x] P2-4 Social/Guild/Party: Anchor long-lived state fields + Guild/Party objects + contracts 18-27 (37 msgs; ON/OFF 33/33)

## P3 — Deployment / Performance / Observability / DevEx

- [x] P3-1 Deployment and process topology: baseappmgr domain directory cross-process + machined supervision surface + G-1 full chain + instance offload protocol contract + rename batch (a5750eda) + rename-revert batch
- [x] P3-2 Performance: three-model benchmark stubs + intake surface measured ~0.1µs/intent + flush segment + persist segment (934f5df2 / fd46aaf9 / 97b84a54; ON 58/58, OFF 49/49, san 49/49)
- [x] P3-3 Observability: log convergence + LoggerApp + L4 push channel + MetricRegistry + metrics admin channel + trace subset + histogram + alert notifier + proc liveness + sampling assembly + BI fan-out (L1-L9; ON 59/59, OFF 50/50, san 50/50)
- [x] P3-4 DevEx: dev_fleet.sh + gateway three-step prescription + net M1 kernel + ingress wiring + M1 contract + cell/gateway M1 + fleet member registration (0de62ea9; ON 56/56)

## P4 — Documentation / Examples / Benchmark / Tutorial

- [x] P4-1 Documentation revision: quick-start real APIs + apps docs 11-item corrections + tests/README rewrite + README first screen + docs/architecture/overview.md + ADR-001..009
- [x] P4-2 Minimal runnable example: examples/full_loop_demo.cpp in-process eight-step drive (36 assertion gates)
- [x] P4-3 Benchmark/Tutorial: docs/guide/tutorial.md first steps + three-model benchmark reuse
- [x] P4-4 Terminology inheritance: whole-repo wording review + nng base replacement migration five batches fully cleared (9121ebc2; four trees 53/53·44/44·51/51·44/44)

## Legacy Registry (closed / not listed on main line, not expanded)

- Old 9-batch plan (2026-09-29): batch 1 contracts delivered; batches 2-4 overlapping items merged into P1-P4; batches 5-8 (AI pathfinding/scripting/SDK/monitoring) not listed on main line, kept as-is.
- Audit legacy dead-artifact cleanup: comval hand-written union + attribute_id 290 constants cleaned (2026-10-09); LocalServiceDiscovery/queue family/IPC side branches kept as-is and registered.
- G-series capability gaps: G-1 fleet/discovery fully delivered; G-4 timer wheel library part delivered (timer_wheel_tests 12 groups); G-3 shutdown sequence code surface delivered; G-5 detection primitives delivered; G-6 contract inheritance generation-time expansion delivered; G-2 backup/DR prerequisite design established (ADR-010 option A first, standby not started, batch 0 delivered).
- Contract discrepancy cleanup: entities.xml Player parent conflict cleared with P2-3; WorldSpace word root cleared with P0-3; process names with P3-1; nng/modules·protocol archived (M1 self-developed kernel).

## Progress Discipline (task book §37/§40 + improvement-plan §7)

1. After each batch completes, rerun the four gates (Build / Unit / Integration / Examples); commit and push only when all green; CI triggers one run.
2. Small-step commits and pushes: one commit per batch, containing only this batch's files; delete-then-create (convergence) and create-then-delete (nucleus establishment) batches kept separate.
3. Terminology discipline: naming prefers reusing existing BigWorld/KBE concepts; new terms first enter the glossary with a mapping and rationale.
4. In-batch file scope is governed by the file-level list in improvement-plan.md; out-of-scope changes must be reported for coordination.
