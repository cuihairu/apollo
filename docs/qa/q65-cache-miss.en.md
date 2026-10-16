[English](q65-cache-miss.en.md) | [中文](q65-cache-miss.md)

# Q65: How to Reduce CPU Cache Misses?

## Core Conclusion

The key to reducing cache misses is not memorizing cache hierarchy parameters, but making the way the program accesses data more sequential, more predictable, and with less cross-core churn.

What is really most worth attention is:

- Data layout
- Access order
- Pointer chasing
- Shared writes

If these are not changed, relying only on alignment and small tweaks usually gives limited benefit.

## I. Why Cache Misses Deserve Attention

The CPU is far faster than memory.

So performance problems are often not "computing too slowly", but:

- Data not being available
- Data being fetched too scattered
- Threads fighting each other and flushing the cache

This is especially obvious in paths like high-frequency traversal, entity updates, and message handling.

## II. The Most Common Sources of Misses

### 1. Pointer Chasing

For example objects nested layer by layer, too many linked lists, severe fragmentation.

### 2. Poor Data Layout

Hot fields and cold fields mixed together, causing a lot of useless data to be pulled in every time.

### 3. Poor Access Order

Traversal order not matching memory layout makes cache utilization very poor.

### 4. False Sharing and Cross-core Writes

Different threads frequently writing the same cache line causes the cache to be invalidated back and forth.

## III. The Most Effective Optimization Usually Starts from Data Layout

Common approaches include:

- Putting hot fields together
- Splitting cold fields out
- Reducing object nesting levels
- Using contiguous containers instead of scattered nodes

This is more effective than simply adjusting compilation parameters.

## IV. Access Patterns Matter More Than Single Object Size

The same data, if scanned sequentially versus accessed randomly with jumps, can differ a lot in performance.

So when designing, pay attention to:

- Whether batch sequential traversal is possible
- Whether data can be rearranged by processing stage
- Whether back-and-forth jumps across structures can be reduced

## V. Under Multithreading, Watch Out for Shared Writes

Some cache misses are not single-thread layout problems, but threads scattering each other's cache lines.

Common scenarios include:

- Global counters
- Shared queue head/tail
- Adjacent fields written by different threads

At this point you have to consider:

- Sharding
- Local buffering
- Padding
- Reducing shared write paths

## VI. Why Data-oriented Design Is Often Effective

Because it emphasizes:

- Putting data accessed together together
- Processing the same logic in batches
- Minimizing irrelevant fields participating in hot paths

This is naturally consistent with the goal of reducing cache misses.

## VII. A More Pragmatic Optimization Order in Engineering

Common practice is:

1. First use profiling to find high-miss hotspots
2. First look at data structures and access order
3. Then consider alignment, padding, and finer cache line adjustments

Otherwise it is very easy to do a lot of low-benefit micro-optimizations.

## VIII. Common Misconceptions

### 1. Cache Misses Are Just Low-level Details, Not Worth Managing

In high-frequency hot paths, they are often the core bottleneck.

### 2. Linked Lists Are More Flexible Than Arrays, So It Doesn't Matter

Flexible does not equal efficient; linked lists and scattered objects easily create pointer chasing.

### 3. As Long as Objects Get Smaller, Cache Misses Will Decrease

Not necessarily. Access order and sharing patterns matter just as much.

## References

- Materials on Data-Oriented Design, cache locality, and false sharing
