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
    details: loginServer authenticates and hosts directories; gateway handles connections; Base owns persistent gameplay, World topology/executor management, cell simulation, database storage access, and machined local supervision.
  - title: Modern C++20 modules
    details: Networking, data, entity contracts, AOI, combat, and runtime modules; the README and module docs describe implementation status.
---

## Target architecture

Business traffic goes directly through gateway to the owning Base or current cell. worldServer manages placement and topology. Solid lines show business traffic; dashed lines show registration and management.

<ArchitectureDiagram kind="system" />

## Positioning

Apollo is a lightweight instance-based multiplayer game server engine for tower defense, co-op PvE/PvP, arenas, and dungeons. MMO describes a possible product scale or form, not the server architecture. Seamless maps are deferred, with multiple Cells per Space, neighboring visibility, and authority handoff reserved.

The target-role model routes client base gameplay and simulation separately through gateway. Base loads/saves through database, loginServer hosts authentication/directories, and machined optionally supervises local processes. Scene transfer retains Base/storage affiliation.

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

External BaseApp/CellApp names retain their real meanings. Apollo uses baseServer for persistent-player gameplay and worldServer for Space/Cell management and cell for simulation; see the target-role glossary.
