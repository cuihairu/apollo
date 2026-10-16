---
title: Tooling Scripts
icon: terminal
order: 7
---

[English](tooling.en.md) | [中文](tooling.md)

# Tooling scripts (scripts/)

An overview of the ops/acceptance scripts bundled with the repository. Design rationale and delivery notes live in each owning design doc (crash capture / backup and failover / capacity benchmark).

## dev fleet: dev_fleet.sh

One-command start/stop wrapping the machined supervision surface (P3-4 batch A). The fleet = the lifecycle half (spawn/restart/reap); directory-registration semantics are not included.

```bash
scripts/dev_fleet.sh up      # 生成 roster 并后台拉起 machined（含编队子进程）
scripts/dev_fleet.sh status  # machined 存活 + 子进程清单
scripts/dev_fleet.sh down    # SIGTERM machined（批 F 关停面收割 + 超时强杀 + 孤儿兜底）
scripts/dev_fleet.sh roster  # 打印本次编队 roster（未构建的 app 标记跳过）
```

- The build tree must have `GAME_MODULE=ON` and all apps built (`APOLLO_BUILD_DIR` adjustable, default `<repo>/build`).
- Runtime directory `APOLLO_FLEET_DIR` (default `/tmp/apollo-fleet-<uid>`: roster + pidfile + logs).
- dev port layout (script convention, no config source of truth): login 9001 / gateway 9002 / baseappmgr 9003 / base-app 9004 / cell 9005 / game (bootstrap demo exits) / logger (scan/follow retrieval surface + serve collection surface, port 9102 inside the fleet — each app's structured file log/*.log aggregates line by line into fleet.log).
- Known boundary: ingress is wired (a004a220, 2026-10-11) — `acceptNewConnections` receives client frames via `TcpClientIngressServer`, and gateway has a real client entry; the "exit code" boundary has been closed out. **Prescription status**: all three steps landed — ② roster passes the three backend URLs (5ccfc651), ① start downgrades connection failure to a warning and returns empty components / ③ ChatApp ghost dependencies removed at four sites (d7f21831); the standalone smoke test without backends empirically showed all three backend degradation warnings and survival to timeout (the original `exit=1` no longer reproduces).
- Live walkthrough (2026-10-10): up → status → down full chain re-verified — born×6, game bootstrap demo exit=0, gateway exit=1 backoff×5 give-up, consistent with the batch A smoke baseline; **the walkthrough fixed one defect**: down's orphan fallback was `pgrep -P <dead pid>` (once machined dies its children re-parent to init, always empty = dead code), and login-app's slow shutdown (~3-5s) could leave orphans behind after "fleet stopped" — replaced with precise cleanup by roster binary name via `pgrep -x`/`pkill -x` (shared 5s grace, then KILL the leftovers); re-verified that when down returns there are zero orphans.

## kill -9 full-chain drill: drill_kill9.sh

G-2 backup/failover batch 0 acceptance chain (backup-revive §6): kill -9 the base-app inside the fleet and measure the RTO of each segment — "death on the wire → backoff restart → journal replay → recovery phase ready → full re-report convergence → directory mirror convergence".

```bash
scripts/drill_kill9.sh       # 全自动：起编队 → kill -9 → 断言收敛 → 打印 RTO 表 → 收割
```

- Independent port range (19003/19004/19600-19602); coexists with dev_fleet without conflict.
- Runtime directory `APOLLO_DRILL_DIR` (default `/tmp/apollo-drill-kill9-<pid>`, fleet.log archived).
- The session surface is a `--demo-anchors 3` smoke seed; the journal replay segment is the empty-session lower-bound baseline; client resume lives at the directory level.

## Crash symbol splitting: split_symbols.sh

Deployment-side symbol management (crash-capture §2.3). Three steps per binary: `objcopy --only-keep-debug` produces `.debug` → `strip --strip-debug` produces the release artifact → `objcopy --add-gnu-debuglink` links them back by the same build-id.

```bash
scripts/split_symbols.sh <binary> [<binary> ...]
```

- Linux-only (deployment-target baseline); missing binutils is reported explicitly.
- `.debug` goes into the ops symbol store; the restoration pipeline (tools in the next section) back-fills symbolication by build-id.

## Crash tools on-demand build: build_crash_tools.sh

Builds the Breakpad tooling surface (`dump_syms` + `minidump_stackwalk`) — client capture uses Crashpad while processing uses the Breakpad tools (a minidump-family pairing); only the tools are taken, zero Breakpad client introduced.

```bash
scripts/build_crash_tools.sh                 # shallow clone breakpad + lss → configure → make
tools/crash-bin/dump_syms  <bin> > x.sym     # ELF+DWARF → 符号文件
tools/crash-bin/minidump_stackwalk --symbol-path <symdir> <dump>   # minidump → 符号化栈
```

- Artifacts go to `tools/crash-bin/` (.gitignore); `BREAKPAD_SRC` can override the source/build directory.
- **Not in the three-tree gate**: a one-shot acceptance/ops item; network loss or build failure reports an explicit error and does not exit zero — no silence, no fallback.
- Symbol builds require `APOLLO_ENABLE_DEBUG_SYMBOLS=ON` (Release adds `-g`; the GCC side pins `-gdwarf-4` — under DWARF5 dump_syms silently drops functions/line numbers, crash-capture §2.3 batch note).

## CI helpers (scripts/ci/)

- `contract_pack.sh`: contract packaging (SDK distribution surface).
- `schema_hash_report.sh`: contract schema hash report (client_hash dual-domain discipline check).

## Related docs

- [Crash capture](/design/crash-capture) — the full chain of the symbol surface and acceptance
- [Backup, failover and crash takeover](/design/backup-revive) — the batch 0 drill chain and RTO numbers
- [Capacity and benchmark](/design/capacity-and-benchmark) — metric baselines
- Test suite: repository `tests/README.md` — gate baseline and conventions for adding tests
