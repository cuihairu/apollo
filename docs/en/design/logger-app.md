
# LoggerApp Initiation Prerequisite Design (the landed form of the process shell and structured-line wiring)

> Status: **landed (2026-10-10, L1+L2+L3 three batches delivered the same day; L4 hung on M1)**. Positioning: the landed-form expansion of ADR-013 "LoggerApp initiation" — pinning the evidence of the apps wiring status quo, pinning the v1 collection-form three-way choice, pinning the batch split and decision points. The genre follows net-abstraction §7 "P3 prerequisite design"/battle-instance-offload/room-server-single-writer. Terminology is governed by this piece (the log-collection process logger / the code identifier `LoggerApp`); the authoritative semantics = ADR-013 (initiation split: shell first + switch on + push leaves an interface) + logging.md §5 (the collector-form parent piece). Batch notes are in the ADR-013 landing annotations (adr.md).

---

## 1. Status Declaration (Grade-A direct read, at the 2026-10-10 initiation point)

> **EXPIRED**: this section pins the actual state at the initiation point. After L1+L2 landed, the seven apps'
> output surfaces have fully switched to logger (console for humans + local structured files in parallel);
> the apps wiring status quo = full; the line numbers no longer correspond to the current source.
> The initiation-point evidence is kept, not deleted (the decision chain stays traceable);
> the current state is governed by the ADR-013 batch notes.

**At the initiation point: the module face was delivered (P3-3 batch C), apps wiring = zero.**

- The structured-line face is in the library: `FileAppenderConfig::structuredOutput` defaults to false (file_appender.h:46), the write-path three-way branch (file_appender.cpp:79-80 `structuredOutput ? to_structured_line(record) : format(record)`), `to_structured_line` (structured.cpp:116), the six-key fixed order + quoting-rule contract sourced from the structured.h header comment; the `LogManagerConfig::processIdentity` injection point (log_manager.h:46); the test faces test_log_structured / test_core_log / test_log are in the library. FileAppender rolling (ByDate/BySize/ByBoth + restart sequence-number continuation + maxFiles cleanup) is delivered.
- **All seven apps' output surfaces write directly via `std::cout`** — machined (main.cpp:60-71 born/death/restart lines), baseappmgr (:28-99), base-app (:22-65), room-server (:14-53), gateway-app (:16-61), login-app (login_server.cpp:364-431), game-server (main.cpp:123-126). The only log-module touchpoint = game-server main.cpp:73 `global_log_manager()` (the crash-backlog visibility line cat=crash pending=N, out via console).
- Inference one: **the file outlet does not currently exist** (fileEnabled defaults to false and nobody turns it on); the `log/<app>-<instance>.log` layout is logging.md §5's design semantics, not reality.
- Inference two: ADR-013's "each app's FileAppender structuredOutput switch turned on" actually contains two layers — **wire in the log module first (L1), then flip the switch (L2)**. The todo P3-3 batch C boundary "apps wiring not done" refers to L1; this piece pins this batch split to prevent the initiation batch from merging the two layers into one sentence and missing L1.
- Collection face empty: the seven apps/ processes have no collector; machined's supervision face (supervisor.cpp:121-124 backoff parameters / :204-205 restart adjudication) is the roster integration point; dev_fleet fleet_entries has six members (dev_fleet.sh:36-43).
- The authoritative semantics already exist: logging.md §5 collector form (apps/logger + push via net-abstraction §5.7 InterServerLink + the failure semantics "collection never becomes a runtime dependency" + §4 file layout); §5.1 three prohibitions (no direct Kafka connection / no OTel SDK / no per-process HTTP ports) + the external-tail division of labor; §7 P3-row full face (push channel/disconnect degradation/BI split/core archiving/alert notifier). The ADR-013 split: **shell first + structuredOutput on + push leaves an interface, unimplemented (the collector face after M1)**.

## 2. Target Form (authoritative semantics)

The v1 end state: the logger process joins the fleet; the first step of each app's dual outlet = the local structured file (the source of truth, console for humans in parallel); logger is a read-only aggregation and search (filtered by process/level/time/category); the push channel keeps a seam (the interface slot unimplemented), switching to the InterServerLink base after M1 — the literal expansion of ADR-013's three sentences.

The failure semantics are naturally satisfied in v1: under the tail form, logger dying = the apps' files keep writing, zero dependency zero impact — the trivial solution of logging.md §5's "collection never becomes a runtime dependency".

## 3. The Three-Way Choice Expanded (the v1 collection form)

### (a) tail pull — logger reads the structured lines of `log/*.log`

- **Semantics**: the search face is built on top of the file source of truth; structured lines (six keys) reduce filtering to key-value matching; after the push seam is kept and the base is swapped at M1, the search face does not move.
- **Cost**: real-time-ness is bounded by the tail polling interval (acceptable for search-type loads; alert-type loads belong to §5.2 layer B via metrics, not via logs); the files must exist first — the L1/L2 wiring batches are its prerequisite.
- **Relation to existing semantics**: logging.md §5.1's "local files as co-source-of-truth" is isomorphic with the external tail — logger v1 is simply the in-house tail.

### (b) push direct-connect v1 — each app's logger thread pushes straight to the collector

Needs InterServerLink (net-abstraction §5.7, M1 landed on 2026-10-11; the in-service transport = the link M1 base) — **violates ADR-013's "push leaves an interface, unimplemented", no**.

### (c) UDP push (same shape as the G-1 beacon)

Lossy transport vs the audit/forensic semantics of logs (the crash-capture forensic chain and the script-error audit of scripting-lua §6 both require no dropped lines) — **no**.

## 4. Honest Comparison

| Dimension | (a) tail pull | (b) push direct-connect | (c) UDP push |
|---|---|---|---|
| New transport dependency | zero | InterServerLink (M1) | a UDP face (newly written) |
| Dropped-line risk | none (files are the authority) | none (bounded queue + disconnect degradation) | **yes** (conflicts with audit semantics) |
| Real-time-ness | polling-interval grade | push grade | push grade |
| M1 coupling | none (the base can be swapped later) | **strong coupling, violates ADR-013** | none |
| Relation to the three prohibitions | fits (zero new ports) | fits (the inter-process domain) | fits (the inter-process domain) |
| Failure semantics | trivially satisfied | needs a disconnect-degradation face | needs to tolerate dropped lines |
| Search-face reuse | L4 swaps the base without moving it | the search face is the same shape | same shape |

## 5. Decision Points (the settled semantics ruled with ADR-013 on 2026-10-10)

1. **The v1 collection form = (a) tail pull** — (b) push direct-connect violates ADR-013's "push leaves an interface, unimplemented" (this batch's authoritative split), (c) UDP push is lossy and conflicts with the audit/forensic semantics (the §3/§4 double rejection already ruled); once L1+L2's file source of truth settled, (a) is the only viable branch.
2. **The wiring batch split = L1+L2 merged into one batch** — the initiation batch's instruction is literally a merge (apps wiring in the log module + the structuredOutput switch turned on, delivered together); no behavioral-face regression occurred (the kill -9 drill's full-chain assertions green, the message-body substring contracts all preserved).
3. **The search-face v1 form = (a) logger binary subcommands** — `logger scan` (one-shot search) + `logger follow` (follow-and-aggregate, the fleet roster line consumption face); logger_tests 9 groups enter the gate; the (b)/(c) rejection reasons stand as originally recorded.
4. **The archiving/rolling duty = v1 logger is read-only** — rolling stays with FileAppender's existing behavior (ByDate/BySize/ByBoth + maxFiles + restart sequence-number continuation), logger does not take over.
5. **The directory/binary name = apps/logger + binary logger + class `LoggerApp`** — the code identifier lands on the class name and does not change with the directory name.

## 6. Post-Decision Path

```text
Batch L1+L2  ✅ delivered (2026-10-10, 7ad64938): the seven apps' mains configured with LogManagerConfig
          (fileEnabled + processIdentity + structuredOutput, console kept for humans);
          acceptance all green = three-tree gate + seven-app log-file-on-disk assertions + structured-line parseable assertions
          + kill -9 drill assertion regression (message-body substring contracts all preserved)
Batch L3    ✅ delivered (2026-10-10): the apps/logger process + binary logger + class
          LoggerApp; read-side parsing (a mirror of structured.cpp) + scan (proc/level/ts/cat)
          + follow (fleet aggregation) + the push seam IPushSink/NullPushSink (unimplemented);
          acceptance all green = logger_tests 9 groups enter the gate + the dev_fleet roster logger line
          + fleet smoke (7 members, 103 aggregated lines, scan parses all 12 files)
Batch L4    ✅ delivered (2026-10-12, 8d8a1dc0→d8499672): push channel + disconnect degradation
          in full form — kernel passive fan-out + contract 930 logger_line_push +
          LogPushClient[bounded queue 4096 drop-new at cap, kReliable reconnect-exactly-once redelivery,
          stop-drain keeps the queue] + PushAppender/LogManagerConfig.pushSink + logger serve
          [passive "apps" receives 930 raw lines into the archive, flush per line] + the seven apps'
          APOLLO_LOG_PUSH_ADDR one-line wiring (unset = off, v1 unchanged) + dev_fleet
          serve-ized; disconnect-degradation live drill[collector+gateway killed together → machined
          respawns the collector → redelivery fully archived]; the BI split (attribute-sync §8.2) and the
          alert notifier (§5.2 layer B) remain later batches
```

## 7. Intersections

| Piece | Relation |
|---|---|
| logging.md §5/§5.1/§7 | the parent piece: the collector form, the three prohibitions, the P3 full face (this piece is the landed expansion of its ADR-013 split) |
| ADR-013 | the initiation-split source: shell first + switch on + push leaves an interface |
| net-abstraction §5.7 | the L4 push base (InterServerLink, M1) |
| crash-capture | the cat=crash line = the first consumption precedent of structured search; the crashdumps pending count made visible |
| G-1 machined | the roster integration point; death reporting in the same-batch semantics (logging.md §7 P3 row) |
| scripting-lua §6 / attribute-sync §8.2 | the BI bypass converges at the collector (after L4) |
| existing naming | the `LoggerApp` glossary entry (the naming source of truth) |

## 8. Evidence List

- Grade A (2026-10-10 direct read of local source): modules/core/log/include/apollo/core/log/file_appender.h:46, modules/core/log/src/file_appender.cpp:79-80, modules/core/log/src/structured.cpp:116, modules/core/log/include/apollo/core/log/log_manager.h:46, apps/machined/src/main.cpp:60-71, apps/baseappmgr/src/main.cpp:28-99, apps/base-app/src/main.cpp:22-65, apps/room-server/src/main.cpp:14-53, apps/gateway-app/src/main.cpp:16-61, apps/login-app/src/login_server.cpp:364-431, apps/game-server/src/main.cpp:73/:123-126, apps/machined/src/supervisor.cpp:121-124/:204-205, scripts/dev_fleet.sh:36-43, tests/test_log_structured.cpp
- Grade B (this repo's docs): docs/design/logging.md §5/§5.1/§7, docs/architecture/adr.md ADR-013, root todo.md P3-3 (batch C boundary "apps wiring not done")
