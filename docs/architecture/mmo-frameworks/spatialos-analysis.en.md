[English](spatialos-analysis.en.md) | [中文](spatialos-analysis.md)

# SpatialOS Architecture Analysis

## 1. Positioning

SpatialOS was once highly representative in discussions of large multiplayer worlds.  
As of the current point in time, it is better treated as:

- A historical template of the large-scale distributed world
- A reference for the "server meshing / distributed simulation" route

rather than a safe landing choice for mainstream new projects.

## 2. Core Architecture

SpatialOS's core idea has long been:

- Split a large world into a distributed simulation
- Different workers hold authority over different regions or functions
- A platform layer manages state routing and load distribution

What this route solves is not the ordinary room-server problem, but:

- Extra-large worlds
- Multi-worker cooperation
- distributed authority

## 3. MMO Core Capability Analysis

### 3.1 World Model

SpatialOS's value lies mainly in the world layer.  
It is concerned with:

- Distributed worlds
- Authority handover or cooperation between workers
- Large-scale simulation

This is closer to the ideal MMO picture than ordinary room frameworks.

### 3.2 Sync and Authority

Its key question is not how a single object syncs, but:

- How the entire world is carried jointly by multiple services

This is also its fundamental difference from Photon / Colyseus / Mirror.

### 3.3 Real-World Problems

The cost of this route is also very high:

- Cost
- Architectural complexity
- Debugging complexity
- Platform dependency

Looking back from today, SpatialOS is more a reminder that:

- Distributed worlds are tempting
- But the cost of landing them is extremely high

## 4. Distinctive Traits

- Distributed simulation is the headline selling point
- Not simple room-based multiplayer
- Very suitable for studying server meshing thinking

## 5. Strengths

- Huge imaginative room at the world layer
- Gave a very representative answer to the extra-large-world problem

## 6. Weaknesses

- High engineering and commercial cost
- Complex platform history and evolution
- Uncertain today as a direct adoption for new projects

## 7. Applicable Scenarios

Better suited to:

- Serving as an architecture reference
- Studying server meshing
- Absorbing the ideas of large-world distributed simulation

Not a good fit:

- As the default first-choice foundation for most teams today

## 8. Lessons for Apollo

What is most worth absorbing:

- The world and its authority do not necessarily have to be carried by a single process
- A distributed world must design "authority boundaries" and "state routing" up front

## 9. References

- Historical docs entry: https://docs.spatial-os.org/
- Current Spatial Creator Toolkit multiplayer overview: https://toolkit.spatial.io/docs/multiplayer/overview
