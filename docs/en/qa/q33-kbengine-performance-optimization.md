
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q33: Why does KBEngine not pursue extreme performance optimization? If it did, how should it be improved?

## Core Conclusion

The reason many frameworks do not push performance to the extreme is not because they "don't know how to optimize", but because they are making engineering trade-offs:

- Maintainability
- Learning cost
- Development efficiency
- Target user scale

For many teams, "fast enough and scalable" is more valuable than "extremely fast but hard to change".

## 1. Why many frameworks do not pursue extreme performance by default

The reasons usually include:

### 1. The target users do not need the extreme ceiling

If the target is mainly small and mid-sized teams, prototype validation, or medium-scale online, the benefits of extreme optimization may not cover the complexity cost.

### 2. Extreme performance often sacrifices maintainability

For example:

- Heavier custom memory layouts
- More aggressive lock-free structures
- More complex batching and zero-copy links
- Harder-to-read code organization

These all raise the barrier to secondary development.

### 3. The real bottleneck is often not at the framework's bottom layer

In many projects after going live, what really eats resources is:

- Game logic
- AOI
- Broadcasting
- Scripts
- Database

Rather than the network library itself.

## 2. Why "no extreme optimization" does not mean "poor performance"

The realistic goal of many systems is:

- Able to support medium-scale online
- Clear architecture boundaries
- A clear scale-out path

If all of these hold, it already has high engineering value.

So-called "extreme performance" must be measured within the business scenario, not discussed as a single indicator detached from the scenario.

## 3. If you really want to push further, where should you usually start

### 1. First find the real hotspots

Before optimizing, confirm where the bottleneck actually is:

- AOI
- Broadcasting
- Serialization
- Script binding
- Memory allocation
- Lock contention

Talking about refactoring without profiling usually leads to a lot of wasted optimization.

### 2. Lower the object cost on high-frequency paths

For example:

- Reduce frequent allocations
- Reduce copy counts
- Compress hot-path objects
- Improve cache locality

### 3. Reduce invalid broadcasting and sync

For MMO, this is usually worth more than continuing to squeeze the socket.

### 4. Make the thread model and authority boundaries clearer

Much performance loss comes from:

- Lock contention
- Cross-thread shared state
- Chaotic scheduling

Tightening the ownership of an object or logic domain usually yields high returns.

## 4. Which extreme optimizations come with a cost

If you dig deeper, you may consider:

- More aggressive memory pools
- Stronger zero-copy and batching
- Custom serialization formats
- Finer-grained thread affinity and NUMA tuning
- Heavier Actor-ization or lock-free-ization

All of these bring benefits, but also mean:

- Harder-to-understand code
- Harder debugging
- Harder for newcomers to take over

## 5. A more practical engineering optimization order

A more pragmatic order is usually:

1. First use profiling to find hotspots
2. First cut invalid work
3. Then optimize data layout and allocation
4. Then consider deep changes to the thread and network layers

Because "doing less" is often more effective than "doing faster".

## 6. How to judge whether a framework is worth continuing to optimize

The key is three questions:

- Is the current bottleneck a framework common-layer bottleneck
- Can the optimization benefit cover the maintenance cost
- After optimization, is it still suitable for the team's long-term iteration

If none of the answers are clear, rashly pursuing extreme optimization is usually not worth it.

## 7. Common misconceptions

### 1. Not reaching extreme performance means bad design

No. In many cases this is a conscious product and engineering trade-off.

### 2. Rewrite the bottom layer first, and performance naturally goes up

No. The real hotspots are often in the logic and sync layers.

### 3. Only extreme optimization counts as high-level architecture

What matters more in high-level architecture is clear boundaries, locatable bottlenecks, and sustainable evolution.

## References

- Various MMO framework performance analysis and engineering trade-off practice materials
