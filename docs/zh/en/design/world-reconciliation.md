# World Restart Reconciliation Design

> Status: **Batches A and B implemented** (Batch A: §2 record family A/R/B lines + write-ahead order + replay-to-final-state reload + the B-line epoch mint, plus the pending-owner expiry window on reload — the §4 "absent in the expiry window" row, see root todo batch ㊴; Batch B: contract 967 `cell_space_report` + post-admission self-report + the §4 per-item adjudication + the conflict set with per-Space freeze, see root todo batch ㊵); Batch C (restart e2e + lock) not implemented. The implementation checklist remains the root [`todo.md`](https://github.com/cuihairu/apollo/blob/main/todo.md).
>
> Parent document: [World and cell Management](./world-cell-management.md) §4 (loss of contact) and §6 (management records and World restart). This draft refines §6's "restore the same records, raise the management epoch, then pull allocation lists from cells and reconcile item by item" down to record formats, contracts, and per-item verdicts.

## 1. Scope and Landed Preconditions

**Landed** (batches ㉒–㉛, see the todo P2 ledger): the 960/961 registration faces with epoch adjudication; periodic heartbeats plus `--cell-expiry-ms` expiry removal; the 962 full-pool authoritative ownership view (expiry release zeroes the owner with the version kept, re-claim advances the version as an ABA guard, ready=0 revocation markers with occupied-space retry); the cell-side `--link-grace-ms` grace freeze; the World-side epoch watermark plus the `--world-journal` persistent tombstone (batch ㉚: append-only `cell_id|start_epoch`, corrupt lines refuse boot, append failure degrades to the in-memory watermark with a warning); and the disconnect suspect mark (batch ㉛: M1 on_down sets `CellRegistration::suspect`, cleared by a verified 961 re-admission).

**Gaps this design fills**: world-cell-management §4.5's "query and reconcile first, never immediately rebuild the same authoritative simulation elsewhere" — the query face; and §6's "pull allocation lists and reconcile item by item", "activities with missing records, version conflicts, or unclear source isolation stay frozen", and "when management records are unavailable, refuse new allocations; never start with an empty journal substituting for old records".

**Out of scope**: cross-machine hot standby, automatic failover, multiple Cells per Space, and the authentication credential face (tracked separately in the gap table).

## 2. Full Management Journal (journal v2)

The current v1 (batch ㉚) persists only epoch tombstones. v2 brings allocations and boot epochs into the same append-only text journal, one record type per line, type-prefixed:

| Line | Format | Semantics |
|---|---|---|
| T (prefix optional) | `T cell_id\|start_epoch`, or the bare v1 `cell_id\|start_epoch` | Epoch tombstone (semantics unchanged; reload takes the per-cell maximum) |
| A | `A space_id\|cell_id\|version` | Claim / re-claim (one line per transfer where the version advances monotonically) |
| R | `R space_id` | Expiry release (owner→0, version kept; replay takes the last A/R line per space as the final state) |
| B | `B management_epoch` | World boot epoch line (one per boot; reload takes the maximum, +1 mints the new epoch) |

- **Write order (write-ahead)**: the `A` line must append+flush successfully before the 962 push — the durable decision precedes the outward claim. The current call order (`recordEpochWatermark` before `pushSpaceAssignments`) is the v1 shape of this discipline; v2 brings allocation records into the same order. The `B` line is written before the listen face opens (mint the epoch before admitting).
- **Reload**: strict line-by-line parsing; a corrupt line refuses boot (the v1 discipline, no silent partial replay). Replayed final state = the last A/R per space + the max T per cell + the max B.
- **Empty-value semantics**: bare v1 lines parse as T (existing files upgrade in place, no migration).
- **Grounding the empty-journal substitution ban**: when the journal is configured but the file is missing, first boot cannot be distinguished from lost records. The rule — **a cell re-dials in (proving an old management relationship existed) while the journal holds no T line for that cell = evidence of missing records: all of that cell's Spaces freeze for reconciliation and no new allocations open**; a missing file with no cell dialing in is admitted as first boot (no external evidence contradicts it). Cell re-dial is the already-landed batch-㉓ face; no new mechanism is needed.
- **Compaction**: none in the first version (append-only, bounded by allocation churn); snapshot + rename (Windows deletes-then-renames per the known pitfall) waits for a compaction batch.

## 3. Protocol Extension (through the contract generation chain, not hand-edited wire)

- **967 `cell_space_report`** (cell→World, P2P internal, raw-frame admission like 960/962; the design's reserved id 963 was taken by the retire management pair 963/966, so the message takes a new id): after a 961 admission the cell reports its locally applied ownership view one frame per Space — `request_id` / `cell_id` / `start_epoch` / `space_id` / `applied_version` / `applied_mgmt_epoch` (the generation chain has no repeating groups, so one frame per entry dissolves the entry-count cap / segmentation question). Steady state and the restart window share one uplink: after a World restart the cell re-dials (the batch-㉓ face) → 961 admission → the 967 self-report, completing the "pull the allocation list".
- **No new downstream verdict frame**: verdicts land through the existing 962 full-pool authoritative view (match = push as-is; the cell has no basis = a ready=0 revocation marker; expiry = release). Freezes are not delivered via protocol — the cell-side freeze reuses the batch-㉗ loss-of-contact/verification semantics and the 961 rejection (code 3 self-freeze).
- **Management epoch**: 961 already echoes `management_epoch`; the cell records applied_mgmt_epoch and reports it via 967, grounding the "unclear source isolation" verdict (a cell applying an old management epoch's assignment = granted by the old management process; confirm after reconciliation).

## 4. Per-Item Adjudication Table

The journal record (replayed final state — from Batch B on, the authoritative in-memory ownership table) × the cell self-report (967), checked item by item:

| Journal | Cell self-report | Verdict | Landing face |
|---|---|---|---|
| S owner C v N | C reports S v N, same mgmt | **Match, keep** | 962 pushed as-is |
| S owner C v N | C reports S v M≠N | **Version conflict: S freezes pending reconciliation** | World conflict set (observation face) + that cell's Space frozen; no new allocations, no release |
| S owner C v N | C does not report S | Journal authoritative: the cell dropped it → the existing revoke/expiry path | 962 ready=0 or the batch-㉘ release/re-claim |
| No record of S | C reports S v M | **Unrecorded authority: S freezes (source isolation unclear)** | Same conflict set; resolved only after the journal gains an A line (manual / later batch) |
| S owner C | C never returns within the expiry window | The existing release/re-claim | batch-㉘ face (version +1 as the ABA guard) |
| — | C walks in with an epoch below the watermark | code 3 rejection + cell self-freeze | batch-㉗/㉚ face |

Freeze granularity = **a single Space**, not the whole cell (minimal isolation: the same cell's other Spaces keep admitting and assigning normally). A conflicting Space unfreezes once reconciliation repairs the record — the World appends the journal line and re-pushes 962; whether a manual command is required is an open decision in §6.

## 5. World Restart Sequence

1. Persistent-volume lock check (a live lock refuses boot; see §6).
2. Journal reload (corrupt lines refuse boot — the landed v1 discipline).
3. Mint the new management epoch = max B + 1 and write the `B` line (record before opening the admission face).
4. Listen opens; cells re-dial (batch ㉓) → 961 epoch adjudication (the watermark blocks stale executors, batches ㉗/㉚).
5. After admission the cell sends the 967 self-report; the World checks item by item per §4, entering the conflict set or keeping.
6. No conflicts (and no missing-record evidence) → the allocation gate opens = recovery complete; with conflicts → only the conflicting Spaces freeze, the rest proceed.
7. Steady state: later registrations/re-dials keep using 961 + 962 (the 967 self-report re-sends on every 961 admission; repeated 967s are admitted idempotently without re-adjudication).

## 6. Decided vs. Open

**Decided at design level** (implementation batches must not deviate): 962 is the verdict carrier (no downstream verdict frame); 967 rides the same raw-frame link as 960 with no separate admission gate; bare v1 lines load compatibly; journal configured off = the current in-memory semantics (the batch-㉚ decision carries over); freezes are per-Space.

**Open** (implementation parameters, decided after review):

- Durability level: flush or fsync for `A`/`B` lines (power-loss semantics of management decisions; the world-cell-management §10 "state the durability level explicitly" item)
- Persistent-volume lock-file policy: advisory (warn only) or mandatory boot refusal
- Whether unfreezing a conflicting Space needs a manual command (options: a manual admin command / automatic after repair, where automatic = trusting the appended journal line as authority)

(Resolved: the former "963 entry-count cap and segmentation" item — one frame per entry under 967 dissolves it.)

## 7. Acceptance Checklist (for todo ingestion)

- kill World → restart on the same journal → cell re-dials + 967 matches → keep, allocation gate opens;
- version conflict (the journal behind the cell's applied version — e.g. an append-failure degradation window) → the conflicting Space freezes, the rest proceed, recovery after unfreeze;
- a cell reports a Space the journal has no record of → unrecorded authority freezes;
- journal configured + file missing + a cell dials in → full freeze, no allocations; no cell dialing in → admitted as first boot;
- corrupt-line refusal regression; live-lock refusal regression; stale-epoch code 3 regression.

## 8. Implementation Slice Proposal (serial dependencies)

- **Batch A (journal v2 record family)** — implemented (batch ㊴): A/R/B lines + the write-ahead order + replay-to-final-state reload + the B-line epoch mint + the pending-owner expiry window. Pure World side, no protocol change. Implementation decisions: A/R lines for non-pool spaces are skipped at load (counted in a warning); a failed A-line append rolls back the in-memory claim and withholds the 962 declaration (space stays pooled for retry); R/B line failures warn and degrade (batch-㉚ availability-first); released entries are still replayed (reclaim's version +1 depends on the historical version).
- **Batch B (967 + adjudication)** — implemented (batch ㊵): contract 967 `cell_space_report` (one frame per entry) + post-admission self-report + the §4 adjudication table + the conflict-set observation face + the freeze face (`pushSpaceAssignments`/`releaseCellSpaces` skip conflicting Spaces — no claim, no refresh, no revocation marker, no release). Implementation decisions: the self-report re-sends on every 961 admission (idempotent adjudication, doubling as the steady-state retry face); reports from unregistered sources / stale incarnations / reserved keys are dropped silently (the counter does not advance); reports on released spaces (owner=0) or spaces owned elsewhere = convergence lag, not conflict; any space with no authoritative record (out-of-pool, or in-pool never claimed) freezes as unrecorded authority; absent reports carry no negative verdict (missing reports ride the existing 962 revoke/release path); the client `CellRegisterClient` bookkeeping upgraded to a per-Space applied view (version + management epoch + owner + ready) and self-reports automatically inside the admission reply.
- **Batch C (restart e2e + lock)**: kill/restart real-binary smoke + the persistent-volume lock file + the empty-journal determination.
