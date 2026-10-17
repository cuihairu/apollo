
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q74: How Do You Design Lock-Free Data Structures?

## Core Conclusion

A lock-free data structure is not a "more advanced thread-safe container", but a high-cost means of replacing mutexes with atomic operations in specific hotspot scenarios.

It suits:

- Clear concurrency models
- Clear contention hotspots
- Performance benefits large enough

If these premises do not hold, lock-free structures often only raise code complexity and debugging cost very high.

## 1. What Problem Lock-Free Is Actually Solving

What it mainly wants to reduce is:

- Lock waiting
- Context switching
- Serialization of hotspot critical sections

So lock-free structures are most worth using where "locks have already been proven to be the hotspot bottleneck", not for the sake of conceptual sophistication.

## 2. Lock-Free Does Not Mean No Cost

Common costs include:

- Complex memory ordering understanding
- ABA problem
- Difficult lifecycle management
- High debugging and verification cost

In other words, it just moves the problem from mutex to the atomic and memory model layer.

## 3. The Simpler the Concurrency Model, the More Chance Lock-Free Has to Succeed

For example:

- SPSC queue
- MPSC queue

These models are usually easier to make stable than general MPMC structures.

The more complex the concurrency model, the higher the maintenance cost of the lock-free implementation.

## 4. Lifecycle Management Is One of the Hardest Parts of Lock-Free Structures

Even if the atomic update logic is written correctly, when a node can be safely released is still a big problem.

This is why lock-free structures often need supporting mechanisms:

- Hazard pointers
- Epoch-based reclamation
- Reference counting or deferred reclamation

Many of the hard points are not in CAS itself at all.

## 5. Lock-Free Structures Suit Small, Clear Hotspots More

For example:

- Hot queues
- Statistical counters
- Ring buffers with certain fixed models

If you want to build a "lock-free universal container" that is fully functional, very generic, and arbitrarily extensible, it usually easily gets out of control.

## 6. The More Robust Order in Engineering

The more pragmatic approach is usually:

1. First confirm that the lock is really the bottleneck
2. First try sharding, reducing sharing, shortening critical sections
3. Only when the benefit is clear, move to a lock-free structure

This is more stable than going fully lock-free from the start.

## 7. Common Misconceptions

### 1. Lock-Free Is Definitely Faster Than Locking

Not necessarily. In low-contention scenarios, simple lock structures may be more stable and faster.

### 2. Using CAS Counts as a Lock-Free Design

Far from enough. Memory ordering, ABA, and reclamation are the real hard parts.

### 3. All Thread-Safe Containers Should Pursue Lock-Free

This usually brings excessive complexity, not matching most business scenarios.

## References

- CAS, memory ordering, and lock-free reclamation mechanism related materials
