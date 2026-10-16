[English](q75-thread-pool.en.md) | [中文](q75-thread-pool.md)

# Q75: How to Design a Thread Pool? How to Schedule Tasks?

## Core Conclusion

The value of a thread pool is not "throwing all tasks in and running them in parallel", but providing controlled execution resources for a class of parallelizable tasks.

What truly needs to be designed first is:

- Which tasks suit a thread pool
- Whether tasks are CPU-bound or I/O-bound
- What to do when the queue is full
- How to guarantee execution order and priority

If these boundaries are unclear, a thread pool easily becomes a black box of hidden latency and resource contention.

## 1. What Tasks a Thread Pool Really Suits

Commonly better suited to a thread pool:

- Background I/O
- Compression and serialization
- Database tasks
- Non-real-time statistics and offline processing

Usually not suitable for throwing straight into a general thread pool:

- Strongly ordered state machine tasks
- Logic needing single-object serial progression
- High-consistency object updates in the main combat chain

## 2. A Thread Pool Is Not Better the Larger It Is

Common problems with too many threads:

- More context switching
- Worse lock contention
- Worse cache locality
- Threads idling

So the thread pool size usually should revolve around:

- CPU core count
- Task type
- The blocking proportion of external dependencies

Rather than "open as many as you can".

## 3. Task Queue Design Is More Critical than Thread Count

In a thread pool system, what really tends to go wrong is usually not the threads but the queue:

- Enqueue too fast
- Dequeue cannot keep up
- Priority chaos
- Long tasks blocking short tasks

So you must make clear:

- Whether the queue is bounded
- Whether priorities are separated
- Whether different task types get independent queues

## 4. Scheduling Must First Distinguish Task Types

### 4.1 CPU-bound Tasks

Focus more on:

- Core count utilization
- Avoiding over-switching

### 4.2 I/O-bound Tasks

Focus more on:

- Blocking waits
- Timeout control
- Callbacks or result delivery

Mixing these two task types in one pool often causes them to drag each other down.

## 5. The Result Return Path Must Also Be Designed

A thread pool is not "the task finishes and that's it"; it must also answer:

- Who receives the result
- Whether to switch back to the main thread or owner thread
- How failures propagate

If this layer is unclear, the thread pool is just throwing complexity at the caller.

## 6. A More Sound Approach in Engineering

Common practices are:

- Split the thread pool by task type
- Bounded queues with backpressure strategies
- Separate long tasks from short tasks
- Uniformly dispatch results back to the authoritative thread or event loop

Only then can you avoid the situation of "the pool is busy, but the system is slower".

## 7. Common Misconceptions

### 7.1 A Thread Pool Is a Concurrency Speeder

Wrong. It can only speed up tasks suitable for parallelism; it cannot replace concurrency design with clear boundaries.

### 7.2 Just Throw All Blocking Operations Into a Thread Pool

Without rate limiting and tiering, it only piles blocking onto another place.

### 7.3 A Single Global Thread Pool Is Simplest

Simple, but it easily lets different task types interfere with each other.

## References

- Thread pool scheduling, backpressure, and asynchronous task dispatch practice materials
