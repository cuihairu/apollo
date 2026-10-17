# Log Collector

logger is an observability tool. It does not own player, Scene, Instance, or routing authority. See ADR-013 in the [current architecture decisions](../architecture/adr.md).

## Current functionality

Applications write local structured logs. APOLLO_LOG_PUSH_ADDR enables existing M1 log push to a collector. logger provides scan/follow queries and a serve collection entry point, with process, severity, time, and category filters.

Collector failure uses bounded buffering and degradation, retaining local logs without blocking simulation. World uses the runtime/log identity world and executable world-app.

## Implementation

- apps/logger: collection and query entry points.
- modules/net/logpush: application log push.
- modules/core/log: structured records and file output.
- tests/test_logger_retrieval.cpp and tests/test_log_push.cpp: existing verification entry points.

Kafka, ClickHouse, and Prometheus/Grafana infrastructure remain requirement-triggered work in the root todo.md. See [logging design](./logging.md) for storage, query, and failure rules.
