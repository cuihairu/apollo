[English](q53-scene-management.en.md) | [中文](q53-scene-management.md)

# Q53: How do you design scene management?

## Core Conclusion

Scene management is not "loading a map"; it is "managing the complete lifecycle of a scene from creation, to hosting entities, to synchronization, to switching, to recycling".

What really needs to be designed clearly first is:

- The boundary between scenes and instances
- How entities within a scene are organized
- How scene switching and cross-scene migration work
- When scene resources and logical state are recycled

If these boundaries are unclear, the scene system gets tangled up with instances, AOI, synchronization, and resource management.

## One: First Distinguish Several Concepts

### 1. Scene Template

Describes static content, for example:

- Map resources
- Spawn points
- Monster spawn points
- Navigation data

### 2. Scene Instance

Describes runtime state, for example:

- Current players and monsters
- Mechanism state
- Temporary events

### 3. Space Partitioning or Zoning

This is a hosting and scaling layer concern; it is not entirely the same as the scene itself.

Separating these three layers makes later extension much clearer.

## Two: What Scene Management Is Usually Responsible For

At least it includes:

- Creating and destroying scene instances
- Managing the lifecycle of entities within the scene
- Maintaining scene-level rules and state
- Coordinating scene switching and migration

For instances and battlefields, these responsibilities are even heavier.

## Three: Scene Switching Is a High-Risk Path

When a player moves from one scene into another, it is usually not just changing a coordinate.

You also need to handle:

- Leaving the old scene
- Admission to the new scene
- Entity migration or rebuilding
- AOI reset
- Client state synchronization

If the path has no explicit phases, scene switching easily produces black screens, duplicate entities, or lost state.

## Four: The Scene Instance Lifecycle Must Be Explicit

It can commonly be divided into:

1. Creation
2. Activation
3. Running
4. Idle
5. Recycling

Different scene types have different strategies:

- Main cities usually stay resident
- Instance scenes are usually created and recycled on demand

## Five: Entity Organization Determines Management Cost

A scene usually contains:

- Players
- Monsters
- Drops
- Mechanisms
- Temporary effect objects

The scene management layer has to answer:

- Who is responsible for creating them
- Who is responsible for destroying them
- Who maintains visibility and synchronization boundaries

If every subsystem manages them on its own, scene consistency quickly gets out of control.

## Six: The Scene System and the Resource System Should Also Be Decoupled

Server-side "scene management" cares more about logical state, while client-side "scene loading" cares more about resource presentation.

The two are related, but they cannot be merged into one.

The server side focuses on:

- State and entities
- Scene rules
- Synchronization boundaries

The client side focuses on:

- Map resources
- Models and effects
- Render loading

## Seven: A More Solid Design in Engineering

A common approach is:

- Use scene templates to define static data
- Use scene instances to maintain runtime state
- Scene switching goes through an explicit state machine
- Empty scenes are recycled by policy

That way main cities, instances, battlefields, and other types can all land in one unified framework.

## Eight: Common Misconceptions

### 1. Scene Management Is Map Loading

No. The server side cares more about scene instances and entity lifecycles.

### 2. Scene Switching Is Teleporting

No. It involves sessions, entities, synchronization, and client state rebuilding.

### 3. Destroying a Scene Immediately Once It Has No Players Is Always Best

Not necessarily. You also have to weigh reconnection, recycling cost, and whether delayed entry is needed.

## References

- Various MMO / ARPG scene instance and map-switch management practice materials
