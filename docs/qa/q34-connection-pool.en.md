[English](q34-connection-pool.en.md) | [中文](q34-connection-pool.md)

# Q34: How to design a database connection pool?

## Core Conclusion

The goal of a connection pool is not "open more connections," but to steadily control the cost of database access.

A good connection pool has to at least solve:

- Connection creation and reuse
- Upper-limit control
- Idle reclamation
- Health checks
- Timeout and leak governance

The real problem usually lies not in how to write a pool, but in how to stop the business from treating the database as an infinite resource.

## 1. Why a connection pool is needed

Establishing a database connection is not cheap. It includes:

- TCP connection setup
- Database authentication handshake
- Session initialization

If you create a connection on the fly for every query, both latency and resource overhead are high.

The value of a connection pool is mainly twofold:

- Reuse already established connections
- Control the upper limit of concurrent database access

The latter is often more important than the former, because a pool is essentially still a rate limiter.

## 2. What a connection pool actually has to control

### 1. Maximum concurrent connections

A pool cannot only look at the application-side demand. It also has to look at the database's actual capacity.

If the application borrows connections without limit, the database will be directly overwhelmed.

### 2. Waiting strategy

When the pool is full, you need to make explicit:

- Blocking wait
- Fast fail
- Bounded wait

Different business paths suit different strategies. In online games, the main path is usually better suited to short waits or fast failure than to unlimited queuing.

### 3. Connection lifetime

Connections are not permanently valid. You have to handle:

- Idle timeout
- Half-open connections
- Connections invalidated after a database restart
- Old connections that have not been used for a long time

## 3. Pool parameters are not the bigger the better

When many systems run into problems, the first reaction is "make the pool bigger." That usually only postpones the failure.

Common problems from an oversized pool include:

- More severe database contention
- Slow queries get amplified
- Business threads pile up more requests

So the pool size should be based on:

- Database CPU and concurrency capability
- Query types
- Average latency
- Peak traffic model

## 4. Why health checks matter

A connection pool cannot assume that the connections in the pool are always usable.

Common approaches include:

- Lightweight validation before lending out
- Periodic background liveness probing
- Marking as broken and rebuilding when errors occur

Without this layer, the business will frequently get broken connections, and the error will spread upward.

## 5. Connection leaks are one of the most common real-world problems

The stability of a connection pool depends heavily on whether connections that were lent out and never returned can be discovered.

Common governance measures:

- RAII or scope-based ownership
- Borrow-timeout alerts
- Long-occupancy logging
- Connection usage chain tracing

Without leak detection, no matter how big the pool is, it will slowly be drained empty.

## 6. The thread model and the connection pool have to match

Do not design every thread as "able to grab a database connection at any time."

A more reliable approach is usually:

- Asynchronous tasks or dedicated database worker threads do the concentrated access
- Main logic threads avoid blocking on SQL for long periods
- Heavy and light queries use separate pools or separate channels

That makes tail latency easier to control.

## 7. A more practical engineering design

A common, reliable combination is:

- Warm up a small number of base connections
- Set explicit upper limits
- Borrowing connections carries a timeout
- Full-chain monitoring of borrow and return
- Broken connections are automatically removed
- Idle connections are reclaimed on demand

If query load varies a lot, you can further split:

- Primary write connection pool
- Read-only connection pool
- Background task connection pool

## 8. Common misconceptions

### 1. A bigger pool means higher throughput

No. The database itself has a concurrency ceiling. Beyond that you only intensify contention.

### 2. Having a connection pool means there will be no database bottleneck

No. A pool can only reuse and rate-limit. It cannot mask slow SQL or wrong access patterns.

### 3. It is more convenient to hold a borrowed connection the whole time

This reduces pool reuse efficiency and makes resource starvation more likely.

## References

- Common database connection pool design and high-concurrency access governance practice materials
