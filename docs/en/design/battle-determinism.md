
# Apollo Battle Determinism and Replay (battle-determinism)

---

## Executive Summary

1. **Determinism in apollo is a "half" problem — rule the domain first, then set the constraints**: under the server-authoritative model there is no lockstep cross-end floating-point consistency problem (the client does not adjudicate — attribute-sync §9 prediction/presentation correction, drift tolerated); determinism has only three consumers — **replay recomputation** (anti-cheat reconciliation/bug reproduction), **cross-process battle offload** (No. 36 #16 instances can run on another machine), **AI recomputation** (re-deriving decisions during replay). The three are isomorphic in their requirements: **same binary same platform = same determinism domain**. Cross-compiler/cross-OS is not promised (the replayer uses the same-version binary as at recording time).
2. **Four constraints**: ① single judgment domain — all adjudication happens inside the owning scene thread's tick boundary, async results never enter adjudication; ② floating-point discipline — IEEE754 + compile-profile lock (`-ffp-contract=off` etc.), **no fixed-point** (fixed-point is a lockstep cross-end device; no corresponding benefit here); ③ iteration order — the simulation segment requires no order (independent entities), the settlement segment applies effects in the **target entity_id total order**; ④ random numbers — `(world_seed, tick, stream_id)` substream derivation (PCG32), no global-state cursor, any tick independently replayable.
3. **Replay = four-tuple + hash chain**: `(binary_id, world_seed, starting snapshot position, input sequence)`; the replayer re-simulates tick by tick, and at each tick end computes a rolling hash of the battle event stream in canonical order (entity_id) and compares it with the live one — the divergence point is the bug/cheat point. **Two forms kept separate**: spectate/highlight replay = event-stream re-enactment (no re-simulation, cheap); recomputation replay = re-simulation + hash comparison (expensive, enabled on demand).
4. **AI is recomputed, not recorded**: AI decisions draw from a deterministic substream — at replay the same seed and tick re-derive the same decisions; the recording surface contains only player upstream intents, keeping size controllable.
5. **Same discipline on the Lua side**: rng/clock via injection (the sandbox already strips `os.time`/`math.random`, scripting-lua §4); settlement traversal bans `pairs` (unordered); the instruction-budget truncation is itself deterministic (deterministic instruction stream → deterministic truncation point).
6. **Prerequisite**: C-34 convergence of the three ECS sets (battle systems unified into one) — with three coexisting there is no single host for the "battle pipeline" and this doc's constraints have nowhere to land.

## 0. Why a "Half" Problem: Negative Space and Precedents

## 1. Judgment Domain: Single Writer and Tick Boundaries

- **Adjudication = the pure-function segment inside the scene thread's tick boundary**: input = (world state before this tick, the player intent set, deterministic substreams); output = (new world state, the battle event set). The simulate segment of attribute-sync §10's six phases is exactly this; the first four phases (input/validate/execute/settle) are deterministic, **the broadcast phase is not** (delivery order/trim/packet loss does not affect world state — the event set itself is the output; how it is sent is net's business).
- **Async never enters adjudication**: external data needed in battle (target attributes, config tables) must be ready before the tick starts; async tasks (DB/GM/cross-server queries) return via the scripting-lua §8 handoff + re-validate — the reply lands as **input of some future tick**, never barges into the middle of this tick's adjudication.
- **The determinism domain of cross-process offload** (No. 36 #16): a battle instance moved to an independent process/machine = same-binary deployment (build artifacts produced uniformly by CI), the determinism domain unchanged; the interaction between the instance and the owner process = event stream + intent stream (InterServerLink), each of the two segments deterministic on its own.

## 2. Floating-Point Discipline

| Constraint | Rule | Why |
|---|---|---|
| Representation | IEEE754 binary32/64; float allowed for key adjudication quantities (hit distance/numeric comparison) | Same binary same platform replays naturally consistent — determinism is guaranteed by the "single judgment domain", not by fixed-point |
| Compile profile | `-ffp-contract=off` (no FMA contraction), `-fno-fast-math` (no reassociation/approximation), SSE2 baseline (x86-64 default, no x87 80-bit path), **no `-march=native`** (build artifacts must be uniform across machines — native artifacts take different vector paths on different CPUs) | FMA/fast-math floating point is the number-one source of same-source drift across compile settings; a native build breaks the "same binary" premise. The toolchain profile goes into the CMake preset (registered in a code batch) |
| No fixed-point/fixed64 | Adjudication does not cross ends; float is already deterministic | Fixed-point is the dev cost lockstep mobile games pay for cross-end consistency/bandwidth savings (trig lookups/precision management); no corresponding benefit here — the concretization of the No. 36 #18 seam semantics |
| Mixed arithmetic | The adjudication path bans implicit double↔float conversion (compiler-warning-level lint) | Implicit truncation-point drift is a debugging hell |
| Lua side | Lua 5.5 number = double/int64 (scripting-lua §2 version lock: the 5.5 mainline follows vcpkg, currently 5.5.x — number semantics unchanged); adjudication scripts **ban math trig/transcendental functions** (implementation differences across libc versions) — angle/distance tools are injected into `apollo.battle` as C++-side fixed-point angles + lookup-table functions; `string.format` does not enter adjudication | libc's sin/exp do not promise bit-identical results; C++ lookup tables are same-binary data |

## 3. Iteration Order: Stable Order Sources

- **The simulation segment requires no order**: each entity's simulation only reads world state and writes itself (the natural corollary of the single-writer discipline) — traversal order does not affect results, containers may be unordered.
- **The settlement segment goes by the target entity_id total order**: when one action settles multiple targets (AOE hit order affects who dies first/trigger chains), effects are collected as a set and **applied in ascending target entity_id order** — settlement sets are small (tens to hundreds in one AOE), sorting cost negligible; the monotonic allocation of entity_id (id_pool) guarantees the total order.
- **Container discipline**: the adjudication path bans unordered-container iteration order from taking part in settlement (iteration order changes after insertions/deletions within the same process too); using unordered containers for broadcast/collection is fine, **the replay hash uses canonical order** (§5), not send order.
- Same source as §2: the iteration-order problem exists only among "multiple objects writing each other within the same tick" — total-order application normalizes any iteration order to the same result.

## 4. Random Numbers: Substream Derivation

```
state = pcg32_init( splitmix64( world_seed, tick, stream_id ) )   // 每 (tick, stream) 独立子流 (one independent substream per (tick, stream))
```

- **Substream model**: no global RNG state, no order-sensitive cursor — any tick any stream independently replayable (no "continuing the stream by call count" — a change in call order is drift, and partial replay becomes impossible).
- **stream_id domains**: `combat_roll` (hit/crit/resist) / `drop` (drops) / `ai_decision` (AI decisions) / `proc` (trigger chains) — systems do not share substreams; one system drawing a few more times does not affect another system's sequence (cross-system coupling is a hidden order dependency).
- **world_seed**: randomly generated once at server creation and persisted (into server metadata, not per battle); the replay anchor = the world_seed at the recording start point.
- **AI recomputation**: AI decisions consume the `ai_decision` substream → at replay the same seed and tick re-derive the same decisions — **the recording surface contains only player input** (the key to §5's size control).
- **Client presentation-stream separation**: adjudication randomness is isolated from presentation randomness — the client takes (adjudicated results + presentation seed derivation) and does its own crit text/particles/shake; presentation randomness drift is harmless (it never enters adjudication).
- **Implementation and prohibitions**: homegrown PCG32 + SplitMix64 (tens of lines, no dependencies, statistically sufficient); banned: `rand()`/`mt19937` global instances (state cursors are not replayable), Lua `math.random` (already stripped by the sandbox; `apollo.rng(stream, n)` is injected instead), and "time as seed".

## 5. Replay Format and Verification

**The recorded items = the four-tuple**:

| Item | Content | Source |
|---|---|---|
| `binary_id` | engine + contract version hash (the schema_hash family) | already exists in build/handshake |
| `world_seed` | server-creation seed | §4 |
| Starting snapshot position | journal position + tick number (clock-and-time §7 dual-write) | attribute-sync §8.2 |
| Input sequence | player upstream intents recorded by (session, seq, tick) | after session-layer L1 deframing — same source as the attribute seq; AI is not recorded (§4 recomputation) |

**Verification = event-stream hash chain**: at each tick end the battle event set is serialized in canonical order (entity_id), rolling `h_t = H(h_{t-1} ‖ event_set_t)`; the chain head advances with the tick, is kept in a memory ring, and is flushed periodically (after a crash the last beat is locatable). The replayer (an offline tool on the apps/ side) re-simulates tick by tick from the snapshot position, recomputes the chain, and compares with the live one — **the divergent tick is the localization point** (bug-reproduction bisection / cheat-reconciliation evidence).

**Two forms kept separate** (a three-orders-of-magnitude cost difference):

- **Spectate/highlight replay = event-stream re-enactment**: the client plays back the recorded event set in order (no re-simulation, no binary_id match needed) — recording the event stream suffices, cheap.
- **Recomputation replay = re-simulation + hash comparison**: anti-cheat reconciliation, bug reproduction, settlement-dispute arbitration — needs same binary + four-tuple, triggered on demand.

**Recording strategy**: ranked/punitive-critical battles keep input recording **always on**; ordinary battles are sampled (the capacity batch §5 sets the sampling rate); movement intent is recorded in full (re-simulation needs movement trajectories) but highly compressed (intents are themselves small, within the <100B/entry net-abstraction budget semantics).

**Client-authoritative verification (cross-end tier, added 2026-09-30)**: under client-authoritative battle (the cashed-in form of the #18 seam), this section's four-tuple/hash chain is consumed by battle-verification-service — the chain is computed on the client and only checkpoint digests go upstream; the verifier recomputes, compares, and exports the **authoritative settlement**; the cross-end determinism increments (operation whitelist/version anchor/drift degradation) and the `binary_id` value-domain difference (cross-end tier = `combat_bundle_hash` + VM version line) are in that draft's §2.2/§8.

## 6. Lua-Side Determinism Discipline (hooking into scripting-lua)

- **Injection replaces globals**: rng (§4) / clock (reading the tick parameter, the clock-and-time §2 mnemonic "adjudication uses only the tick") come through the `apollo.*` module table; the sandbox environment already strips `os.time`/`os.clock`/`math.random` (scripting-lua §4) — a battle script touching them errors at startup.
- **Traversal**: settlement logic bans `pairs` (hash order is not guaranteed and drifts across runs) — use array order/`ipairs`/injected ordered iterators; §3's total-order application has its Lua-side twin (collect into a table → sort by id → apply).
- **Instruction-budget truncation determinism**: the truncation point = the instruction count (deterministic instruction stream → deterministic count → deterministic truncation point) — scripting-lua §6's budget enforcement is replay-compatible, no extra mechanism needed.
- **Hot-replacement boundary**: the script version is locked for the lifetime of a battle instance (hot replacement swaps the table at a tick boundary; the replayer loads the version pointer from recording time — scripting-lua §3.2's version-pointer persistence is exactly the replay's script-version anchor).

## 7. Closure with the No. 36 Decision Table

- **#16 independent battle instances and replay**: the instance-offload form (Matchmaker/Instance Manager skeleton, traceable to docs/25 §3.1-3.2) belongs to the P3+ gameplay batch; **its determinism prerequisite = this doc's four constraints** — a cross-machine instance with the same binary is in the same determinism domain (§1). That No. 36 row's dangling docs/25 reference now points to this doc (a registry annotation, syncing with the next No. 36 batch).
- **#18 frame sync keeps only the seam**: the seam's content made concrete = "server-authoritative replay determinism" (this doc); **lockstep (client participating in adjudication) does not enter the roadmap** — cross-end floating-point consistency + input latency compensation is a problem of another magnitude, unnecessary for an MMO server-authoritative model; "keeping the seam" means that if small-room gameplay is built in the future (a 1v1 arena lockstep-ized), the input-driven model (intent recording + tick replay) is reusable, with bounded adjudication-migration cost.

## 8. Phased Landing

- **P1**: the RNG substream component (modules/base, same family as the timer wheel) + compile-profile floating-point lock (CMake preset) + settlement-order discipline into the code-review red lines; the Lua sandbox prohibitions land with the scripting-lua implementation batch; prerequisite = C-34 ECS convergence.
- **P2**: input recording (session layer, same source as the attribute seq) + the event-stream hash chain (memory ring + periodic flush) + spectate event-stream re-enactment.
- **P3**: the offline replayer (an apps/-side tool, recomputation comparison) + the recomputation sampling strategy wired into the capacity batch metrics (hash-chain divergence rate/recording size) + battle instance offload (No. 36 #16 proper).

## 9. Intersections with Other Designs

| Related design | Placement |
|---|---|
| clock-and-time | the tick number = the replay timeline; "adjudication uses only the tick" is the time face of this doc's judgment-domain discipline |
| attribute-sync | §9 prediction boundary (the client does not adjudicate) = the premise of the determinism-domain contraction; §10 six phases = the host of the adjudication segment; §8.2 journal position = the replay snapshot anchor |
| scripting-lua | §4 sandbox prohibitions (os.time/math.random); §6 instruction-budget truncation determinism; §8 async handoff re-validate (async never enters adjudication); §3.2 version pointer = the replay's script-version anchor |
| net-abstraction | the input recording point = after session-layer L1 deframing (same source as seq); the broadcast segment's non-determinism belongs to net (trim/resend does not affect world state) |
| battle-verification-service | the verification consumer of client-authoritative battle: §5 four-tuple/hash chain cross-end tier + §2 four constraints inherited in full (the three cross-end increments in that draft's §2.2); its M1/M5 interlocks with this doc's P2/P3 (the offline replayer is the same tool family) |
| architecture-review | No. 36 #16/#18 closure and the docs/25 dangling-reference repoint (registry annotation); C-34 ECS convergence as this doc's prerequisite (existing registration); the compile-profile floating-point lock registered as a code-impact item |

---
