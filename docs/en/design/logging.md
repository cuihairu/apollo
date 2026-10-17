# Runtime Logs and Failure Evidence

Current logging belongs to `modules/core/log`. Applications configure console output, local files, and optional push through the shared manager. This page separates existing paths, deployment rules, and unverified failure guarantees without dated migration records.

## Executive summary

Structured logs, centralized collection, metrics, sampled message tracing, and crash reports serve different purposes. Collection is not a simulation dependency. Returning from a logging call does not automatically establish durability across power loss.

## 1. Failure boundaries

| Scenario | Required verification |
|---|---|
| Normal operation | Searchable process identity, severity, category, and message within an output budget |
| Collector unavailable | Local evidence remains available; push buffering stays bounded |
| Local file failure | Visible errors without unbounded retries or caching |
| Process crash | Evidence through the independent capture path |
| Restart | Correlated logs, dumps, and supervisor events; active gameplay recovery is separate |

## 2. Ownership

core/log owns APIs and file output. Applications set process identity and outputs. logger collects and queries; admin owns metric aggregation and management egress. Business processes do not send HTTP directly to external alert receivers.

## 3. Local logging

### 3.1 Severity and durability

Interfaces provide severity filtering, appenders, and flush. Current logging/file paths include locks; they must not be described as entirely lock-free, formatting-free, or guaranteed never to lose high-severity messages.

ERROR bypass, FATAL fsync, disk-full fallback, and logging-thread failure need specific implementation and fault-injection evidence. Severity labels alone do not establish persistence guarantees.

### 3.2 Threads and budgets

Avoid frequent string construction and large synchronous output from simulation. Push clients use their own bounded queues and pumps instead of blocking simulation on collection. Overflow, retries, and drops need observable outcomes.

### 3.3 Lifecycle

Initialize logging before runtime facilities that may log. Preserve it until business and dependency shutdown complete. Applications still contain explicit startup/shutdown code that requires individual failure-path validation.

## 4. Crash evidence

### 4.1 Independent capture

runtime integrates Crashpad without relying on ordinary logging locks or workers surviving the fault. See [crash capture](./crash-capture.md).

### 4.2 Signal boundaries

Signal contexts must not log through locking APIs, join threads, or run complex business shutdown. Hand signals to normal execution threads. Current entry points still need signal-wiring review; a signalHandler function does not establish async-signal safety.

### 4.3 Runtime evidence

Correlate process identity, incarnation, build, and time across logs, death events, and dumps. OS core-dump configuration and symbol storage are deployment work, not results established by compiler flags.

## 5. Collection and metrics

logger provides scan/follow queries and serve collection. `APOLLO_LOG_PUSH_ADDR` enables M1 log push; absent configuration leaves this output disabled. Collection failure does not alter player authority or business outcomes.

### 5.1 Format and external consumers

Structured lines have stable ordered keys: `ts`, `level`, `proc`, `tick`, `cat`, `msg`, followed by extensions. Escape whitespace, quotes, backslashes, and control characters to preserve line boundaries.

`APOLLO_METRICS_PUSH_ADDR` enables metric snapshot push; admin exposes aggregation and an HTTP metrics endpoint. Available endpoints do not establish a deployed Prometheus/Grafana stack. The trace subset carries message-boundary identity and sampling without claiming a complete external tracing deployment.

Logs provide event evidence, metrics provide trends, and introspection provides current state. Player profiles and settlement use persistence rather than treating collected logs as data authority.

### 5.2 Alerting

admin notifier/webhook primitives require configuration, destination restrictions, rates, and failure policy. Business processes do not wait for notification delivery. Integrate external observation systems according to deployment needs; templates and dependencies do not establish a live service.

## 6. Current boundary

Existing facilities include application structured-file configuration, logger queries/collection, M1 push, metric output, sampling, and crash primitives. Process recovery, active-instance recovery, lossless logging, and external observation deployment require separate acceptance.

## 7. Acceptance

Validate file output, escaping, queries, collector disconnection, and buffer occupancy before logging-facility faults and process crashes. Measure output latency against simulation budgets and bound retry memory.

## 8. Related designs

- [Crash capture](./crash-capture.md)
- [Network boundaries](./net-abstraction.md)
- [Lua fault isolation](./scripting-lua.md)
