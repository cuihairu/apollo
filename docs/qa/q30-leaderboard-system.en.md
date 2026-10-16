[English](q30-leaderboard-system.en.md) | [中文](q30-leaderboard-system.md)

# Q30: How Do You Design a Leaderboard System?

## Core Conclusion

What a leaderboard system truly has to solve is not "sorting people by score", but handling all of the following at once:

- Rank update cost
- Query hotspots
- Periodic settlement
- Cross-server aggregation
- Reward distribution and auditing

In most projects, the leaderboard is not a single-table-structure problem, but a "real-time view plus settlement flow" problem.

## 1. First Clarify the Leaderboard Type

Different leaderboards have completely different design priorities.

Common types include:

- Real-time growth boards, such as level and power
- Competitive boards, such as points and rank tiers
- Periodic event boards, such as weekly boards and season boards
- Guild or cross-server boards

You need to distinguish first:

- Whether it is mainly real-time display, or mainly settlement rewards
- Whether it is single-server or cross-server
- Whether it only queries Top N, or queries individual ranks in large volume

## 2. The Two Core Types of Leaderboard Access

Two are usually indispensable:

### 1. Top N Query

For example, top 10, top 100, top 1000.

### 2. Individual Rank Query

For example, "what is my rank now" and "who are the people near me".

If these two kinds of queries are not designed in advance by access pattern, they easily become slow later.

## 3. Why Leaderboards Are Not Suited to Scanning the Database Every Time

The reasons are direct:

- Frequent queries
- High sorting cost
- Concentrated hotspots

So most real-time leaderboards are put into structures more suited to sorting and fast queries, such as:

- Redis sorted sets
- In-memory sorting structures
- Dedicated aggregation services

The database is better suited to holding persisted snapshots and settlement results, rather than directly bearing all real-time ranking queries.

## 4. Real-Time Boards and Settlement Boards Must Be Considered Separately

### 1. Real-Time Board

More concerned with:

- Fast score updates
- Fast Top N queries
- Fast individual rank queries

### 2. Settlement Board

More concerned with:

- The final result at the deadline
- Reward distribution order
- Auditing and review

Many incidents come from treating the "still-changing real-time board" directly as the "final settlement basis".

The safer approach is usually:

- Generate a frozen snapshot at settlement time
- Distribute rewards by the frozen result

## 5. Why Cross-Server Leaderboards Are Harder

Because it is not only a sorting problem; it also involves:

- Aggregation of data from multiple servers
- Update frequency control
- Consistent definitions
- Acceptable latency range

Many cross-server boards are not suited to full real-time synchronization on every change. Common practice is:

- Each server first maintains a local board
- The aggregation layer merges by period or in streaming fashion
- The client accepts short-term delay

## 6. Reward Distribution Must Be Decoupled from Ranking

The leaderboard display being off by a second or two is usually only an experience problem; a reward distributed wrong is a serious incident.

So the settlement link usually requires:

- A frozen leaderboard
- Idempotent reward distribution
- Reward transaction records
- Compensation capability

Do not directly use "the latest board seen by the client" as the basis for reward distribution.

## 7. The Common Robust Structure in Engineering

Many projects eventually end up with:

- Redis or in-memory structures carrying real-time ranking
- The database saving character basic data and settlement snapshots
- A separate settlement task handling season or event rewards
- The high-value reward link has idempotency and auditing

This way responsibilities are clearer.

## 8. Common Misconceptions

### 1. The Leaderboard Is Just Redis ZSet

ZSet is very common, but it only solves part of the sorting and query problem; it does not automatically solve settlement, reward distribution, or cross-server aggregation.

### 2. Rankings Must Be Absolutely Real-Time

Many scenarios do not need it. Paying high system cost for full real-time within a few seconds is often not worthwhile.

### 3. Leaderboard Display and Reward Distribution Can Share the Same Link

The risk is high. Display allows small delays; rewards cannot be wrong.

## References

- Redis Sorted Set official documentation
- Various online game season board and event board settlement practice materials