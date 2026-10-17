
# Architecture References

Apollo's authoritative target architecture is documented in the [architecture status and target model](../analysis/architecture-review.md), [architecture overview](../guide/architecture.md), and [instance-based GameServer design](../30-Instance_GameServer_Design.md).

BigWorld, KBEngine, AOI, and other MMO framework materials in this directory describe external implementations. They do not mean Apollo adopts those process models or provides the same capabilities. In particular, `BaseApp`, `CellApp`, `BaseAppMgr`, `Witness`, `Ghost`, and `Space` are not Apollo target concepts.

Apollo's target terms are:

| Name | Meaning |
|---|---|
| `Login` | Account authentication and login credentials. |
| `Gateway` | Client connections, sessions, and message routing. |
| `World` | A process that hosts instances and runs authoritative match logic. |
| `Instance` | One match and its temporary state. |

`login-app`, `gateway-app`, `world-app`, `base-app`, and `baseappmgr` are current prototype executable names. See [server applications](../apps/index.md) for their mapping to target responsibilities.
