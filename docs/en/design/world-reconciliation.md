# World Restart Reconciliation Design

> Status: **design draft (under review), not implemented**. This draft only defines the full management journal, the reconciliation protocol extension, and per-item adjudication rules; it touches no runtime code. Implementation slices are in §8; the sole implementation checklist remains the root [`todo.md`](https://github.com/cuihairu/apollo/blob/main/todo.md).
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

- **963 `cell_space_report`** (cell→World, P2P internal, raw-frame admission like 960/962): after a 961 admission the cell actively reports its locally applied ownership view — `request_id` / `cell_id` / `start_epoch` / entry count + N×(`space_id`, `applied_version`, `applied_mgmt_epoch`). Steady state and the restart window share one uplink: after a World restart the cell re-dials (the batch-㉓ face) → 961 admission → the 963 self-report, completing the "pull the allocation list".
- **No new downstream verdict frame**: verdicts land through the existing 962 full-pool authoritative view (match = push as-is; the cell has no basis = a ready=0 revocation marker; expiry = release). Freezes are not delivered via protocol — the cell-side freeze reuses the batch-㉗ loss-of-contact/verification semantics and the 961 rejection (code 3 self-freeze).
- **Management epoch**: 961 already echoes `management_epoch`; the cell records applied_mgmt_epoch and reports it via 963, grounding the "unclear source isolation" verdict (a cell applying an old management epoch's assignment = granted by the old management process; confirm after reconciliation).

## 4. Per-Item Adjudication Table

The journal record (replayed final state) × the cell self-report (963), checked item by item:

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
5. After admission the cell sends the 963 self-report; the World checks item by item per §4, entering the conflict set or keeping.
6. No conflicts (and no missing-record evidence) → the allocation gate opens = recovery complete; with conflicts → only the conflicting Spaces freeze, the rest proceed.
7. Steady state: later registrations/re-dials keep using 961 + 962 (963 is sent once after admission; repeated 963s are admitted idempotently without re-adjudication).

## 6. Decided vs. Open

**Decided at design level** (implementation batches must not deviate): 962 is the verdict carrier (no downstream verdict frame); 963 rides the same raw-frame link as 960 with no separate admission gate; bare v1 lines load compatibly; journal configured off = the current in-memory semantics (the batch-㉚ decision carries over); freezes are per-Space.

**Open** (implementation parameters, decided after review):

- Durability level: flush or fsync for `A`/`B` lines (power-loss semantics of management decisions; the world-cell-management §10 "state the durability level explicitly" item)
- Persistent-volume lock-file policy: advisory (warn only) or mandatory boot refusal
- Whether unfreezing a conflicting Space needs a manual command (options: a manual admin command / automatic after repair, where automatic = trusting the appended journal line as authority)
- The 963 entry-count cap and segmentation (when the Space pool grows past it)

## 7. Acceptance Checklist (for todo ingestion)

- kill World → restart on the same journal → cell re-dials + 963 matches → keep, allocation gate opens;
- version conflict (the journal behind the cell's applied version — e.g. an append-failure degradation window) → the conflicting Space freezes, the rest proceed, recovery after unfreeze;
- a cell reports a Space the journal has no record of → unrecorded authority freezes;
- journal configured + file missing + a cell dials in → full freeze, no allocations; no cell dialing in → admitted as first boot;
- corrupt-line refusal regression; live-lock refusal regression; stale-epoch code 3 regression.

## 8. Implementation Slice Proposal (serial dependencies)

- **Batch A (journal v2 record family)**: A/R/B lines + the write-ahead order + replay-to-final-state reload + the B-line epoch mint. Pure World side, no protocol change.
- **Batch B (963 + adjudication)**: contract 963 (message count 87→88, through the apollo_gen generation chain) + post-admission self-report + the §4 adjudication table + the conflict-set observation face.
- **Batch C (restart e2e + lock)**: kill/restart real-binary smoke + the persistent-volume lock file + the empty-journal determination.
