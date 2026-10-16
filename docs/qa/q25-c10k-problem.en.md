[English](q25-c10k-problem.en.md) | [中文](q25-c10k-problem.md)

# Q25: What Determines the Connection Limit? How to Break Through the C10K Problem?

## Core Conclusion

The connection limit is never a single "network library metric", but the result of the operating system, protocol stack, memory, CPU, event model, and business load acting together.

The so-called C10K is no longer a mysterious threshold today; what is truly difficult is continuing to guarantee under high connection counts:

- Stable latency
- Controllable broadcasting
- Heartbeat scanning without overload
- A single connection anomaly does not bring down the whole system

So "being able to connect 10,000 sockets" and "being able to stably serve 10,000 online players" are not the same thing.

## 1. What Actually Affects the Connection Limit

### 1.1 File Descriptors and Kernel Parameters

Every connection consumes system resources; the most intuitive are:

- File descriptor limit
- Listen backlog
- Socket buffers
- Ports and kernel network parameters

These are basic thresholds, but usually not the final bottleneck.

### 1.2 Per-Connection Memory Cost

A point easily overlooked in high-connection systems is "how much memory each connection actually occupies".

Besides the kernel socket itself, it also includes:

- Send buffer
- Receive buffer
- Session object
- Timers
- Authentication information
- Pending send queue

If each connection wastes a few dozen KB extra, tens of thousands of connections quickly mean hundreds of MB to GB of overhead.

### 1.3 Event Model and Thread Model

If you still use "one thread per connection", C10K is quickly dragged down by thread stacks, scheduling overhead, and context switching.

High-connection systems usually rely on:

- `epoll`
- `kqueue`
- `IOCP`
- Event-driven or few-thread Reactor/Proactor models

### 1.4 Business Processing Cost

A high connection count is not necessarily scary; what is scary is:

- Every connection sends messages at high frequency
- Every message requires complex decoding, authentication, and routing
- Every update triggers large-scale broadcasting

The actual bottleneck in many MMOs is not accepting connections, but the business load that comes after connections are established.

## 2. Why "One Thread Per Connection" Cannot Hold Up

The problem with this model is not that it cannot work in concept, but that its cost expands linearly too fast:

- Thread stacks occupy memory
- Scheduler overhead increases
- Lock contention worsens
- Even idle connections must be managed by threads

Once the connection count is high, the proportion of CPU time spent on scheduling and synchronization becomes increasingly absurd.

So the real breakthrough of the C10K era was switching from the blocking model to the event-driven multiplexing model.

## 3. Today, the Focus of Discussing C10K Has Changed

In modern systems, a single machine carrying tens of thousands or more long connections is not uncommon. What truly needs to be answered is:

- Whether these connections are all active
- How heartbeat and timeout scanning are done
- How broadcasting and routing are shared out
- How slow connections are isolated
- Whether the gateway and the logic server are decoupled

In other words, the difficulty has shifted from "whether 10,000 connections can be accepted" to "how to keep the system stable under large-scale connections".

## 4. How to Raise the Connection Count in Engineering

### 4.1 Use Event-Driven I/O

This is the prerequisite.

The typical approach is:

- A small number of I/O threads handle sending and receiving
- Business processing threads or logic threads are decoupled
- Connection state advances in the event loop

This can significantly reduce thread count and scheduling cost.

### 4.2 Separate the Access Layer from the Business Layer

A common MMO practice is for the gateway to carry connections alone, while the business server handles logic.

The benefits are:

- Connection jitter does not directly hit the logic server
- TLS, heartbeat, rate limiting, and compression can be handled centrally
- The gateway can scale out horizontally

If the logic server directly carries a large number of public connections, scaling out and fault isolation both become harder.

### 4.3 Control the Per-Connection State Size

This point is very plain, but extremely critical.

Common optimizations include:

- Shrink session objects
- Avoid per-connection independent large buffers
- Lazily allocate heavy resource objects
- Move rarely accessed data out of the hot path

### 4.4 Manage Slow Connections Well

High-connection systems greatly fear a few slow connections blowing up the send queue.

Clear strategies are needed:

- Send queue limit
- Expired state messages can be overwritten or dropped
- Connections that chronically keep up poorly are actively disconnected

Otherwise the broadcast system will be amplified by backpressure.

## 5. In MMOs, the Connection Count Is Not the Only Metric

With the same 10,000 connections, the pressure can be completely different.

For example:

- 10,000 idle connections that almost only send heartbeats
- 10,000 players fighting on the same screen with high-frequency state broadcasting
- 10,000 dispersed online players with only a few interactions within AOI

In these three cases, the real bottleneck may fall on:

- Heartbeats and timers
- Broadcasting and serialization
- Routing and state synchronization

So evaluating connection capacity must be combined with the online behavior model, not just the socket count.

## 6. How to Continue from C10K Toward Larger Scale

### 6.1 Scale Out the Gateway Horizontally

Evenly spreading connections across multiple access nodes is usually more realistic than infinitely squeezing a single machine.

### 6.2 Business Partitioning

Let different scenes, rooms, shards, and logical domains be handled by different nodes, avoiding all connections eventually converging on the same logical hotspot.

### 6.3 Reduce Invalid Traffic

Including:

- AOI culling
- Batch sending
- Compression
- Lowering the frequency of low-priority messages

Reducing the average cost per connection is often more effective than continuing to tweak kernel parameters.

### 6.4 Complete Observability

Must continuously observe:

- Active connection count
- Heartbeat timeout count
- Per-connection send/receive rate
- Send queue backlog
- Event loop latency
- GC or main thread stalls

Otherwise it is hard to know where the real bottleneck is.

## 7. Common Misconceptions

### 7.1 Raising `ulimit` Counts as Solving C10K

This is just the admission ticket, not the final answer.

### 7.2 The Higher the Connection Count, the Stronger the System

Meaningless. More important is whether the system can still maintain stable latency and recoverability under the target business model.

### 7.3 If a Single Machine Can Handle Many Connections, the Gateway Layer Is Unnecessary

Wrong. Connection carrying, protocol handling, rate limiting, TLS, observability, and business logic are best kept in separate layers.

## References

- Dan Kegel, *The C10K Problem*
- Linux `epoll` / Windows IOCP related materials
- Various long-connection gateway and MMO access layer practice materials
