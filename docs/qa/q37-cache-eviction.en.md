[English](q37-cache-eviction.en.md) | [中文](q37-cache-eviction.md)

# Q37: How to Implement Cache Eviction Strategies for Data?

## Core Conclusion

The goal of a cache eviction strategy is not simply to raise the hit rate, but to prioritize keeping the most valuable data within limited memory.

What really needs to be thought through first is:

- What data is in the cache
- Whether this data is read-hot or write-hot
- What is the cost after eviction
- Whether short-term dirty reads or origin-fetch jitter is acceptable

Talking only about `LRU` and `LFU` while ignoring data value is usually of little meaning.

## 1. First Judge Which Data Is Suitable for the Cache

Not all data is worth caching.

More suitable for caching are usually:

- Read more, write less
- High origin-fetch cost
- Concentrated access
- Clear lifecycle

For example:

- Configuration
- Leaderboard summaries
- Player profile summaries
- Popular guild information

## 2. Eviction Strategy Is Strongly Tied to Data Type

### 2.1 LRU

Suitable for scenarios where "data accessed recently is likely to be accessed again".

Its advantage is being simple and intuitive, but it is not necessarily stable for periodic scans and bursty access.

### 2.2 LFU

Suitable for "long-term stable hotspots" scenarios.

For example, long-term popular leaderboards or continuously high-frequency accessed public data.

But LFU tends to let old hotspots occupy slots long-term, so a decay mechanism is often needed.

### 2.3 TTL-Driven

Suitable for data that inherently has a clear lifecycle.

For example:

- Temporary event state
- Session summaries
- Rate limiting counters

This kind of data is often better managed directly by expiration time than by complex eviction algorithms.

## 3. Eviction Strategy Cannot Rely on a Single Global Rule

The problem with many systems is that:

- All caches share one big pool
- All keys share one eviction strategy

This causes low-value large objects to evict high-value hotspots.

A more sound approach is usually:

- Split cache domains by module
- Set different TTLs and capacities for different data
- Protect critical hotspots separately

## 4. What Happens After Eviction Matters More

The real cost of cache eviction lies in origin fetching.

First judge:

- Whether hitting the database will form a spike
- Whether the origin data is expensive
- Whether eviction triggers mass reconstruction

If evicting a batch of hot keys would blow up the database directly, no hit rate number matters.

## 5. Common Governance Means in Engineering

### 5.1 Preheating

Preheating predictable hotspots reduces cold-start jitter.

### 5.2 Tiered Caching

For example:

- Local cache
- Redis cache
- Database origin fetch

### 5.3 Large Key Control

Cache eviction failures are often not algorithm problems, but large keys distorting the memory structure.

### 5.4 Protecting Critical Data

Certain core hotspots should not be entirely left to natural eviction; they need more stable capacity and TTL strategies.

## 6. Redis Eviction Strategy Is Only a Baseline, Not a Complete Solution

Redis strategies such as `allkeys-lru` and `allkeys-lfu` are very useful, but they only decide who is deleted first when memory is tight.

Real system design must also add:

- Key design
- TTL design
- Hotspot tiering
- Origin-fetch rate limiting
- Cache breakdown protection

## 7. Common Misconceptions

### 7.1 LRU Is Always the Best

There is no universal optimum; it depends on the access pattern.

### 7.2 A High Hit Rate Means Good Cache Design

Not necessarily. Whether what is hit is high-value requests, and whether origin fetching is smooth, are equally important.

### 7.3 Eviction Strategy Can Be Thought About Later

Once the cache reaches scale, the eviction strategy directly affects database stability and business experience.

## References

- Redis memory eviction strategy documentation
- Various online business multi-tier cache and hotspot origin-fetch governance practice materials
