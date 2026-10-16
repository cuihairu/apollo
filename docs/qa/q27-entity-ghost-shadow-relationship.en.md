[English](q27-entity-ghost-shadow-relationship.en.md) | [中文](q27-entity-ghost-shadow-relationship.md)

# Q27: How Do Real, Ghost, and Shadow Entities Convert Between Each Other? How to Sync Efficiently?

## Core Conclusion

What these three concepts really require you to grasp is "authority ownership" and "replica responsibilities":

- `Real` is the only authoritative entity
- `Ghost` is a controlled mirror on other server nodes
- `Shadow` is the local state on the client used for display and compensation

The focus of conversion is not swapping nouns, but "who owns write permission" and "how not to lose state during the switch".

## 1. The Boundaries of the Three

### 1.1 Real

Responsible for:

- Authoritative state advancement
- Combat and movement logic
- Final attribute modification

Only one Real can exist at the same time.

### 1.2 Ghost

Responsible for:

- Providing remote mirrors for other Cells or neighboring regions
- Participating in boundary awareness and migration preheating
- Accepting controlled synchronization

Ghost generally is not responsible for final settlement.

### 1.3 Shadow

Responsible for:

- Client display
- Interpolation and smoothing
- Local player prediction and correction

It is not a simple translated copy of the server-side authoritative object.

## 2. When Conversions Happen

### 2.1 Real Becomes Ghost

The most typical scenario is entity migration across partitions.

After the node that originally handled the entity no longer has authority, it is demoted to Ghost or enters a recycling state.

### 2.2 Ghost Becomes Real

When the target partition takes over authority, it promotes the preheated mirror to a new Real.

### 2.3 Shadow Updates

The client's Shadow usually does not "get promoted to Real"; it mostly receives server-side authoritative state and does local smoothing.

## 3. The Steps That Truly Matter During Migration

A more sound migration flow is usually:

1. The target node first creates or preheats the Ghost
2. Continuously synchronize the necessary state
3. After reaching the switch condition, switch authority ownership
4. The old Real is demoted or cleaned up
5. The surrounding AOI and client synchronization relationships are updated

What is most prone to problems here is:

- Two Reals coexisting
- Old Ghost data becoming stale
- Events being lost at the instant of switching

## 4. What Should Synchronization Actually Sync

The prerequisite for efficient synchronization is not to send all fields at once.

You should usually distinguish:

- State required for migration
- Neighboring region observation state
- Client display state

For example, a neighboring Ghost may only need:

- Position
- Facing direction
- Faction
- Basic lifecycle state

And not the entire inventory and quest details.

## 5. Why Shadow's Synchronization Strategy Differs from Ghost's

Ghost is closer to a server-side logic mirror, while Shadow is closer to rendering and interaction state.

So:

- Ghost focuses on state correctness and boundary cooperation
- Shadow focuses on display continuity and player feel

The client often does the following to Shadow:

- Interpolation
- Prediction
- Smooth correction

None of these suit being applied directly to a server-side Ghost.

## 6. How to Improve Synchronization Efficiency in Engineering

Common practices include:

- Dirty field synchronization
- Incremental version numbers
- Cull fields per observer
- Batch sending
- AOI constrains the replica count

The core goal is always:

- Reduce replica cost
- Keep the switch stable
- Avoid duplicate settlement and double writes

## 7. Common Misconceptions

### 7.1 A Ghost Can Incidentally Execute Some Key Logic

This is very dangerous, easily causing divergent states and duplicate settlement.

### 7.2 What the Client Shadow Sees Is the Authoritative State

Wrong. Shadow usually carries prediction and interpolation, and is experience-layer state.

### 7.3 Conversion Just Needs a Flag Bit Change

Wrong. What is truly difficult is the master-slave switch order, state completeness, and no lost events.

## References

- The Real / Ghost / Shadow design documentation in KBEngine / BigWorld architecture materials
