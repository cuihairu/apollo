
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q30: What is KBEngine's Space? How Does It Differ from Physical Space Partitioning?

## Core Conclusion

`Space` is better understood as a "logical space container," rather than "automatically equivalent to physical map partitioning."

It can represent:

- A map
- An instance
- An instance
- A logical scene container

So when discussing `Space`, the first step is to separate "logical space" from "underlying physical partitioning or Cell splitting."

## I. What Exactly Is Space

More precisely, Space is the abstract container the server uses to hold a set of entities and spatial logic.

It is typically responsible for:

- Holding the entities within the scene
- Supporting AOI and spatial relationships
- Associating map, navigation, or scene script semantics

It emphasizes "how entities within the same logical scene are organized."

## II. Why It Is Not the Same as Physical Partitioning

Many people tend to interpret it as:

- One Space is one CellApp
- One Space is naturally and uniformly split into several physical blocks

This understanding is too coarse.

A more accurate distinction is:

- `Space` is a logical concept
- Physical partitioning is an implementation strategy the runtime uses for load and scale-out

In other words, the same logical Space may be carried cooperatively by multiple nodes; or a single node may host multiple Spaces at the same time.

## III. Space Is More Like a "Scene Container" Than a "Map Slice"

For example:

- The main city is one Space
- A certain instance is another Space
- A battlefield instance can also be a Space

What they have in common is not size, but:

- The entities belong to the same logical scene
- They have their own lifecycle
- They have their own rules and script context

## IV. Physical Space Partitioning Solves a Different Problem

Physical partitioning usually focuses more on:

- Load balancing
- Multi-node hosting
- Hotspot area splitting
- Neighbor-cell migration

This belongs to the runtime scaling and scheduling layer, not the Space concept itself.

So a more reasonable understanding is:

- Space defines "what this world logically is"
- The partitioning mechanism defines "how this world is split and hosted at runtime"

## V. Why Confusing These Two Concepts Causes Problems

If Space is directly equated with physical partitioning, subsequent design easily becomes chaotic:

- Map logic becomes coupled with deployment strategy
- Instance abstraction becomes difficult
- Dynamic migration boundaries are hard to express
- Concepts distort when multiple Spaces coexist

## VI. A More Practically Useful Way to Understand It

Space can be seen as a stable business abstraction layer:

- For the gameplay layer, it represents a scene semantic boundary
- For the runtime, it is just a carrier object; whether it is partitioned underneath is decided by the scheduling system

This allows upper-layer gameplay and underlying deployment to be better decoupled.

## VII. Common Misconceptions

### 1. Space is map coordinate blocking

Inaccurate. Blocking is a hosting strategy; Space is a logical container.

### 2. One CellApp can only have one Space

Not necessarily. Whether that holds depends on the specific implementation and resource allocation strategy.

### 3. Gameplay instances don't count as Space

On the contrary, they are usually very well suited to be carried by a logical container like Space.

## References

- Space concept descriptions in KBEngine architecture materials
