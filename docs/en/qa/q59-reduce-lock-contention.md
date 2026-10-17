
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q59: How do you reduce lock contention?

## Core Conclusion

The core of reducing lock contention is not deleting locks from the code, but reducing shared state and critical section length.

Truly effective approaches are usually:

- Let more data be owned by only one thread
- Shorten lock hold time
- Reduce high-frequency global structures
- Turn contention from "grabbing the same lock" into "sharding or serial ownership"

Lock-free structures are only one option, not the default answer.

## 1. What lock contention really brings

Common costs include:

- Thread blocking
- Context switching
- Cache line bouncing
- Worse tail latency

So the problem with lock contention is not only "high CPU", but often also shows up as system instability, occasional stutters, and sudden throughput collapse.

## 2. First find where the contention comes from

High-contention locks usually concentrate in these positions:

- Global maps
- Message queues
- Timer managers
- Logging systems
- Object pools or connection pools

So the first step is not changing the concurrency model, but first finding which data structures are really being contended.

## 3. The most effective approach is usually to reduce sharing

### 1. Single-thread ownership

Let a certain kind of object always be owned by a fixed thread or Actor, with other threads interacting through messages.

This is usually more stable than adding many locks to the object.

### 2. Sharding

Split one global lock into:

- Sharding by key
- Sharding by room
- Partitioning by player or scene

This noticeably reduces contention.

### 3. Read/write separation or snapshotting

For read-heavy write-light structures, it is more suited to:

- Read-only snapshots
- Copy-on-write
- Versioned data

Rather than all accesses grabbing the same mutex.

## 4. Shortening the critical section matters more than switching lock types

Many lock contention problems are not a problem of the lock itself, but of too-large lock scope.

Common bad smells include:

- Doing I/O while holding the lock
- Doing complex computation while holding the lock
- Calling other modules while holding the lock

This turns an originally controllable lock into a hotspot bottleneck.

## 5. Lock-free is not a silver bullet

Lock-free structures can reduce some blocking, but also bring:

- Rising code complexity
- Hard debugging
- Memory ordering issues
- Concurrency risks like ABA

Only when the hotspot is clear and the benefit is clear is it worth adopting.

## 6. Logging, timers, and queues most easily become invisible hotspots

In many systems the main logic looks fine, but the common-facility layer becomes a global lock black hole.

For example:

- Global log write lock
- Single priority-queue timer
- Central object pool

Such components should prioritize:

- Per-thread buffering
- Batch merging
- Sharded structures

## 7. A more robust engineering optimization order

Common practice is:

1. First find hotspot locks through profiling and tracing
2. First shrink the critical sections
3. Then reduce sharing and do sharding
4. Finally consider more aggressive lock-free structures

This is usually more stable than rewriting to lock-free from the start.

## 8. Common misconceptions

### 1. High lock contention means locks cannot be used

No. In many cases the sharing design has problems, not the lock primitive itself.

### 2. Read-write locks are always better than mutexes

Not necessarily. When writes are many and the read critical section is short, read-write locks may be worse.

### 3. Lock-free is always higher performance

No. It depends on the contention pattern, data structure, and implementation quality.

## References

- Concurrency profiling, Actor-ization, and sharded lock practice materials
