# Apollo migration checklist

The target is a lightweight room-based multiplayer server engine for tower defense, co-op, arenas, and dungeons. It does not target a persistent seamless world. `Login`, `Gateway`, `RoomServer`, and `Room` are the canonical terms. One Room is one match; one RoomServer may host multiple Rooms; a player has one authoritative gameplay object in a Room at a time.

Two update modes are documented. Protocol updates regenerate contract artifacts and require old/new compatibility during rollout. Code-only updates keep the wire contract unchanged; Lua has runtime replacement primitives, while C++ changes use process replacement after Rooms drain or transfer. Production rollout, connection handoff, and uninterrupted active-Room updates remain unproven.

## P0 — Documentation baseline

- [x] Remove retired architecture archives and duplicated task ledgers that introduced competing process names. Keep BigWorld/KBEngine terms only in clearly labeled external-engine references.
- [x] Define the target process roles, Room boundary, single-authority player object, and Gateway update limits in the architecture entry points.
- [x] Document protocol-update and code-only update modes and distinguish existing runtime primitives from incomplete production rollout.
- [x] Move English pages to the VitePress locale directory `docs/en/`; use `locales` configuration and the built-in locale switcher.
- [x] Remove dead Markdown links with a static check across 467 pages.
- [ ] Run the VitePress build and fix any build-time issues. The repository's `.gitignore` explicitly forbids operations in `node_modules`, so this check must wait until it can run without touching that directory.

## P1 — Runtime naming

- [x] Rename `apps/cell-app` to `apps/room-server`; update the CMake target, classes, namespace, includes, CLI flags, Gateway configuration, fleet script, logs, metrics, and related tests. Preserve existing `cell_*` wire message names and IDs.
- [x] Review `base-app` and `baseappmgr`; map session, player-data, and routing responsibilities to `Login`, `Gateway`, and `Room` without adding standalone target processes. The prototype code migration remains tracked below.
- [x] Route the current Gateway M1 link/configuration to `room-server`; remove user-facing claims that Cell/Space processes are part of Apollo target topology.
- [ ] Confirm the RoomServer ownership boundary in code; lifecycle implementation is tracked in P2.

## P2 — Room runtime

- [ ] Add Room creation, admission, multi-Room hosting, settlement, and destruction to RoomServer.
- [ ] Enforce one authoritative player object through admission, reconnect, and transfer paths.
- [ ] Complete and verify the client → Gateway → RoomServer → Room message path.
- [ ] Implement cross-RoomServer authority transfer with idempotency, route update, failure recovery, and tests.

## P3 — Update and availability behavior

- [ ] Add protocol-version negotiation and an N/N-1 compatibility window; enforce generated-artifact and compatibility checks in CI.
- [ ] Complete Lua patch distribution and rollout control across RoomServer instances, including canary health checks and rollback.
- [ ] Implement graceful Gateway drain and validate how existing sockets behave through the drain deadline.
- [ ] Define reconnect/session recovery for clients that reconnect through another Gateway.
- [ ] Define RoomServer drain behavior for completion, migration, timeout, and forced shutdown.
- [ ] Validate protocol and code-only rolling releases separately; do not claim seamless active-Room updates without end-to-end evidence.

## P4 — Project surface and verification

- [ ] Align README, examples, scripts, and deployment guidance with implemented behavior and distinguish target design from prototype status.
- [ ] Run relevant build and test suites for each migration slice; preserve the pre-existing untracked `tests/test_timer_main.cpp` and unrelated `.gitignore` changes.
- [x] Update and verify the GitHub About description and topics against the corrected documentation.
