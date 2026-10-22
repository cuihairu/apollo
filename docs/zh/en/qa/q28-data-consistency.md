
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q28: How to Solve Data Consistency Problems?

## Core Conclusion

When talking about consistency in online games, you cannot vaguely only talk about distributed transactions. A more useful approach is to first ask:

- Which data must be strongly consistent
- Which data allows a short-term delay to consistency
- Which operations can be solved by idempotence and compensation

The real engineering solution is usually not "one transaction to rule them all", but handling things in layers according to data value and business consequences.

## 1. First Clarify Where the Consistency Problem Occurs

Common sources include:

- Concurrent updates to the same data
- Multiple services each holding partial state
- Cache and database being out of sync in sequence
- Retries causing duplicate execution
- Cross-server or asynchronous chains experiencing partial success

So consistency is not just a database transaction problem, but also a system boundary problem.

## 2. Which Scenarios Require Stronger Consistency

Typically including:

- Currency and assets
- Trading
- Auction deals
- Mail attachment claims
- Top-up crediting

Once these chains go wrong, it is not just numerical jitter, but a direct impact on user assets and reconciliation.

More common practices are:

- Single authoritative write
- Idempotent operation IDs
- Reliable persistence
- Audit ledger

## 3. Which Scenarios Can Accept Eventual Consistency

For example:

- Leaderboard display
- Friend online status
- World announcement distribution
- Certain cache views

This kind of data allows short-term delay, as long as it eventually converges.

The key is not to wrongly stuff them into heavy transaction chains.

## 4. Several Common Means of Solving Consistency

### 4.1 The Single Writer Model

Many game systems have a certain class of data always modified by a unique authoritative node.

The benefits are:

- Few concurrent conflicts
- Clear logic
- No large-scope locks needed

This is very common in player entities, scene entities, and inventory primary objects.

### 4.2 Database Transactions

Local transactions are suitable for strong-consistency modifications within a single database, for example:

- Deducting money and adding items
- Mail status plus attachment claim flag

But the premise is that the operation boundary is clear, and the transaction is not made too large.

### 4.3 Idempotence

As long as a chain has timeouts, retries, and compensation, it must have idempotent capability.

Typical practices:

- Business-unique IDs
- Deduplication tables
- Unique constraints
- Duplicate requests return the old result

### 4.4 Events and Compensation

In cross-service chains, many times more realistic than forcing distributed transactions is:

- Clearly defining the primary operation
- Recording events
- Downstream asynchronous consumption
- Doing compensation or replay on error

## 5. Do Not Overuse Distributed Transactions

Online games are not prohibited from doing distributed transactions, but many chains are not suitable for them.

Reasons include:

- Long call chains
- Latency sensitivity
- Common node fluctuation
- Complex rollback semantics

In many businesses, single authority plus idempotence plus ledger is more maintainable than 2PC.

## 6. How to Handle Cache Consistency

Cache consistency is one of the most common practical problems.

Common strategies include:

- Writing the database first, then deleting the cache
- Letting the authoritative node be responsible for updating the cache
- Setting short TTLs for hot data
- Avoiding multi-writer caches on critical chains

The focus is reducing multi-write entry points, rather than fantasizing about the cache being forever fully in sync with the database.

## 7. A More Sound Combination in Engineering

Many systems eventually converge to:

- Asset operations: single authoritative write plus local transactions plus ledger
- Cross-service updates: event-driven plus idempotent consumption
- Display data: allow eventual consistency
- Cache: as an acceleration layer, not as the sole source of truth

This is more realistic than "everything is strongly consistent".

## 8. Common Misconceptions

### 8.1 Consistency Is Just a Database Isolation Level Problem

Far more than that. Many errors come from retries, caches, asynchronous consumption, and multi-service writes.

### 8.2 As Long as You Use Transactions, Nothing Goes Wrong

Transactions can only cover the data boundary they live in; cross-service, cross-cache, and cross-queue problems remain.

### 8.3 All Data Should Pursue Strong Consistency

The cost is usually too high, and much of the display data is not worth it.

## References

- Various online game asset chain, idempotence, and compensation design practice materials
