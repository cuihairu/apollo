---
title: Tooling
icon: terminal
order: 7
---

# Tooling Scripts

Tools must wire the target roles gateway, loginServer, baseServer, worldServer, cell, database, and machined rather than define another production topology.

## Development fleet

scripts/dev_fleet.sh entries, configuration, and ownership need migration with Base/database and World/cell separation. Starting existing processes does not establish the target-role topology. A local profile endpoint inside World is not the target Base/database deployment. See [applications](../apps/index.md) and root todo.md.

machined supervises local processes only. World may start independently and register with loginServer's service directory. Cross-server admission and Base lookup do not require local machined. Distinguish process liveness, service readiness, admission, and business availability.

## Process and update drills

Existing drill_kill9.sh, drill_migration.sh, drill_update.sh, drill_proto_update.sh, and drill_module_update.sh retain their actual primitive coverage. Revalidate durable Base/database confirmation, scene transfer retaining Base, complete Instance replacement, independent Base rollout, and cross-gateway recovery under the target-role topology.

Protocol drills must exercise real old/new business messages and state conversion. Handshake allowlists do not validate payload compatibility. Restart or waiting for matches to finish does not establish active-match migration. Record availability, no repeated login, client TCP, and gameplay continuity separately.

## Crash symbol tools

```bash
scripts/split_symbols.sh <binary>
scripts/build_crash_tools.sh
```

Keep symbols and binaries on the same build-id; tools live in tools/crash-bin. Validate capture, symbolization, profile recovery, and match recovery separately. See [crash capture](../design/crash-capture.md).

## Performance and telemetry status

Self-built logging, MetricRegistry, and span/trace primitives exist. No dedicated Profiler, OpenTelemetry SDK, OTLP exporter, or complete external telemetry pipeline is integrated. External-tool guidance does not establish built-in engine functionality; integration remains requirement-driven.

## CI helpers

- scripts/ci/contract_pack.sh packages contract artifacts.
- scripts/ci/schema_hash_report.sh reports contract hashes.
- scripts/ci/docs_i18n_check.py checks native VitePress locale pairs, routes, and generated HTML language/descriptions.

Build documentation in an isolated directory outside the repository, respecting workspace dependency restrictions. Chinese pages live at docs root and English pages under docs/en, each with its own language content.
