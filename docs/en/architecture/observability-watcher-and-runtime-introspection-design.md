---
title: Observability, Watcher, and Runtime Introspection Design
icon: binoculars
order: 30
category:
  - Architecture
  - Apollo
tag:
  - Apollo
  - Observability
  - Watcher
  - Introspection
---

# Observability, Watcher, and Runtime Introspection Design

This document settles a question that must be pinned down early as Apollo's overall framework keeps pushing toward a closed runtime-governance loop:

`how exactly the running system should be observed, queried, diagnosed, and explained.`

If this layer is left unclear, what follows can easily become:

- some metrics
- some logs
- some watchers
- and yet another separate scheme for debug queries

In the end you get:

- no unified entry point when something goes wrong
- the docs have layers, but the layers are invisible in the runtime

## 1. Design Goals

This layer of design must solve 6 problems:

1. How runtime state is exposed in structured form.
2. How metrics, logs, traces, watchers, and debug queries are unified.
3. How modules, hosts, business domains, and world instances form a queryable tree.
4. How the local console, remote debugging, and ops observation can share one view.
5. How ordinary MMO mode and BigWorld mode can share one runtime-observation language.
6. How to stop observation capability from staying scattered inside individual modules.

## 2. Reference Sources

### 1. Drawing on KBE's watcher approach

Points adopted:

- a structured runtime observation tree
- querying state by path
- exposing internal runtime information in a unified way

Points not copied:

- the watcher will not be built to serve only one class of process and one class of object

### 2. Drawing on modern observability thinking

Points adopted:

- metrics
- logs
- traces
- runtime introspection

### 3. Drawing on Apollo's new layering

Points adopted:

- `Runtime Ops Host`
- `Platform Foundation`
- `Game Foundation`
- `Domain Components`

Observation should map onto these layers, not form its own system detached from the architecture.

## 3. Why the Design Is Shaped This Way

Apollo has already committed to supporting:

- runtime / ops
- platform
- script
- world
- session
- battle
- distributed world

Without a unified observation tree, these capabilities will only keep scattering further.

The more reasonable arrangement is:

- `Watcher Tree`
  - owns the structured state view
- `Metrics`
  - owns aggregated metrics
- `Tracing`
  - owns causal chains and relationships
- `Runtime Introspection`
  - owns diagnostic queries and online explanation

## 4. Strengths

- a more unified runtime view
- a better fit for the console, remote control, ops, and test integration
- easier to locate cross-layer problems
- easier to map the documented layers onto the runtime

## 5. Costs and Risks

- a unified path-naming convention must be maintained
- an oversized exposure surface brings performance and security pressure
- if every layer hangs watchers arbitrarily, consistency is lost

## 6. Why the Alternatives Were Rejected

### Not choosing "metrics only, no structured watcher"

Because many problems cannot be explained by simple numeric metrics.

### Not choosing "logs only, no introspection"

Because logs cannot replace a query of the runtime state at the current moment.

### Not choosing "every module defines its own debug interface"

Because in the end you lose the unified observation language.

## 7. Recommended Overall Model

It is recommended that Apollo's observation layer adopt:

```text
Observability Layer
    ├── Watcher Tree
    ├── Metrics Export
    ├── Trace Bridge
    ├── Runtime Query
    └── Debug Actions
```

### What each means

- `Watcher Tree`
  - the structured state tree
- `Metrics Export`
  - aggregated metric output
- `Trace Bridge`
  - unified trace intake
- `Runtime Query`
  - online diagnostic queries
- `Debug Actions`
  - constrained debug actions

## 8. Recommended Watcher Tree Structure

It is recommended that watchers be organized at least by the following levels:

```text
/runtime
/ops
/platform
/script
/session
/world
/battle
/distributed
```

### Example paths

- `/runtime/host/state`
- `/ops/health/readiness`
- `/platform/redis/pool`
- `/world/instances/{id}/entities`
- `/session/anchors/{playerId}`
- `/battle/runtime/queues`

### Why paths are mandatory

Because a watcher is not merely "printing a few variables"; it must support:

- query
- navigation
- aggregation
- access control

## 9. Recommended Node Types

Watcher nodes should not come in only one kind.

It is recommended to support at least:

### 1. Value Node

Suits:

- current values
- state strings

### 2. Collection Node

Suits:

- entity lists
- instance lists
- route tables

### 3. Computed Node

Suits:

- current load
- tick budget usage
- AOI hit rate

### 4. Action Node

Suits:

- dump
- reload
- trace toggle

### Principle

`Action Node` must be strictly controlled and must not be open to anyone by default.

## 10. The Relationship Between Metrics and Watchers

These two must not be conflated into one layer.

### Metrics

Suits:

- aggregated numbers
- time series
- alerting

### Watcher

Suits:

- a structured view of the current runtime state
- online diagnosis

### Design conclusion

Apollo should let:

- metrics answer "trends"
- watchers answer "the present state"

## 11. The Relationship Between Tracing and Runtime Introspection

### Tracing

Answers:

- how a call chain got to where it is

### Introspection

Answers:

- what state the system is in right now

The two should complement each other, not replace each other.

## 12. Recommended Runtime Query Model

It is recommended that Apollo explicitly distinguish two classes of queries:

### 1. Passive Query

Read-only queries.

For example:

- query the current entity count of some world instance
- query the current state of some anchor

### 2. Controlled Action

Controlled actions.

For example:

- trigger a dump
- switch trace sampling
- trigger a module reload

### Why they must be separated

Because reads and writes carry different risk in operational governance.

## 13. Differences Between Ordinary MMO and BigWorld

### Ordinary MMO mode

Focus of observation:

- session
- world instance
- anchor
- tick
- replication

### BigWorld mode

Additionally adds:

- partition
- ghost route
- authority transfer
- distributed witness

### Design conclusion

Apollo should let:

- the ordinary MMO watcher tree stabilize first
- BigWorld extend the distributed branch on top of it

## 14. Recommended Object Model

```text
ObservabilityService
    ├── WatcherRegistry
    ├── MetricRegistry
    ├── TraceBridge
    ├── RuntimeQueryService
    ├── DebugActionRegistry
    └── AccessController
```

### `WatcherRegistry`

Responsibilities:

- node registration
- path resolution
- tree-structure maintenance

### `RuntimeQueryService`

Responsibilities:

- the unified query entry point
- shared by the local console and remote queries

### `AccessController`

Responsibilities:

- permission checks
- read-only/action tiering

## 15. Relation to the Testing Strategy

This layer should correspond to:

- watcher path contract tests
- metrics mapping tests
- runtime query assembly tests
- scenario diagnostics tests

Otherwise the observation layer easily becomes:

- beautiful in design
- yet nobody actually uses it at runtime

## 16. Direct Implications for Current Apollo

If Apollo keeps pushing into code next, it is recommended to prioritize adding:

- `WatcherRegistry`
- `RuntimeQueryService`
- `MetricRegistry`
- `TraceBridge`

And to prioritize letting:

- runtime
- ops
- world
- session

these four classes of module attach to the unified watcher tree first.

## 17. Conclusion

Apollo's runtime observation layer cannot be stitched together from logs and metrics alone.

The more reasonable arrangement is:

- use `Watcher Tree` to express structured current state
- use `Metrics` to express trends
- use `Tracing` to express causal chains
- use `Runtime Introspection` to provide the unified diagnostic entry point

Only then does the whole framework truly become governable.

## Related Reading

