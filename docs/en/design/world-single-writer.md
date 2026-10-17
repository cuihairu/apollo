
# World Simulation Thread and Write Boundary

This document defines single-writer ownership for match simulation and separates the target model from the current gameplay prototype. Target terminology is defined in the [concept glossary](./concept-glossary.md).

## Target Rule

One logical thread applies authoritative mutations for each Instance in order. Network IO, administrative requests, and background workers submit intents with correlation IDs; the World validates and applies them at a simulation boundary, then returns results. This gives each match a clear mutation order and prevents concurrent writes to its Entity, Scene, or AOI state.

One World may schedule multiple Instances on one simulation thread or use multiple simulation threads that do not share mutable state. Thread allocation is an implementation and capacity decision; it does not change the single-authority rule for an Instance.

## Current Code

The existing `world-app` prototype uses `WorldApp::gameLoop` as the `World` write thread. The loop pumps the M1 link, whose callbacks enqueue intents into a bounded `acceptanceQueue_`; it consumes intents at a fixed point, runs Lua integration, and calls `WorldHost::tick`. After stopping acceptance, the loop performs a final queue drain before destroying the script host. The source class name remains part of the implementation mapping; this process has the target responsibility of a World.

The prototype currently creates one default `Scene` and one `Instance` at startup. The underlying `World` has an API for creating more scenes, and Instance already supplies an eight-state lifecycle. Group-triggered creation, allocation, objective settlement, and resource cleanup are not integrated end to end. Existing `Scene` and `Instance` objects should not be described as a complete World implementation.

## Overload and Shutdown

When the queue is full, the server rejects new intents and reports overload; it does not silently discard accepted work. Shutdown closes acceptance, lets the simulation thread perform its final drain, then stops the link and host. Production behavior still needs a defined drain deadline, failure replies for unfinished requests, and recovery behavior after forced termination; track acceptance criteria in the root [`todo.md`](https://github.com/cuihairu/apollo/blob/main/todo.md).

## Concurrency Acceptance

- Network pumping and background jobs never mutate Instance simulation state directly.
- Each Instance has a reproducible input order, and duplicate requests cannot repeat non-idempotent writes.
- Queue saturation produces observable rejection results and metrics.
- Normal shutdown completes accepted work or returns explicit failures; forced shutdown can recover or explicitly fail the match.
- Tests cover queue saturation, duplicate requests, shutdown races, and simulation-thread failures.
