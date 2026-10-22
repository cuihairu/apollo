
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q57: How Do You Optimize Memory Usage?

## Core Conclusion

Memory optimization is not simply "saving a bit of RAM", but controlling at the same time:

- Total footprint
- Allocation frequency
- Fragmentation
- Cache locality

Truly effective memory optimization often does not come from fiddling with the allocator at the end, but from first reducing meaningless objects, shrinking hot-path data, and controlling lifecycles.

## I. First Figure Out Which Kind of Memory Problem You Have

There are at least four common kinds:

- Memory leak
- Oversized footprint
- Allocation too frequent
- Layout that causes poor cache hits

These four kinds of problems have completely different solutions.

## II. Many Memory Problems Actually Come From the Design

For example:

- Object models that are too heavy
- Containers nested too deep
- Large numbers of duplicated strings
- Temporary state hanging on objects long-term

If the design layer is already bloated, later allocator optimization often yields little benefit.

## III. Look at Hot-Path Objects First

In online games, what is most worth optimizing is usually not all objects, but these high-frequency ones:

- Entity state
- Network messages
- AOI temporary structures
- Timer nodes
- Path points and combat events

If these objects are too large, too fragmented, or allocated frequently, the cost keeps getting amplified.

## IV. Controlling Allocation Frequency Is Usually Worth More Than Squeezing the Total

A lot of stuttering and jitter in systems comes from:

- High-frequency `new`/`delete`
- Sudden spikes in temporary objects
- Containers expanding continuously

So common fixes are:

- Reserve capacity
- Object reuse
- Bulk allocation
- Use an arena or object pool

This kind of optimization often directly affects tail latency.

## V. Cache Locality Is Often Underestimated

With the same 100MB of memory, taking a bit more space is not necessarily the most fatal problem; the bigger problem is often that the data is too scattered, causing:

- More CPU cache misses
- Severe pointer chasing
- Poor traversal performance

So memory optimization is not just "occupy less", but also "lay it out more reasonably".

## VI. Strings and Containers Are Often Invisible Heavyweights

Common problems include:

- Large numbers of duplicated strings
- Abuse of `unordered_map` / `std::map`
- Too many small objects
- Too many pointers and wrapper layers stored inside containers

These problems become very visible in large-scale entity systems.

## VII. Leaks and Peaks Should Be Governed Separately

Continuous memory growth and instantaneous spikes are handled differently:

- Leaks focus more on lifecycle closure
- Peaks focus more on instantaneous allocation and the peak path

Do not mix the two, or troubleshooting will be very slow.

## VIII. A Safer Optimization Order in Engineering

A more effective common order is:

1. First measure the heavy objects and the growth curve clearly
2. First cut invalid objects and duplicated data
3. Then optimize high-frequency allocation
4. Then consider custom allocators and more aggressive strategies

Because the latter has higher development and maintenance cost.

## IX. Common Misconceptions

### 1. Memory Optimization Means Using an Object Pool

Object pools are useful, but they only solve part of the allocation problems.

### 2. If Memory Has Not Blown Up, You Do Not Need to Care

Many problems first appear as jitter, cache misses, or GC-style stuttering.

### 3. Making Every Object Smaller Is Optimization

If readability and boundaries are broken, it may not be worth it. The key is optimizing the hot path.

## References

- Materials on memory profiling, arena allocation, and data-oriented layout
