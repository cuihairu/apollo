[English](q56-profiling-tools.en.md) | [中文](q56-profiling-tools.md)

# Q56: How to Do Performance Profiling? What Tools Are There?

## Core Conclusion

The first principle of performance profiling is not "optimize first", but "locate first".

Effective performance profiling usually has to answer three things:

- Where it is slow
- Why it is slow
- Whether this bottleneck is worth optimizing

Without clear metrics and evidence, the vast majority of optimizations become guessing.

## 1. First Distinguish "Feels Slow" from "Slow by Metric"

Many performance problems start from a felt sensation:

- Latency suddenly goes high
- Frame time jitter
- Memory keeps rising
- A certain scene gets laggy when crowded

But once you actually start analyzing, you must turn it into quantifiable metrics, for example:

- TPS
- Response time percentiles
- CPU hotspots
- Memory growth curves
- Network throughput

Otherwise it is hard to know whether the problem has been reproduced, and hard to verify whether an optimization really works.

## 2. Performance Profiling Is Usually Split by Layer

### 2.1 System Layer

Focus on:

- CPU
- Memory
- Disk I/O
- Network
- Context switching

### 2.2 Process Layer

Focus on:

- Which thread is busy
- Which module eats CPU
- Which allocations are growing

### 2.3 Business Layer

Focus on:

- Which message is heaviest
- Which scene is hottest
- Which kind of entity update is most expensive
- Which call chain times out most

Real production performance problems usually require looking at these three layers connected together.

## 3. Define the Analysis Path First, Then Choose Tools

There are many tools, but more is not always better.

A more practical approach is usually:

- First use lightweight metrics to discover anomalies
- Then use profiling to narrow the scope
- Finally do fine-grained analysis on the hotspots

In other words, monitoring is the entrance and the profiler is the magnifier; do not turn on heavy tools at full volume from the start.

## 4. Common Analysis Targets and Tool Directions

### 4.1 CPU Hotspots

Common things to look at:

- Which functions take the largest share
- Whether high-frequency small functions accumulate
- Whether there is lock waiting or spinning

### 4.2 Memory Problems

Common things to look at:

- Whether resident memory keeps rising
- Whether there are leaks
- Whether allocations are too frequent
- Where the large objects and hot containers are

### 4.3 Threads and Locks

Common things to look at:

- Which threads are blocked
- Whether there is lock contention
- Whether there is queue backlog

### 4.4 Network and I/O

Common things to look at:

- Which connections or message types are heaviest
- Whether there is send queue backlog
- Whether I/O waiting is abnormal

## 5. Tools Are Only Means; the Key Is the Chain of Evidence

A complete performance localization should ideally form a chain like this:

1. Monitoring discovers an anomalous metric
2. Logs or tracing narrow it to a specific scene
3. The profiler finds the hotspot function or hotspot path
4. After the fix, re-run stress tests or replay to verify

If the verification step is missing, it is easy to mistake "correlation" for "causation".

## 6. Production and Local Should Be Treated Separately

Local profiling is suitable for:

- Reproducing stable problems
- Deep analysis of hotspots

Production analysis is better suited for:

- Lightweight sampling
- Metric comparison
- Localized capture

Do not casually turn on high-overhead tools in production, otherwise the analysis itself may become a new source of problems.

## 7. Common Tool Categories

They differ by platform and language, but common ones can be categorized by use:

- System metrics tools
- CPU profiler
- Memory profiler
- Thread and lock analysis tools
- Network packet capture and throughput analysis tools
- tracing and flame graph

What truly matters is not memorizing tool names, but knowing which kind of tool to use for which kind of problem.

## 8. Common Misconceptions

### 8.1 First Optimize from Experience, Then See the Result

Often wasted effort, or even made worse.

### 8.2 High CPU Means the Algorithm Is Bad

It could also be caused by lock contention, ineffective polling, frequent allocations, or I/O waiting.

### 8.3 The Function with the Highest Share in Tool Output Must Be Optimized First

Not necessarily. You also need to check whether it is on the core path and whether it really affects user experience.

## References

- Linux perf / flame graph / tracing related materials
- Various server-side profiling and production observability practice materials
