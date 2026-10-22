# Crash Capture

Apollo uses Crashpad for process-level minidumps. Capture and supervised recovery are separate: a dump does not recover active gameplay, and a machined restart does not establish restored player authority or routing.

## Executive summary

Application entry points call `apollo::runtime::init_crash_capture`; an independent `crashpad_handler` writes reports. The default is local storage without upload. Initialize logging first so capture does not trigger an unintended default logging configuration.

## 1. Dependencies and build

### 1.1 Capture and processing

Use Crashpad for capture. Breakpad's `dump_syms` and `minidump_stackwalk` can process symbols and reports without introducing another capture client.

### 1.2 Current configuration

The vcpkg manifest, overrides, and overlay ports manage dependencies. `modules/runtime/CMakeLists.txt` detects Crashpad and sets `APOLLO_HAS_CRASHPAD`. Without an available SDK the implementation is a no-op; inspect initialization logs instead of treating successful compilation as enabled capture.

## 2. Integration boundaries

### 2.1 Initialization and lifecycle

Login, Gateway, World, and other application entry points initialize capture. Initialization failure is logged without preventing business startup. The handler runs separately; capture does not take ownership of gameplay or player recovery.

### 2.2 Reports and handler

The default directory is `crashdumps/<process>/`, using the application's actual identity. Override it with `--crash-dump-dir`; specify the handler using `--crash-handler-path`.

Handler lookup checks the explicit path, executable directory, and build-time cached path. CMake's `apollo_package_crash_handler` packages the tool beside applications. Deploy the matching architecture with executable permissions.

### 2.3 Symbols

`APOLLO_ENABLE_DEBUG_SYMBOLS=ON` enables symbol builds. `scripts/split_symbols.sh <binary>` separates `.debug` and associates the release executable. Symbols and binary must share a build-id.

`build_crash_tools.sh` builds processing tools under `tools/crash-bin/`. Generate symbols with `dump_syms`, then use `minidump_stackwalk` to recover functions, files, and lines. Missing tools or mismatched symbols must be visible; raw addresses do not establish completed symbolization acceptance.

### 2.4 Upload

Empty upload URLs and disabled uploading retain local mode. Destination policy, access control, and retention need deployment configuration. Existing API parameters do not establish a working upload path.

### 2.5 Logs and metrics

Initialization reports capture state, directory, and handler. Startup inspects pending reports and publishes logs plus `crash_reports_pending`, without changing report state.

Supervisor death logs and dumps are separate evidence for one failure. Correlate process identity, incarnation, and time. Death detection, capture, and automatic restart cannot individually establish recovery.

## 3. Acceptance procedure

1. Use the current executable and a symbol-enabled build in an isolated run directory; confirm handler startup.
2. Trigger a controlled failure through a dedicated test driver and verify abnormal exit plus report output. The library `crash_test` function does not mean every application exposes a matching CLI.
3. Generate matching symbols and inspect the recovered stack.
4. Restart and verify pending-report visibility.
5. If supervised, correlate death and restart results; separately validate profiles, routing, and active-instance failure/recovery policy.

Records from removed processes are not acceptance evidence for current applications.

## 4. Current limits

Local capture, handler packaging, symbol tools, and startup visibility primitives exist. Upload, cleanup quotas, retention policy, and platform runtime verification follow deployment needs. Windows compilation alone does not validate its capture pipeline.

## 5. Related designs

- [Logging](./logging.md): structured failure evidence.
- [Networking](./net-abstraction.md): outbound destinations and asynchronous boundaries.
- [Capacity and benchmarks](./capacity-and-benchmark.md): runtime evidence and metric semantics.
