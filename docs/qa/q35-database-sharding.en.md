[English](q35-database-sharding.en.md) | [中文](q35-database-sharding.md)

# Q35: How do you implement database sharding?

## Core Conclusion

Sharding is not to "look architecturally advanced", but when a single database can no longer hold up in capacity, throughput, maintenance window, or failure domain, only then does the data need to be split.

What really matters is not how many shards you split into, but first answering:

- Why shard
- Shard by which key
- How to scale out later
- How cross-shard queries and cross-shard transactions are brought to closure

If these questions are not thought through, sharding only copies the problem from one database into many databases.

## 1. First judge whether sharding is really needed

Before actually sharding, many systems still have cheaper means:

- Index optimization
- Read/write splitting
- Hot/cold tiering
- Archiving historical data
- Caching hot data

If none of these have been done, sharding too early usually only increases complexity.

## 2. The two most common kinds of splitting

### 1. Horizontal splitting

Distribute the same kind of data by row into different databases or tables.

Common for:

- Player main profiles
- Inventories
- Mail
- Log flows

### 2. Vertical splitting

Split different business modules into different databases.

For example:

- Account database
- Character main profile database
- Social database
- Auction database

This is more like splitting by business boundary, not by data volume.

## 3. Choosing the sharding key matters most

The sharding key determines:

- Whether data distribution is even
- Whether queries are easy to route
- Whether scale-out is easy

Common sharding keys in online games include:

- `player_id`
- `account_id`
- `server_id`
- Time dimension

But there is no universal key; the key depends on the business access pattern.

For example, logs are very suited to split by time, while character main profiles are more suited to split by character or account.

## 4. What sharding fears most

What is feared most is that the query and transaction model does not match the sharding key.

Common costs include:

- Cross-shard aggregation becomes heavy
- Cross-shard transactions are complex
- Scale-out migration is difficult
- Hotspots concentrate on a few shards

So the sharding key cannot only look at "easy to compute", but also at "how to query, how to migrate, how to scale out later".

## 5. Scale-out must be thought through on day one

Many systems run fine the first time they shard, but the second scale-out starts to hurt.

What needs to be considered in advance:

- Whether the shard count is expandable
- How to migrate between old and new shards
- How to upgrade routing rules
- Whether historical data is moved back

Consistent hashing, logical shard numbers, and routing layer abstractions all serve later scale-out.

## 6. Do not easily beautify cross-shard transactions

Once cross-shard writes increase, system complexity rises noticeably.

A more pragmatic approach is usually:

- Let the same business object land in the same shard as much as possible
- Reduce cross-shard transactions on critical paths
- Use idempotency and compensation when necessary

Rather than pressing all cross-shard consistency onto distributed transactions from the start.

## 7. A more realistic sharding approach in online games

In many cases a combined approach is used:

- Player main data is sharded by `player_id`
- Logs and flows are sharded by time
- Cross-server data is sharded by `server_id` or by region
- High-value modules get their own databases

This is more practical than forcing one rule to cover all tables.

## 8. Common misconceptions

### 1. Sharding is just taking a modulus

Modulus is only the simplest routing method; it does not equal a complete sharding solution.

### 2. Shard into 64 pieces first and it's worry-free later

No. Too many empty shards and unreasonable routing equally bring operational costs.

### 3. Performance problems naturally disappear after sharding

Hotspots, unreasonable queries, and cross-shard aggregation can still drag the system down.

## References

- Various online game database sharding and time-based archiving practice materials
