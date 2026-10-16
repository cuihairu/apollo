[English](q58-object-pool.en.md) | [中文](q58-object-pool.md)

# Q58: What Is an Object Pool? How to Design One?

## Core Conclusion

The value of an object pool lies not in "avoiding writing `new`/`delete`", but in reducing the jitter and fragmentation cost of high-frequency object creation and collection.

It is suitable for:

- Short lifespan
- Relatively stable type
- Very frequent creation and destruction

If the object has a complex lifespan, unpredictable count, or difficult state cleanup, an object pool may actually create more problems.

## 1. What Problems Does an Object Pool Really Solve

Mainly these categories:

- High-frequency allocation and release overhead
- Memory fragmentation
- Latency jitter
- Hot-path objects repeatedly constructed and destructed

So an object pool is more like a performance stabilization means, not a general design pattern.

## 2. Which Objects Are Suitable for Pooling

Commonly suitable for pooling:

- Network packet objects
- Temporary event objects
- Bullets, projectiles, effect instances
- Certain fixed-size message nodes

These objects share:

- Large count
- Frequent use
- Relatively simple structure

## 3. Which Objects Are Not Suitable for Pooling

Usually including:

- Large objects with very complex state
- Objects with very long lifespans
- Sparsely created objects
- Objects hard to fully reset after release

If the reset cost is higher than creating new, an object pool is meaningless.

## 4. Key Points of Object Pool Design

### 4.1 Acquire and Return Must Be Light Enough

If the pool itself needs heavy locks, complex lookups, or lots of validation, the benefit is eaten up.

### 4.2 State Cleanup Must Be Reliable

One of the biggest risks of pooled objects is residual dirty state.

Every return or re-borrow must guarantee the object returns to a predictable state.

### 4.3 Capacity Strategy Must Be Clear

Must first define:

- Initial capacity
- Maximum capacity
- Whether to expand when the pool is empty, or degrade to ordinary allocation

Do not just assume the object count is always exactly right.

## 5. Object Pools and Memory Pools Are Not the Same Thing

An object pool focuses on "object instance reuse", while a memory pool focuses more on "allocating memory blocks".

The two are often used together, but with different goals:

- Object pools solve construction and object reuse problems
- Memory pools solve low-level allocation problems

Do not blur them into one concept.

## 6. Be Extra Careful in Multi-threaded Scenarios

Once an object pool is shared across threads, it encounters:

- Lock contention
- ABA risk
- Returning to the wrong thread
- cache line jitter

So common practices in high-concurrency scenarios are:

- Thread-local pools
- Sharded pools
- Central pool plus local cache

This is usually more stable than a pool with a single global lock.

## 7. A More Sound Landing Approach in Engineering

Common practices are:

- First find high-frequency short-lived objects via profiling
- Pool only those objects
- Clarify reset rules and capacity limits
- Keep an over-limit fallback path

This makes benefits more controllable and easier to verify.

## 8. Common Misconceptions

### 8.1 Whenever There Is a Performance Problem, Use an Object Pool

Wrong. Object pools are only suitable for part of the allocation hotspots.

### 8.2 More Pooled Objects Is Always Better

An oversized pool also wastes memory and hides real lifespan problems.

### 8.3 Just Overwrite on Next Use Instead of Cleaning After Return

This creates extremely hard-to-locate dirty state bugs.

## References

- Object pool, arena, and high-frequency allocation optimization practice materials
