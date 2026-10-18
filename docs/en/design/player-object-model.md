# Player Object Model

Persistent player objects and scene entities have distinct owners, lifecycles, and write boundaries. See [concepts and names](./concept-glossary.md).

## Ownership

| Object | Owner | State and lifecycle |
|---|---|---|
| PlayerAnchor | baseServer | Inventory, equipment, progression, quests, and base gameplay; loading, online retention, disconnection, and unloading are independent of Instances. |
| Avatar / Entity | authoritative Cell within a cell executor (current Scene semantics) | Position, combat health, skill execution, and temporary effects; created, transferred, and released under scene rules. |
| Session | gateway connection and online-directory association | Binds authenticated identity to a connection and rejects stale session versions. |

Only one Base writes the complete persistent player data; only one cell advances the scene entity. cell consumes gameplay snapshots and requests validated consumption or rewards from Base. Explicitly settle results that persist across matches instead of automatically treating every simulation field as profile data.

## Lifecycle

After authentication, the owning Base loads or restores persistent data through database. cell receives admission information and a read-only base-stat snapshot, then creates the scene entity. On departure or completion, Base validates and commits idempotent settlement while cell releases scene entities. Destroying an Instance does not destroy persistent data.

## Cross-server transfer

<ArchitectureDiagram kind="cross-server" />

Retain the owning Base and home-server affiliation. Do not move the full profile, storage affiliation, or database credentials. Source and destination cannot both simulate the entity. Reconcile ambiguous commits. Preserving an ongoing match also requires its shared Instance state; see [cross-server scenes](./cross-server-scenes.md).

## Base replacement

Stable ownership does not require an irreplaceable process. A Base replacement hands off persistent objects, sessions, and in-flight operations, then updates the logical Base endpoint while retaining storage affiliation. Drain writes before switching, reject stale-process writes afterward, and reconcile late settlement and retries.

## Implementation status

PlayerAnchor, Avatar, Scene, and Instance are reusable code types. Persistent-player and storage responsibilities still need to move out of the current WorldApp host and integrate with Base/database boundaries. This documentation update does not implement that migration. Track work in root todo.md.

worldServer manages Space/Cell placement without another simulation object. PlayerAnchor corresponds to the player-side Base entity; Avatar/Entity to Real/Cell entities. See the [mapping](./bigworld-apollo-mapping.md) for terms and gaps.
