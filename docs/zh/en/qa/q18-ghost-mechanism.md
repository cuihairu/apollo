
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q18: What Is the Ghost / Shadow Mechanism?

## Core Conclusion

The essence of the `Ghost / Shadow` mechanism is splitting an entity into copies with different responsibilities:

- `Real Entity` is responsible for authoritative logic
- `Ghost Entity` is responsible for the remote mirror across processes and cells
- `Shadow State` is responsible for client-side display, prediction, and smoothing

What it solves is not "how to copy an object," but three more critical problems in a distributed world:

- Authoritative state can only have one copy
- Other nodes must still be able to see it
- The client needs a local representation suited for rendering and compensation

## I. Why This Mechanism Is Needed

In a single-process game, an entity usually has only one copy of state; but in a large world or multi-Cell architecture, the same player often involves multiple nodes at once:

- The cell they are in must run its real logic
- Adjacent cells must perceive its existence and handle boundary interaction and AOI
- Login, social, quest, and other services may need to read parts of its state
- The client must also render it and predict for the local player

If all nodes directly read and write the same object, the system immediately spirals out of control:

- Unclear sources of state writes
- Hard to switch the primary during migration
- Blurry failure recovery boundaries
- The client easily mistakes display state for authoritative state

So in engineering, explicit layering is usually adopted.

## II. What the Three Kinds of Copies Are Each Responsible For

### 1. Real Entity

`Real Entity` is the authoritative entity, usually living in the Cell or logical cell currently responsible for that entity.

It is responsible for:

- Movement and collision
- Skills, combat, threat
- AOI computation
- Final commit of state changes

There is only one core constraint: at any moment there can be only one authoritative Real.

### 2. Ghost Entity

`Ghost Entity` is the remote mirror, usually living in:

- Neighboring Cells
- Other logical nodes that need to observe the entity
- Certain service processes that need read-only state

Its job is not to make independent decisions, but to:

- Receive incremental sync from the Real
- Provide locally visible state
- Participate in neighbor-cell AOI, boundary broadcasts, and migration warmup

A Ghost should generally not advance authoritative logic on its own, otherwise double-writes and divergence appear.

### 3. Shadow State

`Shadow State` is the client-side local representation. The term "client entity" is deliberately not used here, to emphasize that it is not a flat copy of the server-side authoritative entity, but a layer of state wrapping oriented toward rendering and interaction.

It is usually responsible for:

- Character models, animations, effects
- Interpolation and smoothing
- Local player prediction
- Receiving server corrections

For the local player, the Shadow may move a small step ahead; for remote entities, the Shadow more commonly does interpolated display rather than aggressive prediction.

## III. Typical Data Flow

A common data path is:

`Real Entity -> Ghost Entity -> Client Shadow State`

But this path is not fixed at only one hop. In real engineering, the following may occur:

- Real broadcasts directly to the client
- Real syncs to multiple neighbor-cell Ghosts
- Ghosts then participate in local AOI or boundary computation

No matter how the path changes, the principles do not:

- The final source of logic is the Real
- Ghost is a controlled mirror
- Shadow is presentation-layer state

## IV. Its Relationship with AOI and Cell Migration

### 1. Relationship with AOI

AOI decides "who should see whom"; Ghost/Shadow decides "after they see it, which copy carries it."

In other words:

- AOI is responsible for filtering sync targets
- Ghost is responsible for keeping remote mirrors across nodes on the server side
- Shadow is responsible for displaying these sync results on the client

Without AOI, the Ghost count explodes; without Ghost, cross-cell perception becomes very heavy; without Shadow, the client can only rigidly display the latest packet.

### 2. Relationship with Migration

When an entity moves across cells, the system usually does not "instantly delete the old Real and then create a new Real," but first lets the target cell prepare the mirror, then completes the primary/secondary switchover.

A more robust migration flow is usually:

1. The target cell creates the Ghost of that entity in advance
2. It continuously receives authoritative state to complete warmup
3. When the switch condition is reached, the target cell is promoted to the new Real
4. The old cell is demoted to a Ghost or enters the reclamation flow
5. Surrounding AOI and client connection relationships are then updated

The value of this is reducing switch jitter and the risk of state loss.

## V. Common Design Boundaries

### 1. Ghost Is Not a "Second Real"

A Ghost can hold a lot of state, but it should not independently arrive at final combat results, nor should it privately rewrite key attributes.

If a Ghost can also execute key logic, the following appear:

- The same skill is resolved twice
- Neighbor-cell and home-cell state diverge
- During migration it is hard to judge which is the real result

### 2. Shadow Is Not a Simple Copy of the Server-Side Mirror

What the client needs is "a copy of state suited for rendering and interaction," not a field-by-field verbatim copy of the server-side object.

For example:

- Server-side position may be synced as discrete snapshots
- The client must do interpolation and prediction
- Some server-side internal fields should never be exposed to the client

So the Shadow usually has its own representation structure.

### 3. Must BaseApp or Other Services Hold a Ghost?

Not necessarily; it depends on the architecture.

If a service only needs to query the current authoritative state via RPC, it does not necessarily need to hold a mirror long-term; if it needs high-frequency reads, low-latency access, or participation in complex collaboration, a mirror copy is more valuable.

The key is not "whether there is a Ghost," but:

- How high the read frequency is
- Whether fast local access is needed
- Whether asynchronous query latency is acceptable

## VI. The Real Benefits This Mechanism Brings

### 1. Controlling the Write Source

Only the Real is responsible for final writes, so the system more easily maintains consistency.

### 2. Reducing Cross-Node Interaction Cost

Neighboring cells or other processes do not need to remotely query the authoritative node every time; they can read the local Ghost first.

### 3. Supporting Smooth Migration

Preheating the Ghost before a cell switch makes the migration process more stable.

### 4. Making Client Sync More Controllable

The client only cares about the data the Shadow needs, and does not have to directly bear the complexity of the server-side entity model.

## VII. Common Questions

### 1. Are More Ghosts Always Better?

No. More Ghosts means:

- More memory usage
- More sync links
- More dirty data propagation
- More complex reclamation and failure handling

The number of Ghosts should be constrained by AOI, boundary warmup, and service dependencies, not replicated without limit.

### 2. Do All Entities Need a Complete Ghost?

Also not necessary. Often only a "trimmed mirror" is needed.

For example, a neighboring cell only cares about:

- Position
- Facing direction
- Faction
- Lifecycle

Then there is no need to sync the entire inventory, quests, and script private state.

### 3. Must Shadow Do Prediction?

Not necessarily.

- The local player usually needs prediction
- Remote players mostly use interpolation
- Monsters or low-value objects may even only do simple smoothing

Prediction is an experience optimization, not something that must be applied to all objects.

## VIII. Points That Matter More When Landing in Engineering

To truly make the Ghost / Shadow mechanism stable, the following questions usually must be answered at the same time:

- How to define the field boundary between Real and Ghost
- Whether sync is full, incremental, or dirty-field
- Who manages the Ghost lifecycle
- How to do the primary/secondary switchover when an entity migrates
- What the correction threshold for client Shadow is
- How to create, freeze, and reclaim mirrors when AOI shrinks and expands

These matter more to system quality than merely memorizing the terminology.

## References

- Entity / Ghost / Shadow design descriptions in the KBEngine documentation
- The Real / Ghost entity migration mechanism in BigWorld architecture materials
- Glenn Fiedler, *Snapshot Interpolation*
