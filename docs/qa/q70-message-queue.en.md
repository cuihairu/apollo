[English](q70-message-queue.en.md) | [中文](q70-message-queue.md)

# Q70: How to Design a High-performance Message Queue?

## Core Conclusion

The core of a high-performance message queue is not the two words "lock-free", but making the relationship between producing, queuing, consuming, and back pressure clear enough.

What really needs to be designed first is:

- Single producer or multi-producer
- Single consumer or multi-consumer
- Whether there are priorities
- What to do when the queue is full

If these boundaries are unclear, even a highly performant queue implementation will easily get out of control in a real system.

## I. First Clarify What Problem the Message Queue Solves

Common uses include:

- Decoupling between threads
- Smoothing production and consumption rates
- Asynchronizing expensive tasks
- Batch processing messages

So a message queue is essentially not just a data structure, but also a flow regulator.

## II. Queue Design First Looks at the Concurrency Model

The most common models include:

- SPSC: single producer, single consumer
- MPSC: multi-producer, single consumer
- MPMC: multi-producer, multi-consumer

This step is very critical, because different models determine:

- How much synchronization cost is needed
- Whether lock-free is worth it
- How high the implementation complexity gets

Don't default to the most complex model from the start.

## III. Bounded or Unbounded Is a Key Trade-off

### 1. Bounded Queue

Advantages:

- Controllable memory
- Easier to express back pressure

Disadvantages:

- When full, you must decide whether to drop, block, or degrade

### 2. Unbounded Queue

Advantages:

- Producers are not blocked immediately

Disadvantages:

- Easily turns the problem into memory bloat and tail latency

In online services, most critical queues ultimately lean bounded, because "queueing infinitely" usually does not really solve the problem.

## IV. Priority and Fairness Must Be Thought Through in Advance

Some messages are naturally more important than others, for example:

- Critical control messages
- High-value business events
- High-frequency low-value state updates

If everything is mixed in one FIFO queue, peak hours easily let low-value messages clog the critical path.

But priority also has costs:

- More complex implementation
- Easy to starve
- Harder to debug

## V. Back Pressure Strategy Matters More Than Throughput Numbers

When consumers cannot keep up, you must be explicit about:

- Blocking producers
- Dropping low-priority messages
- Merging stale messages
- Degraded handling

This is one of the most important real problems of a queue system.

Many systems don't die from "enqueueing too slowly", but from "too much queued and no one can process it all".

## VI. Batch Processing Is Usually Very Valuable

For many message queues, batch dequeue and batch processing significantly reduce:

- Lock overhead
- Atomic operation cost
- Context switching

But the batch window cannot be too large, otherwise it increases latency and jitter.

## VII. Lock-free Queues Are Not the Default Answer

Lock-free structures suit scenarios with clear hotspots and stable concurrency models, but they also bring:

- Rising code complexity
- Difficult debugging
- Memory ordering problems

If the concurrency model is simple, a locked but low-contention implementation may actually be more stable.

## VIII. A More Pragmatic Design in Engineering

Common practice is:

- First choose the simplest viable structure according to the concurrency model
- Keep queues bounded where possible
- Be explicit about the back pressure strategy when full
- Support priority or separate queues on critical paths
- Use batch consumption to reduce scheduling cost

This fits real production needs better than purely chasing theoretical throughput.

## IX. Common Misconceptions

### 1. Lock-free Queues Are Definitely Fastest

Not necessarily. It depends on the concurrency model, contention pattern, and implementation quality.

### 2. Unbounded Queues Are Safer

More often than not it just turns the problem from blocking into a memory and latency disaster.

### 3. FIFO Is Enough

In peak scenarios, critical control messages and low-value messages usually should not have exactly equal weight.

## References

- Materials on concurrent queues, back pressure, and async task scheduling practices
