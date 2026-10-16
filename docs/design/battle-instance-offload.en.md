[English](battle-instance-offload.en.md) | [中文](battle-instance-offload.md)

# Battle Instance Offload Pre-Design (instance process shape)

> Status: **pre-design draft (2026-10-07); the three decision points were decided 2026-10-10 (ADR-012: on-demand spawn request protocol / in-memory stub transition / crash voids the instance)**. Positioning: the process-shape skeleton for doc 36 decision table #16 "Battle standalone instance + Replay/Inspector adaptation seam" — following the format of net-abstraction §7's "P3 pre-design": pin the shape, pin the dependency order, pin the decision points, and do not write implementation details. Terminology is anchored in this document (**instance process**: the offload shape where a single scene runs in its own dedicated process).

---

## 1. Statement of Current State (explicit)

- The battle runtime **runs in-process**: the `BattleRuntime` five-stage state machine delivered by P2-2 is attached via `Instance::attach_battle` (world→battle same-process link); P2-4's `AnchorRewardSink` settles rewards in-process. **Cross-process battle is zero** — this is phasing, not an omission.
- The determinism prerequisites **are delivered**: battle-determinism's four constraints (decision domain / floating point / iteration order / random substreams) + the ReplayTuple/event hash chain (P2-2) — "cross-machine instances with the same binary share the same determinism domain" (battle-determinism #16) is already in place; the instance-offload shape (Matchmaker/Instance Manager skeleton, traceable to docs/25 §3.1-3.2) belongs to this batch's design.
- Cross-process transport **has not landed**: the M1 self-built kernel is being re-estimated with the P3-2 batch (todo contract-difference debt row); nng is retired.

## 2. Target Shape (doc 36 #16 scope)

Large-scale fleet battles offload to **short-lived instance processes**: fault domain and tick rate independently tunable (25 §3.2/§3.3/§8); results **land back one-way** via the Result Dispatcher (25 §3.4); Gate↔Battle do not connect directly (25 §5); keyframe recording leaves the seam for deterministic replay (25 §6). The three adaptation seams (doc 36): (1) a contract battle-dedicated channel (an extension beyond sdk-contract §2.3), (2) instance tick 20-50ms independently configurable, (3) Replay keyframe recording. **Lockstep is not implemented now** (doc 36 #18: leave the seam only).

## 3. Components and Responsibilities (counterparts to the docs/25 §3.1-3.2 skeleton, not the whole bundle)

| docs/25 piece | apollo counterpart | Status |
|---|---|---|
| Matchmaker / Scheduler | **landing point = the baseappmgr domain's "allocation and admission" origin** (net-abstraction §7: load is observation — frame time/entity count/queue watermark reported over the control channel; observation and scheduling consume the same data, no second set of load statistics) | not landed (baseappmgr-domain main body in a later batch) |
| Battle Instance Manager | **instance lifecycle**: spawn / teardown / crash handling — spawn reuses the machined supervision surface (batch F delivered); teardown = self-termination after settlement (short-lived) | supervision surface delivered; the on-demand launch protocol is undecided (decision 1) |
| ECS + Battle Pipeline | the `BattleRuntime` five-stage pipeline (P2-2) | delivered (in-process shape) |
| Result Dispatcher | one-way settlement landing: reward semantics go along `IRewardSink` (P2-4) across processes to the baseapp where the PlayerAnchor lives — battle only exports, never imports, never writes back (one-way decoupling, 25 §3.4/§5) | semantics ready (in-process version); cross-process not landed |
| Persistence | battle settlement lands in the archive via the write-behind journal (attribute-sync §8.2 single-writer discipline) — the instance process **never writes the DB directly** | discipline established |

## 4. Lifecycle Protocol — [Decision 1 decided: (b) on-demand spawn request protocol, 2026-10-10 / ADR-012]

How instance processes are born (machined's current state: UDP discovery/heartbeat/death directory + static roster supervision, no on-demand request channel):

- **(a) Static pre-pool**: instance processes stay resident pre-launched in the machined roster; demanders take an idle instance from the directory. — zero machined changes; cost: idle occupancy + pool size driven by configuration.
- **(b) On-demand spawn request protocol**: the demander (baseapp/baseappmgr) sends machined a launch request; once the instance is ready it becomes visible via the directory. — semantically the most accurate; cost: machined needs a new control plane (UDP request/reply packets, or a §7 control-channel cross-process extension — **the control-plane shape must be designed together with the baseappmgr domain's consumers**, to avoid two control protocols).

**Ruling**: (b), but the control plane reuses the §7 fleet-event pattern (Query/Advertise have already established the "discovery surface" precedent — a launch request/ready reply is the same extension direction); (a) serves as the transition before (b) is ready. **Compatibility note (2026-10-10 / ADR-017)**: after the acceptance-surface response wiring was ruled ack-on-accept, the "ready reply" is not a synchronous RPC response — the launch-request acceptance point returns an ack, and the ready signal goes via the directory Advertise ("visible via the directory once the instance is ready" is this decision's original wording); M1's delayed-reply landing does not change the ready-signal path either.

## 5. Data Plane and Dependency Order — [Decision 2 decided: (b) in-memory stub transition, 2026-10-10 / ADR-012]

- **The battle feed = internal-domain envelope messages** (net-abstraction §7 "envelope/contract convergence": `InternalMessageEnvelope` = sdk-contract §11 id 900+, produced by the generator, no hand-written second protocol); the battle-dedicated channel (doc 36 adaptation seam 1) enters the repo with the contract batch — **the contract lands following the implementation** (P2-3 discipline: protocol enters the repo together with its implementation, not on credit).
- **Transport = the M1 self-built kernel** (§5.6/§5.7 InterServerLink connection semantics) — the P3-2 batch. Two dependency-order options:
  - (a) **M1 first**: the offload batch is scheduled after P3-2 and lands directly on the real transport;
  - (b) **In-memory stub transition**: offload first lands the lifecycle and protocol skeleton on same-machine pipes (pipe/shmem SPSC, a candidate shape of §7's same-machine mirror stream), swapping the base once M1 is ready.
- **Ruling**: (b) — the design risk of lifecycle/protocol/fault handling is decoupled from transport and settled first; the cost: one base swap.

## 6. Determinism Carrier (delivered; no new design)

Cross-machine instances with the same binary share the same determinism domain (battle-determinism §1's four constraints); ReplayTuple + the event hash chain = the replay/Inspector seam (25 §6); the instance tick and the baseapp main tick have **independent timelines** (clock-and-time.md: the tick number = the replay timeline — the instance process carries its own tick cadence, 20-50ms configurable, doc 36 adaptation seam 2).

## 7. Fault Domain — [Decision 3 decided: (a) crash voids the instance, 2026-10-10 / ADR-012]

Instance crash detection = the machined death event (G-1 directory surface, delivered); process restart = supervision-surface backoff restart (batch F, delivered). **The battle-state ruling**:
- (a) **Crash voids the instance**: the fleet battle restarts; the player side gets an error-surface notification (simplest; under short-lived semantics the loss window is small);
- (b) **Keyframe-replay recovery**: a standby/reborn instance recovers from the latest keyframe + input replay (the disaster-recovery consumption of 25 §6's replay seam; cost: a keyframe-persistence surface + a replay protocol).

**Ruling**: (a) first, (b) leaves the seam (the ReplayTuple is already keyframe material; the protocol surface comes later).

## 8. Dependency and Phasing Summary

1. **Delivered**: the deterministic runtime (P2-2) / long-lived state + one-way reward port (P2-4) / machined directory + discovery + supervision (P3-1 batches B-F).
2. **The three decision points are decided** (2026-10-10 / ADR-012): §4 on-demand spawn request protocol, §5 in-memory stub transition, §7 crash voids the instance.
3. **Path after the decisions**: the protocol-message contract batch (internal domain) → the offload skeleton batch (lifecycle + landing-back, in-memory stub transition) → swap the base once M1 is ready → the baseappmgr-domain landing point plugs in (the Matchmaker responsibilities settle home).
   **Landing progress (2026-10-11)**: the contract batch is delivered (the internal-domain instance family 919-923 entered the repo, client_hash unchanged; the three no-entry gaps are pinned in XML comments); the skeleton batch is delivered (modules/game/battle offload_wire/offload_host/offload_node — envelope identity takes the generator MsgId, a six-state lifecycle, landing back goes out one-way via `IRewardSink`, the crash-voids-instance abandon surface; BattleOffloadTests with 10 groups including cross-boundary determinism comparisons). Seams left: the rejection surface goes with the machined control-plane batch, the projection payload encoding goes with battle-determinism finalization, and the base swap to InterServerLink goes with M1.

## 9. Intersections with the Other Designs

- net-abstraction §7: control-plane reuse, envelope convergence, load from one source, same-machine mirror-stream candidate — **the master outline**.
- battle-determinism.md: the four constraints and the replay format — the determinism prerequisite; this document's §6 cites it wholesale without redefining.
- battle-verification-service.md: the VerifierApp's client-authority reconciliation — another battle-verification line, orthogonal to offload (they may breathe the same hash chain).
- session-and-online-directory.md: session/online directory = another responsibility piece of the baseappmgr domain; the RouteResolver host resolution serves the landing-back routing.
- The instance-process naming anchor (this design).
