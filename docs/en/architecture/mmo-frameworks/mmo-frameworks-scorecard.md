
# MMO Framework Five-Dimension Scorecard

## 1. How to Read the Scores

This scorecard is not a "who is best" total ranking; it breaks the frameworks down across five key dimensions:

- World model
- Sync model
- Persistence capability
- Extensibility
- Operations and hosting capability

Score range:

- `1` very weak
- `2` weak
- `3` mid
- `4` strong
- `5` very strong

Note:

- This is not a product-maturity leaderboard
- Frameworks of different categories are naturally strong in different areas

## 2. Score Table

| Framework | World model | Sync model | Persistence | Extensibility | Ops & hosting | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| KBEngine | 5 | 5 | 4 | 4 | 3 | Archetypal MMO world engine |
| Ryzom Core | 4 | 4 | 3 | 3 | 4 | Traditional commercial MMO service mesh |
| TrinityCore | 3 | 3 | 4 | 3 | 2 | Classic world server |
| AzerothCore | 3 | 3 | 4 | 4 | 2 | Modular content server |
| Nakama | 2 | 4 | 4 | 5 | 4 | Very strong online infrastructure |
| Photon | 2 | 5 | 2 | 3 | 3 | Strong real-time sync |
| Colyseus | 2 | 4 | 2 | 4 | 3 | patch sync fits room servers well |
| SmartFoxServer | 3 | 4 | 2 | 4 | 3 | MMORoom + AoI is distinctive |
| Mirror | 1 | 4 | 1 | 3 | 2 | Unity network layer |
| FishNet | 1 | 4 | 1 | 4 | 2 | Observer System is more flexible |
| O3DE Multiplayer | 3 | 4 | 1 | 4 | 2 | Engine-level multiplayer framework |
| PlayFab MPS | 1 | 2 | 4 | 3 | 4 | Commercial hosting platform |
| Beamable | 1 | 3 | 4 | 4 | 4 | LiveOps and microservice platform |
| AccelByte | 1 | 2 | 4 | 4 | 4 | Commercial game platform and DS management |
| Unity Gaming Services | 1 | 3 | 3 | 3 | 4 | Unity official services suite |
| AWS GameLift | 1 | 2 | 2 | 3 | 5 | Cloud-hosted dedicated server |
| Edgegap | 1 | 2 | 1 | 2 | 5 | edge orchestration / hosting |
| Pragma | 1 | 2 | 4 | 4 | 4 | Commercial backend stack |
| Open World Server | 3 | 2 | 3 | 3 | 4 | Unreal orchestration layer |
| Agones | 1 | 1 | 1 | 2 | 5 | Orchestration layer, not a logic framework |
| Hathora | 1 | 2 | 1 | 2 | 4 | Low-latency multiplayer hosting |
| SpatialOS | 5 | 4 | 2 | 3 | 4 | Strong distributed-world ideas, high real-world landing barrier |

## 3. Dimension Notes

### 3.1 World Model

What is being judged:

- Whether there is explicit large-world modeling
- Whether spatial partitioning is supported
- Whether there is a world-level authority model

Strongest:

- KBEngine
- SpatialOS
- Ryzom Core

### 3.2 Sync Model

What is being judged:

- Whether there is a mature state sync mechanism
- Whether there is AOI / interest management / patch sync
- Whether it suits large-scale multiplayer

Strongest:

- KBEngine
- Photon
- Colyseus
- SmartFoxServer
- Nakama
- O3DE Multiplayer

### 3.3 Persistence

What is being judged:

- Whether there is a formal backend storage model
- Whether it connects with the entity or online systems

Strongest:

- KBEngine
- TrinityCore
- AzerothCore
- Nakama
- PlayFab MPS
- Beamable
- AccelByte
- Unity Gaming Services
- Pragma

### 3.4 Extensibility

What is being judged:

- The module / runtime / hook / extension ecosystem
- Whether it suits long-term customization

Strongest:

- Nakama
- AzerothCore
- SmartFoxServer
- Colyseus
- FishNet
- O3DE Multiplayer
- Beamable
- AccelByte
- Pragma

### 3.5 Ops & Hosting

What is being judged:

- Whether there are clustering, autoscaling, allocation, and orchestration capabilities
- Whether it suits production deployment

Strongest:

- Agones
- Nakama
- Open World Server
- SpatialOS
- Ryzom Core
- PlayFab MPS
- Beamable
- AccelByte
- Unity Gaming Services
- Hathora
- AWS GameLift
- Edgegap
- Pragma

## 4. How to Use This Table

### Building a Large-World MMO

Look first at high world-model scores:

- KBEngine
- Ryzom Core
- SpatialOS

### Building Real-Time Instance / Room-Based MMORPG

Look first at high sync scores:

- Photon
- Nakama
- Colyseus
- SmartFoxServer
- O3DE Multiplayer

### Building a Long-Term Maintainable Content Server

Look first at high persistence and high extensibility:

- AzerothCore
- TrinityCore

### Building a Production Deployment System

Look first at high ops & hosting:

- Agones
- OWS
- Nakama
- PlayFab MPS
- AccelByte
- Unity Gaming Services
- Hathora
- AWS GameLift
- Edgegap

## 5. Conclusion

The most important thing is not who scores highest, but not grabbing the wrong layer:

- `Agones`'s score is not low — it simply does not address world and sync at all
- `Mirror` and `FishNet` do not score poorly — they are network layers by design
- `KBEngine` and `SpatialOS` score high on world model, but that does not mean they are the most realistic landing option for every team

The correct way to use this table:

- Choose frameworks by the problem you want to solve
- Do not choose frameworks by "total score"
