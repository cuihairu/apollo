
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q6: Single-Server Architecture vs Distributed Architecture — How to Choose?

## Core Conclusion

This is not a question of "which one is more advanced", but a question of "which complexity is most suitable for the current stage".

- Single-server architecture suits early-stage validation, lightweight gameplay, smaller teams, and projects with limited operations capability
- Distributed architecture suits large worlds, high concurrency, long-term operation, and projects that need elastic scale-out and fault isolation
- A truly mature solution is usually not fully distributed from day one, but evolves step by step by bottleneck

---

## I. First Clarify the Concepts

In real discussions, "single server" and "distributed" are often used loosely. A more accurate division should be three layers:

### 1. Single Process, Single Machine

All networking, business logic, state, timers, and database access are in one process.

Characteristics:
- Simplest to develop and debug
- Memory is shared, so function call cost is low
- No cross-process serialization or network forwarding
- Capacity, isolation, and fault tolerance are all limited to a single process

### 2. Single Machine, Multiple Processes

Still deployed on one machine, but processes start being split by responsibility, for example gateway process, logic process, database proxy process.

Characteristics:
- Complexity is clearly lower than multi-machine distributed
- You can first establish process boundaries and protocol boundaries
- Convenient for a smooth later migration to multi-machine deployment
- Still limited to a single machine's CPU, memory, network, and failure domain

### 3. Multi-Machine Distributed

Different responsibilities or different shards are deployed on multiple machines, communicating via RPC, a message bus, or dedicated protocols.

Characteristics:
- Can scale out horizontally
- Can do resource isolation and fault isolation
- Deployment, debugging, monitoring, and consistency problems are all more complex
- Demands more from the team's engineering and operations capability

Therefore, the real path of many projects is not an either-or of "single server or distributed", but:

`single process single machine -> single machine multiple processes -> multi-machine distributed`

---

## II. The Essential Differences Between the Two Architectures

| Dimension | Single-Server Architecture | Distributed Architecture |
|------|----------|------------|
| Computing boundary | Within one process or one machine | Multiple processes, multiple machines |
| Communication | Function calls / shared memory | RPC / messages / network protocols |
| State management | Centralized local state | State needs sharding, replication, or migration |
| Failure blast radius | Process or whole-machine level | Component level, isolatable |
| Scale-out method | Upgrading the single machine's config | Horizontal scale-out |
| Debugging difficulty | Low | High |
| Operations cost | Low | High |
| Data consistency | Relatively simple | Clearly harder |
| Latency overhead | Low | Higher; the chain length must be controlled |

In essence, single server trades "simplicity" for development efficiency, while distributed trades "complexity" for capacity, elasticity, and isolation.

---

## III. Don't Look Only at Online Player Count

Online player count matters, but it is not the only metric. Architecture choice should look at at least five dimensions.

### 3.1 Whether the Gameplay Naturally Demands Spatial Splitting

If the game is one of these types, distributed has high value:

- Large-world MMORPG
- Large-map survival
- Parallel simulation of multiple regions
- High-density real-time interaction scenarios

Because the core bottleneck is not the number of login connections, but:

- Spatial simulation load
- AOI broadcast load
- Combat settlement load
- Entity migration and cross-region routing

If it is one of these types, single server is often more suitable:

- Card games
- Turn-based games
- Room-based mini-games
- Low-realtime social gameplay

The core pressure of these projects is usually the database, gateway connections, and activity peaks, rather than continuous spatial simulation.

### 3.2 Whether the Hotspot Can Be Cut Apart

Distributed does not automatically solve hotspots.

If 2000 people in one scene must share the same battlefield state, the same skill event stream, and the same AOI result, then more machines may not easily split that hotspot. Because the hotspot itself may be strongly coupled.

For these problems you need to answer first:

- Can it be partitioned by map
- Can it be split by room
- Can it be split by channel
- Can it be split by logical domain
- Can partial degradation be accepted

If the hotspot is inherently indivisible, distributed can only relieve peripheral pressure; it cannot eliminate the core hotspot.

### 3.3 Whether the Team Can Handle the Complexity

Distributed introduces a whole set of new problems:

- Service discovery
- Routing and addressing
- Timeout, retry, idempotency
- State migration
- Node removal and recovery
- Canary release
- Full-chain monitoring
- Distributed consistency

If the team is currently unstable even at single-machine performance analysis, online diagnosis, and stress-test modeling, going distributed early usually just spreads the problem across more nodes.

### 3.4 Whether Long-Term Operation Capability Is Needed

Projects that only do version validation, are cost-sensitive, have a short lifecycle — single server is usually more cost-effective.

But if the goal is:

- Long-term commercial operation
- Obvious peaks during activity periods
- Cross-server events needed
- Non-stop migration or elastic scale-out needed
- Strong dependence on disaster recovery and fast recovery

then the investment in distributed is usually worth it.

### 3.5 Whether Clear Bottlenecks Have Already Appeared

The correct migration method is not "anticipating it will be popular, so put the full distributed stack on first", but advancing based on actual bottlenecks.

Common signals include:

- A single process's CPU has long been near its ceiling
- A specific map or combat thread has become a stable hotspot
- Peripheral systems like login, chat, and leaderboard drag down the main logic
- Release, restart, and fault recovery costs are too high
- Upgrading a single machine can no longer effectively solve the problem

---

## IV. How to Judge in MMO Scenarios

### 4.1 When Single Server Is Enough

The following situations usually can start with single server or single machine multiple processes:

- The project is still validating gameplay
- The world scale is not large
- The per-scene player cap is not high
- Interaction intensity is not high
- Getting combat, quest, and progression chains working first is more important

Typical goals are:

- Launch as fast as possible
- Stress test as fast as possible
- See real player behavior as fast as possible

At this point what is most valuable is not "the architecture looks big", but "the system can iterate fast and problems can be located fast".

### 4.2 When Distributed Must Be Seriously Considered

The following situations usually indicate the distributed threshold is near:

- There is a continuously running large map or continuous space
- A single map must carry a large number of real-time entities
- AOI, broadcast, and combat settlement have become the main bottlenecks
- Different maps, channels, and battlefields need independent scale-out
- Cross-region player migration is a routine capability rather than an occasional requirement
- Online disaster recovery, live migration, and elastic scale-out are hard requirements

In these projects, separating the space logic layer and the account logic layer is usually important. The `BaseApp / CellApp` split in KBEngine, essentially, separates "player persistent state" from "spatial real-time state" so they can be scaled and migrated separately.

---

## V. Distributed Does Not Equal "Infinite Scalability"

Many documents like to write "in theory, as long as you keep adding machines, it can scale infinitely". This can only be used as a directional statement, not an engineering conclusion.

Actual limits usually come from:

- Single-scene hotspots that cannot be split
- Rising cross-node synchronization overhead
- Broadcast fan-out growing too fast
- Data consistency and rollback costs getting higher
- Schedulers and management nodes hitting bottlenecks
- Database, cache, and message queue becoming the new central bottleneck

A more accurate statement would be:

Distributed can significantly raise the system's ceiling, but whether it can actually scale depends on whether the load is splittable, whether the state is migratable, whether the chain is short enough, and whether the hotspots are controllable.

---

## VI. A More Practical Selection Method

### 6.1 Do Architecture Layering First, Then Decide Whether to Go Distributed

Even if currently deployed on a single machine, it is still recommended to split the boundaries clearly first:

- Access layer: connections, authentication, rate limiting
- Session layer: player online state, session routing
- Space layer: maps, AOI, combat, movement
- Data layer: persistence, cache, index
- Backend layer: mail, leaderboard, GM, events

The benefits of doing this are:

- Still simple in the single-machine stage
- Clearer module dependencies
- Later you can split processes along the boundaries instead of rewriting everything

### 6.2 Recommended Evolution Route

#### Stage 1: Single Process, Single Machine

Suitable for:
- Prototype validation
- First version of a small team
- Core gameplay not yet stable

Goal:
- Form a playable version as fast as possible
- Establish stress testing and monitoring basics

#### Stage 2: Single Machine, Multiple Processes

First split out the high-value boundaries:

- Gateway
- Scene logic
- Data access
- Backend async tasks

This stage is the most critical, because it determines whether you can smoothly enter true distributed later.

#### Stage 3: Multi-Machine Distributed

Only advance after the following premises hold:

- Load hotspots are clear
- Protocol boundaries are stable
- Monitoring, logging, and tracing are in place
- Operations processes are mature

At this point, splitting the gateway, session, space, and backend systems by machine has a higher input-output ratio.

---

## VII. A Pragmatic Comparison Table

| Scenario | More Suitable Choice | Reason |
|------|--------------|------|
| First MMO prototype of a small team | Single server or single machine multiple processes | Prioritize validating gameplay and processes |
| Room-based game | Single server | State is naturally isolated by room; a single machine is enough |
| Small-to-medium turn-based | Single server | No high demands on realtime or spatial continuity |
| Large-world MMORPG | Distributed | Spatial simulation, AOI, migration all need splitting |
| Large amounts of cross-server events | Distributed | Independent scale-up/down and isolation needed |
| Mature project in operation period | Distributed | Release, disaster recovery, elasticity requirements are higher |

Special attention is needed:

- `MMORPG = distributed` is not an absolute conclusion; a small-scale MMO can also start single-server
- `FPS/MOBA = distributed` is also not an absolute conclusion; many room-based match services are essentially large numbers of independent single-room instances
- What truly matters is not the genre name, but the state coupling degree, realtime requirements, capacity targets, and operation requirements

---

## VIII. Practical Suggestions

### 8.1 Priorities for Small Teams

If team size is limited, it is recommended to guarantee four things first:

- Architecture boundaries are clear
- Stress test models are real
- Monitoring and logging are usable
- Hotspots can be located

These capabilities are more important than "going distributed first".

### 8.2 When Not to Rush Into Distributed

The following situations are usually not suitable for immediately going distributed:

- Core gameplay is not decided yet
- Even the single-machine bottleneck has not been measured
- No online monitoring and tracing
- No stable automated deployment
- The team lacks experience in fault locating

Because at this point the rate of increase in complexity is often faster than the rate of increase in benefit.

### 8.3 When Investing in Distributed Is Worth It

The following situations usually indicate that investing in distributed is already worthwhile:

- The single-machine performance ceiling is being hit continuously
- Hotspots can be split relatively cleanly
- The business enters the long-term operation stage
- The architecture team and operations processes are in place
- Single-point failures and scale-out efficiency start to affect business goals

---

## IX. Summary

A more prudent way to judge is:

1. First use a single machine to stabilize gameplay and engineering basics
2. Then use multiple processes to establish module boundaries
3. Finally, do distributed splitting against real bottlenecks

Therefore, single server and distributed are not opposites; they are different forms of the same project at different stages.

If the project's core is "fast validation", prioritize single server.
If the project's core is "a continuously operated large-world high-concurrency simulation", prioritize distributed.
If you are not sure now, it usually means the most reasonable choice is to first build an evolvable single-machine architecture, rather than making the system heavy in one step.

---

## References

- [KBEngine GitHub](https://github.com/kbengine/kbengine)
- [KBEngine Lab - Engine Overview](https://www.kbelab.com/manual/engine-overview.html)
- [Scalable Game Server Architecture - GDC](https://www.gdcvault.com/play/1022800/Scalable-Game-Server-Architecture-in)
