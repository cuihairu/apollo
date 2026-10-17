# World and Instance Design

Apollo targets lightweight multiplayer gameplay. `World` is the game-logic process. `Instance` is a temporary gameplay instance created for a group of players and destroyed after objectives, settlement, and cleanup. See [concepts and names](design/concept-glossary.md).

## Processes and objects

```text
Client → Login (authentication and credentials)
Client → Gateway (connections and routing) → World (game-logic process)
                                             ├── Scene (entities, AOI, tick)
                                             └── Instance (lifecycle, associated Scene)
```

One World can host multiple isolated instances. Reuse the existing Instance lifecycle and Scene simulation containers. A World may also host persistent scenes; gameplay determines whether a temporary Instance is required.

## Temporary instance flow

1. A group requests an activity; the server validates membership, capacity, and admission rules.
2. Select a World with capacity, create an Instance and temporary Scene, and bind gameplay configuration.
3. Admit members and start gameplay when ready; run enemy spawning, combat, and objective checks.
4. On success or failure, stop gameplay progression and commit idempotent, recoverable settlement.
5. Players leave or are removed by policy. Reject late input; release entities, AOI, tasks, and scene resources; destroy the Instance.

Reuse `Create → Initialize → Waiting → Running → Finishing → Rewarding → Draining → Destroyed`. Empty groups, disconnection, disbanding, and timeouts need explicit cleanup without dropping valid settlement results.

## Authority and cross-server play

Persistent player data uses persistence modules; the current Scene in World authoritatively advances the player's match entity. Cross-World transfer requires source freeze, export, target confirmation, route switching, and source cleanup. Only one entity can accept player input and authoritative writes at a time. Validate existing in-process SceneTransfer separately from cross-process takeover.

## Updates

Protocol updates regenerate XML/XSD contract artifacts; version compatibility and rolling release validation remain incomplete. Code-only updates preserve the protocol: Lua provides preparation, tick-boundary commit, observation, and rollback; C++ uses process replacement.

World drain stops assigning new instances and waits for settlement and resource cleanup before exiting. A forced-stop deadline needs defined failure and recovery behavior. Gateway owns TCP connections; replicas do not automatically transfer sockets. Continuing active instances across versions requires state transfer, compatibility checks, and rollback.

## Implementation status

Reuse World, Scene, Instance, SceneTransfer, BattleRuntime, and Lua update primitives. `world-app` currently creates a default Scene/Instance; group-triggered creation, dynamic placement, objective settlement, and resource cleanup are not integrated end to end. Client broadcast delivery also has prototype gaps. See the root [`todo.md`](https://github.com/cuihairu/apollo/blob/main/todo.md).
