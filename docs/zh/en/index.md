---
layout: home
title: Apollo
titleTemplate: false
hero:
  name: Apollo
  text: Lightweight Instance-Based Multiplayer Game Server Engine
  tagline: A C++20 runtime for tower defense, co-op, arenas, and dungeons. MMO is a possible product form; instances come first, with seamless-map design reserved.
  image:
    src: /apollo.png
    alt: Apollo
  actions:
    - theme: brand
      text: Architecture Overview
      link: /en/guide/architecture
    - theme: alt
      text: Concepts and Names
      link: /en/design/concept-glossary
features:
  - title: Instances and matches
    details: One Instance is one gameplay session. A cell executor can host multiple Space/Cells and Instances and reclaims temporary state when matches end.
  - title: Separate profile and simulation authority
    details: Base persistent objects and Cell simulation entities are distinct. Each entity has one simulation authority; cross-server play retains Base.
  - title: Clear ingress responsibilities
    details: login authenticates and hosts directories; gateway handles connections; Base owns persistent gameplay, World topology/executor management, cell simulation, database storage access, and machined local supervision.
  - title: Modern C++20 modules
    details: Networking, data, entity contracts, AOI, combat, and runtime modules; the README and module docs describe implementation status.
---

## Target architecture

Business traffic goes directly through gateway to the owning Base or current cell. world manages placement and topology. Solid lines show business traffic; dashed lines show registration and management.

<ArchitectureDiagram kind="system" />

## External HTTP Callbacks and Developer Entry Points

Optional interfaces receives payment/channel callbacks over HTTP/HTTPS. Durable intake precedes asynchronous delivery to the current Base, resolved by stable player identity. Base validates and fulfills through database. Gameplay WS/WSS belongs to the gateway game-message path — TCP/WS/WSS share one accepted path — and never enters the HTTP handler.

<ArchitectureDiagram kind="callbacks" />

Implementation status: the gateway game TCP/WS/WSS entries are accepted (including WSS certificate-chain/hostname rejection and key-configuration negatives); the interfaces intake chain (byte-exact raw-request retention, request-timeout and queue budgets, bounded single-writer), the order transaction domain (durable intake and pending work, epoch-based current-Base resolution, atomic fulfillment dedup credentials), and the apolloc init scaffolding are implemented with test coverage; the channel verification executor and inter-process wiring (interfaces→order intake, base write-authority lifecycle, reward apply executor) remain planned. See the [Mistlane/FOGS review](./analysis/mistlane-review.md); root todo.md tracks implementation.

## Processes, Tools, and CI

- Runtime processes: gateway, login, base, world, database, interfaces, machined; cell runs as a role of the world binary. gateway serves the game TCP/WS/WSS entries (accepted); interfaces serves the HTTP/HTTPS callback entry (intake chain implemented).
- CLI tools: apolloc (project init, delivered), fleetctl (fleet drill assertions), admin and logger (observability helpers) — developer tools, not online services.
- CI: GitHub Actions (build/tests, contracts, docs) runs in the repository, outside the online path.

## Positioning

Apollo is a lightweight instance-based multiplayer game server engine for tower defense, co-op PvE/PvP, arenas, and dungeons. MMO describes a possible product scale or form, not the server architecture. Seamless maps are deferred, with multiple Cells per Space, neighboring visibility, and authority handoff reserved.

The target-role model routes client base gameplay and simulation separately through gateway. Base loads/saves through database, login hosts authentication/directories, and machined optionally supervises local processes. Scene transfer retains Base/storage affiliation.

## Documentation

| Topic | Document |
|---|---|
| Target architecture | [Architecture Overview](/en/guide/architecture) |
| Target names and boundaries | [Concepts and Names](/en/design/concept-glossary) |
| Current executable prototypes | [Server Applications](/en/apps/) |
| World and cross-server transfer | [Instance-Based GameServer Design](/en/30-Instance_GameServer_Design) |
| Building the code | [Build Guide](/en/BUILDING) |
| BigWorld–Apollo correspondence | [Roles, concepts, and code mapping](/en/design/bigworld-apollo-mapping) |
| External engine comparisons | [MMO Framework Comparison](/en/36-MMO_Frameworks_Comparative_Analysis) |

External BaseApp/CellApp names retain their real meanings. Apollo uses base for persistent-player gameplay and world for Space/Cell management and cell for simulation; see the target-role glossary.
