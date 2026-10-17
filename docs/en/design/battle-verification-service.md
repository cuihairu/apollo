
# Apollo Battle Verification Service (battle-verification-service)

> Status: design draft (under review; implementation M0-M5 all follow code batches — source-freeze discipline). Positioning: **the anti-cheat reconciliation and authoritative settlement piece for client-authoritative combat** — a deferred gameplay capability with no new target process role; if initiated, it uses RPC interaction: uplink input streams/battle reports (battle-determinism §5 four-tuple + client hash claims), downlink verification and settlement results (verdict + authoritative settlement payload). Language side = **Lua on both ends sharing one copy of the combat logic** (client xLua / server scripting-lua) — same shape as the industry's JS on both ends (Node verification server) / C# on both ends (Unity + .NET); the expansion piece of the concept-glossary "battle verification service" entry. Cross-referenced with `battle-determinism.md` (§2 four constraints fully inherited, §5 recording/chain reuse), `scripting-lua.md` (§6.1 three-level event form ported, §2/§4 sandbox discipline), `attribute-sync.md` (§8.2 :285 pre-settlement snapshot, §9 prediction boundary), `net-abstraction.md` (§7 G-1, §5.7 InterServerLink, §4.3 violation_score), `sdk-contract.md` (§11 internal domain). This remains a deferred gameplay capability; it is not implemented and does not add a target process role.

---

## Executive Summary

1. **The precondition of existence is fixed in writing**: this service exists only for **client-authoritative combat** (the small-instance lockstep cashing in No. 36 slot #18 / client-computed combat). The apollo mainline (intent-only, server-authoritative) **does not need this service** — BW/KBE/skynet have nothing built in for the same reason: all three are state-sync + server-authoritative (§0).
2. **Shape**: independent process `verifier` (fleet component, G-1 machined+UDP two-layer registration, VERIFIER added to the Type segment), InterServerLink + internal domain (id 900+) with three message families: Submit (uplink four-tuple + hash claim, ReliableEvent) / Result (downlink verdict + settlement payload, OneWay) / Query (arbitration control plane, RequestReply).
3. **Compute surface = Lua two-end shadow recomputation**: the same combat logic bundle (anchored by `combat_bundle_hash`) runs on the client's xLua and is re-run in the server verifier's sandbox (one `lua_State` per match); the determinism subset = battle-determinism §2 four constraints **fully inherited** + **three cross-end increments** (operation whitelist / bundle+VM version anchor / residual drift downgraded to WARN — cross-end floating-point bit-exactness is an engineering goal, not a hard promise; three layers of protection keep cross-end micro-drift from hurting real players).
4. **Two tiers of verification strength**: tier one = hash chain final-state comparison (cheap, always on — the client computes the chain locally per the same formula as §5 and uplinks only checkpoints + the final digest; the verifier recomputes and compares against the final state); tier two = per-tick recomputation replay (expensive — dispute arbitration / divergence localization / sampling re-review, reusing the §5 recording strategy: ranked always on, normal sampled).
5. **Settlement authority = recomputation output**: beyond the verdict, the **authoritative settlement payload (win/loss / drops / settlement attribute final state) is exported from the resimulated final state** — client-claimed values serve only for comparison, never as a data source (guards against lying about damage/drops; recomputing the drop substream yields the authoritative drops).
6. **Four verdict outputs × two settlement-gate levels**: PASS / DRIFT (numeric micro-drift → WARN observation, no penalty) / MISMATCH (structural divergence → FAULT + suspended settlement + violation_score) / INCONCLUSIVE (cannot recompute → by responsible party); the hard gate (ranked / with rewards and penalties) suspends until a conclusion, the soft gate (normal gameplay) releases on timeout + post-hoc audit. The three-level event form = a same-form port of scripting-lua §6.1 (independent-service edition).
7. **Deployment**: the worker pool is deployed separately from instance services — asynchronous verification does not block combat (submission happens only after the battle ends; the settlement gate delays only the payout, not the battle); stateless and re-deliverable (worker crash = machined relaunch + queue re-delivery, no G-2 hot-standby requirement); the Compact single-machine form embeds the verifier-kernel library (directly consuming the M1 artifact).
8. **Naming discipline**: the word battle belongs exclusively to the combat logic domain (verification/settlement — concept-glossary §0 ruling 1-②); space/instance naming always uses the instance family, never reused.

## 0. Applicable and Not Applicable (Fixed in Writing)

| Verdict | Scenario | Reason and destination |
|---|---|---|
| **Applicable** | Client-authoritative combat gameplay: lockstep small instances (1v1 arena class, No. 36 slot #18), client-computed combat (casual/card auto-battle class) | Judgment is on the client → the server has no authoritative values to check against; it can only do **same-logic recomputation reconciliation** — this service's sole reason to exist |
| **Not applicable** | Server-authoritative combat (apollo mainline, intent-only) | The server is itself the judge; recomputing itself yields no anti-cheat benefit — attribute-sync §9 prediction/correction already covers presentation-layer drift; BW/KBE/skynet have no built-in verification service for the same reason (all three server-authoritative, deep-dive §18) |
| **Not applicable** | Spectating / highlight replays | battle-determinism §5 event-stream re-enactment (no resimulation, cheap), does not go through this service |
| **Not applicable** | Physical-layer cheating (speed hacks / memory tampering / protocol forging) | net-abstraction §5.9 threat-model boundary ("encryption defends against network third parties, not against the player holding the client"); input plausibility belongs to the intent-validation domain + §4.3 rate limiting; client integrity detection is an anti-cheat client piece, not a server domain. This piece governs only **logic-layer reconciliation**: whether the combat result the client computed matches a faithful recomputation on the same inputs |

**The ruling sentence**: whether a gameplay mode should hook into this service has exactly one criterion — **which end holds the combat judgment**. Server-side judgment (even if the script runs Lua): not needed. Client-side judgment: needed.

## 1. Shape and Topology

### 1.1 Three-Party Data Flow

```text
Client (xLua combat VM, the judge)
  │  four-channel intents (movement/events…) + final-state claim (checkpoint hash set + final hash + end event)
  ▼
Instance (instance process) = input convergence point + recorder + settlement-gate holder
  │  battle final-state/interruption submission: BattleVerifySubmit (four-tuple + claim digest)   ── ReliableEvent
  ▼
verifier (worker pool) = recomputation + authoritative settlement export
  │  BattleVerifyResult (verdict + authoritative settlement payload + comparison report)        ── OneWay
  ▼
instance settlement gate → settlement persistence/leaderboard/rewards (attribute/journal)
```

- **Recording source = instance**: it is already the convergence point of all participants' intents for that gameplay mode (where frames are decoded after Gate passthrough); the input sequence is recorded by (session, seq, tick), same source as the attribute seq (battle-determinism §5 input-sequence row) — **never relayed through the client**; the client has no channel to lie about inputs.
- **Claim source = client**: the hash chain is computed locally on the client (at each tick end, in the entity_id canonical order `h_t = H(h_{t-1} ‖ event_set_t)`, same formula as §5), and only digests go up (one checkpoint per N ticks + one final) — on the order of KB, no per-tick chain upload.
- **Recomputation source = verifier**: resimulates from the starting snapshot position to the final state, recomputing its own chain and settlement state.

### 1.2 Registration and Messages (G-1 + internal domain)

- **Registration**: verifier = G-1 fleet component (net-abstraction §7), VERIFIER added to the ServerID Type segment; machined launches/restarts it; the fleet config declares the worker count. Instance discovery = the existing G-1 two-layer semantics, zero new discovery mechanisms.
- **Message families** (sdk-contract §11 internal domain, id 900+ segment, XSD locks the segment boundary):
  - `BattleVerifySubmit`: instance → verifier. Four-tuple (`combat_bundle_hash`/world_seed/starting snapshot position/input-sequence handle) + claim digest (checkpoint hash array + final hash) + gameplay tier (hard gate/soft gate) + verification mode (tier one/tier two). Large battle reports go through recording-storage handles (journal/recording-file references), not inlined.
  - `BattleVerifyResult`: verifier → instance. Four-value verdict + authoritative settlement payload (win/loss/drops/settlement attribute final state export) + comparison report (divergence tick position, DRIFT statistics). The instance settlement gate **consumes idempotently** (dedup by battle-report id).
  - `BattleVerifyQuery`: GM/arbitration surface → verifier. RequestReply low-frequency control plane (recompute a specified battle report, fetch a comparison report, sampling-parameter queries — the remote-entity-call invoke_mode three-way semantics: OneWay default, RequestReply restricted to the control plane).
- **Zero new domains on the client-side contract**: the final-state claim goes through one message on the client domain's existing events channel (fields follow the contract batch; client-domain field-number compatibility discipline unchanged) — this service's internal domain never intersects the client handshake surface.

## 2. Compute Surface: Lua Two-End Shadow Recomputation

### 2.1 One Bundle, Running on Both Ends

- **Combat logic = one Lua bundle** (combat rule scripts + lookup assets + rule parameters); `combat_bundle_hash` (content hash, schema_hash-family algorithm) is the sole version anchor: delivered to the client through the asset pipeline and declared in the match-opening handshake; the server verifier loads it from the scripting-lua §3.2 version-pointer library. **Hash mismatch between the two ends = match opening refused** (anchoring the version at the source, not rejecting verification after the fact — the precondition for the version inside the four-tuple being recomputable).
- **Server side = one sandbox per match**: one independent `lua_State` per verification (the verifier-side form of the scripting-lua §2 single-writer discipline: one match runs serially inside a worker thread, no shared state); instruction budgets, sandbox bans, and `apollo.*` injection all follow the existing scripting-lua §4/§6 discipline — verifier starts no new scripting facility.
- **VM version line identical on both ends**: the server's Lua 5.5 mainline comes with vcpkg (scripting-lua §2); the bundle manifest declares the target VM version, and the xLua side refuses to load on mismatch. No pre-set ban on xLua's JIT compilation path — drift risk is backstopped by the third protection layer of §2.3 and re-ruled with M4 sampling data.

### 2.2 Determinism Subset: §2 Four Constraints Fully Inherited + Cross-End Increments

The battle-determinism §2 four constraints (unique judgment domain / floating-point discipline / iteration order / random substreams) had the original **server-authoritative-domain** semantics of "same binary same platform = same determinism domain"; this service moves judgment to the client, and the determinism domain goes **cross-end**, so beyond inheritance it adds three increments:

| Increment | Semantics | Variable eliminated |
|---|---|---|
| ① Operation whitelist | Inside the verification domain, double permits only `+ − × ÷`, comparisons, and a bitwise-deterministic math whitelist (floor/ceil/abs/min/max — IEEE754 bit-deterministic); math trigonometric/transcendentals banned (already fixed in §2 — angles/distances go through C++ fixed-point angles + lookup tables injected into `apollo.battle`, **lookup data delivered from the same source as the bundle, both ends using the same table data at the same index**); string↔number round-trips banned, `string.format` does not enter judgment | libc/compiler implementation differences |
| ② Version anchor | Dual anchor of `combat_bundle_hash` + VM version line (§2.1); settlement segment in target entity_id total order, unordered iteration order/`pairs` banned on judgment paths, PCG32 `(world_seed, tick, stream_id)` substreams — verbatim per §3/§4 | Code and data at different versions |
| ③ Drift downgrade | Cross-end floating-point bit-exactness is an **engineering goal, not a hard promise**: residual micro-drift (last-bit differences across CPU/VM implementations) is downgraded by the §3 verdict grading to DRIFT→WARN observation, and the drift rate enters the capacity-batch metric set — **not treated as a fault, no violation_score counted** | A hard promise hurting real players |

### 2.3 Settlement Authority = Recomputation Output

- **The authoritative settlement payload is exported from the verifier's resimulated final state**: win/loss, drops (drop substream recomputed — client-claimed drops have no data-source standing), settlement attribute final state (HP/score etc.). Client-claimed values appear only in the comparison report.
- This differs from "trust the client once reconciliation balances": **the server's recomputation result is the settlement data source**; verification is a byproduct of recomputation (verdict = the comparison conclusion of recomputed state vs claimed state). Logic cheating (changing values/results) has no escape channel here — unless the change is made to agree exactly with a faithful recomputation (which amounts to no change at all).

## 3. Two Tiers of Verification Strength

| Tier | What it does | Cost | Trigger |
|---|---|---|---|
| **Tier one: final-state comparison** (cheap, always on) | The verifier resimulates the whole match, takes the recomputed chain's final hash and compares it against the claimed final hash — match means PASS (no per-tick comparison) | One resimulation per battle (simulation cost ≈ the battle's duration in simulation time) | All submitted battle reports |
| **Tier two: per-tick replay** (expensive, on demand) | Compares checkpoint by checkpoint to localize the divergence tick; hooks into the battle-determinism P3 offline replayer (same tool family) for manual arbitration forensics | Several times tier one + toolchain | MISMATCH localization, dispute arbitration, sampling re-review, DRIFT-rate anomaly investigation |

- **checkpoint**: the client uplinks one chain-value digest per N ticks (N set in the contract batch, on the order of 100); tier-two localization = checkpoint bisection + partial resimulation, converging to a single tick.
- **Recording strategy inherits §5 verbatim**: input recording **always on** for ranked battles / battles with rewards and penalties; normal battles sampled (sampling rate set in the capacity batch §5); movement intents fully recorded with high compression (<100B/entry per the net-abstraction budget semantics).
- Battle-report storage = the existing journal/recording-file carriers (attribute-sync §8.2 positions); Submit carries only the handle — battle reports are not stored twice.

## 4. Verdict Outputs and the Settlement Gate

### 4.1 Four Verdict Values → Three-Level Events (scripting-lua §6.1 same-form port)

| verdict | Criterion | Event level (§6.1 form) | Disposition |
|---|---|---|---|
| PASS | Final hashes match | — | Settlement released; authoritative payload lands in settlement |
| DRIFT | Divergence arrives late and the event sequence is **structurally identical** (numeric micro-drift — the residue of §2.3 increment ③) | WARN (counted as observation, debounced, no disposition) | Settlement released (with the **recomputed values** as authoritative); drift rate enters the metric set |
| MISMATCH | Event sets structurally different / divergence early / inputs do not match a faithful recomputation | FAULT (settlement suspended) | Hard gate suspends into arbitration; **violation_score counted** (net-abstraction §4.3 single bucket, account-level); comparison report kept as evidence |
| INCONCLUSIVE | Recording incomplete / version mismatch / input stream incomplete — **cannot recompute** | WARN (engineering-incident surface, not a player surface) | By responsible party: server-side missing piece = incident channel, not counted against the player; client refusing to submit / partial submission = treated strictly as MISMATCH (a concealed battle report is treated as structural divergence) |

- The verifier-side landing of the three-level events = a §6.1 **same-form port**: single cold-path point (verifier edition of `onVerifyFault` — ERROR bypass logging + MetricRegistry counter; synchronous IO/cross-process calls banned on fault paths); TRIP level = on consecutive MISMATCHes from the same bundle source, that gameplay mode's verification surface **circuit-breaks into degradation** (switch to soft gate + alarm; parameters follow the capacity batch) — §6.1's module circuit breaker translated to "battle-report source" granularity.
- The verifier's own script faults (budget overrun/sandbox violation) go through the native scripting-lua §6.1 three-level disposition (FAULT aborts that match's recomputation → INCONCLUSIVE returned).

### 4.2 Two Settlement-Gate Levels (held by the instance)

| Gate | Gameplay | Behavior |
|---|---|---|
| **Hard gate** | Ranked / with rewards and penalties | Settlement suspended until verdict ∈ {PASS, DRIFT} or a manual arbitration conclusion — **rather delay the payout than release an unverified settlement** |
| **Soft gate** | Normal gameplay | Suspension has a timeout (parameters follow the capacity batch); on timeout release + post-hoc audit mark; a later MISMATCH verdict goes through clawback/risk-control accountability (execution point = GM surface scripting-lua §7.5 + violation_score account-level admission) |

### 4.3 Boundaries with the Attribute Surface / Risk-Control Surface

- **HP snapshot boundary** (attribute-sync §8.2 :285, original text: "critical L3 (e.g. HP before combat settlement) is explicitly snapshotted by the business"): while the settlement gate is suspended, that explicit snapshot freezes into the display state; once the recomputed final state rules, settlement lands. The tolerance boundary of §9 (:319) "short-term presentation-state inconsistency is allowed" closes here — **the server's recomputation is authoritative for state**.
- **violation_score wiring** (net-abstraction §4.3 single bucket): rate-limit counts + schema-validation failures + authoritative-correction counts + **this piece's MISMATCH counts** land in the same bucket — logic cheating and flooding/semantic illegality carry equal weight on the risk-control surface; DRIFT is not counted (§2.3 increment ③, preventing false hits); disconnect/admission dispositions still go through §4.3's three levels and the login-app gate — this piece only supplies numbers, it does not enforce.
- **Arbitration**: MISMATCH battle reports enter the arbitration queue (consumed by the GM surface §7.5 + tier-two replay forensics); arbitration conclusions write back to the settlement gate.

## 5. Deployment and Capacity

- **Independent worker pool**: apps/verifier-app (from M2), deployed as a separate process from instance services — fault-domain isolation holds both ways (an instance death does not affect in-flight verifications; verification lag does not block combat); scale up/down by queue watermark (same metrics source as G-5).
- **Asynchronous, does not block combat**: the whole verification chain sits outside the combat real-time path — submission happens only after the battle ends; the settlement gate delays only the **settlement payout**, not the battle itself.
- **Stateless and re-deliverable**: battle reports live in recording storage/journal; verifier memory holds only in-progress recomputations — worker crash = machined relaunch + queue re-delivery, **no G-2 hot-standby requirement** (the difference from baseapp hot standby, stated explicitly: no authoritative state of its own).
- **Compact embedded form**: the docs/30 single-machine form does not use a separate process — it embeds the M1 verifier-kernel library for same-process synchronous recomputation (resimulation cost is absorbable at single-machine gameplay volume); the message families degenerate into in-process interfaces, and the contract is not duplicated.
- **Capacity semantics**: recomputation throughput = number of workers × (ticks per second simulatable per worker); the budget form merges into capacity-and-benchmark §5 (same source as the recording-replay form); degradation order = queue backlog → automatic sampling-rate reduction → normal gameplay switched to the soft gate; **the hard gate never degrades**.

## 6. Naming Discipline and Boundaries

- **The word battle is exclusive to this domain** (concept-glossary §0 ruling 1-②): `battle-verification`/`BattleVerify*` messages/verdict terminology are used only in the combat verification and settlement domain; space/instance naming always uses the instance family (instance/instance/dungeon aliases per the glossary) — the historical misnomer "Battle" is retired, never to be reused.
- **Industry same-form comparison** (glossary entry semantics): JS on both ends (client JS + Node verification server) / C# on both ends (Unity + .NET, hot-updatable) are the community-common forms; the apollo form = **Lua on both ends** (xLua + scripting-lua) — the cross-end difference point is not the language but the **version anchor** (C# on both ends likewise needs an assembly-hash anchor, same problem; apollo solves it with the dual anchor of `combat_bundle_hash` + VM version line).
- **Not-doing list**: no general anti-cheat platform (this piece = a single logic-layer reconciliation piece); no real-time interception (real-time-ness belongs to intent validation + §9 authoritative correction; this piece is after-the-fact asynchronous); no client integrity detection (anti-cheat client domain); no swallowing of the combat settlement logic itself (settlement rules live in the bundle; verifier only recomputes and exports; settlement persistence belongs to the instance).

## 7. Phased Landing (M0-M5, all with code batches; acceptance criteria first)

| Stage | Content | Acceptance criterion | Prerequisite |
|---|---|---|---|
| **M0** contract freeze | internal-domain three-message field tables + verdict enums + checkpoint/gate parameter tables finalized (sdk-contract §11 XSD; with the contract batch) | XSD passes the gate + field-surface review passes (this draft's §1.2/§4.1 are the field source) | Contract batch window |
| **M1** recomputation kernel | verifier-kernel library (same process): bundle loading + four-tuple-driven resimulation + chain computation | **Dual-run self-proof**: same four-tuple same bundle run twice, hashes fully equal (in-server determinism, no cross-end involvement); sandbox/budget discipline follows the scripting-lua implementation batch | C-34 ECS convergence (same prerequisite as battle-determinism P1); scripting-lua implementation batch |
| **M2** independent service + settlement gate | apps/verifier-app + G-1 VERIFIER-type registration + Submit/Result chain + instance settlement-gate suspend/release | Normal battle reports pass the whole chain: submit → recompute → PASS → settlement persisted; Compact embedded form in the same batch | G-1/InterServerLink implementation batch; M1 |
| **M3** full verdict outputs | Four verdict values + three-level event port + violation_score bucket wiring + arbitration queue | Three forged-report cases (tampered hash/tampered inputs/lying about drops) all go MISMATCH → suspend → score counted | M2 |
| **M4** sampling, capacity, degradation | Sampling rate/queue watermark/degradation order + circuit-breaker parameters landed in the capacity-batch metric set | Mutual acceptance with gap #15 bots: bots simulate cheating clients (forging/delaying/partial submission) and produce full verdict coverage | M3; **#15 bots** |
| **M5** divergence localization | Checkpoint bisection + hooking into the battle-determinism P3 offline replayer | Given battle reports with manually injected divergence, localize to the exact tick | M4; battle-determinism P3 |

- **Interlock with battle-determinism phasing**: this piece's M1 = the recomputation-side consumer of its P2 recording/hash chain; M5 = the online sibling of its P3 offline replayer (same tool family, one implementation used on both ends).

## 8. Intersections with Other Designs

| Related design | Placement |
|---|---|
| battle-determinism | §2 four constraints fully inherited + three cross-end increments (this draft §2.2); §5 four-tuple/hash chain/recording strategy reused — `binary_id` value-domain difference: server-authoritative recomputation = engine+contract hash, client-authoritative verification = `combat_bundle_hash`+VM version line; P2/P3 interlock with this piece's M1/M5 |
| scripting-lua | §2/§4 sandbox and VM version line (verifier reuses, starts nothing new); §6.1 three-level event form ported (§4.1); §3.2 version pointers = the bundle's server-side loading anchor; §8 asynchronous handoff does not apply to this piece (recomputation itself is a full worker task, not an async segment inside a scene thread) |
| attribute-sync | §9 prediction boundary = the precondition that the server-authoritative form does not need this piece; §8.2 :285 pre-settlement L3 explicit snapshot = the settlement gate's freeze surface; §8.2 journal = battle-report storage/starting snapshot position source |
| net-abstraction | §7 G-1 registration (VERIFIER type); §5.7 InterServerLink invoke_mode three-way split (ReliableEvent/OneWay/RequestReply each in its place); §4.3 violation_score single bucket gains a fourth count source |
| sdk-contract | §11 internal domain id 900+ three message families (M0); zero new client-domain additions (final-state claim goes through one message on the existing events channel) |
| concept-glossary | "Battle verification service" entry (this draft is its expansion piece); naming discipline §0 ruling 1-② (battle word exclusive) and ruling 1-① (instance as the fixed name) both executed |
| capacity-and-benchmark | §5 recomputation throughput/degradation order merged in; DRIFT rate/MISMATCH rate/gate timeout rate enter the metric set (M4 lands parameters) |

---
