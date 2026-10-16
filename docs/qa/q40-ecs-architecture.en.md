[English](q40-ecs-architecture.en.md) | [中文](q40-ecs-architecture.md)

# Q40: What Is ECS Architecture? What Advantages Does It Have in Games?

## Core Conclusion

The value of ECS is not in swapping in a new set of terminology, but in splitting "data layout" and "behavior organization" out of the traditional object hierarchy.

The problems it is best suited to solve are usually:

- Batch updating of large numbers of similar objects
- Data layout affecting performance
- High requirements for behavior composability

But it is also not the default optimal architecture for all game server modules; especially in scenarios with strong business state machines and complex object lifecycles, pure ECS is not necessarily the most handy.

## 1. What ECS Is Actually Solving

Traditional object-oriented design often binds:

- Data
- Behavior
- Inheritance relationships

all in one class hierarchy.

Once the number of entity types keeps growing, it is easy to get:

- Bloated inheritance trees
- Large numbers of conditional branches
- Scattered data layout
- Difficult batch processing

The ECS idea is to split these apart.

## 2. Three Core Concepts

### 1. Entity

An Entity is more like an identity marker; it does not carry complex logic.

### 2. Component

A Component mainly carries data, for example:

- Position
- Velocity
- HP
- Faction

### 3. System

A System is responsible for batch processing entities that have certain components.

For example:

- The movement system processes position and velocity
- The combat system processes HP and attack attributes

## 3. Where Its Advantages Mainly Manifest

### 1. More Conducive to Batch Processing

Once similar data is concentrated, batch traversal and unified updates are easier.

### 2. More Conducive to Performance Optimization

If the component layout is reasonable, it is easier to improve cache locality and reduce object chasing.

### 3. More Conducive to Composition

Adding or removing components from an entity changes its capabilities, without constantly extending the inheritance tree.

## 4. Why It Gets So Much Attention in Games

Because games naturally have large numbers of:

- Similar entities
- High-frequency updates
- Composable capabilities

For example, large numbers of monsters, bullets, projectiles, particles, and temporary effects are all well suited to a more data-oriented way of handling.

## 5. But ECS Is Not a Mindless Full Replacement

Common ECS problems are also obvious:

- The debugging chain is not as intuitive as the object model
- Cross-system dependencies easily scatter
- Lifecycle management is more complex
- Complex business objects are easily split too finely

Especially in online game server, some modules are more like:

- Session state machines
- Transaction flows
- Business objects such as mail and auctions

These are not necessarily suited to pure ECS.

## 6. Mixed Use Is More Common in Game Servers

Many systems do not make all modules pure ECS, but use it by scenario:

- High-frequency scene objects, combat objects, and projectiles suit data orientation
- Accounts, mail, transactions, and quest flows suit the business object model

This is more pragmatic than "full ECS-ification".

## 7. The Relationship Between ECS and Network Synchronization, Authoritative Logic

ECS mainly solves internal organization; it does not automatically solve:

- Network synchronization
- Authoritative adjudication
- Distributed routing
- Persistence boundaries

So it is an internal architecture choice, not the whole answer for an MMO architecture.

## 8. When Is ECS More Worth Considering

The benefit is usually more obvious in these scenarios:

- Large entity counts
- Many homogeneous objects
- Dense per-frame batch processing
- Performance bottlenecks clearly on the object update path

If the object count is not large, the flow state is very complex, and business changes fast, ECS may not be the first choice.

## 9. Common Misconceptions

### 1. ECS Is Definitely Faster Than OOP

Not necessarily. The benefit is only obvious when data layout, system division, and access patterns are all well designed.

### 2. With ECS, You No Longer Need to Design Object Boundaries

No. The boundaries just move from class inheritance to between components and systems.

### 3. All Server Modules Should Be ECS-ified

This usually makes many business modules harder to maintain.

## References

- Data-Oriented Design related materials
- Unity DOTS / Unreal Mass public architecture materials