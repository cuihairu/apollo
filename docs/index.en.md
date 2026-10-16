---
layout: home
title: Apollo
titleTemplate: false
hero:
  name: Apollo
  text: Scene-instance online game server engine
  tagline: C++20 kernel — an MMO framework at instance/baseapp granularity; tower-defense/room-style gameplay as a first-class citizen (Compact form).
  image:
    src: /apollo.png
    alt: Apollo
  actions:
    - theme: brand
      text: Glossary (first stop for new readers)
      link: /design/concept-glossary
    - theme: alt
      text: Tower-defense / Compact form
      link: /30-Compact_GameServer_Design
features:
  - title: Scene-instance based (baseapp / instance)
    details: Scene-instance granularity baseapps carry entity authority; no cell/base split, no ghost seamless world; instances have short, independently configurable lifecycles, and room-style gameplay is just the instance form.
  - title: Self-built four-layer networking
    details: L0 poll reactor → L1 frame (magic+seq+CRC32C) → L2 session (four-level watermark + GameConnection four-channel queue/trim grading/scene-thread dispatch/Resuming) → L3 logical facade; inter-process InterServerLink as one stable-connection family.
  - title: Contract, one source many ends
    details: XML+XSD contracts (attrs/messages/entities/errors) + an independent generator binary; 8-bit visibility-domain mask, schema_hash handshake, domain-segmented ids — server-side contract changes need zero recompiles.
  - title: Lua business layer (planned)
    details: Design draft under review (scripting-lua); native C API bindings and hot reload via table-swap protocol are the design goals — the repo has no Lua dependency or binding code.
  - title: write-behind journal persistence
    details: "Two-phase: append log first, then snapshot; the journal is the crash-recovery anchor (G-2 mirroring stream not built, ADR-010); storage.xml/db-app are planned."
  - title: Clustering G-1/G-2 (P3 prerequisites decided)
    details: Process discovery = machined daemon + UDP broadcast, two layers (no etcd/consul); G-2 option A first, restart-only (standby/reviver not started, ADR-010); recovery-phase exclusivity; metrics from one source, G-5.
---

[English](index.en.md) | [中文](index.md)

## Current Positioning and Architecture Status

**Form**: single-process start (P1-P2), cluster capability via P3 integration — the phasing is an explicit declaration, not an omission. Current topology:

```text
Client ── Gate (passthrough / decode-and-assembly division)
          └─ World / baseappmgr domain (lightest allocation, admission gate, recovery phase)
              ├─ baseapp ×N (scene-instance-granularity process, single-writer tick 10Hz)
              ├─ SceneAoi (3×3 grid, embedded in cell-app; quadtree/shard unwired)
              ├─ instance (optionally a separate process; tower-defense room = embedded instance)
              └─ db-app (planned, unwired)
Process discovery: machined daemon + UDP broadcast (G-1, no registry center)
G-2: option A first, restart-only (standby/reviver not started, ADR-010)
```

**Key Decisions Ruled** (full table in §36, decision traceability table #1-#19): self-built four-layer networking / nng retired; contract XML+XSD; instance system, seamless-world vetoed (#9); AOI independent service (#10); Lua whitelist over Python (#12); process discovery machined+UDP two-layer, no etcd/consul (#13, 2026-09-30); Lua 5.5 via vcpkg, sol2 dropped (#12 revision); frame sync kept only as an adaptation seam (#18).

**Design-gap Scheduling**: design-gap-inventory #1-#11 are all CLOSED; #12 session and online directory ([session-and-online-directory](/design/session-and-online-directory)) and #17 battle verification service ([battle-verification-service](/design/battle-verification-service)) are on disk as CLOSED; **#13-#16 OPEN** (login path / inbound third-party integration / bots stress-testing / map spatial data pipeline — all with #12 directory as prerequisite).

## Documentation Map (authoritative grading)

| Layer | Location | Content |
|------|------|------|
| **Terminology base** | [design/concept-glossary](/design/concept-glossary) | General concepts × engines in which they appear × apollo's stance; three terminology rulings (instance naming for 副本 / the word battle left to the battle-verification domain / no registry center) — **new concepts enter the table before they land in docs** |
| **Authoritative design** | docs/design/ (twelve docs) | net-abstraction (four networking layers + cluster prerequisites), sdk-contract (contract), attribute-sync (attributes/sync/journal), scripting-lua (Lua), xml-generation (generator), logging, clock-and-time, battle-determinism (replay/recomputation), battle-verification-service (battle verification/authoritative settlement), session-and-online-directory (session and online directory/forced logout/offline keepalive), capacity-and-benchmark, concept-glossary |
| **Comparative analysis** | docs/analysis/ | architecture-review (whole-repo audit + registry, authoritatively graded), mmo-mechanism-deep-dive (implementation-level evidence for §36 #1-#18), design-gap-inventory (gap ledger), ssengine-reference |
| **Framework comparison** | [36-MMO_Frameworks_Comparative_Analysis](/36-MMO_Frameworks_Comparative_Analysis) | Ten-framework comparison + decision traceability table #1-#19 (the source and rationale of every apollo design decision) |
| **Lightweight form** | [30-Compact_GameServer_Design](/30-Compact_GameServer_Design) | Tower-defense/fixed-map slim form (embedded AOI / embedded instance / single-machine multithreading) |
| **Batch planning** | [todo](/todo) | Batches 1-9 (contracts delivered; db-app/cell-appmgr/scripting/SDK/monitoring scheduling) |
| **Reference area** | [architecture/README](/architecture/README) | 70+25 full status tables (A directly cited 4 / B engine analysis / C superseded drafts / D historical roadmap) — **this directory is not to be read as apollo's current design** |
| **QA answers** | docs/qa/ (117 questions), [QA](/QA) | Interview-level Q&A library (BW/KBE mechanism analysis conclusions, reference) |

The remaining directories (guide/, api/, modules/, apps/, sdks/, public/) are early-stage manuals and module notes, rewritten alongside the corresponding code batches; the status of the root numbered documents (03/18/32-36) is governed by each document's header annotation and the §36 wording. Deleted documents (00/04/05/17/25, etc.) are traceable in git history.

## Quick Start (reader's path)

1. [Glossary](/design/concept-glossary) → 2. [§36 decision traceability table](/36-MMO_Frameworks_Comparative_Analysis) (why designed this way) → 3. six core docs in docs/design/ (net-abstraction/sdk-contract/attribute-sync/scripting-lua/logging/clock-and-time) → 4. [architecture-review §16.10.2 registry](/analysis/architecture-review) (what isn't done yet, what it's waiting on) → 5. [todo](/todo) (batch scheduling).
