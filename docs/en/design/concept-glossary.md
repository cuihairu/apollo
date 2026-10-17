# Apollo Concepts and Names

The target process roles are `Login`, `Gateway`, and `World`. Gameplay instances use the existing `Instance` type. See [server applications](../apps/index.md) for the implementation mapping.

| Name | Responsibility and boundary |
|---|---|
| `Login` | Authenticate accounts, issue login credentials, and host routing, placement, and migration coordination. |
| `Gateway` | Own client connections, associate sessions, and route messages using the player's current location. |
| `World` | The game-logic process; manage scenes and multiple independent Instances, advance authoritative simulation, and host player data and persistence. The executable is `world-app`, with entry type `WorldApp`. |
| `Scene` | An internal simulation unit owning entities, AOI, admission, and ticks; not a process name. |
| `Instance` | A temporary gameplay instance created for a group of players. Players enter, clear enemies or complete objectives, receive settlement, leave, and release the instance, temporary scene, and resources. |
| `Player` | Persistent player identity; persistent data and an in-match runtime entity have distinct responsibilities. |
| `Avatar` / `Entity` | Runtime simulation entities. A player has only one authoritative entity accepting input and state writes during a match. |
| `AOI` | Scene-local entity visibility. |
| `Persistence` | Modules that store data and settlement results across instances. |

## Instance lifecycle

Target flow: group request → select World → create Instance and Scene → validate members → admit and run → clear objectives → settle → players leave → destroy the instance and temporary scene.

Reuse the existing eight-state lifecycle: `Create → Initialize → Waiting → Running → Finishing → Rewarding → Draining → Destroyed`. Settlement must be idempotent; in-flight requests and departing players need explicit cleanup. Empty groups, timeouts, and failed runs also need defined termination paths.

## Authority and updates

World reserves cross-server scenes: home-server affiliation is separate from runtime location. Cross-server play retains the original affiliation and a single active authority. Local and cross-server gameplay reuse Scene and Instance; see [Cross-server scenes](./cross-server-scenes.md).

A cross-World transfer freezes the source, restores and confirms the target, changes Gateway routing, and cleans up the source. Transfer snapshots may coexist briefly, but there is only one authoritative executor.

Protocol updates change contracts and regenerate artifacts, requiring version compatibility and rollout validation. Code-only updates preserve the wire contract and can use Lua module replacement or C++ process rollout. Runtime primitives do not imply a complete production rollout path.

World does not imply a seamless persistent world. Apollo targets tower defense, co-op PvE/PvP, arenas, and group-created instances destroyed after completion. BigWorld/KBEngine names are reserved for external-engine references.
