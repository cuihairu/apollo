[English](30-Compact_GameServer_Design.en.md) | [中文](30-Compact_GameServer_Design.md)

# Compact GameServer Design (Tower Defense / Fixed-Map Adaptation)

> Purpose: On top of the general MMO architecture, provide a slimmed-down GameServer
> form for fixed-map, lightweight gameplay such as tower defense and circular-green
> games — reusing existing infrastructure while reducing service-split costs.
> Term naming (2026-09-30, docs/design/concept-glossary): the "Battle" in this article
> actually means an **instance** (instance/dungeon, corresponding to KBE Space) — one
> tower-defense room = one instance; cross-engine concepts follow the glossary.

## 1. Scenario and Requirements
- Limited number of maps (1 to several dozen), with relatively few players per map (<200).
- No dynamic instance scheduling; gameplay is more wave-based and defense-oriented.
- It is acceptable to handle AOI/combat/scene logic within a single process, reducing operational complexity.
- Still want to retain unified capabilities such as NetCore, DataProxy, configuration, plugins, monitoring, and SDK, with a smooth upgrade path to MMO mode in the future.

## 2. Architecture Comparison

```
MMO mode: Gate -> World -> Scene Orchestrator -> baseapp + AOI + instance (historical name Battle)
Compact mode: Gate -> Compact GameServer (Scene + AOI + Battle in one)
```

The Compact GameServer still interacts with other services (DataProxy, Social, LogAgent, etc.) via Transport/Registry.

## 3. Module Merging Strategy

| Module | MMO mode | Compact mode description |
|------|----------|------------------|
| Scene Orchestrator | Independent service | Merged into the Compact GS SceneManager, maintaining a fixed-map instance table |
| ZoneServer | Multi-instance/multi-process | Runs as threads or coroutines, one thread per map; multiple logical instances can be configured within one process |
| AOI Service | Independent cluster | Uses the embedded AOI module (sharing `SceneAoi` in the same process as baseapp — the current implementation name, P1-3 convergence; `AOIManager` below refers to the same thing), still shardable |
| Instance (historical name Battle Service) | Optionally independent | A tower-defense room is fine as an internal module; large-scale gameplay can still call an external instance process |
| Transport | AOI/baseapp interconnect | Compact mode can disable the AOI↔baseapp Transport while keeping the Gate/DataProxy channels |
| WorldServer | Global scheduling | For gameplay simple enough, a single Compact GS can manage it — or keep a lightweight World to handle accounts/matching |

## 4. Compact GameServer Structure

```
CompactGameServer
 ├─ NetEndpoint (communicates with Gate)
 ├─ SceneManager
 │    └─ SceneInstance (fixed map)
 │         ├─ SceneAoi (embedded, P1-3 implementation name)
 │         ├─ BattleModule
 │         ├─ WaveManager (tower-defense specific)
 │         └─ ScriptEngine (Lua)
 ├─ PlayerManager
 ├─ DataProxyClient
 ├─ SocialClient / TransportAdapter
 └─ Monitoring/Config/Plugin
```

- SceneManager maintains a `scene_config` table (map_id, max players, wave configuration).
- Each SceneInstance runs its own Tick, responsible for unit spawning, waves, tower building, Buffs, etc.
- AOI (implemented as `SceneAoi`) uses the same data structures as the standalone AOI, but no longer crosses process boundaries; if extension is needed in the future, the AOI interface can be kept and switched to a remote implementation.

## 5. Threading and Scaling

- Recommended configuration: single machine, multithreaded, with thread pools divided by map type. For example, a 4-core machine runs 4 Scene Threads, each managing several maps.
- Multi-instance deployment is supported: each Compact GS can be responsible for a group of maps; `map_range` is registered via the G-1 fleet topology (machined daemon + UDP broadcast — net-abstraction §7; no Consul, ruling of 2026-09-30), and Gate/World routes players to the corresponding instance based on their map.
- If horizontal scaling is needed (more players/maps), deploy more Compact GS instances or upgrade to the full baseapp/AOI layering.

## 6. Data and Configuration

- Player data is still accessed via DataProxy; Compact mode only needs to handle transient state within the Scene.
- The configuration hot-reload flow is identical to the main framework; tower-defense-specific configuration (waves, tower attributes, enemy paths) lives in the Excel/Lua configuration system.
- Attributes/IDL likewise use the unified generation tooling (`docs/22`).

## 7. Impact on Gate/SDK

- Gate still forwards players to the corresponding `CompactGameServer` by session.
- The SDK/client protocol is completely unchanged; it is just that the logical server has fewer hops and lower latency.
- The message flow for coordinate/state synchronization is the same as in MMO mode, making it easy for clients to reuse.

## 8. Migration and Upgrade

- If maps/gameplay expand in the future and independent AOI/Scene services are needed, all that is required is:
  1. Extract the SceneManager/AOI module as a service.
  2. The Gate/World routing logic is unchanged.
  3. The Scene/AOI interfaces in Compact mode are already consistent with the standalone version, so migration cost is low.

## 9. Monitoring and Operations

- Key metrics: players per map, frame time, AOI event count, wave processing time, combat settlement time.
- Failure strategy: when a single instance fails, World/Gate routes players to a standby instance, or rolls them back to the main city.
- Graceful restart is supported: Scene state can be serialized (current wave, enemy state) and restored after restart.

## 10. Roadmap
1. Implement the Compact GameServer core framework (SceneManager + AOI + embedded instance).
2. Integrate existing modules such as NetCore/Transport/DataProxy/plugins/configuration.
3. Wire up a tower-defense gameplay example (waves, tower building, skills) and validate the end-to-end flow.
4. The monitoring and operations solution is shared with the main framework, supplemented with tower-defense-specific metrics.
5. Reserve interfaces for a future upgrade to the full baseapp/AOI layering.

---

This design lets the framework serve both normal MMOs and tower-defense or fixed-map games in a lighter form, while keeping room for future evolution.
