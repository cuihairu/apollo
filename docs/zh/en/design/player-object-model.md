# Player Object Model

Persistent player objects and scene entities have distinct owners, lifecycles, and write boundaries. See [concepts and names](./concept-glossary.md).

## Industry Terms and Naming Evidence

Avatar is established character terminology, but its scope varies by engine. The name implies neither Cell-only execution nor exclusive use for players.

| Source | Original usage | Implication for Apollo |
|---|---|---|
| [BigWorld 2.1 official glossary, archived](https://howarduong.github.io/github.io/doc/html/glossary_of_terms/glossary_of_terms.html) | An Avatar is a character-associated entity, including player characters and NPCs; Entity is a basic addressable object. | Character type and execution location are separate dimensions. Avatar is not CellApp. This cites 2.1 terminology, not a deployment restriction for Apollo. |
| [BigWorld 2.1 official tutorial §3.4, archived](https://howarduong.github.io/github.io/doc/html/bigworld_tutorial/bigworld_tutorial.html) | The same Avatar type has Base and Cell scripts; its Base part derives from Proxy. | Avatar can have a Base part, and is neither a scene-only object nor a mandatory engine player-class name. |
| [KBEngine entity definitions](https://kbengine.github.io/docs/programming/entitydef.html), [Base Avatar](https://github.com/kbengine/kbengine_demos_assets/blob/master/scripts/base/Avatar.py), [Cell Avatar](https://github.com/kbengine/kbengine_demos_assets/blob/master/scripts/cell/Avatar.py) | Entities have base/cell/client parts as needed; demo Avatar derives from Proxy on Base and Entity on Cell. | Reuse entity Base/Cell-part terminology. A demo type name is neither a process name nor proof that Apollo implements the full model. |
| [Unreal official Gameplay Framework](https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-framework-in-unreal-engine) | Pawn is a world object controlled by a player or AI; Character derives from Pawn, while PlayerState and Controller have separate roles. | Separate representation, control, and state, without equating PlayerState with persistent Base or importing the class names wholesale. |
| [Unity official Avatar/retargeting documentation](https://docs.unity.com/en-us/engine/6000.6/manual/animation-section/animation-mecanim/avatar-creationand-setup/retargeting) | A configured Avatar supplies standard humanoid bone hierarchy/names for animation retargeting. | Unity's animation Avatar is distinct from a server entity; Avatar has no identical class meaning across engines. |

Naming assessment: Avatar is suitable character terminology, but a single Avatar type implements only the player's Cell simulation part, narrower than BigWorld's definition. The former PlayerAnchor name is not shared entity terminology in the official sources reviewed here, and Anchor implied a permanent hosting Base process it never guaranteed; the name was dropped in the 2026-10 naming migration and no longer appears in production code or docs.

Describe responsibilities as **the player character entity's Base part and Cell part**, mapping implementation references to `apollo::game::base::Avatar` (Base part) and `apollo::game::cell::Avatar` (Cell part). Do not present this pair as common upstream types or add another synonymous container/class. Production code carries no aliases or forwarding headers; the old class names are gone.

| Documentation concept | Current implementation | Meaning |
|---|---|---|
| Player character entity | Association of persistent and simulation objects | Business semantics, not a claim of a complete generic distributed Entity implementation. |
| Player character Base part | `apollo::game::base::Avatar` | Independent of connection, Cell object, and Instance; supports offline activation as a target, and hosting-process migration. |
| Player character Cell part | `apollo::game::cell::Avatar` | Current player simulation object, not the whole logical character or every Cell/Real entity; NPCs and monsters can use other Entity types. |
| Client session | Session and gateway connection | Connection state, not the character entity itself. |

### Apollo Code Naming

The Base part is implemented as `apollo::game::base::Avatar` (modules/game/base) and the Cell part as `apollo::game::cell::Avatar` (modules/game/cell), with one-way dependencies cell→base→core; module/namespace identifies each distributed part. Use unqualified Avatar only in an unambiguous context. Do not combine the parts into shared memory or rename every NPC/Entity to Avatar. These namespaces are Apollo conventions based on the upstream Base/Cell-part model, not claims about BigWorld C++ type names.

The naming migration is complete (root [todo.md, P0.1](https://github.com/cuihairu/apollo/blob/main/todo.md)): PlayerAnchor→`base::Avatar` and `world::Avatar`→`cell::Avatar`, with headers, supporting symbols, and callers updated and transitional mappings removed; wire state strings and contract fields are byte-identical, with no aliases or forwarding headers.

General industry terminology is maintained in [hello-game](https://github.com/cuihairu/hello-game/blob/main/docs/glossary/11.md); this page retains Apollo responsibilities and implementation mappings.

## Ownership

| Object | Owner | State and lifecycle |
|---|---|---|
| Base part (`base::Avatar`) | baseServer | Inventory, equipment, progression, quests, and base gameplay; loading, online retention, disconnection, and unloading are independent of Instances. |
| Cell part (`cell::Avatar`) | authoritative Cell within a cell executor (current Scene semantics) | Position, combat health, skill execution, and temporary effects; created, transferred, and released under scene rules. |
| Session | gateway connection and online-directory association | Binds authenticated identity to a connection and rejects stale session versions. |

Only one Base writes the complete persistent player data; only one cell advances the scene entity. cell consumes gameplay snapshots and requests validated consumption or rewards from Base. Explicitly settle results that persist across matches instead of automatically treating every simulation field as profile data.

## Lifecycle

In the target lifecycle, login and trusted offline business such as fulfillment may request Base activation through database. Activation acquires exclusive write ownership; reuse an existing object, or use the common allocation/activation mechanism. Disconnection must not cause a second writer. Login separately establishes a client session and does not imply creating the Cell part. cell receives admission information and a read-only base-stat snapshot, then creates the scene entity. On departure or completion, Base validates and commits idempotent settlement while cell releases scene entities. Destroying an Instance does not destroy persistent data.

## Cross-server transfer

<ArchitectureDiagram kind="cross-server" />

Simulation transfer retains home-server/storage affiliation without moving full profiles or database credentials. The Base part can change hosting process for load balancing, recovery, or updates. Resolve the current Base and ownership generation by stable player identity rather than binding orders or business requests permanently to a Base ID. Source and destination cannot both simulate the entity. Reconcile ambiguous commits. Preserving an ongoing match also requires its shared Instance state; see [cross-server scenes](./cross-server-scenes.md).

## Base migration and replacement

Storage affiliation does not fix the hosting process. Load balancing and replacement both hand off persistent objects, sessions, and in-flight operations, conditionally commit the player-to-Base ownership record, and refresh routes while retaining storage affiliation. Drain writes before switching, reject stale-process writes afterward, and reconcile late settlement and retries.

## Implementation status

`base::Avatar`, `cell::Avatar`, Scene, and Instance are reusable code types. Persistent-player and storage responsibilities have moved out of the WorldApp host: the standalone base entry owns them and commits through database, and WorldApp no longer hosts profiles. Remaining open work: the read-only gameplay snapshot and return-request boundary, the no-cross-boundary-pointer discipline (P1), and management/execution separation (P2). Track work in root todo.md.

worldServer manages Space/Cell placement without another simulation object. `base::Avatar` corresponds to the player-side Base entity; `cell::Avatar`/Entity to Real/Cell entities. See the [mapping](./bigworld-apollo-mapping.md) for terms and gaps.
