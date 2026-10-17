
# MMO Framework Architecture Analysis Index

This document set relies mainly on official materials, official repositories, and official documentation available as of 2026-03-23, focusing on the most essential questions of MMORPG / large-scale online game server backends:

- how the world is split
- how entities and state are modeled
- how synchronization is done
- how persistence is done
- how extensibility and operations are done

## Document List

- [KBEngine Architecture Analysis](./kbengine-analysis.md)
- [TrinityCore Architecture Analysis](./trinitycore-analysis.md)
- [AzerothCore Architecture Analysis](./azerothcore-analysis.md)
- [Nakama Architecture Analysis](./nakama-analysis.md)
- [Ryzom Core Architecture Analysis](./ryzom-core-analysis.md)
- [Photon Architecture Analysis](./photon-analysis.md)
- [Colyseus Architecture Analysis](./colyseus-analysis.md)
- [SmartFoxServer Architecture Analysis](./smartfoxserver-analysis.md)
- [Mirror Architecture Analysis](./mirror-analysis.md)
- [FishNet Architecture Analysis](./fishnet-analysis.md)
- [O3DE Multiplayer Architecture Analysis](./o3de-analysis.md)
- [PlayFab Multiplayer Servers Architecture Analysis](./playfab-analysis.md)
- [Beamable Architecture Analysis](./beamable-analysis.md)
- [AccelByte Architecture Analysis](./accelbyte-analysis.md)
- [Hathora Architecture Analysis](./hathora-analysis.md)
- [Unity Gaming Services Architecture Analysis](./unity-gaming-services-analysis.md)
- [AWS GameLift Architecture Analysis](./gamelift-analysis.md)
- [Edgegap Architecture Analysis](./edgegap-analysis.md)
- [Pragma Architecture Analysis](./pragma-analysis.md)
- [SpatialOS Architecture Analysis](./spatialos-analysis.md)
- [Open World Server Architecture Analysis](./open-world-server-analysis.md)
- [Agones Architecture Analysis](./agones-analysis.md)
- [Framework Adoption Guide](./framework-adoption-guide.md)
- [MMO Framework Overall Comparison](./mmo-frameworks-comparison.md)
- [MMO Framework Five-Dimension Scorecard](./mmo-frameworks-scorecard.md)

## Category Notes

To avoid mixing completely different categories of products into one, this document set understands them by capability tier:

### 1. World Engines / MMO Core Frameworks

- KBEngine
- Ryzom Core

Frameworks of this kind provide fairly strong entity, spatial, synchronization, and service-partitioning models of their own.

### 2. Content Servers / MMORPG Core

- TrinityCore
- AzerothCore

Frameworks of this kind are more like mature large-scale MMORPG world server cores; their strengths lie in content systems and business logic, and they are not known for horizontal scaling of distributed large worlds.

### 3. Modern Multiplayer Backends / Room-Style Frameworks

- Nakama
- Photon
- Colyseus
- SmartFoxServer
- PlayFab Multiplayer Servers
- Beamable
- AccelByte
- Unity Gaming Services
- Pragma
- Mirror
- FishNet
- O3DE Multiplayer

Products of this kind excel at real-time sessions, matchmaking, authoritative rooms, and state synchronization, but usually do not directly provide a complete seamless large-world MMO kernel.

### 4. Orchestration / Hosting Layer

- Open World Server
- Agones
- SpatialOS
- Hathora
- AWS GameLift
- Edgegap

Products of this kind lean more toward runtime orchestration, instance allocation, and scaling; they are not equivalent to complete MMO game frameworks.

## Usage Recommendations

If the goal is a general-purpose MMO server framework like Apollo, the most valuable references are:

- World models and entity schemas: KBEngine, Ryzom Core
- Business content and module governance: AzerothCore, TrinityCore
- Modern real-time multiplayer infrastructure: Nakama, Photon, Colyseus
- Production deployment and scaling: OWS, Agones
