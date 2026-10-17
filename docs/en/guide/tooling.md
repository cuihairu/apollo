---
title: Tooling
icon: terminal
order: 7
---

# Tooling

Scripts support current applications and local acceptance without defining additional production process roles.

## Development fleet

`scripts/dev_fleet.sh` uses machined to manage Login, Gateway, and World, with optional logger, admin, and module-assembly examples.

```bash
scripts/dev_fleet.sh up
scripts/dev_fleet.sh status
scripts/dev_fleet.sh roster
scripts/dev_fleet.sh down
```

`APOLLO_BUILD_DIR` selects the build directory; `APOLLO_FLEET_DIR` selects the run directory. Required applications must be built. Missing executables are marked as skipped in the roster.

| Development port | Purpose |
|---|---|
| 9002 | Gateway client TCP |
| 9101 | World gameplay link |
| 9104 | World player-profile link |
| 9103 | Login authentication/admission link |
| 9105 | Login online directory |
| 9102 | Log collection |
| 9106 / 9107 | Metric link / HTTP metrics |

These are script conventions rather than a configuration source of truth. A live fleet does not establish successful login or playable instances. Prototype accounts also require corresponding player profiles.

## Process and update drills

Current application wiring and acceptance for `scripts/drill_kill9.sh`, `drill_migration.sh`, and `drill_update.sh` remain tracked in the root todo.md. Drills must target current Login/Gateway/World applications before serving as acceptance evidence. Primitive tests or existing logs do not replace a complete current path.

Validate profile recovery, exclusive authority transfer, route switching, settlement deduplication, Gateway drain, World instance completion, and rollout rollback separately. Record TCP continuity, no-extra-login recovery, and active-instance continuity as distinct results.

## Crash symbol tools

```bash
scripts/split_symbols.sh <binary>
scripts/build_crash_tools.sh
```

Symbols and release binaries must share a build-id. Processing tools live under `tools/crash-bin/`. Missing tools or symbols must be visible; raw-address traces do not establish symbolization acceptance. See [crash capture](../design/crash-capture.md).

## CI helpers

- `scripts/ci/contract_pack.sh`: contract artifact packaging.
- `scripts/ci/schema_hash_report.sh`: contract hash reporting.
- `scripts/ci/docs_i18n_check.py`: VitePress locale pairs and routing; `--dist <path>` validates generated page languages and descriptions.

Respect workspace dependency-directory restrictions and use an external isolated directory for local documentation builds. Pages use standard locales rather than concatenated translations.
