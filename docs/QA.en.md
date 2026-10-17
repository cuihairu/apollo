[English](QA.en.md) | [中文](QA.md)

# Game Server Architecture QA

> This index covers general game-server architecture and external engine mechanisms. BigWorld/KBEngine questions are comparative references and do not imply that Apollo adopts their topology.
>
> Note: the linked detail pages under `qa/` are Chinese-only in this round — the
> subdirectory bilingualizing scope is registered in `todo.md` for the next round.
> All links below point at the Chinese detail pages.

## 1. Architecture Design

| # | Question | Status | Link |
|---|------|------|------|
| 1 | Draw the MMORPG server architecture you know, explaining each component's responsibilities and communication | Done | [q1-server-architecture.md](qa/q1-server-architecture.md) |
| 2 | What do CellApp and BaseApp each do in the BigWorld architecture? Why is it split this way? | Done | [q2-bigworld-cell-base.md](qa/q2-bigworld-cell-base.md) |
| 3 | What is AOI (Area of Interest)? What implementations exist? What are the pros and cons of each? | Done | [q3-aoi-implementation.md](qa/q3-aoi-implementation.md) |
| 4 | How do you implement seamless switching of large maps? How do you handle cross-server player movement? | Done | [q4-seamless-world.md](qa/q4-seamless-world.md) |
| 5 | How should the server be designed to support dynamic scaling? | Done | [q5-dynamic-scaling.md](qa/q5-dynamic-scaling.md) |
| 6 | Single-server architecture vs distributed architecture — how to choose? | Done | [q6-single-vs-distributed.md](qa/q6-single-vs-distributed.md) |
| 7 | In-process architecture vs multi-process architecture — what are the respective advantages and disadvantages? | Done | [q7-process-architecture.md](qa/q7-process-architecture.md) |
| 8 | How do you design to avoid a single point of failure? | Done | [q8-avoid-spof.md](qa/q8-avoid-spof.md) |
| 9 | What if CellAppMgr goes down? What solutions exist? | Done | [q9-cellappmgr-failure.md](qa/q9-cellappmgr-failure.md) |
| 10 | How do you implement cross-server features (cross-server battlefields, cross-server chat)? | Done | [q10-cross-server.md](qa/q10-cross-server.md) |

## 2. Network Communication

| # | Question | Status | Link |
|---|------|------|------|
| 11 | TCP vs UDP vs KCP — what are their respective use cases in MMOs? | Done | [q11-transport-protocol.md](qa/q11-transport-protocol.md) |
| 12 | How do you handle network jitter and packet loss? | Done | [q12-network-jitter.md](qa/q12-network-jitter.md) |
| 13 | What is reliable UDP? How do you implement it? | Done | [q13-reliable-udp.md](qa/q13-reliable-udp.md) |
| 14 | How do you design a message protocol? Protobuf vs JSON vs a custom protocol? | Done | [q14-message-protocol.md](qa/q14-message-protocol.md) |
| 15 | How do you implement message compression? | Done | [q15-message-compression.md](qa/q15-message-compression.md) |
| 16 | What is synchronization? How do client and server states stay in sync? | Done | [q16-state-sync.md](qa/q16-state-sync.md) |
| 17 | How do you handle latency compensation and prediction? | Done | [q17-latency-compensation.md](qa/q17-latency-compensation.md) |
| 18 | What is the Ghost/Shadow mechanism? | Done | [q18-ghost-mechanism.md](qa/q18-ghost-mechanism.md) |
| 19 | How do you prevent message replay attacks? | Done | [q19-replay-attack.md](qa/q19-replay-attack.md) |
| 20 | How does a long-lived connection keep its heartbeat? How do you design disconnect and reconnect? | Done | [q20-heartbeat-reconnect.md](qa/q20-heartbeat-reconnect.md) |
| 21 | What application scenarios does WebSocket have in MMOs? | Done | [q21-websocket-mmo.md](qa/q21-websocket-mmo.md) |
| 22 | How do you handle out-of-order network messages? | Done | [q22-out-of-order.md](qa/q22-out-of-order.md) |
| 23 | How do you implement RPC calls? | Done | [q23-rpc-implementation.md](qa/q23-rpc-implementation.md) |
| 24 | How do you design a broadcast mechanism? How do you optimize large-scale broadcasts? | Done | [q24-broadcast-optimization.md](qa/q24-broadcast-optimization.md) |
| 25 | What determines the connection limit? How do you break the C10K problem? | Done | [q25-c10k-problem.md](qa/q25-c10k-problem.md) |
| 26 | KBEngine message routing: how are messages efficiently forwarded between Gateway, Proxy, and CellApp? | Done | [q26-kbengine-message-routing.md](qa/q26-kbengine-message-routing.md) |
| 27 | How do Real, Ghost, and Shadow entities convert between each other? How is synchronization efficient? | Done | [q27-entity-ghost-shadow-relationship.md](qa/q27-entity-ghost-shadow-relationship.md) |
| 28 | Does KBEngine have a registry center? How are CellApps deployed and communicated? Is the Actor model applicable? | Done | [q28-kbengine-registry-actor-transport.md](qa/q28-kbengine-registry-actor-transport.md) |
| 29 | How do CellApps communicate with each other? How do they discover their space location? | Done | [q29-cellapp-communication-space-discovery.md](qa/q29-cellapp-communication-space-discovery.md) |
| 30 | What is a KBEngine Space? How does it differ from physical space partitioning? | Done | [q30-kbengine-space-concept.md](qa/q30-kbengine-space-concept.md) |
| 31 | How does KBEngine broadcast efficiently? How does it ensure messages are not duplicated? | Done | [q31-kbengine-broadcast-dedup.md](qa/q31-kbengine-broadcast-dedup.md) |
| 32 | How do KBEngine CellApps communicate on the same machine vs across machines? Is there source-code evidence? | Done | [q32-kbengine-cellapp-communication.md](qa/q32-kbengine-cellapp-communication.md) |
| 33 | Why doesn't KBEngine pursue extreme performance optimization? If you were to, how would you improve it? | Done | [q33-kbengine-performance-optimization.md](qa/q33-kbengine-performance-optimization.md) |

## 3. Data Storage

| # | Question | Status | Link |
|---|------|------|------|
| 26 | When is player data stored to the database? Full save vs incremental save? | Done | [q26-data-persistence.md](qa/q26-data-persistence.md) |
| 27 | How do you design database table structures? | Done | [q27-database-schema.md](qa/q27-database-schema.md) |
| 28 | How do you solve data consistency issues? | Done | [q28-data-consistency.md](qa/q28-data-consistency.md) |
| 29 | What application scenarios does Redis have in MMOs? | Done | [q29-redis-in-mmo.md](qa/q29-redis-in-mmo.md) |
| 30 | How do you design a leaderboard system? | Done | [q30-leaderboard-system.md](qa/q30-leaderboard-system.md) |
| 31 | How do you design a friend system? | Done | [q31-friend-system.md](qa/q31-friend-system.md) |
| 32 | How do you design a mail system? | Done | [q32-mail-system.md](qa/q32-mail-system.md) |
| 33 | How do you handle hot data? | Done | [q33-hot-data.md](qa/q33-hot-data.md) |
| 34 | How do you design a database connection pool? | Done | [q34-connection-pool.md](qa/q34-connection-pool.md) |
| 35 | How do you implement database sharding? | Done | [q35-database-sharding.md](qa/q35-database-sharding.md) |
| 36 | How do you handle database transactions? | Done | [q36-database-transaction.md](qa/q36-database-transaction.md) |
| 37 | How do you implement a cache eviction strategy for data? | Done | [q37-cache-eviction.md](qa/q37-cache-eviction.md) |
| 38 | How do you prevent data from being tampered with? | Done | [q38-data-integrity.md](qa/q38-data-integrity.md) |

## 4. Game Logic

| # | Question | Status | Link |
|---|------|------|------|
| 39 | How do you design a combat system? | Done | [q39-combat-system.md](qa/q39-combat-system.md) |
| 40 | What is the ECS architecture? What advantages does it have in games? | Done | [q40-ecs-architecture.md](qa/q40-ecs-architecture.md) |
| 41 | How do you design a skill system? | Done | [q41-skill-system.md](qa/q41-skill-system.md) |
| 42 | How do you design a Buff/Debuff system? | Done | [q42-buff-system.md](qa/q42-buff-system.md) |
| 43 | How do you design an attribute system? | Done | [q43-attribute-system.md](qa/q43-attribute-system.md) |
| 44 | How do you design an inventory system? | Done | [q44-inventory-system.md](qa/q44-inventory-system.md) |
| 45 | How do you design a trading system? | Done | [q45-trading-system.md](qa/q45-trading-system.md) |
| 46 | How do you design an auction house? | Done | [q46-auction-house.md](qa/q46-auction-house.md) |
| 47 | How do you design a guild system? | Done | [q47-guild-system.md](qa/q47-guild-system.md) |
| 48 | How do you design a quest system? | Done | [q48-quest-system.md](qa/q48-quest-system.md) |
| 49 | How do you design an instance system? | Done | [q49-instance-system.md](qa/q49-instance-system.md) |
| 50 | How do you design a matchmaking system? | Done | [q50-matchmaking.md](qa/q50-matchmaking.md) |
| 51 | How do you design an AI system? | Done | [q51-ai-system.md](qa/q51-ai-system.md) |
| 52 | How do you design a pathfinding system? | Done | [q52-pathfinding.md](qa/q52-pathfinding.md) |
| 53 | How do you design scene management? | Done | [q53-scene-management.md](qa/q53-scene-management.md) |
| 54 | How do you handle multiple players contending for a monster/resource simultaneously? | Done | [q54-loot-contention.md](qa/q54-loot-contention.md) |
| 55 | How do you design damage calculation? | Done | [q55-damage-calculation.md](qa/q55-damage-calculation.md) |

## 5. Performance Optimization

| # | Question | Status | Link |
|---|------|------|------|
| 56 | How do you do performance analysis? What tools are there? | Done | [q56-profiling-tools.md](qa/q56-profiling-tools.md) |
| 57 | How do you optimize memory usage? | Done | [q57-memory-optimization.md](qa/q57-memory-optimization.md) |
| 58 | What is an object pool? How do you design one? | Done | [q58-object-pool.md](qa/q58-object-pool.md) |
| 59 | How do you reduce lock contention? | Done | [q59-reduce-lock-contention.md](qa/q59-reduce-lock-contention.md) |
| 60 | How do you design a timer system? | Done | [q60-timer-system.md](qa/q60-timer-system.md) |
| 61 | How do you optimize the logging system? | Done | [q61-log-optimization.md](qa/q61-log-optimization.md) |
| 62 | How do you do stress testing? | Done | [q62-stress-testing.md](qa/q62-stress-testing.md) |
| 63 | How do you optimize database queries? | Done | [q63-database-optimization.md](qa/q63-database-optimization.md) |
| 64 | How do you optimize network bandwidth? | Done | [q64-bandwidth-optimization.md](qa/q64-bandwidth-optimization.md) |
| 65 | How do you reduce CPU cache misses? | Done | [q65-cache-miss.md](qa/q65-cache-miss.md) |
| 66 | What applications does SIMD have in games? | Done | [q66-simd-gaming.md](qa/q66-simd-gaming.md) |
| 67 | How do you optimize hotspot code? | Done | [q67-hotspot-optimization.md](qa/q67-hotspot-optimization.md) |
| 68 | How do you design memory monitoring? | Done | [q68-memory-monitoring.md](qa/q68-memory-monitoring.md) |
| 69 | What issues should be considered for a single server hosting 5000 players? | Done | [q69-5000-ccu.md](qa/q69-5000-ccu.md) |
| 70 | How do you design a high-performance message queue? | Done | [q70-message-queue.md](qa/q70-message-queue.md) |

## 6. Concurrency and Multithreading

| # | Question | Status | Link |
|---|------|------|------|
| 71 | Multithreading vs multiprocessing — how to choose? | Done | [q71-thread-vs-process.md](qa/q71-thread-vs-process.md) |
| 72 | How do you avoid deadlocks? | Done | [q72-avoid-deadlock.md](qa/q72-avoid-deadlock.md) |
| 73 | What is the Actor model? What advantages does it have? | Done | [q73-actor-model.md](qa/q73-actor-model.md) |
| 74 | How do you design lock-free data structures? | Done | [q74-lock-free.md](qa/q74-lock-free.md) |
| 75 | How do you design a thread pool? How are tasks scheduled? | Done | [q75-thread-pool.md](qa/q75-thread-pool.md) |
| 76 | How do you implement asynchronous I/O? | Done | [q76-async-io.md](qa/q76-async-io.md) |
| 77 | What are the differences between IOCP and Epoll? | Done | [q77-iocp-epoll.md](qa/q77-iocp-epoll.md) |
| 78 | How do you handle race conditions? | Done | [q78-race-condition.md](qa/q78-race-condition.md) |
| 79 | How do you design thread-safe containers? | Done | [q79-thread-safe-container.md](qa/q79-thread-safe-container.md) |
| 80 | What are the application scenarios for coroutines in a game server? | Done | [q80-coroutine.md](qa/q80-coroutine.md) |

## 7. Security

| # | Question | Status | Link |
|---|------|------|------|
| 81 | How do you prevent cheating tools? | Done | [q81-anti-cheat.md](qa/q81-anti-cheat.md) |
| 82 | How do you prevent item duplication? | Done | [q82-item-duping.md](qa/q82-item-duping.md) |
| 83 | How do you prevent speed hacks? | Done | [q83-speed-hack.md](qa/q83-speed-hack.md) |
| 84 | How do you prevent memory modification? | Done | [q84-memory-hack.md](qa/q84-memory-hack.md) |
| 85 | How do you prevent packet forgery? | Done | [q85-packet-forgery.md](qa/q85-packet-forgery.md) |
| 86 | How do you design a permission system? | Done | [q86-permission-system.md](qa/q86-permission-system.md) |
| 87 | How is sensitive data encrypted in transit? | Done | [q87-encryption.md](qa/q87-encryption.md) |
| 88 | How do you prevent SQL injection? | Done | [q88-sql-injection.md](qa/q88-sql-injection.md) |
| 89 | How do you design rate limiting and anti-abuse mechanisms? | Done | [q89-rate-limiting.md](qa/q89-rate-limiting.md) |
| 90 | How do you counter DDoS attacks? | Done | [q90-ddos-protection.md](qa/q90-ddos-protection.md) |

## 8. Operations and Monitoring

| # | Question | Status | Link |
|---|------|------|------|
| 91 | How do you monitor server state? | Done | [q91-server-monitoring.md](qa/q91-server-monitoring.md) |
| 92 | How do you design a logging system? | Done | [q92-log-system.md](qa/q92-log-system.md) |
| 93 | How do you quickly locate production issues? | Done | [q93-troubleshooting.md](qa/q93-troubleshooting.md) |
| 94 | How do you implement hot updates? | Done | [q94-hot-reload.md](qa/q94-hot-reload.md) |
| 95 | How do you implement canary deployment? | Done | [q95-canary-deployment.md](qa/q95-canary-deployment.md) |
| 96 | How do you design configuration management? | Done | [q96-config-management.md](qa/q96-config-management.md) |
| 97 | How do you implement server restart without losing data? | Done | [q97-graceful-shutdown.md](qa/q97-graceful-shutdown.md) |
| 98 | How do you design a GM command system? | Done | [q98-gm-command-system.md](qa/q98-gm-command-system.md) |
| 99 | How do you implement server cluster management? | Done | [q99-cluster-management.md](qa/q99-cluster-management.md) |
| 100 | How do you do capacity planning? | Done | [q100-capacity-planning.md](qa/q100-capacity-planning.md) |

## 9. Scripting Systems

| # | Question | Status | Link |
|---|------|------|------|
| 101 | Why embed a scripting language? Lua vs Python — how to choose? | Done | [q101-script-language.md](qa/q101-script-language.md) |
| 102 | How does C++ call Lua? How does Lua call C++? | Done | [q102-cpp-lua-binding.md](qa/q102-cpp-lua-binding.md) |
| 103 | How do you implement hot updates for scripts? | Done | [q103-script-hot-reload.md](qa/q103-script-hot-reload.md) |
| 104 | How do you limit script execution time? | Done | [q104-script-timeout.md](qa/q104-script-timeout.md) |
| 105 | How do you debug scripts? | Done | [q105-script-debug.md](qa/q105-script-debug.md) |

## 10. Field Experience

| # | Question | Status | Link |
|---|------|------|------|
| 106 | What has been the hardest technical problem you have faced? How did you solve it? | Done | [q106-toughest-problem.md](qa/q106-toughest-problem.md) |
| 107 | What severe production incidents have you seen? How were they handled? | Done | [q107-production-incident.md](qa/q107-production-incident.md) |
| 108 | Building an MMO server from 0 to 1 — what is your approach? | Done | [q108-mmo-from-scratch.md](qa/q108-mmo-from-scratch.md) |
| 109 | How do you assess a server's hosting capacity? | Done | [q109-capacity-assessment.md](qa/q109-capacity-assessment.md) |
| 110 | What do you consider the hardest part of an MMORPG server? | Done | [q110-hardest-part.md](qa/q110-hardest-part.md) |
| 111 | How do you collaborate with client-side engineers? | Done | [q111-client-collab.md](qa/q111-client-collab.md) |
| 112 | How do you communicate technical plans with game designers? | Done | [q112-designer-comm.md](qa/q112-designer-comm.md) |
| 113 | How do you balance code quality and development speed? | Done | [q113-quality-vs-speed.md](qa/q113-quality-vs-speed.md) |
| 114 | Which open-source game server frameworks are you familiar with? What are the features of each? | Done | [q114-open-source-frameworks.md](qa/q114-open-source-frameworks.md) |
| 115 | If you were to redesign it, how would you improve the current framework? | Done | [q115-framework-redesign.md](qa/q115-framework-redesign.md) |
| 116 | What C++ design pattern practices exist in KBEngine? Why designed this way? | Done | [q116-cpp-design-patterns-in-kbengine.md](qa/q116-cpp-design-patterns-in-kbengine.md) |

---

## 11. Deep-Dive Questions

| # | Question | Status | Link |
|---|------|------|------|
| 117 | If you have already answered the MMO fundamentals, what deeper questions should you continue thinking about? | Done | [q117-interviewer-follow-up.md](qa/q117-interviewer-follow-up.md) |

---

**Progress: 117/117 done**

*117 questions in total, covering the core areas of MMO framework development*
