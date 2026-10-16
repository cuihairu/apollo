[English](mmo-frameworks-comparison.en.md) | [中文](mmo-frameworks-comparison.md)

# MMO Framework Master Comparison

## 1. Conclusion First

These frameworks are not products of the same category.  
If you simply line them up as "which is better", the conclusion is bound to be distorted.

A more accurate way to understand them:

- KBEngine, Ryzom Core: closer to MMO world engines
- TrinityCore, AzerothCore: closer to MMORPG content-server cores
- Nakama, Photon, Colyseus, SmartFoxServer, Beamable, AccelByte, Unity Gaming Services, Pragma: closer to modern real-time multiplayer backends or game service platforms
- Mirror, FishNet, O3DE Multiplayer: closer to network sync layers or engine-level multiplayer frameworks
- OWS, Agones, SpatialOS, PlayFab MPS, Hathora, AWS GameLift, Edgegap: closer to hosting and orchestration layers, or distributed world platforms

## 2. Core Comparison Table

| Framework | Category | World model | Entity / Schema | Sync model | Persistence model | Extension approach | Main strength | Main weakness |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| KBEngine | MMO world engine | Strong, Space/Cell | Strong, def/schema | schema-driven | schema-driven | Python + engine | Large worlds, AOI, entity migration | Heavy runtime |
| Ryzom Core | MMO service mesh | Strong, service-oriented world | Mid-strong | Variable database + dedicated network layer | Dedicated services | Multi-service cooperation | Industrial-grade service decomposition | Strongly dated |
| TrinityCore | MMORPG Core | Mid, worldserver | Mid | Dominated by in-world-server logic | Strongly DB-driven | Core secondary development | Complete content systems | Not a distributed large-world engine |
| AzerothCore | Modular MMORPG Core | Mid, worldserver | Mid | Dominated by in-world-server logic | Strongly DB-driven | modules + hooks | Modular governance | Average world-engine capability |
| Nakama | Modern multiplayer backend | Weak to mid, room-based | Weaker than MMO schema | authoritative match | Storage API | Go/TS/Lua runtime | Complete online infrastructure | No complete large-world model |
| Photon | Real-time networking framework | Weak to mid, session/world must be self-built | Mid, network-object level | authority + interest management | Mostly self-built | Unity ecosystem extension | Sync and interest management | Not a complete MMO core |
| Colyseus | Room-based real-time framework | Weak, room-based | Mid, Schema | patch-based sync | Mostly self-built | Node/TS | Lightweight state sync | Insufficient production-grade MMO world layer |
| SmartFoxServer | Multiplayer server platform | Mid, MMORoom/AoI | Mid | AoI + MMORoom | Mostly self-built | Extension | Mature MMO room and virtual-world support | Not a complete world engine |
| Mirror | Unity network layer | Weak | Mid, network-object level | interest management | Self-built | Unity component-style extension | Lightweight and easy to integrate | Not a backend |
| FishNet | Unity network layer | Weak | Mid, observer/scene visibility | observer system | Self-built | Unity component-style extension | Flexible visibility control | Not a backend |
| O3DE Multiplayer | Engine-level multiplayer framework | Mid, entity/component world | Mid | network properties + RPC | Self-built | Engine-level extension | Multiplayer capability inside an open-source engine | Not a backend platform |
| PlayFab MPS | Hosted multiplayer platform | Weak | Weak to mid | Decided by self-built server | Platform-hosted | Platform ecosystem extension | Commercial hosting and accompanying ecosystem | Not a world engine |
| Beamable | Game services platform | Weak | Weak to mid | relay / combined service logic | Platform services | microservices | LiveOps + platformized backend | Not suitable as a large-world kernel |
| AccelByte | Game backend platform | Weak | Weak to mid | Session/DS coordination | Platform services | Platform modules | DS management combined with platform | Not a world engine |
| Unity Gaming Services | Unity online services suite | Weak | Weak to mid | Lobby/Relay/Matchmaker combination | Platform services | Unity official ecosystem | Realistic Unity integration | Not an MMO world framework |
| Pragma | Commercial game backend platform | Weak | Weak to mid | Mostly business backend | Platform services | Platform product capabilities | Platformized backend | Not a world engine |
| OWS | World orchestration layer | Mid, region/instance orchestration | Weak | Depends on Unreal server | API + DB | Microservice extension | Unreal world instance management | Not a complete logic framework |
| Agones | Orchestration layer | None | None | None | None | K8s CRD | dedicated server orchestration | Provides no game logic |
| Hathora | Hosted multiplayer hosting | Weak | Weak | Decided by self-built server | Hosted capacity | Platform integration | Low-latency real-time session hosting | Not a complete platform |
| AWS GameLift | Hosted dedicated server platform | Weak | Weak | session-based hosting | Hosted capacity | Cloud platform capabilities | Mature commercial hosting | Not a world engine |
| Edgegap | Edge orchestration platform | Weak | Weak | edge-hosted session | Hosted capacity | Platform integration | Low-latency edge deployment | Not a complete platform |
| SpatialOS | Distributed world platform | Strong, distributed simulation | Mid | Worker-coordinated sync | Limited platform-side capability | Platform extension | server meshing / distributed authority | High landing cost |

## 3. Differences Viewed from MMO Core Problems

### 3.1 Who Cares Most About the "Large World" Itself

Strongest:

- KBEngine
- Ryzom Core

Middle:

- OWS

Weak:

- TrinityCore
- AzerothCore
- Nakama
- Photon
- Colyseus
- SmartFoxServer
- Mirror
- FishNet
- O3DE Multiplayer
- Agones

### 3.2 Who Cares Most About "Content Systems"

Strongest:

- TrinityCore
- AzerothCore

Middle:

- KBEngine
- Ryzom Core

Weak:

- Nakama
- Photon
- Colyseus
- OWS
- Agones

### 3.3 Who Cares Most About "Real-Time Infrastructure"

Strongest:

- Nakama
- Photon
- Colyseus
- SmartFoxServer
- O3DE Multiplayer

Middle:

- KBEngine
- Ryzom Core
- Mirror
- FishNet
- Beamable

### 3.4 Who Cares Most About "Production Deployment and Hosting"

Strongest:

- Agones
- OWS
- SpatialOS
- PlayFab MPS

Middle:

- Nakama
- SmartFoxServer
- Beamable

## 4. The Most Distinctive Trait of Each Framework

### KBEngine

- Base / Cell binary division of responsibility
- schema-driven properties, methods, sync, persistence

### Ryzom Core

- Fine-grained service cloud
- Dedicated services such as Tick / Naming / AI / Position

### TrinityCore

- Deep content systems
- Very complete traditional world-server logic

### AzerothCore

- Modular governance stronger than its peers
- Emphasizes customization through hooks/modules

### Nakama

- presence / streams / matchmaking / authoritative match

### Photon

- interest management
- Mature topology design

### Colyseus

- Extremely lightweight schema patch sync

### SmartFoxServer

- `MMORoom + AoI`
- Made the MMO room model an official capability very early

### Mirror

- Interest Management is an official first-class capability

### FishNet

- Observer System and Scene Visibility are very flexible

### O3DE Multiplayer

- entity/component network replication unified with the engine

### PlayFab MPS

- Commercial hosted multiplayer server

### Beamable

- LiveOps + microservice backend platform

### OWS

- Dynamic spin-up and region hosting of Unreal dedicated servers

### Agones

- Fleet / Allocation / Autoscaling

### SpatialOS

- distributed simulation
- the server meshing / distributed authority line of thinking

## 5. If We Only Ask What Apollo Should Learn

### Layer One: MMO World Kernel

Look first at:

- KBEngine
- Ryzom Core

Absorb:

- `EntitySchema`
- The spatial authority model
- The sync description model
- World service boundaries

### Layer Two: Business and Module Governance

Look first at:

- AzerothCore
- TrinityCore

Absorb:

- Modularity
- hooks
- Content system boundaries

### Layer Three: Modern Real-Time Infrastructure

Look first at:

- Nakama
- Photon
- Colyseus
- SmartFoxServer
- Mirror
- FishNet
- O3DE Multiplayer
- Beamable

Absorb:

- room / presence / stream / authoritative tick
- interest management
- patch sync

### Layer Four: Production Deployment

Look first at:

- Agones
- OWS
- SpatialOS
- PlayFab MPS

Absorb:

- allocation
- prewarm fleet
- autoscaling
- canary

## 6. Final Judgment

If Apollo's goal is a "general-purpose MMO server framework", the most reasonable route is not to copy any single one of them, but compositional absorption:

- Use KBEngine / Ryzom Core to fill in the world model
- Use AzerothCore to fill in module governance
- Use Nakama / Photon / Colyseus / Beamable to fill in modern real-time and platform abstractions
- Use Agones / OWS / PlayFab MPS to fill in production hosting and orchestration

Put differently:

- KBEngine answers "how the world lives"
- TrinityCore / AzerothCore answer "how content piles up"
- Nakama / Photon / Colyseus / Beamable answer "how real-time runs"
- OWS / Agones / PlayFab MPS answer "how services are kept alive"

What is most valuable to Apollo is not picking a side, but being clear about the problem each layer must solve, then absorbing layer by layer.
