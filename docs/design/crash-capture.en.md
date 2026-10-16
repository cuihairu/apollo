[English](crash-capture.en.md) | [中文](crash-capture.md)

# Crash-Capture Design (crash-capture)

> Status: **delivered (batches ②③④, 2026-10-09 network-recovery continuation; batch ① design piece 2026-10-08)**. **Selection pinned (user decision 2026-10-08): Google Crashpad, direct C++ integration** — Breakpad is kept as historical comparison only; its client is not introduced. Positioning: the process-level crash-capture plane — minidump to disk, symbolization, upload placeholder (off by default). Gap registration: this piece is the crash-capture batch's design surface (todo P3-3 batch note). Cross-references: logging.md (observation three-prohibitions / outbound allowlist discipline), net-abstraction §5.10 (outbound target allowlist — the upload face follows the same discipline), capacity-and-benchmark §5 (a second "ops evidence" plane beside the benchmark facilities), improvement-plan P3-3 (observability: crash reports = post-hoc observation plane).

---

## Executive Summary

1. **Why Crashpad (Breakpad as historical comparison)**: the core difference is the **capture-process model** — Crashpad uses a **separate handler process** (out-of-process): at the instant the target process crashes, the signal handler Crashpad injected does only minimal work (carrying register/thread context), and the entire minidump is written to disk by the living handler process; Breakpad writes the minidump directly inside an in-process signal handler — under heap corruption / stack exhaustion / a second crash inside the handler (secondary fault), the self-capture face is unreliable, which is exactly the Breakpad→Crashpad evolution motive. Supporting pieces: Crashpad ships CrashReportDatabase (report state machine New/Pending/Completed) and Settings (persistent upload switch); Breakpad has no counterpart. Maintenance: the Breakpad client's last release was 2022-02; Crashpad is active (Chrome's primary collector). **Toolchain reuse of the Breakpad side is established fact**: the minidump format is the same family (Microsoft MDMP), Crashpad itself carries no CLI stackwalker, and dump_syms/minidump_stackwalk (Breakpad tools) are the de-facto processing end (Sentry ships the same pairing: crashpad client + breakpad processor) — this design's tool face takes the same road, with zero Breakpad on the client face.
2. **Integration via the vcpkg manifest** (staying within the repo's vcpkg discipline; no FetchContent/submodule third road): **overlay port 2026-07-02 + override** — the repo's baseline `8f7641a3` pins crashpad 2024-04-11#9, which lacks adaptation to this machine's GCC 15.2; the 2026-07-02 port carries the GNU fixes (`mini_chromium_is_clan=false` corrected to `mini_chromium_is_clang=false` + `extra_cflags="-Wno-error"`) and a mini_chromium toolchain patch. **Local compile verification was to be re-verified once the network recovered** (2026-10-08 external-network outage: the vcpkg-tool-gn binary download source chrome-infra-packages.appspot.com TLS fully down — the port files, GN build hosting, and artifact list were all checked against local vcpkg port-tree reads; compile verification was completed with the batch ② wiring). The overlay port is vendored into the repo (`ports/vcpkg-overlays/crashpad/`, 6 files: portfile.cmake / vcpkg.json / crashpadConfig.cmake.in / fix-missing-stdint.patch / mini-chromium-toolchain.patch / zlib.gn); the GN build is closed inside the vcpkg tooling (vcpkg-tool-gn hosted), and the CMake side only sees `find_package(crashpad CONFIG)` → the `crashpad::crashpad` interface target — **no depot_tools, no touching GN**.
3. **Initialization point = the earliest section of each app's main()** (after argument parsing, before server.start()), calling `runtime::init_crash_capture(argc, argv, proc_name)` — a crash can happen in any section after it (including logging/config init), so the capture face must stand first. The helper scans `--crash-*` arguments itself (app argument loops zero-change); init failure **degrades without blocking** (returns false + warn log, the process starts as usual — a capture-face failure must not become a service-face failure; fail-open).
4. **Dump location**: `crashdumps/<proc>/` (cwd-relative default; the fleet's per-process subdirectories isolate naturally — machined roster launches children that inherit cwd, so under the fleet directory this is `crashdumps/base-app/`); `--crash-dump-dir` overrides. Database form = Crashpad SimpleStringDatabase (settings.dat + reports/ inside the db directory).
5. **Handler packaging**: at build time `find_file` locates the vcpkg tools artifact `crashpad_handler`, POST_BUILD-copied next to each app executable (**artifacts carry the handler**); at run time `--crash-handler-path` overrides. The deployment face needs no extra steps — the handler is distributed in the same directory as the app.
6. **Symbol-table management**: the `APOLLO_ENABLE_DEBUG_SYMBOLS` option (Release adds `-g`, default off — the symbol face does not affect behavior; three-tree size/link time untouched); deployment-side separation `scripts/split_symbols.sh` (objcopy `--only-keep-debug` produces `.debug` + `strip --strip-debug` produces the release artifact + build-id association; .debug backfilled on demand); the restoration pipeline = `dump_syms` (ELF+DWARF → .sym) → `minidump_stackwalk --symbol-path` (minidump → symbolized stack). Tools built on demand (`scripts/build_crash_tools.sh` clones google/breakpad + configure+make, artifacts into `tools/crash-bin/`) — **not into the three-tree gates** (acceptance/ops one-shot pieces; gate time-budget discipline).
7. **Upload placeholder (default off)**: handler initialization **configures no URL** — local mode, reports stay Pending, no outbound send. Enabling upload is **data exfiltration** (same-family discipline as the logging three-prohibitions / outbound allowlist), off by default; the future switch-on = handler `--url` + `Settings::SetUploadsEnabled(true)`, two points; the interface is a placeholder, unimplemented, with no reporting service introduced.
8. **Integration with existing logging/monitoring**: init logs one line (enable/db path/handler path/upload off); at startup the db is swept for a pending count → log + MetricRegistry counter (post-hoc observation plane — the crashed process is dead; the living process can only see the previous run's leftovers; the sweep is "leftover visibility"); machined's supervision-plane death log (exit 128+signo convention, already in Batch F) and the dump on disk corroborate each other, with no linked blocking.

## 1. Selection Comparison and vcpkg Status

### 1.1 Breakpad historical comparison (selection pinned; this section only archives the reasoning)

| Dimension | Crashpad (chosen) | Breakpad (comparison) |
|---|---|---|
| Capture model | separate handler process — after the target crashes it only carries context; the dump is written by a living process | in-process signal handler writes the dump directly — unreliable under secondary faults (heap corruption / stack exhaustion / re-crash inside the handler) |
| Report management | CrashReportDatabase (New/Pending/Completed state machine) + Settings (persistent upload switch) | none (the file is the report, disk self-managed) |
| Maintenance status | active (Chrome's primary collector, chromium.googlesource) | client unmaintained (last release 2022-02; the toolchain dump_syms/minidump_stackwalk remain the de-facto standard and stay in use) |
| Dependencies | zlib (linux additionally needs curl — sender-side dependency; with upload off, the dependency can only be explicitly linked) | self-contained |

### 1.2 vcpkg status and the overlay decision

- baseline `8f7641a3` (vcpkg.json `builtin-baseline`) pins crashpad **2024-04-11#9** — older than GCC 15 (released 2025-04), missing GNU adaptation; the GN build is expected red on this machine's GCC 15.2; **no baseline bump** (bumping the baseline re-resolves the versions of openssl/protobuf/gtest and all other dependencies — uncontrollable blast radius), so the route is **overlay port + override**: the repo vendors the same-named port from the vcpkg 2026-07-27 tool release into `ports/vcpkg-overlays/crashpad/` (version-date **2026-07-02**, upstream crashpad @ `efdc820b`), vcpkg.json gains a `"crashpad"` dependency + overrides pinning 2026-07-02 + vcpkg-configuration.json declaring overlay-ports. Overlay's files: portfile.cmake / vcpkg.json / crashpadConfig.cmake.in / fix-missing-stdint.patch / mini-chromium-toolchain.patch / zlib.gn.
- Transitive dependencies recorded faithfully: on linux the crashpad port depends on **curl** (sender-side link face) — same direction as the net-abstraction §5.10 "curl into vcpkg" ruling, conveniently delivered on the dependency face; the vcpkg port also pulls vcpkg-tool-gn/vcpkg-gn (GN build hosting, host dependency).
- **Artifact list (port semantics; local-install empirical evidence completed with batch ②)**: `${VCPKG_INSTALLED_DIR}/<triplet>/lib/{vcpkg_crashpad_client,vcpkg_crashpad_client_common,vcpkg_crashpad_util,vcpkg_crashpad_base}.a` + `include/crashpad/**` + the tools directory `crashpad_handler` executable; CMake consumption `find_package(crashpad CONFIG REQUIRED)` → `crashpad::crashpad` (interface target, with ZLIB::ZLIB transitive linking).
- Gate impact: each of the three trees' configures gains one crashpad build (increment near zero after binary-cache reuse); find_package failure (non-vcpkg / port-less environment) → crash-capture no-op stub (`APOLLO_HAS_CRASHPAD=0`), the three trees stay green as usual — the capture face is an enhancement, not a dependency.

## 2. Integration Design

### 2.1 Initialization point and sequencing

```text
main(argc, argv)
  └─ runtime::init_crash_capture(argc, argv, "<proc>")     ← earliest section (before server.start())
       ├─ self-scan argv: --crash-dump-dir / --crash-handler-path (app argument loops zero-change)
       ├─ defaults: db=crashdumps/<proc>/ (cwd-relative), handler=crashpad_handler beside the executable
       ├─ CrashpadClient::StartHandler(handler, db, metrics="", url="",     ← url empty = local mode
       │                              arguments={}, database_config, ...)   ← attachments placeholder
       ├─ success → APOLLO_LOG info one line + APOLLO_HAS_CRASHPAD=1
       └─ failure → APOLLO_LOG warn one line + return false — the process starts as usual (fail-open)
  └─ … (logging/config/server init — any crash after this point is captured)
```

- **Why earliest**: what the capture face protects is exactly "everything after it" — logging/config/server initialization are themselves potential crash points; init_crash_capture itself depends only on arguments and the filesystem, not on the logging face (when logging is unavailable it falls back to one stderr line).
- **Semantic boundary**: init returns as soon as `StartHandler` succeeds — the handler process is spawned/managed by crashpad itself (the app neither forks nor waits); the attachments interface is a placeholder, unimplemented.
- **Platform semantics**: deployment target is Linux (same as net-abstraction §5.8's server Linux-only); the port supports expression includes win/osx — on non-Linux platforms, if find_package succeeds the same API is available, on failure it is the no-op stub — no platform-branch code is written.

### 2.2 Dump directory and handler packaging

- Directory: `crashdumps/<proc>/` (proc name passed explicitly by the helper, consistent with process identity); `--crash-dump-dir` overrides the whole directory (dual use: fleet and single-process debugging). machined roster children inherit cwd (dev_fleet up empirically confirmed), so under the fleet directory each app's subdirectory isolates naturally, with no shared-directory contention.
- Handler location, three tiers: ① explicit `--crash-handler-path`; ② `crashpad_handler` beside the executable directory (guaranteed by POST_BUILD copy — artifacts carry it); ③ the vcpkg tools path (find_file cached at build time, compiled into the default). All three tiers missing → init fails → fail-open (capture off, service as usual).
- Packaging discipline: crashpad_handler distributed in the same directory as the app is the complete deployment face; version consistency comes from the same build output (the port version is pinned in the overlay — handler and client library share one source, no cross-version mismatch surface).

### 2.3 Symbol-table management

- **build keeps -g**: `APOLLO_ENABLE_DEBUG_SYMBOLS` (an option shared by the ON/OFF/Examples three trees, default OFF). When on, Release appends `-g -gdwarf-4` (wired after option definition and before target creation — add_compile_options in directory scope only affects later targets; the first version mistakenly placed it in the legacy dead block, effectively a no-op, fixed in the symbolization-actual-run batch; optimization tiers untouched); when off, CMake does not strip by default and the ELF symtab still holds function names — dump_syms produces function-level symbols (no line numbers), usable for both acceptance and production. **-gdwarf-4 pin (empirically proven 2026-10-09)**: under GCC 15.2's default DWARF5, breakpad dump_syms silently drops FUNC/LINE records for CUs in trees with crashpad/heavily-templated headers (542 vs 1010 FUNC records, zero line numbers module-wide), while DWARF4 parses fully (1010 FUNC + 74674 LINE) — the symbolized-build tier is pinned with it; the DWARF5 degenerate shape waits for a breakpad-side fix before re-evaluation.
- **Separation (deployment face)**: `scripts/split_symbols.sh <binary>` — `objcopy --only-keep-debug` produces `<binary>.debug`, `strip --strip-debug` produces the release artifact in place, `objcopy --add-gnu-debuglink` associates them (build-id from the same source); `.debug` goes into the symbol store (ops face), the release artifact carries no symbols.
- **Restoration pipeline**: `dump_syms <binary> > symbols/<name>/<build-id>/<name>.sym` (Breakpad directory convention) → `minidump_stackwalk --symbol-path symbols/ <dump>` → frame-level symbolized stack (function/file/line, depending on the -g tier).
- **Tool build**: `scripts/build_crash_tools.sh` — shallow clone google/breakpad + `./configure && make`, artifacts `dump_syms`/`minidump_stackwalk` collected into `tools/crash-bin/` (.gitignore); **not into the three-tree gates** (acceptance/ops one-shot piece; zero introduction of the breakpad client face, tools only). Offline/build failure → the acceptance face reports the lack explicitly, no silence.

### 2.4 Upload placeholder (default off, data-exfiltration discipline)

- Empty URL at handler init = local mode: reports stay **Pending** after being written to the db; Crashpad sends nothing out (Settings::SetUploadsEnabled defaults to false as a second safeguard).
- Upload is data exfiltration: a minidump contains a process memory snapshot (user-data face) — logging.md's observation three-prohibitions and net-abstraction §5.10's outbound allowlist same-family discipline apply; switching on = handler `--url` (the reporting endpoint) + `Settings::SetUploadsEnabled(true)` (two places, both on the decision/approval plane) + the allowlisted target into config. **This batch does not implement the outbound face**; the interface is a placeholder (the StartHandler parameter shape is already in place).

### 2.5 Integration with logging/monitoring

- **init line**: crash-capture enable/disable, db directory, handler path, upload off — the process identity enters the log face (logging §5.1 structured line format, cat=crash).
- **Startup pending sweep**: `CrashReportDatabase::Initialize(db)` → `GetPendingReports()` count → log (cat=crash, pending=N) + MetricRegistry counter (`crash_reports_pending`, the P3-3 Batch B minimal-set consumption face) — a crash is a post-hoc event; the living process's observation face = "the previous run's leftovers made visible"; the sweep is read-only and does not change report states (state transitions belong to the upload/cleanup faces, untouched this batch).
- **machined cross-corroboration**: the supervision plane's death log (Died event exit=128+signo, Batch F semantics) and the dump on disk are two faces of the same event — the crashed process's dump in `crashdumps/<proc>/`, the death row in the machined log, with the ops comparison table recorded in the §3 acceptance chain. No linked blocking (capture failure does not cut the service — fail-open consistency).

## 3. Acceptance chain (fulfilled this batch, Linux local)

1. Symbolized build: ON-tree configure `-DAPOLLO_ENABLE_DEBUG_SYMBOLS=ON` (acceptance-face only, not gate-always-on).
2. Deliberate crash: `base-app --crash-test null` (null-pointer sample switch, same shape on the smoke-driver face as the --demo-anchors precedent — to be dropped with a note once the real use case is verified) → SIGSEGV → crashpad_handler writes the dump to `crashdumps/base-app/`.
3. Tools ready: `scripts/build_crash_tools.sh` → `tools/crash-bin/{dump_syms,minidump_stackwalk}`.
4. Symbolization: `dump_syms base-app → .sym` (build-id associated) → `minidump_stackwalk --symbol-path` → stack contains the crash-test function and call frames.
5. Leftover visibility: restart base-app → init line + pending=N log + MetricRegistry counter.
6. machined cross-check: same crash under dev_fleet → machined Died row (exit=139=128+SIGSEGV) and the dump on disk corroborate both faces.

## 4. Batching

- **Batch ① (2026-10-08)**: ① design piece + local overlay vendoring in advance; ②③④ paused due to the external-network outage (vcpkg-tool-gn binary download unreachable).
- **Batches ②③④ (2026-10-09, network-recovery continuation; details in the todo P3-3 batch note)**: ② vcpkg wiring (manifest dependency + overrides + overlay-ports declaration; install full-build empirical evidence) + the runtime helper (`init_crash_capture`: argv self-scan / default directory / three-tier handler location / local mode / fail-open stub) + all-app wiring (seven apps' main earliest section) + handler packaging (POST_BUILD copy + chmod — empirical fix for vcpkg tools artifacts lacking the execute bit; crashpad_FOUND / imported-target directory-scope pitfalls, function determination via find_file CACHE); ③ the symbol face (APOLLO_ENABLE_DEBUG_SYMBOLS option + split_symbols.sh + build_crash_tools.sh [not into the gates]) + the --crash-test null acceptance switch + the startup pending sweep (log + MetricRegistry counter). **Acceptance chain actually tested**: init line → deliberate crash exit=139 → dump into pending/ → restart pending=1, full chain working. **Symbolization actual run (same-day continuation)**: build_crash_tools.sh built successfully (dump_syms/minidump_stackwalk into tools/crash-bin/) → dump_syms produced .sym → minidump_stackwalk stack backtrace `crash_test`[crash_capture.cpp:163, exact line] / `main`[main.cpp:73] — build-id three-way corroboration (ELF note ≡ minidump CV record ≡ sym CODE_ID); incidentally fixed the option-wiring no-op bug (the dead-block placement never took effect — exposed by the actual run; wiring moved up + the gdwarf-4 pin, see §2.3). §3 acceptance chain 1-5 all passed; **step 6 delivered (2026-10-09, machined cross-check actual run)** — machined --backoff 300 fleet-launched a resident crasher (base-app --crash-test null): born → child death exit=139 ×6 → restart attempt 1-5 → give up after 5; crashpad's per-boot pending sweep visibly increments across machined restarts (pending=1→2→3→4, logged only when non-zero) + 6 dumps into `crashdumps/zone-app/pending/` — the supervision plane's death row and the dumps on disk corroborate both faces + the sweep counter locks in three ways; §3 acceptance chain 1-6 all passed.
- **Follow-ups (not this batch)**: upload-face switch-on (data-exfiltration approval + allowlisted endpoints, on the decision plane); dump retention policy / cleanup face (a Pending on-disk ceiling — small magnitude, observe first); Windows CI adaptation (the port supports expression covers it; apollo on Windows is only a dev-machine shim, not a target); attachments streams.

## 5. Intersections with the Other Designs

| Related design | Landing |
|---|---|
| logging | init/pending lines follow the structured line format (cat=crash); the three-prohibitions discipline is one basis for upload default-off |
| net-abstraction §5.10 | outbound allowlist discipline — the target constraint when the upload face opens; the curl dependency shares the direction |
| capacity-and-benchmark | the crash face of the "evidence first for every item" discipline is same-shaped: dump + symbolized stack = post-hoc evidence, in the same "ops evidence" family as benchmark data |
| improvement-plan P3-3 | the observability batch's crash-face increment; MetricRegistry counter consumption face (Batch B minimal set) |
| architecture-review §15.2 | dependency discipline: the vcpkg manifest as the single dependency face (overlay is a vcpkg-internal mechanism, not a second dependency system); no depot_tools/GN on the user face |
| scripts/dev_fleet.sh | the cwd/log-directory coexistence face of fleet smoke and crash capture (crashdumps/ beside the fleet logs) |

---

*Baseline: apollo main @ a63855ac. Local environment: GCC 15.2.0 / CMake 4.2.3 / vcpkg tooling 2026-07-27 (compile empirical evidence completed 2026-10-09 with batch ② — crashpad:x64-linux@2026-07-02 full build passed, artifacts: four static libraries + crashpad_handler + header tree); crashpad overlay port version-date 2026-07-02 (upstream chromium.googlesource crashpad @ efdc820b, port vendored from vcpkg 2026-07-27 tool release). The Breakpad toolchain is built on demand (google/breakpad master); artifacts stay out of the gates. Upload/exfiltration default off — same family as the data-exfiltration discipline (logging three-prohibitions / outbound allowlist); switching on belongs to the decision/approval plane.*
