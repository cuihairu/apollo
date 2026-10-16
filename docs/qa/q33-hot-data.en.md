[English](q33-hot-data.en.md) | [中文](q33-hot-data.md)

# Q33: How do you handle hot data?

## Core Conclusion

The essence of the hot data problem is not "low cache hit rate", but "the same batch of data gets overwhelmed by too many requests concentrated in a short time".

To handle hot data you must first distinguish which kind of hot spot it is:

- Read hot spot
- Write hot spot
- Mixed hot spot
- Burst hot spot

Different hot spots have very different solutions; you cannot answer "throw Redis at it" for all of them.

## One: What Hot Data Usually Looks Like

Common hot spots in online games include:

- Top-of-leaderboard data
- Popular event state
- Guild summaries
- Player basic profiles during login
- Popular auction items or market prices

Their common characteristics are:

- Many people access it simultaneously
- The access pattern is highly concentrated

## Two: First Separate Read Hot Spots from Write Hot Spots

### 1. Read Hot Spot

Typical characteristics:

- Large numbers of queries hitting the same batch of data
- Writes are not that frequent

Common solutions:

- Local cache
- Redis cache
- Preheating
- Read/write separation

### 2. Write Hot Spot

Typical characteristics:

- Many requests modifying the same state simultaneously

For example:

- World Boss damage accumulation
- Event leaderboard points
- Flash-sale inventory

This kind of problem needs more of:

- Single authoritative write
- Sharded aggregation
- Queue serialization

Just adding a cache usually does not help.

## Three: Mixed Hot Spots Are the Hardest

Data that is both heavily read and frequently written is the most prone to problems.

For example:

- Real-time leaderboards
- Popular market prices
- Popular guild state

This kind of data often requires:

- Read/write layering
- Local caching
- Asynchronous aggregation
- Lowered requirements for display real-time-ness

## Four: Cache Is Not a Panacea

Cache is very effective for read hot spots, but be careful with write hot spots.

The reasons are simple:

- A hot key can still overwhelm a single-point Redis
- High-frequency invalidation creates jitter
- Multiple write entry points amplify consistency problems

So the first step of hot spot governance is usually not "cache first", but "reduce unnecessary access and writes first".

## Five: Common Governance Techniques

### 1. Multi-Level Cache

Suitable for read hot spots:

- In-process cache blocks the hottest reads
- Redis carries the shared cache
- The database serves as the final persistence layer

### 2. Sharding and Bucketing

Suitable for write hot spots or large-scale aggregation:

- Split by player, partition, or time window
- Aggregate afterwards

### 3. Single Writer Model

Let a certain category of hot data be modified by only one authoritative node; other nodes only read or synchronize asynchronously.

### 4. Frequency Reduction and Snapshotting

Not all hot data needs millisecond-level real-time.

Many display-oriented data can:

- Refresh once every few seconds
- Merge updates in batches
- Only show the most recent snapshot

## Six: Why Hot Data Is Often Bound Up with Architecture Problems

Because a hot spot is often not just the data itself being hot, but the system directing all requests to the same write point or read point.

So what you really need to look at is:

- Whether there is a single-point write entry
- Whether there is single-key aggregation
- Whether every request must query the same object

Many so-called hot spot problems are essentially system boundary design problems.

## Seven: A More Pragmatic Approach in Engineering

A common combination is:

- Read hot spots go through multi-level cache
- Write hot spots go through single-authoritative updates or queue aggregation
- Display data tolerates short delays
- Hot keys are split or locally aggregated
- Key results retain a persisted source of truth

## Eight: Common Misconceptions

### 1. The Hot Spot Problem Is the Database Being Too Slow

In many cases the real hot spot has already formed at the cache layer or even the application layer.

### 2. All Hot Spots Can Be Solved with Redis

Read hot spots often can; write hot spots and mixed hot spots not necessarily.

### 3. Hot Data Must Be Completely Real-Time

Many display-oriented hot spots can tolerate snapshots or second-level delays, which is often more cost-effective.

## References

- Various online game hot spot cache, hot key governance, and aggregated write practice materials
