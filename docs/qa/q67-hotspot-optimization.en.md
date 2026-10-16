[English](q67-hotspot-optimization.en.md) | [中文](q67-hotspot-optimization.md)

# Q67: How do you perform hotspot code optimization?

## Core Conclusion

The core of hotspot code optimization is not "making every line faster", but finding the few paths that actually consume time, confirming the bottleneck type, and then changing them with the most appropriate means.

A more pragmatic order is usually:

- Find the hotspots
- Determine the hotspot type
- Change the parts that most affect user experience and system ceilings
- Validate the gains with data

Without an evidence chain, so-called hotspot optimization easily becomes local fiddling.

## One: First Confirm What Counts as a Hotspot

A hotspot is usually not "the code looks complex", but:

- High share of total CPU time
- Extremely high call frequency
- Obvious tail-latency impact
- Repeatedly amplified under peak scenarios

So a hotspot may be:

- One big function
- One small loop
- One highly contended lock
- One repeated serialization

## Two: First Distinguish Which Category the Hotspot Belongs To

Common categories:

- Pure computation hotspot
- Memory access hotspot
- Lock contention hotspot
- I/O or system call hotspot
- Invalid work hotspot

This step is critical, because different hotspot types have completely different solutions.

For example:

- Pure computation may suit algorithm optimization or SIMD
- Memory access problems more likely require changing the data layout
- Lock hotspots require reducing sharing

## Three: Cut Invalid Work First — Usually the Highest Payoff

Many hotspots are not "necessary work being too slow", but "doing too much unnecessary work".

Common examples include:

- Repeated computation
- Invalid broadcasts
- Repeated serialization
- Unnecessary formatting and logging

Reducing the workload is often more effective than doing the same work faster.

## Four: Local Optimization Must Serve the Overall Path

Even if a function is optimized by 30%, if it is not on the critical path, the overall gain may be small.

So hotspot optimization should be combined with:

- Call paths
- Business scenarios
- Amplification effects under peak traffic

This is also why flame graphs, tracing, and business metrics should be looked at together.

## Five: Common Optimization Directions

### 1. Algorithms and Data Structures

If the complexity itself is wrong, other micro-optimizations are very limited.

### 2. Data Layout

Reducing cache misses and pointer chasing is often worth more than changing a few instructions.

### 3. Batch Processing

Many high-frequency small operations show obvious gains once merged.

### 4. Reducing Lock Contention

Breaking up serialization points is often more effective than squeezing the single-threaded path to the extreme.

### 5. Reducing Allocation and Copying

This is very common in message, event, and synchronization paths.

## Six: After Optimization, You Must Re-Validate

A complete hotspot optimization should at least validate:

- Whether the CPU share went down
- Whether tail latency improved
- Whether new bottlenecks were introduced
- Whether maintainability was damaged

Without regression validation, optimization conclusions are usually unreliable.

## Seven: A More Pragmatic Optimization Order in Engineering

A common order is:

1. Use profiling to lock onto hotspots
2. Determine whether it is an algorithm, layout, lock, or I/O problem
3. Prioritize reducing invalid work
4. Then do local deep-dives

This is usually more stable than hand-editing assembly or obsessing over details right from the start.

## Eight: Common Misconceptions

### 1. Hotspot Optimization Is Making Functions Shorter

No. The bottleneck may not be in the amount of function code at all, but in access patterns or concurrency structure.

### 2. Seeing a Hotspot Function, Immediately Inline It and Unroll Loops

That may help, or it may just be noise. Confirming the bottleneck type first matters more.

### 3. Micro-Optimizations Accumulating Will Definitely Yield Big Gains

If the direction is wrong, what accumulates is only maintenance cost.

## References

- Flame graph, hotspot analysis, and performance regression practice materials
