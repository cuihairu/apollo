
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q79: How Do You Design a Thread-Safe Container?

## Core Conclusion

The key to a thread-safe container is not wrapping a lock around a standard container, but first clarifying:

- How many threads will read
- How many threads will write
- Whether iteration is needed
- Whether ordering guarantees are needed

Under different access models, the best designs differ greatly. No single "thread-safe container template" can cover all scenarios.

## 1. First Ask What Access Pattern the Container Must Bear

The most common patterns include:

- Single writer, multiple readers
- Multiple writers, single reader
- Multiple writers, multiple readers
- Reads far outnumber writes

You also need to ask further:

- Whether lookup by key is needed
- Whether ordered traversal is needed
- Whether snapshot reads are allowed

These all directly affect the design.

## 2. Coarse-Grained Locks Are Not a Sin

In many scenarios, a simple mutex wrapping the whole container is already enough:

- Simple logic
- Small capacity
- Not high concurrency

Do not pursue segmented locks or lock-free structures from the start. Over-design adds a lot of complexity.

## 3. Only Refine When the Hotspot Is Clear

If profiling proves the container lock is really a hotspot, then consider:

- Segmented locks
- Read-write locks
- Copy-on-write
- Lock-free structures

At this point the focus should be on the real bottleneck, not conceptual sophistication.

## 4. Thread Safety Is Not Just a Lock Problem

The truly hard parts of many containers are:

- Structural changes during iteration
- Element lifetimes
- Validity after returning references or pointers

In other words, even if add/delete/query/modify are all "locked", the API design may still be unsafe.

## 5. API Design Is Often More Critical Than Internal Implementation

A more robust container interface usually avoids:

- Directly exposing internal references
- Returning invalidatable iterators while holding no lock
- Making ownership unclear to the caller

The value of a thread-safe container is not only having no data race internally, but also making it hard for callers to misuse.

## 6. Read-Mostly Scenarios Can Consider Snapshotting

For things like configuration, routing tables, and read-only indexes, it is often more suited to:

- Immutable snapshots
- Copy-on-write
- Version switching

This way the read path barely needs locking, and is usually more stable than one big read-write lock.

## 7. The More Robust Design Order in Engineering

Common practice is:

1. First clarify the access pattern
2. First use the simplest workable solution
3. Refine lock granularity only after hotspot evidence
4. API design prioritizes lifetime safety

This keeps implementation and maintenance cost more controllable.

## 8. Common Misconceptions

### 1. A Thread-Safe Container Must Be Lock-Free

No. In many scenarios a simple lock solution is already good enough.

### 2. Locking the Internals Makes the API Safe

Not necessarily. Returning references, iterators, and lifetime problems can still go wrong.

### 3. Read-Write Locks Are Always Better for Read-Mostly

In many real scenarios, the overhead and contention of read-write locks are not necessarily worthwhile.

## References

- Concurrent containers, segmented locks, and snapshot reads practice materials
