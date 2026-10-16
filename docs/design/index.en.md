[English](index.en.md) | [中文](index.md)

# Design Docs Overview

> This directory = apollo's landing point for **system design docs**: each doc is the design/survey/decision record of one capability domain.
> Status semantics: **Delivered** (code has landed) / **Design draft** (under review or awaiting approval) / **Prerequisite design** (pre-initiation survey piece; decision points listed inside).
> Terminology discipline: naming reuses existing BigWorld/KBE concepts first; new terms enter [concept-glossary](./concept-glossary.en.md) first, with cross-references and rationale.

## Foundations and Models

| Design doc | Status | One-line positioning |
|---|---|---|
| [Concept glossary](./concept-glossary.en.md) | Living document | New-term registry |
| [Player object model](./player-object-model.en.md) | Delivered | Anchor/Avatar/Scene/Instance ownership and lifecycle |
| [Attribute system and sync](./attribute-sync.en.md) | Delivered (partial batches pending wiring) | Long-term-state field model + visibility/sync domains |
| [Session and online directory](./session-and-online-directory.en.md) | Delivered (skeleton) | Manager-domain directory authority + cross-process mirroring/recovery phases |
| [Login flow](./login-flow.en.md) | Design draft | login/gateway/base three-segment handshake |
| [Clock and time](./clock-and-time.en.md) | Delivered | Wall clock/tick/frame budget semantics |

## Combat

| Design doc | Status | One-line positioning |
|---|---|---|
| [Battle determinism](./battle-determinism.en.md) | Delivered | Four constraints + recording/replay/hash chain |
| [Battle instance offload](./battle-instance-offload.en.md) | Delivered (skeleton) | Offload process form (three decision points ruled, ADR-012) |
| [Battle verification service](./battle-verification-service.en.md) | Design draft (under review) | Client-authoritative battle recomputation and reconciliation (verifier) |

## Networking and Operations

| Design doc | Status | One-line positioning |
|---|---|---|
| [Network abstraction and transport kernel](./net-abstraction.en.md) | Design draft | L0-L3 layering + M1 homegrown kernel (nng replacement route) |
| [Gateway topology survey](./gateway-topology-survey.en.md) | Survey piece ("keep" conclusion in effect, ADR-011) | Nine-framework comparison + the case for an independent gateway-app |
| [Logging system](./logging.en.md) | Delivered (P3-3 batches A/B/C) | One unified closure + structured lines + three-prohibitions discipline |
| [LoggerApp initiation prerequisite design](./logger-app.en.md) | Delivered (L1-L4, 2026-10-10/12) | Landing form of the process shell and structured-line wiring (ADR-013) |
| [Crash capture](./crash-capture.en.md) | Delivered | Crashpad out-of-process + symbolization pipeline |
| [Capacity and benchmark](./capacity-and-benchmark.en.md) | Delivered | 5000 CCU capacity model + three-model benchmark |
| [Backup, disaster recovery and failover](./backup-revive.en.md) | Closed (batch 0 delivered, six decision points ruled, ADR-010: plan A first) | Candidate topology for "service stays up" after an authoritative process dies (G-2) |

## Extension Surfaces

| Design doc | Status | One-line positioning |
|---|---|---|
| [Cell single-writer closure](./cell-single-writer.en.md) | Prerequisite design (all six decision points ruled, ADR-015/016/017) | The concurrency form of RPC acceptance and the simulation thread (P3-2 batch B) |
| [Acceptance queue watermark and overload](./queue-watermark.en.md) | Prerequisite design (ruled, ADR-016) | Two-tier watermark/callback error codes/tick budget anchoring |
| [Inbound third-party interfaces](./inbound-interfaces.en.md) | Design draft (under review) | Channel-callback hosting process with authentication/idempotency |
| [Scripting system (Lua)](./scripting-lua.en.md) | Design draft | Both-end shared battle logic and hot-update discipline |
| [SDK contract](./sdk-contract.en.md) | Living document | Client/server shared contract (contract.lua) |
| [XML contract generation](./xml-generation.en.md) | Delivered | entities.xml → three-language projections |
