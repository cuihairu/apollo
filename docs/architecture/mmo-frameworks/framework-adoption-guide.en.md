[English](framework-adoption-guide.en.md) | [中文](framework-adoption-guide.md)

# MMO Framework Adoption Guide

## 1. Conclusion First

If the question is "which frameworks can be picked up and used directly right now", the answer is not a uniform "yes" or "no" — you first have to tell apart which layer of the problem you are trying to solve:

- World engine
- MMORPG content core
- Online infrastructure
- Real-time sync
- Server orchestration

In real projects, these layers are usually not all done well by the same product at the same time.

## 2. Frameworks Most Worth Adopting Directly

### 2.1 Traditional MMORPG Content Server

Top recommendation:

- `AzerothCore`

Runner-up:

- `TrinityCore`

Reasons:

- Complete content systems
- Mature auth/world/db pipeline
- Abundant community material
- The most realistic option for a classic MMORPG-style world server

`AzerothCore`'s main advantages are:

- Modularity
- hooks
- Secondary-development governance

So if the goal is long-term maintenance and multi-person collaboration, `AzerothCore` is usually preferable to forcing heavy modifications directly onto `TrinityCore`.

### 2.2 Modern Online Backend

Top recommendation:

- `Nakama`

Reasons:

- Complete capabilities for accounts, social, matchmaking, leaderboards, notifications, and real-time rooms
- A clear authoritative match model
- Strong clustering and modern engineering capabilities

Good fit for:

- MMO-lite
- Instance RPGs
- Social online games
- Teams that want to get moving quickly

### 2.3 Node / TypeScript Room Server

Top recommendation:

- `Colyseus`

Reasons:

- Room + Schema + patch sync is very lightweight
- High development efficiency
- Well suited to prototypes and small-to-mid-size multiplayer projects

But it is better suited to:

- Room-based multiplayer
- MMO prototype validation

It is not suited to directly carrying an extra-large persistent world.

### 2.4 Unity Real-Time Instance / Battle Server

Top recommendation:

- `Photon`

Reasons:

- Mature real-time sync
- Mature interest management
- Clear topology design

But Photon is better used as:

- A battle server
- An instance server
- A real-time PvP server

rather than a complete MMORPG backend.

### 2.5 Unity Self-Developed Network Sync Layer

If the team does not want to depend directly on Photon, and instead wants to own more of the upper-layer logic itself, consider:

- `Mirror`
- `FishNet`

Between them:

- `Mirror` is more common and more lightweight
- `FishNet` is more flexible on the observer system and scene visibility

They fit:

- Unity MMO-like prototypes
- Self-built server and sync layers
- Cases where you need to own the visibility rules yourself

But they should not be mistaken for a complete backend platform.

### 2.6 Engine-Level Multiplayer Framework

If you are already building a lot of low-level capability inside the engine anyway, evaluate:

- `O3DE Multiplayer`

It is more like:

- An engine-level network replication and RPC framework

than a ready-made game backend platform.

### 2.7 Commercial Hosted Multiplayer Platforms

Worth evaluating:

- `PlayFab Multiplayer Servers`
- `Beamable`
- `AccelByte`
- `Unity Gaming Services`
- `Hathora`

The differences:

- `PlayFab MPS` leans toward hosted dedicated servers and a platform ecosystem
- `Beamable` leans toward LiveOps, microservices, and a platformized game backend
- `AccelByte` leans toward dedicated server management and a commercial game platform
- `Unity Gaming Services` leans toward Unity's official portfolio of ecosystem services
- `Hathora` leans toward low-latency multiplayer hosting
- `AWS GameLift` leans toward cloud-hosted, session-based dedicated servers
- `Edgegap` leans toward edge orchestration and low-latency deployment
- `Pragma` leans toward a commercial backend stack

All of them suit commercial online projects, but none of them equals a large-world MMO kernel.

### 2.8 Unreal World Instance Hosting

Top recommendation:

- `Open World Server`

Reasons:

- Friendly to the Unreal Dedicated Server workflow
- Supports world-region and instance orchestration
- Fits organizing Unreal servers into a larger online world

The premise is that the team already intends to keep most logic on the Unreal side.

### 2.9 Game Server Orchestration and Autoscaling

Top recommendation:

- `Agones`

Reasons:

- `Fleet`
- `Allocation`
- `Autoscaling`
- Kubernetes-native dedicated game server orchestration

But it must be emphasized:

- `Agones` is not a game framework
- It only solves hosting and lifecycle management

### 2.10 Distributed World Platform Reference

If you want to study extra-large worlds, multi-worker cooperation, and distributed authority, look at:

- `SpatialOS`

But at this point it is better used as:

- An architecture reference
- A server meshing template

rather than the default first choice for most teams to land on.

## 3. Frameworks Better Suited to "Reference and Absorption"

### 3.1 KBEngine

`KBEngine` is not impossible to use directly; it is just better suited to:

- Teams with MMO engine experience using it directly
- General teams treating it as a high-value architecture reference

Reasons:

- Strong world model
- Schema-driven entities, sync, and persistence are advanced
- But the runtime is complex and the debugging barrier is high

What it is most worth learning:

- Base / Cell division of responsibility
- Entity schema
- Attribute automation
- AOI and spatial partitioning

### 3.2 Ryzom Core

`Ryzom Core` is better as a reference than as a direct foundation for a modern project.

Reasons:

- The service decomposition is valuable
- It represents a traditional commercial MMO operations-grade service architecture
- But it feels dated and the maintenance barrier is high

What it is most worth learning:

- Service-mesh boundaries
- Dedicated-service decomposition such as naming / tick / AI / position

## 4. Adoption Advice Summary Table

| Framework | Direct adoption recommended? | Better suited use | Main reason |
| --- | --- | --- | --- |
| KBEngine | Conditionally recommended | MMO world engine | Strong world model, but high complexity |
| Ryzom Core | Not really recommended for direct adoption | Architecture reference | Valuable service decomposition, but heavily dated |
| TrinityCore | Recommended | Traditional MMORPG content server | Complete business coverage, abundant material |
| AzerothCore | Strongly recommended | Maintainable MMORPG content server | Stronger modularity and secondary-development governance |
| Nakama | Strongly recommended | Online infrastructure / MMO-lite | Modern, complete, easy to land |
| Photon | Recommended | Unity real-time battle / instance server | Strong sync and interest management |
| Colyseus | Recommended | Node/TS room server / prototypes | Fast development, lightweight sync |
| OWS | Conditionally recommended | Unreal world instance orchestration | Not a complete logic framework, but close to Unreal |
| Agones | Strongly recommended | Game server orchestration layer | Strong production hosting capability |

## 5. Recommendations by Project Type

### 5.1 Classic MMORPG Content Server

Advice:

- First choice `AzerothCore`
- Runner-up `TrinityCore`

### 5.2 Building a Complete Large-World MMO Kernel

Advice:

- Study and absorb `KBEngine`
- Refer to `Ryzom Core` alongside

Be careful about landing it directly, because this road is not "pick a framework and it just runs".

### 5.3 MMO-lite / Instance RPG / Online Service Games

Advice:

- First choice `Nakama`
- If the stack leans TS, consider `Colyseus`
- If the core is Unity real-time combat, consider `Photon`
- If the Unity team wants to own more upper-layer logic, consider `Mirror` or `FishNet`
- If commercial platform capability matters more, evaluate `Beamable` or `PlayFab`
- If following the official Unity services route, evaluate `Unity Gaming Services`
- If cloud hosting and edge deployment matter more, evaluate `AWS GameLift` or `Edgegap`

### 5.4 Unreal MMO

Advice:

- `OWS + Unreal Dedicated Server`

### 5.5 Light MMO / Virtual World / AoI Community Server

Advice:

- `SmartFoxServer`

It fits map-based online worlds better than an ordinary room server, because it has an explicit `MMORoom + AoI` abstraction.

### 5.6 Large-Scale Cloud-Native Orchestration Needed

Advice:

- `Agones`

## 6. Adoption Advice from Apollo's Perspective

If Apollo's goal is a "general-purpose MMO server framework", the most reasonable route is not to pick one framework as a direct replacement, but to absorb layer by layer:

- World model: `KBEngine`
- Module governance: `AzerothCore`
- Online infrastructure: `Nakama`
- Real-time sync abstractions: `Photon`, `Colyseus`, `SmartFoxServer`, `Mirror`, `FishNet`, `O3DE Multiplayer`
- Commercial platform capabilities: `PlayFab`, `Beamable`, `AccelByte`, `UGS`, `Hathora`, `GameLift`, `Edgegap`, `Pragma`
- Production orchestration: `Agones`
- Distributed world reference: `SpatialOS`

In other words:

- Do not treat `TrinityCore` or `AzerothCore` directly as Apollo's future kernel
- Do not mistake `Nakama` or `Colyseus` for a complete large-world solution
- What is more reasonable is to break them down into "capability sources"

## 7. Final Recommendations

If you want just one realistic shortlist:

- Traditional MMORPG content server: `AzerothCore`
- Modern online backend: `Nakama`
- Room sync framework: `Colyseus`
- Unity real-time instance server: `Photon`
- Unity self-developed network layer: `Mirror`, `FishNet`
- Engine-level multiplayer framework: `O3DE Multiplayer`
- Commercial online platforms: `PlayFab MPS`, `Beamable`, `AccelByte`, `Unity Gaming Services`
- Lightweight hosting platforms: `Hathora`, `AWS GameLift`, `Edgegap`
- Commercial backend stack: `Pragma`
- Light MMO / virtual world platform: `SmartFoxServer`
- Unreal instance world orchestration: `OWS`
- Cloud-native game server orchestration: `Agones`
- MMO world engine research template: `KBEngine`
- Distributed world research template: `SpatialOS`

One-line summary:

- Best for landing directly: `AzerothCore`, `Nakama`, `Colyseus`, `Agones`
- Best as large-world architecture references: `KBEngine`, `Ryzom Core`
- Best as battle / room / hosting-layer supplements: `Photon`, `OWS`, `PlayFab`, `Beamable`, `AccelByte`, `UGS`, `Hathora`, `GameLift`, `Edgegap`, `Pragma`
