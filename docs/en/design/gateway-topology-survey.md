# Gateway Responsibilities and Update Boundaries

See the [BigWorld–Apollo mapping](./bigworld-apollo-mapping.md). gateway owns client sockets, forwards persistent business to Base and simulation input to cell. worldServer manages executors/topology rather than forwarding every tick input.

## Gateway responsibilities

Validate credentials, associate sessions, cache separate Base/simulation routes, forward traffic, and reject stale sessions. Multiple gateways accept new players while existing sockets stay with their original gateway. loginServer hosts directories; World publishes committed location projections. Business traffic goes directly to Base/cell.

## Two update schemes

| Scheme | Behavior | Conditions |
|---|---|---|
| Process replacement with protocol changes | Prepare replacement Base, cell, or World; convert/restore state, transfer authority/routes, retire the source | Dual-version decoding, state conversion, compatibility with unchanged services/clients, idempotent commit, recovery. |
| Code-only update | Preserve message IDs, fields, and wire; update Lua at its owning execution boundary, replace processes for C++ | Explicit private-state, coroutine, side-effect, rollback, and state-handoff rules. |

APOLLO_ROUTE_COMPAT handshake allowlists do not provide business compatibility. Breaking client changes require client upgrades or translation. Storage schema evolution needs a separate compatible rollout.

## Executor replacement

1. Register the new cell with World and stop new placement on the old executor.
2. Freeze state at a tick boundary and reconcile or record in-flight work; gateway buffers subsequent input within bounds.
3. Convert/restore the inactive destination. A complete activity includes members, enemies, objectives, clocks, randomness, and necessary script state.
4. World conditionally commits placement/authority; activate from commit evidence, publish projections, refresh gateway routes, and resume ordered input.
5. Clean the source and retire after migration or agreed drain. Forced match termination is not continuous migration.

Profiles remain in original Base/database. Moving only players or recreating a reset Avatar is insufficient. Use stable settlement IDs and reconcile ambiguous outcomes; never blindly replay or reactivate both sides.

## Base replacement

Pause profile writes, reconcile storage, and export persistent objects, sessions, and pending work. Prepare the destination, conditionally commit ownership/write generation, refresh cell/gateway Base caches, fence old writes, and clean the source. Home server/storage affiliation stays unchanged; storage must reject delayed writes from retired processes.

## World management replacement

Transfer Space/Cell topology, executor registrations/generations, capacity/drain, outstanding creation/migration decisions, and commit records. Executors retain or reconfirm their manager association. Pause conflicting placements and fence the old manager so two managers cannot assign the same space. Define whether cell ticks continue under valid execution authority/leases, including control-plane loss deadlines and recovery.

Management state differs from Base profiles and cell simulation snapshots. This separation is unfinished; player transfer alone does not establish manager upgrade support.

## Availability claims

| Outcome | Required evidence |
|---|---|
| Continued new connections | Multiple ingress endpoints, readiness, and stopping old allocations. |
| No repeated login | Restore identity, session, original Base, and simulation location. |
| Continuous client TCP | Retain gateway; replacement requires separate socket handoff, otherwise drain/reconnect. |
| Continuous activity | Complete state, input ordering, exclusive authority, and recovery; waiting for completion is not continuous migration. |

## Implementation status

Compatibility windows, Lua prepare/commit/canary/rollback, drain, and reconnect primitives exist in current WorldApp wiring. Dedicated Base/database, World/cell separation, dual-version decoding, three distinct state handoffs, and cross-gateway recovery need implementation and acceptance through root todo.md.
