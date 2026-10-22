
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q29: What Are the Use Cases for Redis in an MMO?

## Core Conclusion

Redis suits being the data layer for high-frequency access, low latency, and acceptable memory cost, but it is not a universal primary store for MMOs.

A more reasonable positioning is usually:

- Cache layer
- Leaderboard and counter layer
- Distributed coordination and lightweight state layer
- Short-term message or task relay layer

Treating Redis as the single source of truth for all business later usually costs a lot in persistence, auditing, and consistency.

## I. What Problems Redis Is Best at Solving

Redis's advantages are mainly:

- Low-latency reads and writes
- Rich data structures
- Convenient atomic operations
- Suited to hot data

So it is very well suited to data scenarios that:

- Are accessed frequently
- Are not structurally complex
- Can accept in-memory storage cost
- Need fast aggregation or sorting

## II. Common Use Cases in MMOs

### 1. Leaderboards

This is one of the most classic use cases.

The reasons are simple:

- Rank queries are frequent
- Top N queries naturally suit sorted sets
- Score updates are relatively direct

### 2. Online State and Lightweight Session Information

For example:

- Whether online
- Online gateway
- Last active time
- Simple scene markers

This kind of data is updated frequently and read frequently, so it is very well suited to be carried by Redis.

### 3. Hot Cache

For example:

- Player basic profile summary
- Guild summary information
- Event configuration
- Hot trade item prices

Its key value is blocking hot reads from the database.

### 4. Counters and Rate Limiting

For example:

- Login rate limiting
- API frequency limits
- Event participation counts
- Server-wide statistical counters

### 5. Distributed Coordination

For example:

- Simple locks
- Task placeholders
- Idempotency markers
- Node heartbeat summaries

But be restrained here; don't stuff all complex coordination into Redis scripts.

## III. Scenarios Not Suitable to Hand Entirely to Redis

### 1. High-value Asset Ledgers

For example:

- Final currency ledger
- Item master archive
- Trade transaction master records

This kind of data needs more:

- Persistence reliability
- Audit trails
- Clear transaction boundaries

Redis can assist, but should not easily become the single source of truth.

### 2. Complex Queries and Reports

When multi-condition filtering, aggregate analysis, and historical reports are involved, relational databases or OLAP systems are usually more suitable.

### 3. Large Cold Objects

Low-frequency large objects kept in memory long-term are usually not cost-effective.

## IV. Redis Is Very Powerful When Used Well, and Very Dangerous When Used Badly

Common risks include:

- Hot keys
- Large keys
- Cache avalanches
- Persistence jitter
- Writing the cache as if it were the primary store

Especially leaderboards, hot events, and server-wide statistics easily run into single-key overheating.

## V. A More Pragmatic Way to Use It in Engineering

A common combination is:

- MySQL or another persistence layer keeps the master archive
- Redis carries hot reads, leaderboards, sessions, and short-term state
- Key modifications defer to the database or reliable transaction logs
- Redis data can be rebuilt or backfilled

This makes the system easier to recover when Redis fluctuates.

## VI. Common Misconceptions

### 1. Redis Is Fast, So All Data Should Go In

No. Fast does not mean suitable as the authoritative store for all business.

### 2. Having Persistence Means It Can Be the Database Primary Store

Redis's persistence capabilities and a relational database's transaction, indexing, and audit goals are not the same.

### 3. Distributed Locks Solve All Concurrency Problems

Many problems are better suited to single-authoritative writes or state-machine constraints, rather than adding locks everywhere.

## References

- Redis official documentation
- Various online-game materials on Redis leaderboards, hot caching, and online state practices
