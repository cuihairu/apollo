---
layout: home
title: Apollo
titleTemplate: false
hero:
  name: Apollo
  text: Lightweight Room-Based Multiplayer Game Server Engine
  tagline: A C++20 runtime for tower defense, co-op, arenas, and dungeons. MMO is a possible product form; seamless worlds are not the target.
  image:
    src: /apollo.png
    alt: Apollo
  actions:
    - theme: brand
      text: Architecture Overview
      link: /guide/architecture
    - theme: alt
      text: Concepts and Names
      link: /design/concept-glossary
features:
  - title: Rooms and matches
    details: One Room is one gameplay session. A RoomServer process can host multiple Rooms and reclaims temporary state when matches end.
  - title: One authoritative player object
    details: A player has one authoritative runtime object during a match. Cross-server play transfers its state and authority, then updates Gateway routing.
  - title: Clear ingress responsibilities
    details: Login authenticates accounts, Gateway handles client connections and routing, and RoomServer runs match simulation.
  - title: Modern C++20 modules
    details: Networking, data, entity contracts, AOI, combat, and runtime modules; the README and module docs describe implementation status.
---

## Positioning

Apollo is a lightweight room-based multiplayer game server engine for tower defense, co-op PvE/PvP, arenas, and dungeons. MMO describes a possible product scale or form, not the server architecture. Apollo is not currently designed around a persistent seamless world.

The target path is `Client → Gateway → RoomServer → Room`. A Room is one match; one RoomServer can host multiple Rooms. Cross-server play transfers authority for a player object, and only one process may perform authoritative operations for it at a time.

## Documentation

| Topic | Document |
|---|---|
| Target architecture | [Architecture Overview](/en/guide/architecture) |
| Target names and boundaries | [Concepts and Names](/en/design/concept-glossary) |
| Current executable prototypes | [Server Applications](/en/apps/) |
| RoomServer and cross-server transfer | [Room-Based GameServer Design](/en/30-Room-Based_GameServer_Design) |
| Building the code | [Build Guide](/en/BUILDING) |
| External engine comparisons | [MMO Framework Comparison](/en/36-MMO_Frameworks_Comparative_Analysis) |

BigWorld/KBEngine names such as Base/Cell remain in external engine comparison material to explain those implementations; they are not Apollo target architecture terms.
